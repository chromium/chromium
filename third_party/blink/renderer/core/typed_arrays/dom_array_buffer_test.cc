// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include <array>

#include "gin/array_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_string.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_boolean_string.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_util.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

template <typename T, SharedBufferPolicy policy = SharedBufferPolicy::kDisallow>
concept CanCallAsSpan = requires(T t) { AsSpan<policy>(t); };

static_assert(CanCallAsSpan<V8UnionArrayBufferOrArrayBufferView&>);
static_assert(CanCallAsSpan<const V8UnionArrayBufferOrArrayBufferView&>);
static_assert(CanCallAsSpan<V8UnionArrayBufferOrArrayBufferView&,
                            SharedBufferPolicy::kAllow>);
static_assert(
    std::same_as<decltype(AsSpan<SharedBufferPolicy::kDisallow>(
                     std::declval<V8UnionArrayBufferOrArrayBufferView&>())),
                 base::span<uint8_t>>);
static_assert(std::same_as<
              decltype(AsSpan<SharedBufferPolicy::kDisallow>(
                  std::declval<const V8UnionArrayBufferOrArrayBufferView&>())),
              base::span<const uint8_t>>);
static_assert(
    !CanCallAsSpan<V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared&,
                   SharedBufferPolicy::kDisallow>);
static_assert(!CanCallAsSpan<
              const V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared&,
              SharedBufferPolicy::kDisallow>);
static_assert(
    CanCallAsSpan<V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared&,
                  SharedBufferPolicy::kAllow>);
static_assert(CanCallAsSpan<
              const V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared&,
              SharedBufferPolicy::kAllow>);
static_assert(CanCallAsSpan<V8UnionArrayBufferOrArrayBufferViewOrString&>);
static_assert(CanCallAsSpan<V8UnionArrayBufferOrString&>);
static_assert(!CanCallAsSpan<V8UnionArrayBufferOrArrayBufferView*>);
static_assert(!CanCallAsSpan<V8UnionBooleanOrString&>);
static_assert(!CanCallAsSpan<const V8UnionBooleanOrString&>);
static_assert(
    !CanCallAsSpan<V8UnionBooleanOrString&, SharedBufferPolicy::kAllow>);
static_assert(!CanCallAsSpan<DOMArrayBuffer&>);

ScriptState* IsolatedWorldScriptState(V8TestingScope& v8_scope,
                                      int32_t world_id) {
  DOMWrapperWorld* world =
      DOMWrapperWorld::EnsureIsolatedWorld(v8_scope.GetIsolate(), world_id);
  v8_scope.GetFrame().GetWindowProxy(*world);
  return ToScriptState(&v8_scope.GetFrame(), *world);
}

v8::MaybeLocal<v8::Value> CallTransfer(ScriptState* script_state,
                                       v8::Local<v8::ArrayBuffer> buffer) {
  v8::Local<v8::Context> context = script_state->GetContext();
  v8::Local<v8::Value> transfer =
      buffer->Get(context, V8String(script_state->GetIsolate(), "transfer"))
          .ToLocalChecked();
  return transfer.As<v8::Function>()->Call(context, buffer, 0, nullptr);
}

}  // namespace

TEST(DOMArrayBufferTest, TransferredArrayBufferIsDetached) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ArrayBufferContents src(10, 4, ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kZeroInitialize);
  void* original_data = src.Data();
  auto* buffer = DOMArrayBuffer::Create(std::move(src));
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
  EXPECT_EQ(nullptr, buffer->Data());
  EXPECT_EQ(0u, buffer->ByteLength());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(original_data, dst.Data());
}

TEST(DOMArrayBufferTest, TransferredEmptyArrayBufferIsDetached) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  auto* buffer = DOMArrayBuffer::Create(0, 1);
  ASSERT_FALSE(buffer->IsDetached());
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
  EXPECT_TRUE(dst.IsValid());
}

TEST(DOMArrayBufferTest, WrapDetached) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ArrayBufferContents src;
  auto* buffer = DOMArrayBuffer::Create(src);
  EXPECT_TRUE(buffer->IsDetached());
  v8::Local<v8::Value> wrapped = buffer->Wrap(v8_scope.GetScriptState());
  ASSERT_FALSE(wrapped.IsEmpty());
  EXPECT_TRUE(wrapped.As<v8::ArrayBuffer>()->WasDetached());
}

TEST(DOMArrayBufferTest, WrapEmpty) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  auto* buffer = DOMArrayBuffer::Create(0, 1);
  EXPECT_FALSE(buffer->IsDetached());
  v8::Local<v8::Value> wrapped = buffer->Wrap(v8_scope.GetScriptState());
  ASSERT_FALSE(wrapped.IsEmpty());
  EXPECT_FALSE(wrapped.As<v8::ArrayBuffer>()->WasDetached());
}

