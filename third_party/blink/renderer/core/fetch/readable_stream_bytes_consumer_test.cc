// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer_test_util.h"
#include "third_party/blink/renderer/core/fetch/fetch_data_loader.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

using ::testing::ElementsAre;
using ::testing::InSequence;
using ::testing::StrictMock;
using Checkpoint = StrictMock<::testing::MockFunction<void(int)>>;
using Result = BytesConsumer::Result;
using PublicState = BytesConsumer::PublicState;

class MockClient : public GarbageCollected<MockClient>,
                   public BytesConsumer::Client {
 public:
  MockClient() = default;

  MOCK_METHOD0(OnStateChange, void());
  String DebugName() const override { return "MockClient"; }

  void Trace(Visitor* visitor) const override {}
};

TEST(ReadableStreamBytesConsumerTest, Create) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState& exception_state = scope.GetExceptionState();

  auto* stream = ReadableStream::Create(script_state, exception_state);
  ASSERT_TRUE(stream);
  ASSERT_FALSE(exception_state.HadException());

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);

  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, EmptyStream) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);

  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);

  Checkpoint checkpoint;
  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, ErroredStream) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Error(ScriptValue(
      script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  {
    auto* chunk1 = DOMUint8Array::Create(0);
    auto* chunk2 =
        DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
    auto* chunk3 =
        DOMUint8Array::Create(std::to_array<uint8_t>({0x47, 0x48, 0x49, 0x4a}));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk1)));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk2)));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk3)));
    underlying_source->Close();
  }

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));
  EXPECT_CALL(checkpoint, Call(5));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(6));
  EXPECT_CALL(checkpoint, Call(7));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(8));
  EXPECT_CALL(checkpoint, Call(9));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(10));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  ASSERT_EQ(0u, buffer.size());
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(5);
  test::RunPendingTasks();
  checkpoint.Call(6);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(1));
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(3));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(7);
  test::RunPendingTasks();
  checkpoint.Call(8);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x47, 0x48, 0x49, 0x4a));
  EXPECT_EQ(Result::kOk, consumer->EndRead(4));
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(9);
  test::RunPendingTasks();
  checkpoint.Call(10);
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseReadDetachedDuringRead) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  auto* chunk =
      DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk)));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  chunk->buffer()->DetachForTesting();
  EXPECT_EQ(Result::kError, consumer->EndRead(4));
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseReadDetachedBetweenReads) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  auto* chunk =
      DOMUint8Array::Create(std::to_array<uint8_t>({0x43, 0x44, 0x45, 0x46}));
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<DOMUint8Array>::ToV8(script_state, chunk)));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x43, 0x44, 0x45, 0x46));
  EXPECT_EQ(Result::kOk, consumer->EndRead(1));
  chunk->buffer()->DetachForTesting();
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
}

TEST(ReadableStreamBytesConsumerTest, EnqueueUndefined) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(
      script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, EnqueueNull) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(script_state->GetIsolate(),
                                         v8::Null(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, EnqueueString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  V8String(script_state->GetIsolate(), "hello")));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);
  Checkpoint checkpoint;

  InSequence s;
  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(checkpoint, Call(2));
  EXPECT_CALL(checkpoint, Call(3));
  EXPECT_CALL(*client, OnStateChange());
  EXPECT_CALL(checkpoint, Call(4));

  base::span<const char> buffer;
  checkpoint.Call(1);
  test::RunPendingTasks();
  checkpoint.Call(2);
  EXPECT_EQ(PublicState::kReadableOrWaiting, consumer->GetPublicState());
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  checkpoint.Call(3);
  test::RunPendingTasks();
  checkpoint.Call(4);
  EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
  EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest, Cancel) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);
  underlying_source->Enqueue(ScriptValue(script_state->GetIsolate(),
                                         v8::Null(script_state->GetIsolate())));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);

  consumer->Cancel();

  EXPECT_TRUE(underlying_source->IsCancelled());
  EXPECT_TRUE(underlying_source->IsCancelledWithUndefined());
}

