// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_NALU_TEST_HELPER_H_
#define MEDIA_FORMATS_MP4_NALU_TEST_HELPER_H_

#include <string>
#include <vector>

#include "media/base/subsample_entry.h"
#include "media/formats/mp4/bitstream_converter.h"
#include "media/media_buildflags.h"

namespace media {
namespace mp4 {

// Input string should be one or more NALU types separated with spaces or
// commas. NALU grouped together and separated by commas are placed into the
// same subsample, NALU groups separated by spaces are placed into separate
// subsamples.
// For example: in AVC, input string "SPS PPS I" produces Annex B buffer
// containing SPS, PPS and I NALUs, each in a separate subsample. While input
// string "SPS,PPS I" produces Annex B buffer where the first subsample contains
// SPS and PPS NALUs and the second subsample contains the I-slice NALU.
// The output buffer will contain a valid-looking Annex B (it's valid-looking in
// the sense that it has start codes and correct NALU types, but the actual NALU
// payload is junk).
void AvcStringToAnnexB(const std::string& str,
                       std::vector<uint8_t>* buffer,
                       std::vector<SubsampleEntry>* subsamples);

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
void HevcStringToAnnexB(const std::string& str,
                        std::vector<uint8_t>* buffer,
                        std::vector<SubsampleEntry>* subsamples);
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)

// Helper to compare two results of AVC::Analyze().
bool AnalysesMatch(const BitstreamConverter::AnalysisResult& r1,
                   const BitstreamConverter::AnalysisResult& r2);

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_NALU_TEST_HELPER_H_