TEST(DOMArrayBufferTest, TransferCopiesWhenBackingStoreHasMultipleRefs) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ArrayBufferContents src(4, 4, ArrayBufferContents::kNotShared,
                          ArrayBufferContents::kZeroInitialize);
  static_cast<uint32_t*>(src.Data())[0] = 0x12345678u;
  void* original_data = src.Data();
  // Keep `src` alive so `buffer` does not have unique ownership (use_count >
  // 1).
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_TRUE(buffer->IsDetached());
  EXPECT_EQ(nullptr, buffer->Data());
  EXPECT_EQ(0u, buffer->ByteLength());
  EXPECT_TRUE(dst.IsValid());
  EXPECT_NE(original_data, dst.Data());
  EXPECT_EQ(0x12345678u, static_cast<const uint32_t*>(dst.Data())[0]);
}

TEST(DOMArrayBufferTest, TransferAfterJSTransferCopiesContents) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  v8::Isolate* isolate = v8_scope.GetIsolate();
  ScriptState* script_state = v8_scope.GetScriptState();

  auto* buffer_a = DOMArrayBuffer::Create(4, 4);
  static_cast<uint32_t*>(buffer_a->Data())[0] = 0x11111111u;
  void* original_data = buffer_a->Data();

  v8::Local<v8::ArrayBuffer> wrapper_a =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, buffer_a)
          .As<v8::ArrayBuffer>();
  ASSERT_EQ(original_data, wrapper_a->Data());

  // Calling JS ArrayBuffer.prototype.transfer() detaches `wrapper_a` (and
  // `buffer_a` via OnArrayBufferDetached) and returns a new JS ArrayBuffer over
  // the same BackingStore.
  v8::Local<v8::Value> transferred_val;
  ASSERT_TRUE(CallTransfer(script_state, wrapper_a).ToLocal(&transferred_val));
  ASSERT_TRUE(wrapper_a->WasDetached());
  EXPECT_TRUE(buffer_a->IsDetached());
  EXPECT_EQ(nullptr, buffer_a->Data());
  v8::Local<v8::ArrayBuffer> wrapper_b = transferred_val.As<v8::ArrayBuffer>();
  ASSERT_EQ(original_data, wrapper_b->Data());

  // Materialize a new DOMArrayBuffer for `wrapper_b` and transfer it.
  DOMArrayBuffer* buffer_b = NativeValueTraits<DOMArrayBuffer>::NativeValue(
      isolate, wrapper_b, v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_TRUE(buffer_b);
  ASSERT_NE(buffer_a, buffer_b);

  ArrayBufferContents dst;
  ASSERT_TRUE(buffer_b->Transfer(isolate, dst, v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_TRUE(buffer_b->IsDetached());
  EXPECT_TRUE(wrapper_b->WasDetached());
  EXPECT_EQ(nullptr, buffer_b->Data());
  EXPECT_EQ(0u, buffer_b->ByteLength());

  EXPECT_TRUE(dst.IsValid());
  EXPECT_EQ(original_data, dst.Data());
  EXPECT_EQ(0x11111111u, static_cast<const uint32_t*>(dst.Data())[0]);
}

TEST(DOMArrayBufferTest,
     TransferAfterJSTransferWithIsolatedWorldWrapperDetachesAll) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  v8::Isolate* isolate = v8_scope.GetIsolate();
  ScriptState* main_state = v8_scope.GetScriptState();
  static const int kIsolatedWorldId = 1;
  ScriptState* isolated_state =
      IsolatedWorldScriptState(v8_scope, kIsolatedWorldId);
  ASSERT_TRUE(isolated_state);

  auto* buffer_a = DOMArrayBuffer::Create(4, 4);
  static_cast<uint32_t*>(buffer_a->Data())[0] = 0x11111111u;
  void* original_data = buffer_a->Data();

  v8::Local<v8::ArrayBuffer> main_wrapper =
      ToV8Traits<DOMArrayBuffer>::ToV8(main_state, buffer_a)
          .As<v8::ArrayBuffer>();
  v8::Local<v8::ArrayBuffer> isolated_wrapper;
  {
    v8::Context::Scope isolated_scope(isolated_state->GetContext());
    isolated_wrapper =
        ToV8Traits<DOMArrayBuffer>::ToV8(isolated_state, buffer_a)
            .As<v8::ArrayBuffer>();
  }
  ASSERT_EQ(original_data, main_wrapper->Data());
  ASSERT_EQ(original_data, isolated_wrapper->Data());

  // Step 1: JS ArrayBuffer.prototype.transfer() in the main world detaches
  // `main_wrapper`, `buffer_a`, and `isolated_wrapper`.
  v8::Local<v8::Value> transferred_val;
  ASSERT_TRUE(CallTransfer(main_state, main_wrapper).ToLocal(&transferred_val));
  ASSERT_TRUE(main_wrapper->WasDetached());
  EXPECT_TRUE(isolated_wrapper->WasDetached());
  EXPECT_NE(original_data, isolated_wrapper->Data());
  EXPECT_EQ(main_wrapper->Data(), isolated_wrapper->Data());
  EXPECT_EQ(0u, isolated_wrapper->ByteLength());
  EXPECT_TRUE(buffer_a->IsDetached());
  EXPECT_EQ(nullptr, buffer_a->Data());
  v8::Local<v8::ArrayBuffer> transferred_wrapper =
      transferred_val.As<v8::ArrayBuffer>();
  ASSERT_EQ(original_data, transferred_wrapper->Data());

  // Step 2: Materialize a new DOMArrayBuffer for `transferred_wrapper` (as
  // postMessage transfer does) and transfer it to `dst`.
  DOMArrayBuffer* buffer_b = NativeValueTraits<DOMArrayBuffer>::NativeValue(
      isolate, transferred_wrapper, v8_scope.GetExceptionState());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_TRUE(buffer_b);

  ArrayBufferContents dst;
  ASSERT_TRUE(buffer_b->Transfer(isolate, dst, v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_TRUE(buffer_b->IsDetached());
  EXPECT_TRUE(transferred_wrapper->WasDetached());

  ASSERT_TRUE(dst.IsValid());
  EXPECT_EQ(original_data, dst.Data());
  EXPECT_EQ(0x11111111u, static_cast<const uint32_t*>(dst.Data())[0]);
}

TEST(DOMArrayBufferTest, WrapInIsolatedWorldAfterMainWorldWrapperDetached) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ScriptState* main_state = v8_scope.GetScriptState();
  ScriptState* isolated_state = IsolatedWorldScriptState(v8_scope, 1);
  ASSERT_TRUE(isolated_state);

  auto* buffer = DOMArrayBuffer::Create(16, 1);

  v8::Local<v8::ArrayBuffer> main_wrapper =
      ToV8Traits<DOMArrayBuffer>::ToV8(main_state, buffer)
          .As<v8::ArrayBuffer>();
  ASSERT_FALSE(main_wrapper->WasDetached());

  v8::Local<v8::Value> result;
  ASSERT_TRUE(CallTransfer(main_state, main_wrapper).ToLocal(&result));
  ASSERT_TRUE(main_wrapper->WasDetached());
  EXPECT_TRUE(buffer->IsDetached());

  // Wrapping in another world after the existing wrapper was detached must
  // produce a wrapper that is also detached.
  v8::Local<v8::ArrayBuffer> isolated_wrapper;
  {
    v8::Context::Scope isolated_scope(isolated_state->GetContext());
    isolated_wrapper = ToV8Traits<DOMArrayBuffer>::ToV8(isolated_state, buffer)
                           .As<v8::ArrayBuffer>();
  }
  EXPECT_TRUE(isolated_wrapper->WasDetached());
  EXPECT_EQ(0u, isolated_wrapper->ByteLength());
  EXPECT_TRUE(buffer->IsDetached());
}

TEST(DOMArrayBufferTest, AsSpanBufferSource) {
  constexpr std::array<uint8_t, 8> kData = {1, 2, 3, 4, 5, 6, 7, 8};
  auto* buffer = DOMArrayBuffer::Create(kData);

  auto* buffer_union =
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(buffer);
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kDisallow>(*buffer_union),
            buffer->ByteSpan());

  const auto* const_buffer_union = buffer_union;
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kDisallow>(*const_buffer_union),
            buffer->ByteSpan());

  auto* view = DOMUint8Array::Create(buffer, /*byte_offset=*/2, /*length=*/4);
  auto* view_union = MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMArrayBufferView>(view));
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kDisallow>(*view_union),
            buffer->ByteSpan().subspan(2u, 4u));
}

