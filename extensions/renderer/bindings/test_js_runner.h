// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_TEST_JS_RUNNER_H_
#define EXTENSIONS_RENDERER_BINDINGS_TEST_JS_RUNNER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "extensions/renderer/bindings/js_runner.h"

namespace extensions {

// A JSRunner implementation designed for use in unit tests. Does not handle
// any kind of script suspension. By default, all functions are expected to
// succeed and not throw any exceptions. If running a function is expected to
// result in errors, use a TestJSRunner::AllowErrors object in as narrow a
// scope as possible.
class TestJSRunner : public JSRunner {
 public:
  // A scoped object that handles setting and resetting the instance for the
  // current thread. Note: multiple scopes can be nested, but they must follow
  // a LIFO destruction order. That is, the following is safe:
  // <begin>  // JSRunner::Get() returns the default JSRunner (or none).
  // Scope a();  // JSRunner::Get() returns a's JSRunner.
  // Scope b();  // JSRunner::Get() returns b's JSRunner.
  // ~Scope b();  // JSRunner::Get() returns a's JSRunner.
  // ~Scope a();  // JSRunner::Get() returns the default JSRunner (or none).
  // But the following will DCHECK:
  // Scope a();
  // Scope b();
  // ~Scope a();  // DCHECKs.
  // ~Scope b();
  class Scope {
   public:
    Scope(std::unique_ptr<JSRunner> runner);

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    ~Scope();

   private:
    std::unique_ptr<JSRunner> runner_;
    raw_ptr<JSRunner> old_runner_;
  };

  // A scoped object that allows errors to be thrown from running JS functions.
  // Note: *not* allowed to be nested; only one can be active at a time.
  // Constructing multiple instances at the same time will DCHECK.
  class AllowErrors {
   public:
    AllowErrors();

    AllowErrors(const AllowErrors&) = delete;
    AllowErrors& operator=(const AllowErrors&) = delete;

    ~AllowErrors();
  };

  // A scoped object that suspends script execution through the JSRunner. While
  // in scope, function calls to JSRunner will be queued up, and finally run
  // upon Suspension destruction.
  // Note: *not* allowed to be nested; only one can be active at a time.
  // Constructing multiple instances at the same time will DCHECK.
  class Suspension {
   public:
    Suspension();

    Suspension(const Suspension&) = delete;
    Suspension& operator=(const Suspension&) = delete;

    ~Suspension();
  };

  TestJSRunner();
  // Provides a callback to be called just before JS will be executed.
  explicit TestJSRunner(const base::RepeatingClosure& will_call_js);

  TestJSRunner(const TestJSRunner&) = delete;
  TestJSRunner& operator=(const TestJSRunner&) = delete;

  ~TestJSRunner() override;

  // JSRunner:
  void RunJSFunction(v8::Local<v8::Function> function,
                     v8::Local<v8::Context> context,
                     int argc,
                     v8::Local<v8::Value> argv[],
                     ResultCallback callback) override;
  v8::MaybeLocal<v8::Value> RunJSFunctionSync(
      v8::Local<v8::Function> function,
      v8::Local<v8::Context> context,
      int argc,
      v8::Local<v8::Value> argv[]) override;

 private:
  friend class Suspension;

  struct PendingCall {
    PendingCall();
    ~PendingCall();
    PendingCall(PendingCall&& other);

    raw_ptr<v8::Isolate> isolate;
    v8::Global<v8::Function> function;
    v8::Global<v8::Context> context;
    std::vector<v8::Global<v8::Value>> arguments;
    ResultCallback callback;
  };

  // Runs all pending calls.
  void Flush();

  base::RepeatingClosure will_call_js_;
  std::vector<PendingCall> pending_calls_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_TEST_JS_RUNNER_H_