TEST(ReadableStreamBytesConsumerTest, TwoPhaseReadSharedArrayBuffer) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  auto* underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(script_state);
  auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_source, 0);

  Persistent<DOMSharedArrayBuffer> empty_shared =
      DOMSharedArrayBuffer::Create(0u, 1u);
  auto* empty_chunk = DOMUint8Array::Create(empty_shared, 0u, 0u);

  // Use an unaligned byte_offset (1) and length (19) spanning unaligned head
  // bytes, aligned words, and trailing bytes.
  std::array<uint8_t, 24> initial_bytes = {};
  for (size_t i = 0; i < initial_bytes.size(); ++i) {
    initial_bytes[i] = static_cast<uint8_t>(0x30u + i);
  }
  Persistent<DOMSharedArrayBuffer> shared_buffer =
      DOMSharedArrayBuffer::Create(initial_bytes);
  constexpr size_t kViewOffset = 1u;
  constexpr size_t kViewLength = 19u;
  auto* shared_chunk =
      DOMUint8Array::Create(shared_buffer, kViewOffset, kViewLength);

  auto* unshared_chunk =
      DOMUint8Array::Create(std::to_array<uint8_t>({0x61u, 0x62u, 0x63u}));

  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<MaybeShared<DOMUint8Array>>::ToV8(
                      script_state, MaybeShared<DOMUint8Array>(empty_chunk))));
  underlying_source->Enqueue(
      ScriptValue(script_state->GetIsolate(),
                  ToV8Traits<MaybeShared<DOMUint8Array>>::ToV8(
                      script_state, MaybeShared<DOMUint8Array>(shared_chunk))));
  underlying_source->Enqueue(ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<DOMUint8Array>::ToV8(script_state, unshared_chunk)));
  underlying_source->Close();

  Persistent<BytesConsumer> consumer =
      MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
  Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
  consumer->SetClient(client);

  EXPECT_CALL(*client, OnStateChange()).Times(4);

  base::span<const char> buffer;
  test::RunPendingTasks();

  // 1. Empty shared Uint8Array chunk.
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  test::RunPendingTasks();
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_TRUE(buffer.empty());
  EXPECT_EQ(Result::kOk, consumer->EndRead(0));

  // 2. Shared Uint8Array chunk with unaligned offset; mutate the backing
  // SharedArrayBuffer after the read request fulfills and across partial reads
  // to verify that BeginRead exposes an isolated snapshot.
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  test::RunPendingTasks();
  std::ranges::fill(shared_buffer->ByteSpanMaybeShared(), uint8_t{0x7Au});

  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_NE(buffer.data(), reinterpret_cast<const char*>(
                               shared_chunk->BaseAddressMaybeShared()));
  ASSERT_EQ(kViewLength, buffer.size());
  for (size_t i = 0; i < 5u; ++i) {
    EXPECT_EQ(static_cast<char>(initial_bytes[kViewOffset + i]), buffer[i]);
  }
  EXPECT_EQ(Result::kOk, consumer->EndRead(5));

  std::ranges::fill(shared_buffer->ByteSpanMaybeShared(), uint8_t{0x21u});
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  ASSERT_EQ(kViewLength - 5u, buffer.size());
  for (size_t i = 0; i < buffer.size(); ++i) {
    EXPECT_EQ(static_cast<char>(initial_bytes[kViewOffset + 5u + i]),
              buffer[i]);
  }
  EXPECT_EQ(Result::kOk, consumer->EndRead(buffer.size()));

  // 3. Subsequent unshared Uint8Array chunk continues normally.
  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  test::RunPendingTasks();
  ASSERT_EQ(Result::kOk, consumer->BeginRead(buffer));
  EXPECT_THAT(buffer, ElementsAre(0x61, 0x62, 0x63));
  EXPECT_EQ(Result::kOk, consumer->EndRead(3));

  EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
  test::RunPendingTasks();
  EXPECT_EQ(PublicState::kClosed, consumer->GetPublicState());
  EXPECT_EQ(Result::kDone, consumer->BeginRead(buffer));
}

