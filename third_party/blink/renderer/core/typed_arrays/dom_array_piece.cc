// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"

namespace blink {

DOMArrayPiece::DOMArrayPiece() {
  InitNull();
}

DOMArrayPiece::DOMArrayPiece(DOMArrayBuffer* buffer) {
  InitWithArrayBuffer(buffer);
}

DOMArrayPiece::DOMArrayPiece(DOMArrayBufferView* buffer) {
  InitWithArrayBufferView(buffer);
}

DOMArrayPiece::DOMArrayPiece(
    const V8UnionArrayBufferOrArrayBufferView* array_buffer_or_view) {
  DCHECK(array_buffer_or_view);

  switch (array_buffer_or_view->GetContentType()) {
    case V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBuffer:
      InitWithArrayBuffer(array_buffer_or_view->GetAsArrayBuffer());
      return;
    case V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBufferView:
      InitWithArrayBufferView(
          array_buffer_or_view->GetAsArrayBufferView().Get());
      return;
  }

  NOTREACHED_IN_MIGRATION();
  InitNull();
}

bool DOMArrayPiece::IsNull() const {
  return is_null_;
}

bool DOMArrayPiece::IsDetached() const {
  return is_detached_;
}

void* DOMArrayPiece::Data() const {
  DCHECK(!IsNull());
  return data_.data();
}

unsigned char* DOMArrayPiece::Bytes() const {
  return static_cast<unsigned char*>(Data());
}

size_t DOMArrayPiece::ByteLength() const {
  DCHECK(!IsNull());
  return data_.size_bytes();
}

base::span<uint8_t> DOMArrayPiece::ByteSpan() const {
  DCHECK(!IsNull());
  return data_;
}

void DOMArrayPiece::InitWithArrayBuffer(DOMArrayBuffer* buffer) {
  if (buffer) {
    InitWithData(buffer->ByteSpan());
    is_detached_ = buffer->IsDetached();
  } else {
    InitNull();
  }
}

void DOMArrayPiece::InitWithArrayBufferView(DOMArrayBufferView* buffer) {
  if (buffer) {
    InitWithData(buffer->ByteSpan());
    is_detached_ = buffer->buffer() ? buffer->buffer()->IsDetached() : true;
  } else {
    InitNull();
  }
}

void DOMArrayPiece::InitWithData(base::span<uint8_t> data) {
  data_ = data;
  is_null_ = false;
  is_detached_ = false;
}

void DOMArrayPiece::InitNull() {
  data_ = base::span<uint8_t>();
  is_null_ = true;
  is_detached_ = false;
}

}  // namespace blink
