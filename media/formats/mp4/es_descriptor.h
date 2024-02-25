// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_ES_DESCRIPTOR_H_
#define MEDIA_FORMATS_MP4_ES_DESCRIPTOR_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "media/base/media_export.h"

namespace media {

class BitReader;

namespace mp4 {

// The following values are extracted from ISO 14496 Part 1 Table 5 -
// objectTypeIndication Values. Only values currently in use are included.
enum ObjectType {
  kForbidden = 0,
  kISO_14496_3 = 0x40,         // MPEG4 AAC
  kISO_13818_7_AAC_LC = 0x67,  // MPEG2 AAC-LC
  kAC3 = 0xa5,                 // AC3
  kEAC3 = 0xa6,                // EAC3 / Dolby Digital Plus
  kDTS = 0xa9,                 // DTS
  kDTSE = 0xac,                // DTS Express/LBR
  kDTSX = 0xb2,                // DTS:X
  kAC4 = 0xae                  // AC4
};

enum Tag {
  kESDescrTag = 0x03,
  kDecoderConfigDescrTag = 0x04,
  kDecoderSpecificInfoTag = 0x05
};

// This class parse object type and decoder specific information from an
// elementary stream descriptor, which is usually contained in an esds box.
// Please refer to ISO 14496 Part 1 7.2.6.5 for more details.
class MEDIA_EXPORT ESDescriptor {
 public:
  // Utility function to check if the given object type is AAC.
  static bool IsAAC(uint8_t object_type);

  static std::vector<uint8_t> CreateEsds(
      const std::vector<uint8_t>& aac_extra_data);

  ESDescriptor();
  ~ESDescriptor();

  bool Parse(const std::vector<uint8_t>& data);

  uint8_t object_type() const;
  const std::vector<uint8_t>& decoder_specific_info() const;

 private:
  bool ParseDecoderConfigDescriptor(BitReader* reader);
  bool ParseDecoderSpecificInfo(BitReader* reader);

  uint8_t object_type_;
  std::vector<uint8_t> decoder_specific_info_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_ES_DESCRIPTOR_H_
