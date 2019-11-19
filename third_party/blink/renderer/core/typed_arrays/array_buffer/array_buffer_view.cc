/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_view.h"

#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer.h"

namespace blink {

ArrayBufferView::ArrayBufferView(scoped_refptr<ArrayBuffer> buffer,
                                 unsigned byte_offset)
    : byte_offset_(byte_offset),
      is_detachable_(true),
      buffer_(std::move(buffer)) {
  base_address_ =
      buffer_ ? (static_cast<char*>(buffer_->DataMaybeShared()) + byte_offset_)
              : nullptr;
  if (buffer_)
    buffer_->AddView(this);
}

ArrayBufferView::~ArrayBufferView() {
  if (buffer_)
    buffer_->RemoveView(this);
}

void ArrayBufferView::Detach() {
  buffer_ = nullptr;
  base_address_ = nullptr;
  byte_offset_ = 0;
}

const char* ArrayBufferView::TypeName() {
  switch (GetType()) {
    case kTypeInt8:
      return "Int8";
      break;
    case kTypeUint8:
      return "UInt8";
      break;
    case kTypeUint8Clamped:
      return "UInt8Clamped";
      break;
    case kTypeInt16:
      return "Int16";
      break;
    case kTypeUint16:
      return "UInt16";
      break;
    case kTypeInt32:
      return "Int32";
      break;
    case kTypeUint32:
      return "Uint32";
      break;
    case kTypeBigInt64:
      return "BigInt64";
      break;
    case kTypeBigUint64:
      return "BigUint64";
      break;
    case kTypeFloat32:
      return "Float32";
      break;
    case kTypeFloat64:
      return "Float64";
      break;
    case kTypeDataView:
      return "DataView";
      break;
  }
  NOTREACHED();
  return "Unknown";
}

}  // namespace blink
