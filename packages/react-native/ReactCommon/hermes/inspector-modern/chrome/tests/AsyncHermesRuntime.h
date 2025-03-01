/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include <folly/futures/Future.h>
#include <hermes/hermes.h>
#include <hermes/inspector-modern/detail/SerialExecutor.h>
#include <jsi/jsi.h>

namespace facebook {
namespace hermes {
namespace inspector_modern {
namespace chrome {

/**
 * AsyncHermesRuntime is a helper class that runs JS scripts in a Hermes VM on
 * a separate thread. This is useful for tests that want to test running JS
 * in a multithreaded environment.
 */
class AsyncHermesRuntime {
 public:
  // Create a runtime. If veryLazy, configure the runtime to use completely
  // lazy compilation.
  AsyncHermesRuntime(bool veryLazy = false);
  ~AsyncHermesRuntime();

  std::shared_ptr<HermesRuntime> runtime() {
    return runtime_;
  }

  /**
   * stop sets the stop flag on this instance. JS scripts can get the current
   * value of the stop flag by calling the global shouldStop() function.
   */
  void stop();

  /**
   * start unsets the stop flag on this instance. JS scripts can get the current
   * value of the stop flag by calling the global shouldStop() function.
   */
  void start();

  /**
   * getStoredValue returns a future that is fulfilled with the value passed in
   * to storeValue() by the JS script.
   */
  folly::Future<jsi::Value> getStoredValue();

  /**
   * hasStoredValue returns whether or not a value has been stored yet
   */
  bool hasStoredValue();

  /**
   * awaitStoredValue is a helper for getStoredValue that returns the value
   * synchronously rather than in a future.
   */
  jsi::Value awaitStoredValue(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(2500));

  /**
   * executeScriptAsync evaluates JS in the underlying Hermes runtime on a
   * separate thread.
   *
   * This method should be called at most once during the lifetime of an
   * AsyncHermesRuntime instance.
   */
  void executeScriptAsync(
      const std::string &str,
      const std::string &url = "url",
      facebook::hermes::HermesRuntime::DebugFlags flags =
          facebook::hermes::HermesRuntime::DebugFlags{});

  /**
   * wait blocks until all previous executeScriptAsync calls finish.
   */
  void wait(
      std::chrono::milliseconds timeout = std::chrono::milliseconds(2500));

  /**
   * returns the number of thrown exceptions.
   */
  size_t getNumberOfExceptions();

  /**
   * returns the message of the last thrown exception.
   */
  std::string getLastThrownExceptionMessage();

  /**
   * registers the runtime for profiling in the executor thread.
   */
  void registerForProfilingInExecutor();

  /**
   * unregisters the runtime for profiling in the executor thread.
   */
  void unregisterForProfilingInExecutor();

 private:
  jsi::Value shouldStop(
      jsi::Runtime &runtime,
      const jsi::Value &thisVal,
      const jsi::Value *args,
      size_t count);

  jsi::Value storeValue(
      jsi::Runtime &runtime,
      const jsi::Value &thisVal,
      const jsi::Value *args,
      size_t count);

  std::shared_ptr<HermesRuntime> runtime_;
  std::unique_ptr<folly::Executor> executor_;
  std::atomic<bool> stopFlag_{};
  folly::Promise<jsi::Value> storedValue_;
  std::vector<std::string> thrownExceptions_;
};

/// RAII-style class dealing with sampling profiler registration in tests. This
/// is especially important in tests -- if any test failure is caused by an
/// uncaught exception, stack unwinding will destroy a VM registered for
/// profiling in a thread that's not the one where registration happened, which
/// will lead to a hermes fatal error. Using this RAII class ensure that the
/// proper test failure cause is reported.
struct SamplingProfilerRAII {
  explicit SamplingProfilerRAII(AsyncHermesRuntime &rt) : runtime_(rt) {
    runtime_.registerForProfilingInExecutor();
  }

  ~SamplingProfilerRAII() {
    runtime_.unregisterForProfilingInExecutor();
  }

  AsyncHermesRuntime &runtime_;
};
} // namespace chrome
} // namespace inspector_modern
} // namespace hermes
} // namespace facebook
