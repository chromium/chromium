// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/events/message_event.h"

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

class MessageEventTest : public testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(MessageEventTest, AccountForStringMemory) {
  constexpr int64_t string_size = 10000;
  V8TestingScope scope;

  scope.GetIsolate()->Enter();

  // We are only interested in a string of size |string_size|. The content is
  // irrelevant.
  base::span<UChar> tmp;
  String data =
      String::CreateUninitialized(static_cast<unsigned>(string_size), tmp);

  // We read the |AmountOfExternalAllocatedMemory| before and after allocating
  // the |MessageEvent|. The difference has to be at least the string size.
  // Afterwards we trigger a blocking GC to deallocated the |MessageEvent|
  // again. After that the |AmountOfExternalAllocatedMemory| should be reduced
  // by at least the string size again.
  int64_t initial =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);
  MessageEvent::Create(data);

  int64_t size_with_event =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);
  ASSERT_LE(initial + string_size, size_with_event);

  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  int64_t size_after_gc =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);
  ASSERT_LE(size_after_gc + string_size, size_with_event);

  scope.GetIsolate()->Exit();
}

TEST_F(MessageEventTest, AccountForArrayBufferMemory) {
  constexpr int64_t buffer_size = 10000;
  V8TestingScope scope;

  scope.GetIsolate()->Enter();

  scoped_refptr<SerializedScriptValue> serialized_script_value;
  {
    DOMArrayBuffer* array_buffer =
        DOMArrayBuffer::Create(static_cast<size_t>(buffer_size), 1);
    v8::Local<v8::Value> object = v8::Number::New(scope.GetIsolate(), 13);
    Transferables transferables;
    transferables.array_buffers.push_back(array_buffer);
    ScriptState* script_state = scope.GetScriptState();
    ExceptionState& exception_state = scope.GetExceptionState();

    V8ScriptValueSerializer::Options serialize_options;
    serialize_options.transferables = &transferables;
    V8ScriptValueSerializer serializer(script_state, serialize_options);
    serialized_script_value = serializer.Serialize(object, exception_state);
  }
  // We read the |AmountOfExternalAllocatedMemory| before and after allocating
  // the |MessageEvent|. The difference has to be at least the buffer size.
  // Afterwards we trigger a blocking GC to deallocated the |MessageEvent|
  // again. After that the |AmountOfExternalAllocatedMemory| should be reduced
  // by at least the buffer size again.
  int64_t initial =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);

  MessagePortArray* ports = MakeGarbageCollected<MessagePortArray>(0);
  MessageEvent::Create(ports, serialized_script_value);

  int64_t size_with_event =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);
  ASSERT_LE(initial + buffer_size, size_with_event);

  ThreadState::Current()->CollectAllGarbageForTesting(
      ThreadState::StackState::kNoHeapPointers);

  int64_t size_after_gc =
      scope.GetIsolate()->AdjustAmountOfExternalAllocatedMemory(0);
  ASSERT_LE(size_after_gc + buffer_size, size_with_event);

  scope.GetIsolate()->Exit();
}
}  // namespace blink
