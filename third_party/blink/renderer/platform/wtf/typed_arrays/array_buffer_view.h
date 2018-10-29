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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_VIEW_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_VIEW_H_

#include <limits.h>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/typed_arrays/array_buffer.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

class WTF_EXPORT ArrayBufferView : public RefCounted<ArrayBufferView> {
 public:
  enum ViewType {
    kTypeInt8,
    kTypeUint8,
    kTypeUint8Clamped,
    kTypeInt16,
    kTypeUint16,
    kTypeInt32,
    kTypeUint32,
    kTypeFloat32,
    kTypeFloat64,
    kTypeBigInt64,
    kTypeBigUint64,
    kTypeDataView
  };
  virtual ViewType GetType() const = 0;
  const char* TypeName();

  ArrayBuffer* Buffer() const { return buffer_.get(); }

  void* BaseAddress() const {
    DCHECK(!IsShared());
    return base_address_;
  }
  void* BaseAddressMaybeShared() const { return base_address_; }

  unsigned ByteOffset() const { return byte_offset_; }

  virtual unsigned ByteLength() const = 0;
  virtual unsigned TypeSize() const = 0;

  void SetNeuterable(bool flag) { is_neuterable_ = flag; }
  bool IsNeuterable() const { return is_neuterable_; }
  bool IsShared() const { return buffer_ ? buffer_->IsShared() : false; }

  virtual ~ArrayBufferView();

 protected:
  ArrayBufferView(scoped_refptr<ArrayBuffer>, unsigned byte_offset);

  inline bool SetImpl(ArrayBufferView*, unsigned byte_offset);

  // Helper to verify that a given sub-range of an ArrayBuffer is
  // within range.
  template <typename T>
  static bool VerifySubRange(const ArrayBuffer* buffer,
                             unsigned byte_offset,
                             unsigned num_elements) {
    if (!buffer)
      return false;
    if (sizeof(T) > 1 && byte_offset % sizeof(T))
      return false;
    if (byte_offset > buffer->ByteLength())
      return false;
    unsigned remaining_elements =
        static_cast<unsigned>((buffer->ByteLength() - byte_offset) / sizeof(T));
    if (num_elements > remaining_elements)
      return false;
    return true;
  }

  virtual void Neuter();

  // This is the address of the ArrayBuffer's storage, plus the byte offset.
  void* base_address_;

  unsigned byte_offset_ : 31;
  unsigned is_neuterable_ : 1;

 private:
  friend class ArrayBuffer;
  scoped_refptr<ArrayBuffer> buffer_;
  ArrayBufferView* prev_view_;
  ArrayBufferView* next_view_;
};

bool ArrayBufferView::SetImpl(ArrayBufferView* array, unsigned byte_offset) {
  if (byte_offset > ByteLength() ||
      byte_offset + array->ByteLength() > ByteLength() ||
      byte_offset + array->ByteLength() < byte_offset) {
    // Out of range offset or overflow
    return false;
  }

  char* base = static_cast<char*>(BaseAddress());
  memmove(base + byte_offset, array->BaseAddress(), array->ByteLength());
  return true;
}

}  // namespace WTF

using WTF::ArrayBufferView;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TYPED_ARRAYS_ARRAY_BUFFER_VIEW_H_
