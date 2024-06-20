// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/numerics/safe_conversions.h"
#include "media/parsers/h266_parser.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size) {
    return 0;
  }

  media::H266Parser parser;
  parser.SetStream(data, base::checked_cast<off_t>(size));

  // Parse until the end of stream/unsupported stream/error in stream is
  // found.
  while (true) {
    media::H266NALU nalu;
    media::H266PictureHeader ph;
    media::H266Parser::Result res = parser.AdvanceToNextNALU(&nalu);
    if (res != media::H266Parser::kOk) {
      break;
    }

    switch (nalu.nal_unit_type) {
      case media::H266NALU::kVPS:
        int vps_id;
        res = parser.ParseVPS(&vps_id);
        break;
      case media::H266NALU::kSPS:
        int sps_id;
        res = parser.ParseSPS(nalu, &sps_id);
        break;
      case media::H266NALU::kPPS:
        int pps_id;
        res = parser.ParsePPS(nalu, &pps_id);
        break;
      case media::H266NALU::kPrefixAPS:
      case media::H266NALU::kSuffixAPS:
        media::H266APS::ParamType aps_type;
        int aps_id;
        res = parser.ParseAPS(nalu, &aps_id, &aps_type);
        break;
      case media::H266NALU::kPH:
        res = parser.ParsePHNut(nalu, &ph);
        break;
      // TODO(crbugs.com/1417910): Other NALU types will be checked.
      default:
        // Skip other NALU types
        break;
    }
    if (res != media::H266Parser::kOk) {
      break;
    }
  }

  return 0;
}
