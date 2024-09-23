// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_JS_RUNNER_H_
#define EXTENSIONS_RENDERER_BINDINGS_JS_RUNNER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "v8/include/v8.h"

namespace base {
class Value;
}

namespace extensions {

// A helper class to execute JS functions safely.
class JSRunner {
 public:
  // Returns the instance of the JSRunner for the specified |context|.
  static JSRunner* Get(v8::Local<v8::Context> context);

  // Sets the instance for a given |context|.
  static void SetInstanceForContext(v8::Local<v8::Context> context,
                                    std::unique_ptr<JSRunner> runner);
  // Clears the instance for a given |context|.
  static void ClearInstanceForContext(v8::Local<v8::Context> context);

  virtual ~JSRunner() {}

  // Called with the result of executing the function, as well as the context
  // it was executed in. Note: This callback is *not* guaranteed to be invoked
  // (and won't be if, for instance, the context is destroyed before this is
  // ran).
  // NOTE(devlin): We could easily change that if desired.
  using ResultCallback = base::OnceCallback<void(v8::Local<v8::Context>,
                                                 std::optional<base::Value>)>;

  // Calls the given |function| in the specified |context| and with the provided
  // arguments. JS may be executed asynchronously if it has been suspended in
  // the context.
  void RunJSFunction(v8::Local<v8::Function> function,
                     v8::Local<v8::Context> context,
                     int argc,
                     v8::Local<v8::Value> argv[]);
  // Same as above, but if a |callback| is provided, it will be called with the
  // results of the function running.
  virtual void RunJSFunction(v8::Local<v8::Function> function,
                             v8::Local<v8::Context> context,
                             int argc,
                             v8::Local<v8::Value> argv[],
                             ResultCallback callback) = 0;

  // Executes the given |function| synchronously and returns the result. This
  // should *only* be called in direct response to script running, since it
  // bypasses script suspension.
  virtual v8::MaybeLocal<v8::Value> RunJSFunctionSync(
      v8::Local<v8::Function> function,
      v8::Local<v8::Context> context,
      int argc,
      v8::Local<v8::Value> argv[]) = 0;

  // Sets a global instance for testing that will be returned instead of the
  // per-context version (if any).
  static void SetInstanceForTesting(JSRunner* runner);
  // Returns the global testing instance.
  static JSRunner* GetInstanceForTesting();
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_JS_RUNNER_H_
