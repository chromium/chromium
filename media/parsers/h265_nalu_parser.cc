// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/h265_nalu_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/logging.h"
#include "media/base/decrypt_config.h"

namespace media {

#define READ_BITS_OR_RETURN(num_bits, out)                                 \
  do {                                                                     \
    int _out;                                                              \
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
  stream_ = nullptr;
  bytes_left_ = 0;
  encrypted_ranges_.clear();
  previous_nalu_range_.clear();
}

void H265NaluParser::SetStream(const uint8_t* stream, off_t stream_size) {
  std::vector<SubsampleEntry> subsamples;
  SetEncryptedStream(stream, stream_size, subsamples);
}

void H265NaluParser::SetEncryptedStream(
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
  const uint8_t* stream_end = stream_ + base::checked_cast<size_t>(bytes_left_);
  for (size_t i = 0; i < subsamples.size() && start < stream_end; ++i) {
    start += subsamples[i].clear_bytes;

    const uint8_t* end =
        std::min(start + subsamples[i].cypher_bytes, stream_end);
    encrypted_ranges_.Add(start, end);
    start = end;
  }
}

bool H265NaluParser::LocateNALU(off_t* nalu_size, off_t* start_code_size) {
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
  stream_ += base::checked_cast<size_t>(nalu_start_off);
  bytes_left_ -= nalu_start_off;

  const uint8_t* nalu_data =
      stream_ + base::checked_cast<size_t>(annexb_start_code_size);
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

H265NaluParser::Result H265NaluParser::AdvanceToNextNALU(H265NALU* nalu) {
  off_t start_code_size;
  off_t nalu_size_with_start_code;
  if (!LocateNALU(&nalu_size_with_start_code, &start_code_size)) {
    DVLOG(4) << "Could not find next NALU, bytes left in stream: "
             << bytes_left_;
    return kEOStream;
  }

  DCHECK(nalu);
  nalu->data = (stream_ + base::checked_cast<size_t>(start_code_size)).get();
  nalu->size = nalu_size_with_start_code - start_code_size;
  DVLOG(4) << "NALU found: size=" << nalu_size_with_start_code;

  // Initialize bit reader at the start of found NALU.
  if (!br_.Initialize(nalu->data, nalu->size))
    return kEOStream;

  // Move parser state to after this NALU, so next time AdvanceToNextNALU
  // is called, we will effectively be skipping it;
  // other parsing functions will use the position saved
  // in bit reader for parsing, so we don't have to remember it here.
  stream_ += base::checked_cast<size_t>(nalu_size_with_start_code);
  bytes_left_ -= nalu_size_with_start_code;

  // Read NALU header, skip the forbidden_zero_bit, but check for it.
  int data;
  READ_BITS_OR_RETURN(1, &data);
  TRUE_OR_RETURN(data == 0);

  READ_BITS_OR_RETURN(6, &nalu->nal_unit_type);
  READ_BITS_OR_RETURN(6, &nalu->nuh_layer_id);
  READ_BITS_OR_RETURN(3, &nalu->nuh_temporal_id_plus1);
  TRUE_OR_RETURN(nalu->nuh_temporal_id_plus1 != 0);

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data.get())
           << " size: " << nalu->size;

  previous_nalu_range_.clear();
  previous_nalu_range_.Add(
      nalu->data.get(),
      (nalu->data + base::checked_cast<size_t>(nalu->size)).get());
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