TEST(ReadableStreamBytesConsumerTest,
     FetchDataLoaderStringWithConcurrentSharedBufferUpdates) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kSimdutfTextCodecUtf8Decode);
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

  // 4096 bytes of 2-byte Latin-1 UTF-8 (U+00E9: 0xC3 0xA9) and 4098 bytes of
  // 3-byte UTF-8 (U+20AC: 0xE2 0x82 0xAC) at unaligned offset 1 exercise both
  // 8-bit Latin-1 and 16-bit UTF-16 simdutf decoding paths in TextCodecUtf8.
  constexpr size_t kLatin1ByteLength = 4096u;
  constexpr size_t kUtf16ByteLength = 4098u;
  constexpr size_t kUnalignedOffset = 1u;
  Persistent<DOMSharedArrayBuffer> latin1_shared =
      DOMSharedArrayBuffer::Create(kLatin1ByteLength + kUnalignedOffset, 1u);
  Persistent<DOMSharedArrayBuffer> utf16_shared =
      DOMSharedArrayBuffer::Create(kUtf16ByteLength + kUnalignedOffset, 1u);

  base::span<uint8_t> latin1_span =
      latin1_shared->ByteSpanMaybeShared().subspan(kUnalignedOffset,
                                                   kLatin1ByteLength);
  base::span<uint8_t> utf16_span = utf16_shared->ByteSpanMaybeShared().subspan(
      kUnalignedOffset, kUtf16ByteLength);

  auto fill_multibyte = [](base::span<uint8_t> latin1_bytes,
                           base::span<uint8_t> utf16_bytes) {
    for (size_t i = 0; i < latin1_bytes.size(); i += 2) {
      std::atomic_ref<uint8_t>(latin1_bytes[i])
          .store(0xC3u, std::memory_order_relaxed);
      std::atomic_ref<uint8_t>(latin1_bytes[i + 1])
          .store(0xA9u, std::memory_order_relaxed);
    }
    for (size_t i = 0; i < utf16_bytes.size(); i += 3) {
      std::atomic_ref<uint8_t>(utf16_bytes[i])
          .store(0xE2u, std::memory_order_relaxed);
      std::atomic_ref<uint8_t>(utf16_bytes[i + 1])
          .store(0x82u, std::memory_order_relaxed);
      std::atomic_ref<uint8_t>(utf16_bytes[i + 2])
          .store(0xACu, std::memory_order_relaxed);
    }
  };

  auto decode_single_shared_chunk = [&](DOMSharedArrayBuffer* shared,
                                        size_t byte_length) {
    auto* underlying_source =
        MakeGarbageCollected<TestUnderlyingSource>(script_state);
    auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
        script_state, underlying_source, 0);
    auto* chunk = DOMUint8Array::Create(shared, kUnalignedOffset, byte_length);
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<MaybeShared<DOMUint8Array>>::ToV8(
                        script_state, MaybeShared<DOMUint8Array>(chunk))));
    underlying_source->Close();

    auto* consumer =
        MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
    auto* loader = FetchDataLoader::CreateLoaderAsString(
        TextResourceDecoderOptions::CreateUTF8Decode());
    auto* loader_client = MakeGarbageCollected<
        BytesConsumerTestUtil::MockFetchDataLoaderClient>();

    String result_text;
    EXPECT_CALL(*loader_client, DidFetchDataLoadedString(::testing::_))
        .WillOnce(::testing::SaveArg<0>(&result_text));
    loader->Start(consumer, loader_client);
    test::RunPendingTasks();
    return result_text;
  };

  fill_multibyte(latin1_span, utf16_span);

  // Verify deterministic 8-bit Latin-1 and 16-bit UTF-16 simdutf decoding of
  // SharedArrayBuffer-backed chunks before starting concurrent updates.
  String latin1_decoded =
      decode_single_shared_chunk(latin1_shared, kLatin1ByteLength);
  ASSERT_FALSE(latin1_decoded.IsNull());
  EXPECT_TRUE(latin1_decoded.Is8Bit());
  ASSERT_EQ(kLatin1ByteLength / 2u, latin1_decoded.length());
  EXPECT_EQ(0x00E9u, latin1_decoded[0]);

  String utf16_decoded =
      decode_single_shared_chunk(utf16_shared, kUtf16ByteLength);
  ASSERT_FALSE(utf16_decoded.IsNull());
  EXPECT_FALSE(utf16_decoded.Is8Bit());
  ASSERT_EQ(kUtf16ByteLength / 3u, utf16_decoded.length());
  EXPECT_EQ(0x20ACu, utf16_decoded[0]);

  std::atomic<bool> stop_writer{false};
  base::WaitableEvent writer_started;
  base::WaitableEvent writer_finished;
  base::ThreadPool::PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::span<uint8_t> latin1_bytes, base::span<uint8_t> utf16_bytes,
             std::atomic<bool>* stop, base::WaitableEvent* started,
             base::WaitableEvent* finished,
             decltype(fill_multibyte) write_multibyte) {
            started->Signal();
            while (!stop->load(std::memory_order_relaxed)) {
              for (uint8_t& b : latin1_bytes) {
                std::atomic_ref<uint8_t>(b).store(0x41u,
                                                  std::memory_order_relaxed);
              }
              for (uint8_t& b : utf16_bytes) {
                std::atomic_ref<uint8_t>(b).store(0x41u,
                                                  std::memory_order_relaxed);
              }
              write_multibyte(latin1_bytes, utf16_bytes);
            }
            finished->Signal();
          },
          latin1_span, utf16_span, base::Unretained(&stop_writer),
          base::Unretained(&writer_started), base::Unretained(&writer_finished),
          fill_multibyte));
  writer_started.Wait();

  for (int round = 0; round < 20; ++round) {
    auto* underlying_source =
        MakeGarbageCollected<TestUnderlyingSource>(script_state);
    auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
        script_state, underlying_source, 0);
    auto* chunk1 = DOMUint8Array::Create(latin1_shared, kUnalignedOffset,
                                         kLatin1ByteLength);
    auto* chunk2 =
        DOMUint8Array::Create(utf16_shared, kUnalignedOffset, kUtf16ByteLength);
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<MaybeShared<DOMUint8Array>>::ToV8(
                        script_state, MaybeShared<DOMUint8Array>(chunk1))));
    underlying_source->Enqueue(
        ScriptValue(script_state->GetIsolate(),
                    ToV8Traits<MaybeShared<DOMUint8Array>>::ToV8(
                        script_state, MaybeShared<DOMUint8Array>(chunk2))));
    underlying_source->Close();

    auto* consumer =
        MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
    auto* loader = FetchDataLoader::CreateLoaderAsString(
        TextResourceDecoderOptions::CreateUTF8Decode());
    auto* loader_client = MakeGarbageCollected<
        BytesConsumerTestUtil::MockFetchDataLoaderClient>();

    String decoded_text;
    EXPECT_CALL(*loader_client, DidFetchDataLoadedString(::testing::_))
        .WillOnce(::testing::SaveArg<0>(&decoded_text));

    loader->Start(consumer, loader_client);
    test::RunPendingTasks();
    ASSERT_FALSE(decoded_text.IsNull());
    EXPECT_GE(decoded_text.length(),
              kLatin1ByteLength / 2u + kUtf16ByteLength / 3u);
    EXPECT_LE(decoded_text.length(), kLatin1ByteLength + kUtf16ByteLength);
  }

  stop_writer.store(true, std::memory_order_relaxed);
  writer_finished.Wait();
}

