// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message_mojom_traits.h"

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

scoped_refptr<SerializedScriptValue> BuildSerializedScriptValue(
    v8::Isolate* isolate,
    v8::Local<v8::Value> value,
    Transferables& transferables) {
  SerializedScriptValue::SerializeOptions options;
  options.transferables = &transferables;
  ExceptionState exceptionState(isolate, ExceptionState::kExecutionContext,
                                "MessageChannel", "postMessage");
  return SerializedScriptValue::Serialize(isolate, value, options,
                                          exceptionState);
}

TEST(BlinkTransferableMessageStructTraitsTest,
     ArrayBufferTransferOutOfScopeSucceeds) {
  // More exhaustive tests in web_tests/. This is a sanity check.
  // Build the original ArrayBuffer in a block scope to simulate situations
  // where a buffer may be freed twice.
  mojo::Message mojo_message;
  {
    V8TestingScope scope;
    v8::Isolate* isolate = scope.GetIsolate();
    size_t num_elements = 8;
    v8::Local<v8::ArrayBuffer> v8_buffer =
        v8::ArrayBuffer::New(isolate, num_elements);
    auto backing_store = v8_buffer->GetBackingStore();
    uint8_t* original_data = static_cast<uint8_t*>(backing_store->Data());
    for (size_t i = 0; i < num_elements; i++)
      original_data[i] = static_cast<uint8_t>(i);

    DOMArrayBuffer* array_buffer =
        NativeValueTraits<DOMArrayBuffer>::NativeValue(
            isolate, v8_buffer, scope.GetExceptionState());
    Transferables transferables;
    transferables.array_buffers.push_back(array_buffer);
    BlinkTransferableMessage msg;
    msg.sender_origin = SecurityOrigin::CreateUniqueOpaque();
    msg.sender_agent_cluster_id = base::UnguessableToken::Create();
    msg.message = BuildSerializedScriptValue(scope.GetIsolate(), v8_buffer,
                                             transferables);
    mojo_message = mojom::blink::TransferableMessage::SerializeAsMessage(&msg);
  }

  BlinkTransferableMessage out;
  ASSERT_TRUE(mojom::blink::TransferableMessage::DeserializeFromMessage(
      std::move(mojo_message), &out));
  ASSERT_EQ(out.message->GetArrayBufferContentsArray().size(), 1U);
  ArrayBufferContents& deserialized_contents =
      out.message->GetArrayBufferContentsArray()[0];
  Vector<uint8_t> deserialized_data;
  deserialized_data.Append(static_cast<uint8_t*>(deserialized_contents.Data()),
                           8);
  ASSERT_EQ(deserialized_data.size(), 8U);
  for (wtf_size_t i = 0; i < deserialized_data.size(); i++) {
    ASSERT_TRUE(deserialized_data[i] == i);
  }
}

TEST(BlinkTransferableMessageStructTraitsTest,
     ArrayBufferContentsLazySerializationSucceeds) {
  // More exhaustive tests in web_tests/. This is a sanity check.
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  size_t num_elements = 8;
  v8::Local<v8::ArrayBuffer> v8_buffer =
      v8::ArrayBuffer::New(isolate, num_elements);
  auto backing_store = v8_buffer->GetBackingStore();
  void* originalContentsData = backing_store->Data();
  uint8_t* contents = static_cast<uint8_t*>(originalContentsData);
  for (size_t i = 0; i < num_elements; i++)
    contents[i] = static_cast<uint8_t>(i);

  DOMArrayBuffer* original_array_buffer =
      NativeValueTraits<DOMArrayBuffer>::NativeValue(isolate, v8_buffer,
                                                     scope.GetExceptionState());
  Transferables transferables;
  transferables.array_buffers.push_back(original_array_buffer);
  BlinkTransferableMessage msg;
  msg.sender_origin = SecurityOrigin::CreateUniqueOpaque();
  msg.sender_agent_cluster_id = base::UnguessableToken::Create();
  msg.message =
      BuildSerializedScriptValue(scope.GetIsolate(), v8_buffer, transferables);
  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(std::move(msg));

  BlinkTransferableMessage out;
  ASSERT_TRUE(mojom::blink::TransferableMessage::DeserializeFromMessage(
      std::move(mojo_message), &out));
  ASSERT_EQ(out.message->GetArrayBufferContentsArray().size(), 1U);

  // When using WrapAsMessage, the deserialized ArrayBufferContents should own
  // the original ArrayBufferContents' data (as opposed to a copy of the data).
  ArrayBufferContents& deserialized_contents =
      out.message->GetArrayBufferContentsArray()[0];
  ASSERT_EQ(originalContentsData, deserialized_contents.Data());

  // The original ArrayBufferContents should be detached.
  ASSERT_EQ(nullptr, v8_buffer->GetBackingStore()->Data());
  ASSERT_TRUE(original_array_buffer->IsDetached());
}

