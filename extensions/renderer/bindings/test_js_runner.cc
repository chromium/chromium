// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/renderer/bindings/test_js_runner.h"

#include <ostream>

#include "base/functional/bind.h"
#include "content/public/renderer/v8_value_converter.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"

namespace extensions {

namespace {

// NOTE(devlin): These aren't thread-safe. If we have multi-threaded unittests,
// we'll need to expand these.
bool g_allow_errors = false;
bool g_suspended = false;

std::optional<base::Value> Convert(v8::MaybeLocal<v8::Value> maybe_value,
                                   v8::Local<v8::Context> context) {
  v8::Local<v8::Value> v8_value;
  if (!maybe_value.ToLocal(&v8_value))
    return std::nullopt;

  if (std::unique_ptr<base::Value> value =
          content::V8ValueConverter::Create()->FromV8Value(v8_value, context)) {
    return base::Value::FromUniquePtrValue(std::move(value));
  }
  return std::nullopt;
}

}  // namespace

TestJSRunner::Scope::Scope(std::unique_ptr<JSRunner> runner)
    : runner_(std::move(runner)),
      old_runner_(JSRunner::GetInstanceForTesting()) {
  DCHECK_NE(runner_.get(), old_runner_);
  JSRunner::SetInstanceForTesting(runner_.get());
}

TestJSRunner::Scope::~Scope() {
  DCHECK_EQ(runner_.get(), JSRunner::GetInstanceForTesting());
  JSRunner::SetInstanceForTesting(old_runner_);
}

TestJSRunner::AllowErrors::AllowErrors() {
  DCHECK(!g_allow_errors) << "Nested AllowErrors() blocks are not allowed.";
  g_allow_errors = true;
}

TestJSRunner::AllowErrors::~AllowErrors() {
  DCHECK(g_allow_errors);
  g_allow_errors = false;
}

TestJSRunner::Suspension::Suspension() {
  DCHECK(!g_suspended) << "Nested Suspension() blocks are not allowed.";
  g_suspended = true;
}

TestJSRunner::Suspension::~Suspension() {
  DCHECK(g_suspended);
  g_suspended = false;
  TestJSRunner* test_runner =
      static_cast<TestJSRunner*>(JSRunner::GetInstanceForTesting());
  DCHECK(test_runner);
  test_runner->Flush();
}

TestJSRunner::PendingCall::PendingCall() = default;
TestJSRunner::PendingCall::~PendingCall() = default;
TestJSRunner::PendingCall::PendingCall(PendingCall&& other) = default;

TestJSRunner::TestJSRunner() = default;
TestJSRunner::TestJSRunner(const base::RepeatingClosure& will_call_js)
    : will_call_js_(will_call_js) {}
TestJSRunner::~TestJSRunner() = default;

void TestJSRunner::RunJSFunction(v8::Local<v8::Function> function,
                                 v8::Local<v8::Context> context,
                                 int argc,
                                 v8::Local<v8::Value> argv[],
                                 ResultCallback callback) {
  if (g_suspended) {
    // Script is suspended. Queue up the call and return.
    v8::Isolate* isolate = context->GetIsolate();
    PendingCall call;
    call.isolate = isolate;
    call.function.Reset(isolate, function);
    call.context.Reset(isolate, context);
    call.arguments.reserve(argc);
    call.callback = std::move(callback);
    for (int i = 0; i < argc; ++i)
      call.arguments.push_back(v8::Global<v8::Value>(isolate, argv[i]));
    pending_calls_.push_back(std::move(call));
    return;
  }

  // Functions should always run in the scope of the context.
  v8::Context::Scope context_scope(context);

  if (will_call_js_)
    will_call_js_.Run();

  v8::MaybeLocal<v8::Value> result;
  if (g_allow_errors) {
    result = function->Call(context, context->Global(), argc, argv);
  } else {
    result = RunFunctionOnGlobal(function, context, argc, argv);
  }

  if (callback)
    std::move(callback).Run(context, Convert(result, context));
}

v8::MaybeLocal<v8::Value> TestJSRunner::RunJSFunctionSync(
    v8::Local<v8::Function> function,
    v8::Local<v8::Context> context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  // Note: deliberately circumvent g_suspension, since this should only be used
  // in response to JS interaction.
  if (will_call_js_)
    will_call_js_.Run();

  if (g_allow_errors) {
    v8::MaybeLocal<v8::Value> result =
        function->Call(context, context->Global(), argc, argv);
    return result;
  }
  return RunFunctionOnGlobal(function, context, argc, argv);
}

void TestJSRunner::Flush() {
  // Move pending_calls_ in case running any pending calls results in more calls
  // into the JSRunner.
  std::vector<PendingCall> calls = std::move(pending_calls_);
  pending_calls_.clear();
  for (auto& call : calls) {
    v8::Isolate* isolate = call.isolate;
    v8::Local<v8::Context> context = call.context.Get(isolate);
    v8::Context::Scope context_scope(context);
    v8::LocalVector<v8::Value> local_arguments(isolate);
    local_arguments.reserve(call.arguments.size());
    for (auto& arg : call.arguments)
      local_arguments.push_back(arg.Get(isolate));
    v8::MaybeLocal<v8::Value> result =
        RunJSFunctionSync(call.function.Get(isolate), context,
                          local_arguments.size(), local_arguments.data());
    if (call.callback)
      std::move(call.callback).Run(context, Convert(result, context));
  }
}

}  // namespace extensions
