// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_VP9_RAW_BITS_READER_H_
#define MEDIA_PARSERS_VP9_RAW_BITS_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/base/media_export.h"

namespace media {

class BitReader;

// A class to read raw bits stream. See VP9 spec, "RAW-BITS DECODING" section
// for detail.
class MEDIA_EXPORT Vp9RawBitsReader {
 public:
  Vp9RawBitsReader();

  Vp9RawBitsReader(const Vp9RawBitsReader&) = delete;
  Vp9RawBitsReader& operator=(const Vp9RawBitsReader&) = delete;

  ~Vp9RawBitsReader();

  // |data| is the input buffer with |size| bytes.
  void Initialize(const uint8_t* data, size_t size);

  // Returns true if none of the reads since the last Initialize() call has
  // gone beyond the end of available data.
  bool IsValid() const { return valid_; }

  // Returns how many bytes were read since the last Initialize() call.
  // Partial bytes will be counted as one byte. For example, it will return 1
  // if 3 bits were read.
  size_t GetBytesRead() const;

  // Reads one bit.
  // If the read goes beyond the end of buffer, the return value is undefined.
  bool ReadBool();

  // Reads a literal with |bits| bits.
  // If the read goes beyond the end of buffer, the return value is undefined.
  int ReadLiteral(int bits);

  // Reads a signed literal with |bits| bits (not including the sign bit).
  // If the read goes beyond the end of buffer, the return value is undefined.
  int ReadSignedLiteral(int bits);

  // Consumes trailing bits up to next byte boundary. Returns true if no
  // trailing bits or they are all zero.
  bool ConsumeTrailingBits();

 private:
  std::unique_ptr<BitReader> reader_;

  // Indicates if none of the reads since the last Initialize() call has gone
  // beyond the end of available data.
  bool valid_;
};

}  // namespace media

#endif  // MEDIA_PARSERS_VP9_RAW_BITS_READER_H_
