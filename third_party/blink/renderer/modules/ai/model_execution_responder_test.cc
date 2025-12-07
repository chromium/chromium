// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ai/model_execution_responder.h"

#include <optional>
#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom-blink.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-blink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/modules/ai/ai_metrics.h"
#include "third_party/blink/renderer/modules/ai/ai_utils.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace {

std::tuple<String, bool> ReadString(ReadableStreamDefaultReader* reader,
                                    V8TestingScope& scope) {
  String result;
  bool done = false;
  auto read_promise = reader->read(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), read_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  v8::Local<v8::Value> v8value;
  EXPECT_TRUE(V8UnpackIterationResult(scope.GetScriptState(),
                                      tester.Value().V8Value().As<v8::Object>(),
                                      &v8value, &done));
  if (v8value->IsString()) {
    result = ToCoreString(scope.GetIsolate(), v8value.As<v8::String>());
  }
  return std::make_tuple(result, done);
}

}  // namespace

TEST(CreateModelExecutionResponder, Simple) {
  uint64_t kTestTokenNumber = 1u;
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  base::RunLoop disconnect_runloop;
  base::RunLoop complete_runloop;
  base::RunLoop overflow_runloop;
  auto pending_remote = CreateModelExecutionResponder(
      script_state, /*signal=*/nullptr,
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      /*complete_callback=*/
      blink::BindOnce(
          [](uint64_t expected_tokens, base::RunLoop* runloop,
             ScriptPromiseResolver<IDLString>* resolver, const String& response,
             mojom::blink::ModelExecutionContextInfoPtr context_info) {
            EXPECT_TRUE(context_info);
            EXPECT_EQ(context_info->current_tokens, expected_tokens);
            ResolvePromiseOnCompletion(resolver, response,
                                       std::move(context_info));
            runloop->Quit();
          },
          kTestTokenNumber, blink::Unretained(&complete_runloop),
          WrapPersistent(resolver)),
      /*overflow_callback=*/overflow_runloop.QuitClosure(),
      base::BindOnce(&RejectPromiseOnError<IDLString>,
                     WrapPersistent(resolver)),
      base::BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
                     nullptr, WrapPersistent(script_state)));

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  responder.set_disconnect_handler(disconnect_runloop.QuitClosure());
  responder->OnStreaming("a");
  responder->OnStreaming("b");
  responder->OnQuotaOverflow();
  responder->OnCompletion(
      mojom::blink::ModelExecutionContextInfo::New(kTestTokenNumber));
  // Check that the promise will be resolved with the "result" string.
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_TRUE(tester.Value().V8Value()->IsString());
  EXPECT_EQ("ab", ToCoreString(scope.GetIsolate(),
                                tester.Value().V8Value().As<v8::String>()));

  // Check that the complete and overflow callback is run.
  complete_runloop.Run();
  overflow_runloop.Run();
  // Check that the Mojo handle will be disconnected.
  disconnect_runloop.Run();
}

