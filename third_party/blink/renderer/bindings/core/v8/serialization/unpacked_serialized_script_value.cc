// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_shared_array_buffer.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

UnpackedSerializedScriptValue::UnpackedSerializedScriptValue(
    scoped_refptr<SerializedScriptValue> value)
    : value_(std::move(value)) {
  value_->RegisterMemoryAllocatedWithCurrentScriptContext();
  auto& array_buffer_contents = value_->array_buffer_contents_array_;
  if (!array_buffer_contents.empty()) {
    array_buffers_.Grow(array_buffer_contents.size());
    base::ranges::transform(
        array_buffer_contents, array_buffers_.begin(),
        [](ArrayBufferContents& contents) {
          return contents.IsShared()
                     ? static_cast<DOMArrayBufferBase*>(
                           DOMSharedArrayBuffer::Create(contents))
                     : DOMArrayBuffer::Create(contents);
        });
    array_buffer_contents.clear();
  }

  auto& image_bitmap_contents = value_->image_bitmap_contents_array_;
  if (!image_bitmap_contents.empty()) {
    image_bitmaps_.Grow(image_bitmap_contents.size());
    base::ranges::transform(
        image_bitmap_contents, image_bitmaps_.begin(),
        [](scoped_refptr<StaticBitmapImage>& contents) {
          return MakeGarbageCollected<ImageBitmap>(std::move(contents));
        });
    image_bitmap_contents.clear();
  }
}

UnpackedSerializedScriptValue::~UnpackedSerializedScriptValue() = default;

void UnpackedSerializedScriptValue::Trace(Visitor* visitor) const {
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
