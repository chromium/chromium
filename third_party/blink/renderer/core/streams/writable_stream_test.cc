// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/test_utils.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

TEST(WritableStreamTest, CreateWithoutArguments) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  WritableStream* stream =
      WritableStream::Create(scope.GetScriptState(), scope.GetExceptionState());
  ASSERT_TRUE(stream);
  ASSERT_FALSE(scope.GetExceptionState().HadException());
}

// Testing getWriter, locked and IsLocked.
TEST(WritableStreamTest, GetWriter) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  WritableStream* stream =
      WritableStream::Create(script_state, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(stream);

  EXPECT_FALSE(stream->locked());

  stream->getWriter(script_state, ASSERT_NO_EXCEPTION);

  EXPECT_TRUE(stream->locked());
}

TEST(WritableStreamTest, Serialize) {
  test::TaskEnvironment task_environment;
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
  EXPECT_TRUE(stream->locked());

  auto* transferred =
      WritableStream::Deserialize(script_state, channel->port2(),
                                  /*optimizer=*/nullptr, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(transferred);

  WritableStreamDefaultWriter* writer =
      transferred->getWriter(script_state, ASSERT_NO_EXCEPTION);

  auto* isolate = script_state->GetIsolate();
  writer->write(script_state, ScriptValue(isolate, V8String(isolate, "a")),
                ASSERT_NO_EXCEPTION);

  // Run the message loop to allow messages to be delivered.
  test::RunPendingTasks();
  // Allow Promises to resolve.
  scope.PerformMicrotaskCheckpoint();

  v8::Local<v8::Value> result;
  auto context = script_state->GetContext();
  ASSERT_TRUE(context->Global()
                  ->Get(context, V8String(isolate, "result"))
                  .ToLocal(&result));
  ASSERT_TRUE(result->IsString());
  EXPECT_EQ(ToCoreString(scope.GetIsolate(), result.As<v8::String>()), "a");
}

}  // namespace

}  // namespace blink