TEST(CreateModelExecutionResponder, ErrorPermissionDenied) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  auto pending_remote = CreateModelExecutionResponder(
      script_state, /*signal=*/nullptr,
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      base::BindOnce(&ResolvePromiseOnCompletion<IDLString>,
                     WrapPersistent(resolver)),
      /*overflow_callback=*/base::DoNothing(),
      base::BindOnce(&RejectPromiseOnError<IDLString>,
                     WrapPersistent(resolver)),
      base::BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
                     nullptr, WrapPersistent(script_state)));

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());
  responder->OnError(
      blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied,
      blink::mojom::blink::QuotaErrorInfo::New(0u, 0u));

  // Check that the promise will be rejected with an ErrorInvalidRequest.
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kNotAllowedError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionResponder, AbortWithoutResponse) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  auto pending_remote = CreateModelExecutionResponder(
      script_state, controller->signal(),
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      base::BindOnce(&ResolvePromiseOnCompletion<IDLString>,
                     WrapPersistent(resolver)),
      /*overflow_callback=*/base::DoNothing(),
      base::BindOnce(&RejectPromiseOnError<IDLString>,
                     WrapPersistent(resolver)),
      base::BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
                     WrapPersistent(controller->signal()),
                     WrapPersistent(script_state)));

  controller->abort(scope.GetScriptState());

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());

  // Check that the promise will be rejected with an AbortError.
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kAbortError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionResponder, AbortAfterResponse) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();
  auto pending_remote = CreateModelExecutionResponder(
      script_state, controller->signal(),
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      base::BindOnce(&ResolvePromiseOnCompletion<IDLString>,
                     WrapPersistent(resolver)),
      /*overflow_callback=*/base::DoNothing(),
      base::BindOnce(&RejectPromiseOnError<IDLString>,
                     WrapPersistent(resolver)),
      base::BindOnce(&RejectPromiseOnAbort<IDLString>, WrapPersistent(resolver),
                     WrapPersistent(controller->signal()),
                     WrapPersistent(script_state)));

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());
  responder->OnStreaming("result");
  responder->OnCompletion(mojom::blink::ModelExecutionContextInfo::New(
      /*current_tokens=*/1u));

  controller->abort(scope.GetScriptState());

  // Check that the promise will be rejected with an AbortError.
  ScriptPromiseTester tester(scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kAbortError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionStreamingResponder, Simple) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto [stream, pending_remote] = CreateModelExecutionStreamingResponder(
      script_state, /*signal=*/nullptr,
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());
  responder->OnStreaming("result");
  responder->OnCompletion(mojom::blink::ModelExecutionContextInfo::New(
      /*current_tokens=*/1u));

  // Check that we can read the stream.
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  String result;
  bool done;
  std::tie(result, done) = ReadString(reader, scope);
  EXPECT_EQ("result", result);
  EXPECT_FALSE(done);
  std::tie(result, done) = ReadString(reader, scope);
  EXPECT_TRUE(result.IsNull());
  EXPECT_TRUE(done);

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionStreamingResponder, ErrorPermissionDenied) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto [stream, pending_remote] = CreateModelExecutionStreamingResponder(
      script_state, /*signal=*/nullptr,
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());
  responder->OnError(
      blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied,
      blink::mojom::blink::QuotaErrorInfo::New(0u, 0u));

  // Check that the NotAllowedError is passed to the stream.
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), read_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kNotAllowedError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionStreamingResponder, AbortWithoutResponse) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto [stream, pending_remote] = CreateModelExecutionStreamingResponder(
      script_state, controller->signal(),
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());

  controller->abort(scope.GetScriptState());

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());

  // Check that the AbortError is passed to the stream.
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), read_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kAbortError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

TEST(CreateModelExecutionStreamingResponder, AbortAfterResponse) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  auto* controller = AbortController::Create(scope.GetScriptState());
  auto [stream, pending_remote] = CreateModelExecutionStreamingResponder(
      script_state, controller->signal(),
      blink::scheduler::GetSequencedTaskRunnerForTesting(),
      AIMetrics::AISessionType::kLanguageModel,
      /*complete_callback=*/base::DoNothing(),
      /*overflow_callback=*/base::DoNothing());

  controller->abort(scope.GetScriptState());

  mojo::Remote<blink::mojom::blink::ModelStreamingResponder> responder(
      std::move(pending_remote));
  base::RunLoop runloop;
  responder.set_disconnect_handler(runloop.QuitClosure());
  responder->OnStreaming("result");
  responder->OnCompletion(mojom::blink::ModelExecutionContextInfo::New(1u));

  // Check that the AbortError is passed to the stream.
  auto* reader =
      stream->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);
  auto read_promise = reader->read(scope.GetScriptState(), ASSERT_NO_EXCEPTION);
  ScriptPromiseTester tester(scope.GetScriptState(), read_promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
  auto* dom_exception = V8DOMException::ToWrappable(script_state->GetIsolate(),
                                                    tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(DOMException(DOMExceptionCode::kAbortError).name(),
            dom_exception->name());

  // Check that the Mojo handle will be disconnected.
  runloop.Run();
}

}  // namespace blink
