// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

#include "gin/array_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

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
  ArrayBufferContents src;
  auto* buffer = DOMArrayBuffer::Create(src);
  ArrayBufferContents dst;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), dst,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  ASSERT_EQ(true, buffer->IsDetached());
  EXPECT_TRUE(dst.IsValid());
}

TEST(DOMArrayBufferTest, WrapEmpty) {
  test::TaskEnvironment task_environment;
  V8TestingScope v8_scope;
  ArrayBufferContents src;
  auto* buffer = DOMArrayBuffer::Create(src);
  v8::Local<v8::Value> wrapped = buffer->Wrap(v8_scope.GetScriptState());
  ASSERT_FALSE(wrapped.IsEmpty());
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

  v8::Local<v8::ArrayBuffer> wrapper_a =
      ToV8Traits<DOMArrayBuffer>::ToV8(script_state, buffer_a)
          .As<v8::ArrayBuffer>();
  ASSERT_EQ(buffer_a->Data(), wrapper_a->Data());

  // Calling JS ArrayBuffer.prototype.transfer() detaches `wrapper_a` and
  // returns a new JS ArrayBuffer over the same BackingStore, while `buffer_a`
  // on the native side still holds a reference to that BackingStore.
  v8::Local<v8::Value> transferred_val;
  ASSERT_TRUE(CallTransfer(script_state, wrapper_a).ToLocal(&transferred_val));
  ASSERT_TRUE(wrapper_a->WasDetached());
  v8::Local<v8::ArrayBuffer> wrapper_b = transferred_val.As<v8::ArrayBuffer>();
  ASSERT_EQ(buffer_a->Data(), wrapper_b->Data());

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

  // Because `buffer_a` still held a reference to the original BackingStore,
  // TransferOrCopy() must have copied the contents into a fresh BackingStore.
  EXPECT_TRUE(dst.IsValid());
  // Note the below generally does not have to hold true if we change the
  // implementation to also detach `buffer_a` when the wrapper is detached.
  EXPECT_NE(buffer_a->Data(), dst.Data());
  EXPECT_EQ(0x11111111u, static_cast<const uint32_t*>(dst.Data())[0]);
}

TEST(DOMArrayBufferTest,
     TransferAfterJSTransferWithIsolatedWorldWrapperDoesNotAlias) {
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
  // `main_wrapper`, leaving `isolated_wrapper` attached to `original_data`.
  v8::Local<v8::Value> transferred_val;
  ASSERT_TRUE(CallTransfer(main_state, main_wrapper).ToLocal(&transferred_val));
  ASSERT_TRUE(main_wrapper->WasDetached());
  ASSERT_FALSE(isolated_wrapper->WasDetached());
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

  // Step 3: Verify `dst` does not alias `isolated_wrapper`'s memory.
  ASSERT_TRUE(dst.IsValid());
  EXPECT_NE(dst.Data(), isolated_wrapper->Data());
  EXPECT_EQ(0x11111111u, static_cast<const uint32_t*>(dst.Data())[0]);

  // Writes to `dst` (e.g. on a worker thread) must not be visible in the
  // isolated world wrapper, and vice versa.
  static_cast<uint32_t*>(dst.Data())[0] = 0xDEADBEEFu;
  EXPECT_EQ(0x11111111u,
            static_cast<const uint32_t*>(isolated_wrapper->Data())[0]);

  static_cast<uint32_t*>(isolated_wrapper->Data())[0] = 0xCAFEBABEu;
  EXPECT_EQ(0xDEADBEEFu, static_cast<const uint32_t*>(dst.Data())[0]);
}

}  // namespace blink
