// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

/*
 * Copyright (c) 2010, The WebM Project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google, nor the WebM Project, nor the names
 *     of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This file is modified from the dboolhuff.{c,h} from the WebM's libvpx
// project. (http://www.webmproject.org/code)
// It is used to decode bits from a vp8 stream.

#ifndef MEDIA_PARSERS_VP8_BOOL_DECODER_H_
#define MEDIA_PARSERS_VP8_BOOL_DECODER_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/macros.h"
#include "media/parsers/media_parsers_export.h"

namespace media {

// A class to decode the VP8's boolean entropy coded stream. It's a variant of
// arithmetic coding. See RFC 6386 - Chapter 7. Boolean Entropy Decoder.
class MEDIA_PARSERS_EXPORT Vp8BoolDecoder {
 public:
  Vp8BoolDecoder();

  // Initializes the decoder to start decoding |data|, |size| being size
  // of |data| in bytes. Returns false if |data| is NULL or empty.
  bool Initialize(const uint8_t* data, size_t size);

  // Reads a boolean from the coded stream. Returns false if it has reached the
  // end of |data| and failed to read the boolean. The probability of |out| to
  // be true is |probability| / 256, e.g., when |probability| is 0x80, the
  // chance is 1/2 (i.e., 0x80 / 256).
  bool ReadBool(bool* out, uint8_t probability);

  // Reads a boolean from the coded stream with the default probability 1/2.
  // Returns false if it has reached the end of |data| and failed to read the
  // boolean.
  bool ReadBool(bool* out);

  // Reads a "literal", that is, a "num_bits"-wide unsigned value whose bits
  // come high- to low-order, with each bit encoded at probability 1/2.
  // Returns false if it has reached the end of |data| and failed to read the
  // literal.
  bool ReadLiteral(size_t num_bits, int* out);

  // Reads a literal with sign from the coded stream. This is similar to
  // the ReadListeral(), it first read a "num_bits"-wide unsigned value, and
  // then read an extra bit as the sign of the literal. Returns false if it has
  // reached the end of |data| and failed to read the literal or the sign.
  // This is different from the "read_signed_literal(d, n)" defined in RFC 6386.
  bool ReadLiteralWithSign(size_t num_bits, int* out);

  // The following methods are used to get the internal states of the decoder.

  // Returns the bit offset to the current top bit of the coded stream. It is
  // also the number of bits that have been written in the corresponding
  // encoding state. More specifically, we have the following constraint:
  //    w + (bottom * S) <= v < w + (bottom + range) * S,
  // where "w" is for the bits already written,
  //       "v" is for the possible values of the coded number.
  //       "S" is the scale for the current bit position,
  //           i.e., S = pow(2, -(n + 8)), where "n" is the bit number of "w".
  // BitOffset() returns the bit count of "w", i.e., "n".
  size_t BitOffset();

  // Gets the "bottom" of the current coded value. See BitOffset() for
  // more details.
  uint8_t GetBottom();

  // Gets the "range" of the current coded value. See BitOffset() for
  // more details.
  uint8_t GetRange();

 private:
  // Reads the next bit from the coded stream. The probability of the bit to
  // be one is |probability| / 256.
  int ReadBit(int probability);

  // Fills more bits from |user_buffer_| to |value_|. We shall keep at least 8
  // bits of the current |user_buffer_| in |value_|.
  void FillDecoder();

  // Returns true iff we have ran out of bits.
  bool OutOfBuffer();

  const uint8_t* user_buffer_;
  const uint8_t* user_buffer_start_;
  const uint8_t* user_buffer_end_;
  size_t value_;
  int count_;
  size_t range_;

  DISALLOW_COPY_AND_ASSIGN(Vp8BoolDecoder);
};

}  // namespace media

#endif  // MEDIA_PARSERS_VP8_BOOL_DECODER_H_
