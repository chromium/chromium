// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"

#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"

namespace blink {

UnpackedSerializedScriptValue::UnpackedSerializedScriptValue(
    scoped_refptr<SerializedScriptValue> value)
    : value_(std::move(value)) {
  value_->RegisterMemoryAllocatedWithCurrentScriptContext();
  auto& array_buffer_contents = value_->array_buffer_contents_array_;
  if (!array_buffer_contents.IsEmpty()) {
    array_buffers_.Grow(array_buffer_contents.size());
    std::transform(array_buffer_contents.begin(), array_buffer_contents.end(),
                   array_buffers_.begin(),
                   [](ArrayBufferContents& contents) -> DOMArrayBufferBase* {
                     if (contents.IsShared())
                       return DOMSharedArrayBuffer::Create(contents);
                     return DOMArrayBuffer::Create(contents);
                   });
    array_buffer_contents.clear();
  }

  auto& image_bitmap_contents = value_->image_bitmap_contents_array_;
  if (!image_bitmap_contents.IsEmpty()) {
    image_bitmaps_.Grow(image_bitmap_contents.size());
    std::transform(image_bitmap_contents.begin(), image_bitmap_contents.end(),
                   image_bitmaps_.begin(),
                   [](scoped_refptr<StaticBitmapImage>& contents) {
                     return ImageBitmap::Create(std::move(contents));
                   });
    image_bitmap_contents.clear();
  }
}

UnpackedSerializedScriptValue::~UnpackedSerializedScriptValue() = default;

void UnpackedSerializedScriptValue::Trace(blink::Visitor* visitor) {
  visitor->Trace(array_buffers_);
  visitor->Trace(image_bitmaps_);
}

v8::Local<v8::Value> UnpackedSerializedScriptValue::Deserialize(
    v8::Isolate* isolate,
    const DeserializeOptions& options) {
  return SerializedScriptValueFactory::Instance().Deserialize(this, isolate,
                                                              options);
}

}  // namespace blink
