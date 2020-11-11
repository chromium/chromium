// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transferable_streams.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_iterator_result_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

// We only do minimal testing here. The functionality of transferable streams is
// tested in the layout tests.
TEST(TransferableStreamsTest, SmokeTest) {
  V8TestingScope scope;

  auto* channel =
      MakeGarbageCollected<MessageChannel>(scope.GetExecutionContext());
  auto* script_state = scope.GetScriptState();
  auto* writable = CreateCrossRealmTransformWritable(
      script_state, channel->port1(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(writable);
  auto* readable = CreateCrossRealmTransformReadable(
      script_state, channel->port2(), ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(readable);

  auto* writer = writable->getWriter(script_state, ASSERT_NO_EXCEPTION);
  auto* reader =
      readable->GetDefaultReaderForTesting(script_state, ASSERT_NO_EXCEPTION);

  writer->write(script_state, ScriptValue::CreateNull(scope.GetIsolate()),
                ASSERT_NO_EXCEPTION);

  class ExpectNullResponse : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state,
                                          bool* got_response) {
      auto* self =
          MakeGarbageCollected<ExpectNullResponse>(script_state, got_response);
      return self->BindToV8Function();
    }

    ExpectNullResponse(ScriptState* script_state, bool* got_response)
        : ScriptFunction(script_state), got_response_(got_response) {}

   private:
    ScriptValue Call(ScriptValue value) override {
      *got_response_ = true;
      if (!value.IsObject()) {
        ADD_FAILURE() << "iterator must be an object";
        return ScriptValue();
      }
      bool done = false;
      auto* script_state = GetScriptState();
      auto chunk_maybe =
          V8UnpackIteratorResult(script_state,
                                 value.V8Value()
                                     ->ToObject(script_state->GetContext())
                                     .ToLocalChecked(),
                                 &done);
      EXPECT_FALSE(done);
      v8::Local<v8::Value> chunk;
      if (!chunk_maybe.ToLocal(&chunk)) {
        ADD_FAILURE() << "V8UnpackIteratorResult failed";
        return ScriptValue();
      }
      EXPECT_TRUE(chunk->IsNull());
      return ScriptValue();
    }

    bool* got_response_;
  };

  // TODO(ricea): This is copy-and-pasted from transform_stream_test.cc. Put it
  // in a shared location.
  class ExpectNotReached : public ScriptFunction {
   public:
    static v8::Local<v8::Function> Create(ScriptState* script_state) {
      auto* self = MakeGarbageCollected<ExpectNotReached>(script_state);
      return self->BindToV8Function();
    }

    explicit ExpectNotReached(ScriptState* script_state)
        : ScriptFunction(script_state) {}

   private:
    ScriptValue Call(ScriptValue) override {
      ADD_FAILURE() << "ExpectNotReached was reached";
      return ScriptValue();
    }
  };

  bool got_response = false;
  reader->read(script_state, ASSERT_NO_EXCEPTION)
      .Then(ExpectNullResponse::Create(script_state, &got_response),
            ExpectNotReached::Create(script_state));

  // Need to run the event loop to pass messages through the MessagePort.
  test::RunPendingTasks();

  // Resolve promises.
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());

  EXPECT_TRUE(got_response);
}

}  // namespace

}  // namespace blink
