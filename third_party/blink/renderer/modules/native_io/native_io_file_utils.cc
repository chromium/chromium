// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/native_io/native_io_file_utils.h"

#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8-isolate.h"

namespace blink {

int NativeIOOperationSize(const DOMArrayBufferView& buffer) {
  // On 32-bit platforms, clamp operation sizes to 2^31-1.
  return base::saturated_cast<int>(buffer.byteLength());
}

DOMArrayBufferView* TransferToNewArrayBufferView(
    v8::Isolate* isolate,
    NotShared<DOMArrayBufferView> source,
    ExceptionState& exception_state) {
  size_t offset = source->byteOffset();
  size_t length = source->byteLength() / source->TypeSize();

  ArrayBufferContents target_contents;
  // Avoid transferring a non-detachable ArrayBuffer, to prevent copying and
  // ensure source detachment.
  if (!source->buffer()->IsDetachable(isolate)) {
    exception_state.ThrowTypeError("Could not transfer ArrayBuffer");
    return nullptr;
  }
  if (!source->buffer()->Transfer(isolate, target_contents, exception_state)) {
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

// static
std::unique_ptr<NativeIODataBuffer> NativeIODataBuffer::Create(
    ScriptState* script_state,
    NotShared<DOMArrayBufferView> source,
    ExceptionState& exception_state) {
  DCHECK(script_state);
  DCHECK(source);

  DOMArrayBufferView::ViewType type = source->GetType();
  size_t offset = source->byteOffset();
  size_t byte_length = source->byteLength();
  size_t length = byte_length / source->TypeSize();

  // Explicitly fail if the source buffer is not detachable. On its own,
  // Transfer() copies non-detachable input buffers.
  DOMArrayBuffer* buffer = source->buffer();
  v8::Isolate* isolate = script_state->GetIsolate();
  if (!buffer->IsDetachable(isolate)) {
    exception_state.ThrowTypeError("Could not transfer ArrayBuffer.");
    return nullptr;
  }
  ArrayBufferContents contents;
  if (!buffer->Transfer(isolate, contents, exception_state))
    return nullptr;
  DCHECK(source->IsDetached());

  return std::make_unique<NativeIODataBuffer>(
      std::move(contents), type, offset,
#if DCHECK_IS_ON()
      byte_length,
#endif  // DCHECK_IS_ON()
      length, base::PassKey<NativeIODataBuffer>());
}

NativeIODataBuffer::NativeIODataBuffer(ArrayBufferContents contents,
                                       DOMArrayBufferView::ViewType type,
                                       size_t offset,
#if DCHECK_IS_ON()
                                       size_t byte_length,
#endif  // DCHECK_IS_ON()
                                       size_t length,
                                       base::PassKey<NativeIODataBuffer>)
    : contents_(std::move(contents)),
      type_(type),
      offset_(offset),
#if DCHECK_IS_ON()
      byte_length_(byte_length),
#endif  // DCHECK_IS_ON()
      length_(length) {
  DCHECK(IsValid());
  DCHECK(!contents_.IsShared());

  // DataLength() returns 0 when called on an invalid ArrayBufferContents
  // (backing an empty array). This works as expected.
  DCHECK_LE(offset, contents_.DataLength());
#if DCHECK_IS_ON()
  DCHECK_LE(length, byte_length);
  DCHECK_LE(byte_length, contents_.DataLength() - offset);
#endif  // DCHECK_IS_ON()
}

NativeIODataBuffer::~NativeIODataBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool NativeIODataBuffer::IsValid() const {
  // The ArrayBufferContents is not shared when this instance is constructed. It
  // should not become shared while the instance is valid, because no other code
  // can gain access and make it shared.
  //
  // ArrayBufferContents::IsShared() returns false for invalid instances, which
  // works out well for this check.
  DCHECK(!contents_.IsShared());

  // Transferring the data out of an empty ArrayBuffer yields an invalid
  // ArrayBufferContents.
  return length_ == 0 || contents_.IsValid();
}

NotShared<DOMArrayBufferView> NativeIODataBuffer::Take() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsValid());

  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(std::move(contents_));

  DOMArrayBufferView* view = nullptr;
  switch (type_) {
    case DOMArrayBufferView::kTypeInt8:
      view = DOMInt8Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeUint8:
      view = DOMUint8Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeUint8Clamped:
      view = DOMUint8ClampedArray::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeInt16:
      view = DOMInt16Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeUint16:
      view = DOMUint16Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeInt32:
      view = DOMInt32Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeUint32:
      view = DOMUint32Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeFloat32:
      view = DOMFloat32Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeFloat64:
      view = DOMFloat64Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeBigInt64:
      view = DOMBigInt64Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeBigUint64:
      view = DOMBigUint64Array::Create(array_buffer, offset_, length_);
      break;

    case DOMArrayBufferView::kTypeDataView:
      view = DOMDataView::Create(array_buffer, offset_, length_);
      break;
  }
  return NotShared<DOMArrayBufferView>(view);
}

}  // namespace blink
