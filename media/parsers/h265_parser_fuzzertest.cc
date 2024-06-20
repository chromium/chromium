// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/parsers/h265_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  media::H265Parser parser;
  parser.SetStream(data, base::checked_cast<off_t>(size));

  // Parse until the end of stream/unsupported stream/error in stream is
  // found.
  media::H265SliceHeader shdr;
  media::H265SliceHeader prior_shdr;
  while (true) {
    media::H265NALU nalu;
    media::H265SEI sei;
    media::H265Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != media::H265Parser::kOk)
      break;

    switch (nalu.nal_unit_type) {
      case media::H265NALU::VPS_NUT:
        int vps_id;
        res = parser.ParseVPS(&vps_id);
        break;
      case media::H265NALU::SPS_NUT:
        int sps_id;
        res = parser.ParseSPS(&sps_id);
        break;
      case media::H265NALU::PPS_NUT:
        int pps_id;
        res = parser.ParsePPS(nalu, &pps_id);
        break;
      case media::H265NALU::PREFIX_SEI_NUT:
        res = parser.ParseSEI(&sei);
        break;
      case media::H265NALU::TRAIL_N:
      case media::H265NALU::TRAIL_R:
      case media::H265NALU::TSA_N:
      case media::H265NALU::TSA_R:
      case media::H265NALU::STSA_N:
      case media::H265NALU::STSA_R:
      case media::H265NALU::RADL_N:
      case media::H265NALU::RADL_R:
      case media::H265NALU::RASL_N:
      case media::H265NALU::RASL_R:
      case media::H265NALU::BLA_W_LP:
      case media::H265NALU::BLA_W_RADL:
      case media::H265NALU::BLA_N_LP:
      case media::H265NALU::IDR_W_RADL:
      case media::H265NALU::IDR_N_LP:
      case media::H265NALU::CRA_NUT:  // fallthrough
        res = parser.ParseSliceHeader(nalu, &shdr, &prior_shdr);
        prior_shdr = shdr;
        break;
      default:
        // Skip any other NALU.
        break;
    }
    if (res != media::H265Parser::kOk)
      break;
  }

  return 0;
}
