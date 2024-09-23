// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cdm/cenc_utils.h"

#include <memory>

#include "media/base/media_util.h"
#include "media/formats/mp4/box_definitions.h"
#include "media/formats/mp4/box_reader.h"

namespace media {

// The initialization data for encrypted media files using the ISO Common
// Encryption ('cenc') protection scheme may contain one or more protection
// system specific header ('pssh') boxes.
// ref: https://w3c.github.io/encrypted-media/cenc-format.html

// CENC SystemID for the Common System.
// https://w3c.github.io/encrypted-media/cenc-format.html#common-system
const uint8_t kCencCommonSystemId[] = {0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2,
                                       0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e,
                                       0x52, 0xe2, 0xfb, 0x4b};

// Returns true if |input| contains only 1 or more valid 'pssh' boxes, false
// otherwise. |pssh_boxes| is updated as the set of parsed 'pssh' boxes.
// Note: All boxes in |input| must be 'pssh' boxes. However, if they can't be
//       properly parsed (e.g. unsupported version), then they will be skipped.
static bool ReadAllPsshBoxes(
    const std::vector<uint8_t>& input,
    std::vector<mp4::FullProtectionSystemSpecificHeader>* pssh_boxes) {
  DCHECK(!input.empty());

  // TODO(wolenetz): Questionable MediaLog usage, http://crbug.com/712310
  NullMediaLog media_log;

  // Verify that |input| contains only 'pssh' boxes.
  // ReadAllChildrenAndCheckFourCC() is templated, so it checks that each
  // box in |input| matches the box type of the parameter (in this case
  // mp4::ProtectionSystemSpecificHeader is a 'pssh' box).
  // mp4::ProtectionSystemSpecificHeader doesn't validate the 'pssh' contents,
  // so this simply verifies that |input| only contains 'pssh' boxes and
  // nothing else.
  std::unique_ptr<mp4::BoxReader> input_reader(
      mp4::BoxReader::ReadConcatentatedBoxes(input.data(), input.size(),
                                             &media_log));
  std::vector<mp4::ProtectionSystemSpecificHeader> raw_pssh_boxes;
  if (!input_reader->ReadAllChildrenAndCheckFourCC(&raw_pssh_boxes))
    return false;

  // Now that we have |input| parsed into |raw_pssh_boxes|, reparse each one
  // into a mp4::FullProtectionSystemSpecificHeader, which extracts all the
  // relevant fields from the box. Since there may be unparsable 'pssh' boxes
  // (due to unsupported version, for example), this is done one by one,
  // ignoring any boxes that can't be parsed.
  for (const auto& raw_pssh_box : raw_pssh_boxes) {
    std::unique_ptr<mp4::BoxReader> raw_pssh_reader(
        mp4::BoxReader::ReadConcatentatedBoxes(raw_pssh_box.raw_box.data(),
                                               raw_pssh_box.raw_box.size(),
                                               &media_log));
    // ReadAllChildren() appends any successfully parsed box onto it's
    // parameter, so |pssh_boxes| will contain the collection of successfully
    // parsed 'pssh' boxes. If an error occurs, try the next box.
    if (!raw_pssh_reader->ReadAllChildrenAndCheckFourCC(pssh_boxes))
      continue;
  }

  // Must have successfully parsed at least one 'pssh' box.
  return pssh_boxes->size() > 0;
}

bool ValidatePsshInput(const std::vector<uint8_t>& input) {
  // No 'pssh' boxes is considered valid.
  if (input.empty())
    return true;

  std::vector<mp4::FullProtectionSystemSpecificHeader> children;
  return ReadAllPsshBoxes(input, &children);
}

bool GetKeyIdsForCommonSystemId(const std::vector<uint8_t>& pssh_boxes,
                                KeyIdList* key_ids) {
  // If there are no 'pssh' boxes then no key IDs found.
  if (pssh_boxes.empty())
    return false;

  std::vector<mp4::FullProtectionSystemSpecificHeader> children;
  if (!ReadAllPsshBoxes(pssh_boxes, &children))
    return false;

  // Check all children for an appropriate 'pssh' box, returning the
  // key IDs found.
  KeyIdList result;
  std::vector<uint8_t> common_system_id(
      kCencCommonSystemId,
      kCencCommonSystemId + std::size(kCencCommonSystemId));
  for (const auto& child : children) {
    if (child.system_id == common_system_id) {
      key_ids->assign(child.key_ids.begin(), child.key_ids.end());
      return key_ids->size() > 0;
    }
  }

  // No matching 'pssh' box found.
  return false;
}

bool GetPsshData(const std::vector<uint8_t>& input,
                 const std::vector<uint8_t>& system_id,
                 std::vector<uint8_t>* pssh_data) {
  if (input.empty())
    return false;

  std::vector<mp4::FullProtectionSystemSpecificHeader> children;
  if (!ReadAllPsshBoxes(input, &children))
    return false;

  // Check all children for an appropriate 'pssh' box, returning |data| from
  // the first one found.
  for (const auto& child : children) {
    if (child.system_id == system_id) {
      pssh_data->assign(child.data.begin(), child.data.end());
      return true;
    }
  }

  // No matching 'pssh' box found.
  return false;
}

}  // namespace media
