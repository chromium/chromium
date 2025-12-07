// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/to_blink_string.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

TEST(StreamsExceptionHandlingTest, ExceptionDuringConstruction) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  static constexpr char kScript[] = R"((
    async function (promise) {
      await promise;
      new CompressionStream("gzip").readable;
    }
  ))";
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, V8String(isolate, kScript)).ToLocalChecked();
  v8::Local<v8::Function> func =
      script->Run(scope.GetContext()).ToLocalChecked().As<v8::Function>();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      scope.GetScriptState());

  v8::Local<v8::Value> args[] = {resolver->V8Promise()};
  std::ignore = func->Call(scope.GetContext(), v8::Null(isolate), 1, args);
  // TODO(caseq): consider enforcing the callback is not called more than once.
  scope.GetContext()->SetAbortScriptExecution(
      [](v8::Isolate*, v8::Local<v8::Context>) {});
  resolver->Resolve();
  v8::MicrotasksScope::PerformCheckpoint(isolate);
}

}  // namespace

}  // namespace blink