TEST(DOMArrayBufferTest, AsSpanAllowSharedBufferSource) {
  constexpr std::array<uint8_t, 8> kData = {10, 20, 30, 40, 50, 60, 70, 80};
  auto* shared_buffer = DOMSharedArrayBuffer::Create(kData);

  auto* shared_buffer_union = MakeGarbageCollected<
      V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared>(shared_buffer);
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kAllow>(*shared_buffer_union),
            shared_buffer->ByteSpanMaybeShared());

  auto* shared_view =
      DOMUint8Array::Create(shared_buffer, /*byte_offset=*/3, /*length=*/3);
  auto* shared_view_union = MakeGarbageCollected<
      V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared>(
      MaybeShared<DOMArrayBufferView>(shared_view));
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kAllow>(*shared_view_union),
            shared_buffer->ByteSpanMaybeShared().subspan(3u, 3u));
}

TEST(DOMArrayBufferTest, AsSpanMixedUnion) {
  constexpr std::array<uint8_t, 4> kData = {9, 8, 7, 6};
  auto* buffer = DOMArrayBuffer::Create(kData);

  auto* ab_or_str = MakeGarbageCollected<V8UnionArrayBufferOrString>(buffer);
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kDisallow>(*ab_or_str),
            buffer->ByteSpan());

  auto* view = DOMUint8Array::Create(buffer, /*byte_offset=*/1, /*length=*/2);
  auto* ab_abv_or_str =
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferViewOrString>(
          NotShared<DOMArrayBufferView>(view));
  EXPECT_EQ(AsSpan<SharedBufferPolicy::kDisallow>(*ab_abv_or_str),
            buffer->ByteSpan().subspan(1u, 2u));
}

}  // namespace blink
