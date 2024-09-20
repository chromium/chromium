// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/nalu_test_helper.h"

#include "base/check_op.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "media/parsers/h264_parser.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/parsers/h265_nalu_parser.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

namespace media {
namespace mp4 {
namespace {

template <typename T>
void WriteNALUType(std::vector<uint8_t>* buffer,
                   const std::string& nal_unit_type);

H264NALU::Type H264StringToNALUType(const std::string& name) {
  if (name == "P")
    return H264NALU::kNonIDRSlice;

  if (name == "I")
    return H264NALU::kIDRSlice;

  if (name == "SDA")
    return H264NALU::kSliceDataA;

  if (name == "SDB")
    return H264NALU::kSliceDataB;

  if (name == "SDC")
    return H264NALU::kSliceDataC;

  if (name == "SEI")
    return H264NALU::kSEIMessage;

  if (name == "SPS")
    return H264NALU::kSPS;

  if (name == "SPSExt")
    return H264NALU::kSPSExt;

  if (name == "PPS")
    return H264NALU::kPPS;

  if (name == "AUD")
    return H264NALU::kAUD;

  if (name == "EOSeq")
    return H264NALU::kEOSeq;

  if (name == "EOStr")
    return H264NALU::kEOStream;

  if (name == "FILL")
    return H264NALU::kFiller;

  if (name == "Prefix")
    return H264NALU::kPrefix;

  if (name == "SubsetSPS")
    return H264NALU::kSubsetSPS;

  if (name == "DPS")
    return H264NALU::kDPS;

  CHECK(false) << "Unexpected name: " << name;
  return H264NALU::kUnspecified;
}

template <>
void WriteNALUType<H264NALU>(std::vector<uint8_t>* buffer,
                             const std::string& nal_unit_type) {
  buffer->push_back(H264StringToNALUType(nal_unit_type));
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
// Convert NALU type string to NALU type. It only supports a subset of all the
// NALU types for testing purpose.
H265NALU::Type H265StringToNALUType(const std::string& name) {
  if (name == "AUD")
    return H265NALU::AUD_NUT;

  if (name == "SPS")
    return H265NALU::SPS_NUT;

  if (name == "FD")
    return H265NALU::FD_NUT;

  if (name == "EOS")
    return H265NALU::EOS_NUT;

  if (name == "EOB")
    return H265NALU::EOB_NUT;

  // There're lots of H265 NALU I/P frames, return one from all possible types
  // for testing purpose, since we only care about the order of I/P frames and
  // other non VCL NALU.
  if (name == "P")
    return H265NALU::TRAIL_N;

  if (name == "I")
    return H265NALU::IDR_W_RADL;

  CHECK(false) << "Unexpected name: " << name;
  return H265NALU::EOB_NUT;
}

template <>
void WriteNALUType<H265NALU>(std::vector<uint8_t>* buffer,
                             const std::string& nal_unit_type) {
  uint8_t header1 = 0;
  uint8_t header2 = 1;  // nuh_temporal_id_plus1 = 1

  uint8_t type = static_cast<uint8_t>(H265StringToNALUType(nal_unit_type));
  DCHECK_LT(type, 64);

  header1 |= (type << 1);

  buffer->push_back(header1);
  buffer->push_back(header2);
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

template <typename T>
void WriteStartCodeAndNALUType(std::vector<uint8_t>* buffer,
                               const std::string& nal_unit_type) {
  buffer->push_back(0x00);
  buffer->push_back(0x00);
  buffer->push_back(0x00);
  buffer->push_back(0x01);
  WriteNALUType<T>(buffer, nal_unit_type);
}

template <typename T>
void StringToAnnexB(const std::string& str,
                    std::vector<uint8_t>* buffer,
                    std::vector<SubsampleEntry>* subsamples) {
  DCHECK(!str.empty());

  std::vector<std::string> subsample_specs = base::SplitString(
      str, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  DCHECK_GT(subsample_specs.size(), 0u);

  buffer->clear();
  for (size_t i = 0; i < subsample_specs.size(); ++i) {
    SubsampleEntry entry;
    size_t start = buffer->size();

    std::vector<std::string> subsample_nalus =
        base::SplitString(subsample_specs[i], ",", base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    DCHECK_GT(subsample_nalus.size(), 0u);
    for (size_t j = 0; j < subsample_nalus.size(); ++j) {
      WriteStartCodeAndNALUType<T>(buffer, subsample_nalus[j]);

      // Write junk for the payload since the current code doesn't
      // actually look at it.
      buffer->push_back(0x32);
      buffer->push_back(0x12);
      buffer->push_back(0x67);
    }

    entry.clear_bytes = buffer->size() - start;

    if (subsamples) {
      // Simulate the encrypted bits containing something that looks
      // like a SPS NALU.
      WriteStartCodeAndNALUType<T>(buffer, "SPS");
    }

    entry.cypher_bytes = buffer->size() - start - entry.clear_bytes;

    if (subsamples) {
      subsamples->push_back(entry);
    }
  }
}
}  // namespace

void AvcStringToAnnexB(const std::string& str,
                       std::vector<uint8_t>* buffer,
                       std::vector<SubsampleEntry>* subsamples) {
  StringToAnnexB<H264NALU>(str, buffer, subsamples);
}

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
void HevcStringToAnnexB(const std::string& str,
                        std::vector<uint8_t>* buffer,
                        std::vector<SubsampleEntry>* subsamples) {
  StringToAnnexB<H265NALU>(str, buffer, subsamples);
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

bool AnalysesMatch(const BitstreamConverter::AnalysisResult& r1,
                   const BitstreamConverter::AnalysisResult& r2) {
  return r1.is_conformant == r2.is_conformant &&
         r1.is_keyframe == r2.is_keyframe;
}

}  // namespace mp4
}  // namespace media
