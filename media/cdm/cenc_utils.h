// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_CENC_UTILS_H_
#define MEDIA_CDM_CENC_UTILS_H_

#include <stdint.h>

#include <vector>

#include "media/base/media_export.h"
#include "media/cdm/json_web_key.h"

namespace media {

// Validate that |input| is a set of concatenated 'pssh' boxes and the sizes
// match. Returns true if |input| looks valid, false otherwise.
MEDIA_EXPORT bool ValidatePsshInput(const std::vector<uint8_t>& input);

// Gets the Key Ids from the first 'pssh' box for the Common System ID among one
// or more concatenated 'pssh' boxes. Returns true if a matching box is found
// and it contains 1 or more key IDs. Returns false otherwise.
// Notes:
// 1. If multiple PSSH boxes are found, the "KIDs" of the first matching 'pssh'
//    box will be set in |key_ids|.
// 2. Only PSSH boxes are allowed in |input|. Any other boxes in |pssh_boxes|
//    will result in false being returned.
MEDIA_EXPORT bool GetKeyIdsForCommonSystemId(
    const std::vector<uint8_t>& pssh_boxes,
    KeyIdList* key_ids);

// Gets the data field from the first 'pssh' box containing |system_id|.
// Returns true if such a box is found and successfully parsed. Returns false
// otherwise.
// Notes:
// 1. If multiple PSSH boxes are found, the "Data" of the first matching 'pssh'
//    box will be set in |pssh_data|.
// 2. Only PSSH boxes are allowed in |input|. Any other boxes in |pssh_boxes|
//    will result in false being returned.
MEDIA_EXPORT bool GetPsshData(const std::vector<uint8_t>& input,
                              const std::vector<uint8_t>& system_id,
                              std::vector<uint8_t>* pssh_data);

}  // namespace media

#endif  // MEDIA_CDM_CENC_UTILS_H_
