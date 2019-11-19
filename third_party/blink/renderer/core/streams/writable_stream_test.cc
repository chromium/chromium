// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this sink code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_extras_test_utils.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

TEST(WritableStreamTest, CreateWithoutArguments) {
  V8TestingScope scope;

  WritableStream* stream =
      WritableStream::Create(scope.GetScriptState(), scope.GetExceptionState());
  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

// Testing getWriter, locked and IsLocked.
TEST(WritableStreamTest, GetWriter) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  WritableStream* stream =
      WritableStream::Create(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->locked(script_state, ASSERT_NO_EXCEPTION));
  EXPECT_EQ(stream->IsLocked(script_state, ASSERT_NO_EXCEPTION),
            base::make_optional(false));

  stream->getWriter(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->locked(script_state, ASSERT_NO_EXCEPTION));
  EXPECT_EQ(stream->IsLocked(script_state, ASSERT_NO_EXCEPTION),
            base::make_optional(true));
}

TEST(WritableStreamTest, Serialize) {
  ScopedTransferableStreamsForTest enable_transferable_streams(true);

  V8TestingScope scope;
  auto* script_state = scope.GetScriptState();

  const char underlying_sink_script[] =
      R"JS(
const underlying_sink = {
  write(chunk) {
    result = chunk;
  }
};
underlying_sink)JS";
  ScriptValue underlying_sink =
      EvalWithPrintingError(&scope, underlying_sink_script);
  ASSERT_FALSE(underlying_sink.IsEmpty());
  auto* stream = WritableStream::Create(script_state, underlying_sink,
                                        ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());

  stream->Serialize(script_state, channel->port1(), ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(stream->locked(script_state, ASSERT_NO_EXCEPTION));

  auto* transferred = WritableStream::Deserialize(
      script_state, channel->port2(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(transferred);

  ScriptValue writer =
      transferred->getWriter(script_state, ASSERT_NO_EXCEPTION);
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Object> global = context->Global();
  ASSERT_TRUE(
      global->Set(context, V8String(isolate, "writer"), writer.V8Value())
          .FromMaybe(false));
  EvalWithPrintingError(&scope, "writer.write('a')");
  // Run the message loop to allow messages to be delivered.
  test::RunPendingTasks();
  // Allow Promises to resolve.
  v8::MicrotasksScope::PerformCheckpoint(isolate);

  v8::Local<v8::Value> result;
  ASSERT_TRUE(
      global->Get(context, V8String(isolate, "result")).ToLocal(&result));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ(ToCoreString(result.As<v8::String>()), "a");
}

}  // namespace

}  // namespace blink