ImageBitmap* CreateBitmap() {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(8, 4);
  surface->getCanvas()->clear(SK_ColorRED);
  return MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot()));
}

TEST(BlinkTransferableMessageStructTraitsTest,
     BitmapTransferOutOfScopeSucceeds) {
  // More exhaustive tests in web_tests/. This is a sanity check.
  // Build the original ImageBitmap in a block scope to simulate situations
  // where a buffer may be freed twice.
  mojo::Message mojo_message;
  {
    V8TestingScope scope;
    ImageBitmap* image_bitmap = CreateBitmap();
    v8::Local<v8::Value> wrapper =
        ToV8Traits<ImageBitmap>::ToV8(scope.GetScriptState(), image_bitmap)
            .ToLocalChecked();
    Transferables transferables;
    transferables.image_bitmaps.push_back(image_bitmap);
    BlinkTransferableMessage msg;
    msg.sender_origin = SecurityOrigin::CreateUniqueOpaque();
    msg.sender_agent_cluster_id = base::UnguessableToken::Create();
    msg.message =
        BuildSerializedScriptValue(scope.GetIsolate(), wrapper, transferables);
    mojo_message = mojom::blink::TransferableMessage::SerializeAsMessage(&msg);
  };

  BlinkTransferableMessage out;
  ASSERT_TRUE(mojom::blink::TransferableMessage::DeserializeFromMessage(
      std::move(mojo_message), &out));
  ASSERT_EQ(out.message->GetImageBitmapContentsArray().size(), 1U);
}

TEST(BlinkTransferableMessageStructTraitsTest,
     BitmapLazySerializationSucceeds) {
  // More exhaustive tests in web_tests/. This is a sanity check.
  V8TestingScope scope;
  ImageBitmap* original_bitmap = CreateBitmap();
  // The original bitmap's height and width will be 0 after it is transferred.
  size_t original_bitmap_height = original_bitmap->height();
  size_t original_bitmap_width = original_bitmap->width();
  scoped_refptr<SharedBuffer> original_bitmap_data =
      original_bitmap->BitmapImage()->Data();
  v8::Local<v8::Value> wrapper = ToV8(original_bitmap, scope.GetScriptState());
  Transferables transferables;
  transferables.image_bitmaps.push_back(std::move(original_bitmap));
  BlinkTransferableMessage msg;
  msg.sender_origin = SecurityOrigin::CreateUniqueOpaque();
  msg.sender_agent_cluster_id = base::UnguessableToken::Create();
  msg.message =
      BuildSerializedScriptValue(scope.GetIsolate(), wrapper, transferables);
  mojo::Message mojo_message =
      mojom::blink::TransferableMessage::WrapAsMessage(std::move(msg));

  // Deserialize the mojo message.
  BlinkTransferableMessage out;
  ASSERT_TRUE(mojom::blink::TransferableMessage::DeserializeFromMessage(
      std::move(mojo_message), &out));
  ASSERT_EQ(out.message->GetImageBitmapContentsArray().size(), 1U);
  scoped_refptr<blink::StaticBitmapImage> deserialized_bitmap_contents =
      out.message->GetImageBitmapContentsArray()[0];
  auto* deserialized_bitmap = MakeGarbageCollected<ImageBitmap>(
      std::move(deserialized_bitmap_contents));
  ASSERT_EQ(deserialized_bitmap->height(), original_bitmap_height);
  ASSERT_EQ(deserialized_bitmap->width(), original_bitmap_width);
  // When using WrapAsMessage, the deserialized bitmap should own
  // the original bitmap' data (as opposed to a copy of the data).
  ASSERT_EQ(original_bitmap_data, deserialized_bitmap->BitmapImage()->Data());
  ASSERT_TRUE(original_bitmap->IsNeutered());
}

}  // namespace blink
