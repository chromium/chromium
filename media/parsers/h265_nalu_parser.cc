// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/parsers/h265_nalu_parser.h"

#include <stddef.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/types/to_address.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_switches.h"
#include "media/parsers/bit_reader_macros.h"

namespace media {

H265NALU::H265NALU() = default;

H265NaluParser::H265NaluParser()
    : validate_extended_bitstream_(
          base::FeatureList::IsEnabled(kExtendedVideoBitstreamValidation)) {
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
  base::span<const uint8_t> remaining = stream;
  for (size_t i = 0; i < subsamples.size() && !remaining.empty(); ++i) {
    // Clamp to the stream since subsample sizes may be malformed.
    remaining = remaining.subspan(
        std::min<size_t>(subsamples[i].clear_bytes, remaining.size()));
    base::span<const uint8_t> encrypted = remaining.take_first(
        std::min<size_t>(subsamples[i].cypher_bytes, remaining.size()));
    encrypted_ranges_.Add(encrypted.data(), base::to_address(encrypted.end()));
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
  // The two-byte NAL unit header may fall inside an encrypted subsample when a
  // CENC packager leaves only a minimal clear leader (as little as one byte).
  // In that case the header bits read above are ciphertext, so validating them
  // by value would spuriously reject a valid stream. Only validate when the
  // header is known to be in the clear; otherwise reset the fields that live in
  // the encrypted second header byte to safe defaults so bogus values are not
  // propagated downstream (they are re-parsed after decryption).
  if (!IsNaluHeaderEncrypted(nalu->data)) {
    IN_RANGE_IF_OR_RETURN(nalu->nuh_layer_id, 0, 62,
                          validate_extended_bitstream_);
    TRUE_OR_RETURN(nalu->nuh_temporal_id_plus1 != 0);
  } else {
    nalu->nuh_layer_id = 0;
    nalu->nuh_temporal_id_plus1 = 1;
  }

  DVLOG(4) << "NALU type: " << static_cast<int>(nalu->nal_unit_type)
           << " at: " << reinterpret_cast<const void*>(nalu->data.data())
           << " size: " << nalu->data.size();

  previous_nalu_range_.clear();
  previous_nalu_range_.Add(nalu->data.data(),
                           base::to_address(nalu->data.end()));
  return kOk;
}

bool H265NaluParser::IsNaluHeaderEncrypted(
    base::span<const uint8_t> nalu_data) const {
  // The NAL unit header is the first two bytes of the NALU; nuh_layer_id's low
  // bits and nuh_temporal_id_plus1 live in the second byte. Some CENC packagers
  // leave only a minimal clear leader, placing that second byte inside an
  // encrypted subsample. Return true when the header overlaps an encrypted
  // range so the caller can skip value-based validation of ciphertext.
  if (encrypted_ranges_.empty() || nalu_data.size() < 2u) {
    return false;
  }
  auto header = nalu_data.first(2u);
  Ranges<const uint8_t*> header_range;
  header_range.Add(header.data(), base::to_address(header.end()));
  return encrypted_ranges_.IntersectionWith(header_range).size() > 0u;
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
