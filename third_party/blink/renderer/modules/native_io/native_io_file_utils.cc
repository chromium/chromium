// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_utils.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"

namespace blink {

int NativeIOOperationSize(const DOMArrayBufferView& buffer) {
  // On 32-bit platforms, clamp operation sizes to 2^31-1.
  return base::saturated_cast<int>(buffer.byteLength());
}

DOMArrayBufferView* TransferToNewArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source) {
  size_t offset = source->byteOffset();
  size_t length = source->byteLength() / source->TypeSize();

  ArrayBufferContents target_contents;
  // Avoid transferring a non-detachable ArrayBuffer, to prevent copying and
  // ensure source detachment.
  if (!source->buffer()->IsDetachable(isolate) ||
      !source->buffer()->Transfer(isolate, target_contents)) {
    return nullptr;
  }
  DOMArrayBuffer* target_buffer =
      DOMArrayBuffer::Create(std::move(target_contents));

  DOMArrayBufferView* target;
  switch (source->GetType()) {
    case DOMArrayBufferView::kTypeInt8:
      target = DOMInt8Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint8:
      target = DOMUint8Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint8Clamped:
      target = DOMUint8ClampedArray::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeInt16:
      target = DOMInt16Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint16:
      target = DOMUint16Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeInt32:
      target = DOMInt32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeUint32:
      target = DOMUint32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeFloat32:
      target = DOMFloat32Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeFloat64:
      target = DOMFloat64Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeBigInt64:
      target = DOMBigInt64Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeBigUint64:
      target = DOMBigUint64Array::Create(target_buffer, offset, length);
      break;
    case DOMArrayBufferView::kTypeDataView:
      target = DOMDataView::Create(target_buffer, offset, length);
      break;
  }
  return target;
}

}  // namespace blink