TEST(ReadableStreamBytesConsumerTest, EnqueueGrowableAndResizableUint8Array) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();

  for (const char* expr :
       {"new Uint8Array(new SharedArrayBuffer(8, {maxByteLength: 16}))",
        "new Uint8Array(new ArrayBuffer(8, {maxByteLength: 16}))",
        "new Uint16Array(new SharedArrayBuffer(8))",
        "new DataView(new SharedArrayBuffer(8))"}) {
    v8::Local<v8::Value> chunk =
        v8::Script::Compile(context, V8String(isolate, expr))
            .ToLocalChecked()
            ->Run(context)
            .ToLocalChecked();
    ASSERT_TRUE(chunk->IsArrayBufferView());

    auto* underlying_source =
        MakeGarbageCollected<TestUnderlyingSource>(script_state);
    auto* stream = ReadableStream::CreateWithCountQueueingStrategy(
        script_state, underlying_source, 0);
    underlying_source->Enqueue(ScriptValue(isolate, chunk));
    underlying_source->Close();

    Persistent<BytesConsumer> consumer =
        MakeGarbageCollected<ReadableStreamBytesConsumer>(script_state, stream);
    Persistent<MockClient> client = MakeGarbageCollected<MockClient>();
    consumer->SetClient(client);
    EXPECT_CALL(*client, OnStateChange());

    base::span<const char> buffer;
    test::RunPendingTasks();
    EXPECT_EQ(Result::kShouldWait, consumer->BeginRead(buffer));
    EXPECT_FALSE(isolate->HasPendingException());
    test::RunPendingTasks();
    EXPECT_EQ(PublicState::kErrored, consumer->GetPublicState());
    EXPECT_EQ(Result::kError, consumer->BeginRead(buffer));
    EXPECT_FALSE(isolate->HasPendingException());
  }
}

}  // namespace

}  // namespace blink
