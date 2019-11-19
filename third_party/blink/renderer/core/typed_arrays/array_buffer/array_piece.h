// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_PIECE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_PIECE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ArrayBuffer;
class ArrayBufferView;

// This class is for passing around un-owned bytes as a pointer + length.
// It supports implicit conversion from several other data types.
//
// ArrayPiece has the concept of being "null". This is different from an empty
// byte range. It is invalid to call methods other than isNull() on such
// instances.
//
// IMPORTANT: The data contained by ArrayPiece is NOT OWNED, so caution must be
//            taken to ensure it is kept alive.
class CORE_EXPORT ArrayPiece {
  DISALLOW_NEW();

 public:
  // Constructs a "null" ArrayPiece object.
  ArrayPiece();

  // Constructs an ArrayPiece from the given ArrayBuffer. If the input is a
  // nullptr, then the constructed instance will be isNull().
  ArrayPiece(ArrayBuffer*);
  ArrayPiece(ArrayBufferView*);

  bool IsNull() const;
  bool IsDetached() const;
  void* Data() const;
  unsigned char* Bytes() const;
  unsigned ByteLength() const;

 protected:
  void InitWithArrayBuffer(ArrayBuffer*);
  void InitWithArrayBufferView(ArrayBufferView*);
  void InitWithData(void* data, unsigned byte_length);

 private:
  void InitNull();

  void* data_;
  unsigned byte_length_;
  bool is_null_;
  bool is_detached_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_ARRAY_BUFFER_ARRAY_PIECE_H_
