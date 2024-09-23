// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h266_nalu_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "media/base/decrypt_config.h"
#include "media/parsers/bit_reader_macros.h"

namespace media {

H266NALU::H266NALU() {
  memset(reinterpret_cast<void*>(this), 0, sizeof(*this));
}

H266NaluParser::H266NaluParser() {
  Reset();
}

H266NaluParser::~H266NaluParser() = default;

void H266NaluParser::Reset() {
  stream_ = nullptr;
  bytes_left_ = 0;
  encrypted_ranges_.clear();
  previous_nalu_range_.clear();
}

void H266NaluParser::SetStream(const uint8_t* stream, off_t stream_size) {
  std::vector<SubsampleEntry> subsamples;
  SetEncryptedStream(stream, stream_size, subsamples);
}

void H266NaluParser::SetEncryptedStream(
    const uint8_t* stream,
    off_t stream_size,
    const std::vector<SubsampleEntry>& subsamples) {
  DCHECK(stream);
  DCHECK_GT(stream_size, 0);
  stream_ = stream;
  bytes_left_ = stream_size;
  previous_nalu_range_.clear();
  encrypted_ranges_.clear();
  const uint8_t* start = stream;
  const uint8_t* stream_end = stream_ + bytes_left_;
  for (size_t i = 0; i < subsamples.size() && start < stream_end; ++i) {
    start += subsamples[i].clear_bytes;
    const uint8_t* end =
        std::min(start + subsamples[i].cypher_bytes, stream_end);
    encrypted_ranges_.Add(start, end);
    start = end;
  }
}

bool H266NaluParser::LocateNALU(off_t* nalu_size, off_t* start_code_size) {
  // Find the start code of next NALU.
  off_t nalu_start_off = 0;
  off_t annexb_start_code_size = 0;
  if (!H264Parser::FindStartCodeInClearRanges(
          stream_, bytes_left_, encrypted_ranges_, &nalu_start_off,
          &annexb_start_code_size)) {
    DVLOG(4) << "Could not find start code, end of stream?";
    return false;
  }
  // Move the stream to the beginning of the NALU (pointing at the start code).
  stream_ += nalu_start_off;
  bytes_left_ -= nalu_start_off;
  const uint8_t* nalu_data = stream_ + annexb_start_code_size;
  off_t max_nalu_data_size = bytes_left_ - annexb_start_code_size;
  if (max_nalu_data_size <= 0) {
    DVLOG(3) << "End of stream";
    return false;
  }
  // Find the start code of next NALU;
  // if successful, |nalu_size_without_start_code| is the number of bytes from
  // after previous start code to before this one;
  // if next start code is not found, it is still a valid NALU since there
  // are some bytes left after the first start code: all the remaining bytes
  // belong to the current NALU.
  off_t next_start_code_size = 0;
  off_t nalu_size_without_start_code = 0;
  if (!H264Parser::FindStartCodeInClearRanges(
          nalu_data, max_nalu_data_size, encrypted_ranges_,
          &nalu_size_without_start_code, &next_start_code_size)) {
    nalu_size_without_start_code = max_nalu_data_size;
  }
  *nalu_size = nalu_size_without_start_code + annexb_start_code_size;
  *start_code_size = annexb_start_code_size;
  return true;
}

H266NaluParser::Result H266NaluParser::AdvanceToNextNALU(H266NALU* nalu) {
  off_t start_code_size;
  off_t nalu_size_with_start_code;
  if (!LocateNALU(&nalu_size_with_start_code, &start_code_size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << bytes_left_;
    return kEndOfStream;
  }
  DCHECK(nalu);
  nalu->data = stream_ + start_code_size;
  nalu->size = nalu_size_with_start_code - start_code_size;
  DVLOG(4) << "NALU found: size=" << nalu_size_with_start_code;
  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data, nalu->size)) {
    return kEndOfStream;
  }
  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ += nalu_size_with_start_code;
  bytes_left_ -= nalu_size_with_start_code;
  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  int data;
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);
  // nuh_reserved_zero_bit
  READ_BITS_OR_RETURN(1, &data);
  if (data == 1) {
    // Current spec requires ignoring NALU with nuh_reserved_zero_bit == 1.
    return kIgnored;
  }
  READ_BITS_OR_RETURN(6, &nalu->nuh_layer_id);
  if (nalu->nuh_layer_id < 0 || nalu->nuh_layer_id > 55) {
    return kIgnored;
  }
  READ_BITS_OR_RETURN(5, &nalu->nal_unit_type);
  READ_BITS_OR_RETURN(3, &nalu->nuh_temporal_id_plus1);
  TRUE_OR_RETURN(nalu->nuh_temporal_id_plus1 != 0);
  if (nalu->nal_unit_type >= H266NALU::kIDRWithRADL &&
      nalu->nal_unit_type <= H266NALU::kReservedIRAP11) {
    TRUE_OR_RETURN(nalu->nuh_temporal_id_plus1 == 1);
  }
  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data)
           << " size: " << nalu->size;
  previous_nalu_range_.clear();
  previous_nalu_range_.Add(nalu->data, nalu->data + nalu->size);
  return kOk;
}

std::vector<SubsampleEntry> H266NaluParser::GetCurrentSubsamples() {
  DCHECK_EQ(previous_nalu_range_.size(), 1u)
      << "This should only be called after a "
         "successful call to AdvanceToNextNalu()";
  auto intersection = encrypted_ranges_.IntersectionWith(previous_nalu_range_);
  return EncryptedRangesToSubsampleEntry(
      previous_nalu_range_.start(0), previous_nalu_range_.end(0), intersection);
}

}  // namespace media
