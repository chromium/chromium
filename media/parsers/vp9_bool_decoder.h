// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_VP9_BOOL_DECODER_H_
#define MEDIA_PARSERS_VP9_BOOL_DECODER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "media/base/media_export.h"

namespace media {

class BitReader;

class MEDIA_EXPORT Vp9BoolDecoder {
 public:
  Vp9BoolDecoder();

  Vp9BoolDecoder(const Vp9BoolDecoder&) = delete;
  Vp9BoolDecoder& operator=(const Vp9BoolDecoder&) = delete;

  ~Vp9BoolDecoder();

  // |data| is the input buffer with |size| bytes.
  // Returns true if read first marker bit successfully.
  bool Initialize(const uint8_t* data, size_t size);

  // Returns true if none of the reads since the last Initialize() call has
  // gone beyond the end of available data.
  bool IsValid() const { return valid_; }

  // Reads one bit. B(p).
  // If the read goes beyond the end of buffer, the return value is undefined.
  bool ReadBool(int prob);

  // Reads a literal. L(n).
  // If the read goes beyond the end of buffer, the return value is undefined.
  uint8_t ReadLiteral(int bits);

  // Consumes padding bits up to end of data. Returns true if no
  // padding bits or they are all zero.
  bool ConsumePaddingBits();

 private:
  // The highest 8 bits of BigBool is actual "bool value". The remain bits
  // are optimization of prefill buffer.
  using BigBool = size_t;
  // The size of "bool value" used for boolean decoding defined in spec.
  const int kBoolSize = 8;
  const int kBigBoolBitSize = sizeof(BigBool) * 8;

  bool Fill();

  std::unique_ptr<BitReader> reader_;

  // Indicates if none of the reads since the last Initialize() call has gone
  // beyond the end of available data.
  bool valid_ = true;

  BigBool bool_value_ = 0;

  // Need to fill at least |count_to_fill_| bits. Negative value means extra
  // bits pre-filled.
  int count_to_fill_ = 0;
  unsigned int bool_range_ = 0;
};

}  // namespace media

#endif  // MEDIA_PARSERS_VP9_BOOL_DECODER_H_
