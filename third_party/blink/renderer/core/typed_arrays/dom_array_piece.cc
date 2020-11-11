// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"

namespace blink {

DOMArrayPiece::DOMArrayPiece(
    const ArrayBufferOrArrayBufferView& array_buffer_or_view) {
  if (array_buffer_or_view.IsArrayBuffer()) {
    DOMArrayBuffer* array_buffer = array_buffer_or_view.GetAsArrayBuffer();
    InitWithArrayBuffer(array_buffer);
  } else if (array_buffer_or_view.IsArrayBufferView()) {
    DOMArrayBufferView* array_buffer_view =
        array_buffer_or_view.GetAsArrayBufferView().View();
    InitWithArrayBufferView(array_buffer_view);
  }
}
///////////////////////////////////////////////////////
DOMArrayPiece::DOMArrayPiece() {
  InitNull();
}

DOMArrayPiece::DOMArrayPiece(DOMArrayBuffer* buffer) {
  InitWithArrayBuffer(buffer);
}

DOMArrayPiece::DOMArrayPiece(DOMArrayBufferView* buffer) {
  InitWithArrayBufferView(buffer);
}

bool DOMArrayPiece::IsNull() const {
  return is_null_;
}

bool DOMArrayPiece::IsDetached() const {
  return is_detached_;
}

void* DOMArrayPiece::Data() const {
  DCHECK(!IsNull());
  return data_;
}

unsigned char* DOMArrayPiece::Bytes() const {
  return static_cast<unsigned char*>(Data());
}

size_t DOMArrayPiece::ByteLength() const {
  DCHECK(!IsNull());
  return byte_length_;
}

void DOMArrayPiece::InitWithArrayBuffer(DOMArrayBuffer* buffer) {
  if (buffer) {
    InitWithData(buffer->Data(), buffer->ByteLength());
    is_detached_ = buffer->IsDetached();
  } else {
    InitNull();
  }
}

void DOMArrayPiece::InitWithArrayBufferView(DOMArrayBufferView* buffer) {
  if (buffer) {
    InitWithData(buffer->BaseAddress(), buffer->byteLength());
    is_detached_ = buffer->buffer() ? buffer->buffer()->IsDetached() : true;
  } else {
    InitNull();
  }
}

void DOMArrayPiece::InitWithData(void* data, size_t byte_length) {
  byte_length_ = byte_length;
  data_ = data;
  is_null_ = false;
  is_detached_ = false;
}

void DOMArrayPiece::InitNull() {
  byte_length_ = 0;
  data_ = nullptr;
  is_null_ = true;
  is_detached_ = false;
}

}  // namespace blink
