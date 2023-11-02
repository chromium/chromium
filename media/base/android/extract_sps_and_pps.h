// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_EXTRACT_SPS_AND_PPS_H_
#define MEDIA_BASE_ANDROID_EXTRACT_SPS_AND_PPS_H_

#include <stdint.h>

#include <vector>

#include "media/base/media_export.h"

namespace media {

// Extracts the SPS and PPS lists from the given AVCC. Each SPS and
// PPS is prefixed with the Annex B framing bytes (0x0001). The out parameters
// are not modified on failure.
void MEDIA_EXPORT ExtractSpsAndPps(const std::vector<uint8_t>& avcc,
                                   std::vector<uint8_t>* sps_out,
                                   std::vector<uint8_t>* pps_out);

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_EXTRACT_SPS_AND_PPS_H_
