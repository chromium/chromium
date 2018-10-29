// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// HpackVarintDecoder decodes HPACK variable length unsigned integers. These
// integers are used to identify static or dynamic table index entries, to
// specify string lengths, and to update the size limit of the dynamic table.
//
// The caller will need to validate that the decoded value is in an acceptable
// range.
//
// In order to support naive encoders (i.e. which always output 5 extension
// bytes for a uint32 that is >= prefix_mask), the decoder supports an an
// encoding with up to 5 extension bytes, and a maximum value of 268,435,582
// (4 "full" extension bytes plus the maximum for a prefix, 127). It could be
// modified to support a lower maximum value (by requiring that extensions bytes
// be "empty"), or a larger value if valuable for some reason I can't see.
//
// For details of the encoding, see:
//        http://httpwg.org/specs/rfc7541.html#integer.representation
//
// The decoder supports decoding integers up to 2^28 + 2^prefix_length - 2.
//
// TODO(jamessynge): Consider dropping support for encodings of more than 4
// bytes, including the prefix byte, as in practice we only see at most 3 bytes,
// and 4 bytes would cover any desire to support large (but not ridiculously
// large) header values.

#ifndef NET_THIRD_PARTY_HTTP2_HPACK_VARINT_HPACK_VARINT_DECODER_H_
#define NET_THIRD_PARTY_HTTP2_HPACK_VARINT_HPACK_VARINT_DECODER_H_

#include <cstdint>
#include <limits>

#include "base/logging.h"
#include "net/third_party/http2/decoder/decode_buffer.h"
#include "net/third_party/http2/decoder/decode_status.h"
#include "net/third_party/http2/platform/api/http2_export.h"
#include "net/third_party/http2/platform/api/http2_string.h"

namespace http2 {
// Decodes an HPACK variable length unsigned integer, in a resumable fashion
// so it can handle running out of input in the DecodeBuffer. Call Start or
// StartExtended the first time (when decoding the byte that contains the
// prefix), then call Resume later if it is necessary to resume. When done,
// call value() to retrieve the decoded value.
//
// No constructor or destructor. Holds no resources, so destruction isn't
// needed. Start and StartExtended handles the initialization of member
// variables. This is necessary in order for HpackVarintDecoder to be part
// of a union.
class HTTP2_EXPORT_PRIVATE HpackVarintDecoder {
 public:
  // |prefix_value| is the first byte of the encoded varint.
  // |prefix_length| is number of bits in the first byte that are used for
  // encoding the integer.  |db| is the rest of the buffer,  that is, not
  // including the first byte.
  DecodeStatus Start(uint8_t prefix_value,
                     uint8_t prefix_length,
                     DecodeBuffer* db);

  // The caller has already determined that the encoding requires multiple
  // bytes, i.e. that the 3 to 7 low-order bits (the number determined by
  // |prefix_length|) of the first byte are are all 1.  |db| is the rest of the
  // buffer,  that is, not including the first byte.
  DecodeStatus StartExtended(uint8_t prefix_length, DecodeBuffer* db);

  // Resume decoding a variable length integer after an earlier
  // call to Start or StartExtended returned kDecodeInProgress.
  DecodeStatus Resume(DecodeBuffer* db);

  uint32_t value() const;

  // This supports optimizations for the case of a varint with zero extension
  // bytes, where the handling of the prefix is done by the caller.
  void set_value(uint32_t v);

  // All the public methods below are for supporting assertions and tests.

  Http2String DebugString() const;

  // For benchmarking, these methods ensure the decoder
  // is NOT inlined into the caller.
  DecodeStatus StartForTest(uint8_t prefix_value,
                            uint8_t prefix_length,
                            DecodeBuffer* db);
  DecodeStatus StartExtendedForTest(uint8_t prefix_length, DecodeBuffer* db);
  DecodeStatus ResumeForTest(DecodeBuffer* db);

 private:
  // Protection in case Resume is called when it shouldn't be.
  void MarkDone() {
#ifndef NDEBUG
    offset_ = std::numeric_limits<uint32_t>::max();
#endif
  }
  void CheckNotDone() const {
#ifndef NDEBUG
    DCHECK_NE(std::numeric_limits<uint32_t>::max(), offset_);
#endif
  }
  void CheckDone() const {
#ifndef NDEBUG
    DCHECK_EQ(std::numeric_limits<uint32_t>::max(), offset_);
#endif
  }

  // These fields are initialized just to keep ASAN happy about reading
  // them from DebugString().
  uint32_t value_ = 0;
  uint32_t offset_ = 0;
};

}  // namespace http2

#endif  // NET_THIRD_PARTY_HTTP2_HPACK_VARINT_HPACK_VARINT_DECODER_H_
