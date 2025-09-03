// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h265_nalu_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "base/types/to_address.h"
#include "media/base/decrypt_config.h"

namespace media {

#define READ_BITS_OR_RETURN(num_bits, out)                                 \
  do {                                                                     \
    uint32_t _out;                                                         \
    if (!br_.ReadBits(num_bits, &_out)) {                                  \
      DVLOG(1)                                                             \
          << "Error in stream: unexpected EOS while trying to read " #out; \
      return kInvalidStream;                                               \
    }                                                                      \
    *out = _out;                                                           \
  } while (0)

#define TRUE_OR_RETURN(a)                                            \
  do {                                                               \
    if (!(a)) {                                                      \
      DVLOG(1) << "Error in stream: invalid value, expected " << #a; \
      return kInvalidStream;                                         \
    }                                                                \
  } while (0)

H265NALU::H265NALU() = default;

H265NaluParser::H265NaluParser() {
  Reset();
}

H265NaluParser::~H265NaluParser() {}

void H265NaluParser::Reset() {
  stream_ = {};
  encrypted_ranges_.clear();
  previous_nalu_range_.clear();
}

void H265NaluParser::SetStream(base::span<const uint8_t> stream) {
  std::vector<SubsampleEntry> subsamples;
  SetEncryptedStream(stream, subsamples);
}

void H265NaluParser::SetEncryptedStream(
    base::span<const uint8_t> stream,
    const std::vector<SubsampleEntry>& subsamples) {
  CHECK(!stream.empty());

  stream_ = stream;
  previous_nalu_range_.clear();

  encrypted_ranges_.clear();
  const uint8_t* start = stream.data();
  const uint8_t* stream_end = base::to_address(stream_.end());
  for (size_t i = 0; i < subsamples.size() && start < stream_end; ++i) {
    UNSAFE_TODO(start += subsamples[i].clear_bytes);

    const uint8_t* end =
        std::min(UNSAFE_TODO(start + subsamples[i].cypher_bytes), stream_end);
    encrypted_ranges_.Add(start, end);
    start = end;
  }
}

bool H265NaluParser::LocateNALU(size_t* nalu_size, size_t* start_code_size) {
  // Find the start code of next NALU.
  size_t nalu_start_off = 0;
  size_t annexb_start_code_size = 0;

  if (!H264Parser::FindStartCodeInClearRanges(stream_, encrypted_ranges_,
                                              &nalu_start_off,
                                              &annexb_start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }

  // Move the stream to the beginning of the NALU (pointing at the start code).
  stream_ = stream_.subspan(nalu_start_off);
  if (stream_.size() <= annexb_start_code_size) {
    DVLOG(3) << "End of stream";
    return false;
  }

  // Find the start code of next NALU;
  // if successful, |nalu_size_without_start_code| is the number of bytes from
  // after previous start code to before this one;
  // if next start code is not found, it is still a valid NALU since there
  // are some bytes left after the first start code: all the remaining bytes
  // belong to the current NALU.
  size_t next_start_code_size = 0;
  size_t nalu_size_without_start_code = 0;
  if (!H264Parser::FindStartCodeInClearRanges(
          stream_.subspan(annexb_start_code_size), encrypted_ranges_,
          &nalu_size_without_start_code, &next_start_code_size)) {
    nalu_size_without_start_code = stream_.size() - annexb_start_code_size;
  }
  *nalu_size = nalu_size_without_start_code + annexb_start_code_size;
  *start_code_size = annexb_start_code_size;
  return true;
}

H265NaluParser::Result H265NaluParser::AdvanceToNextNALU(H265NALU* nalu) {
  size_t start_code_size;
  size_t nalu_size_with_start_code;
  if (!LocateNALU(&nalu_size_with_start_code, &start_code_size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << stream_.size();
    stream_ = {};
    return kEOStream;
  }

  DCHECK(nalu);
  nalu->data = stream_.subspan(start_code_size,
                               nalu_size_with_start_code - start_code_size);
  DVLOG(4) << "NALU found: size=" << nalu_size_with_start_code;

  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data)) {
    return kEOStream;
  }

  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ = stream_.subspan(nalu_size_with_start_code);

  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  uint32_t data;
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);

  READ_BITS_OR_RETURN(6, &nalu->nal_unit_type);
  READ_BITS_OR_RETURN(6, &nalu->nuh_layer_id);
  READ_BITS_OR_RETURN(3, &nalu->nuh_temporal_id_plus1);
  TRUE_OR_RETURN(nalu->nuh_temporal_id_plus1 != 0);

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data.data())
           << " size: " << nalu->data.size();

  previous_nalu_range_.clear();
  previous_nalu_range_.Add(nalu->data.data(),
                           base::to_address(nalu->data.end()));
  return kOk;
}

std::vector<SubsampleEntry> H265NaluParser::GetCurrentSubsamples() {
  DCHECK_EQ(previous_nalu_range_.size(), 1u)
      << "This should only be called after a "
         "successful call to AdvanceToNextNalu()";

  auto intersection = encrypted_ranges_.IntersectionWith(previous_nalu_range_);
  return EncryptedRangesToSubsampleEntry(
      previous_nalu_range_.start(0), previous_nalu_range_.end(0), intersection);
}

}  // namespace media
