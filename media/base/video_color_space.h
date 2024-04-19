// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_VIDEO_COLOR_SPACE_H_
#define MEDIA_BASE_VIDEO_COLOR_SPACE_H_

#include "media/base/media_export.h"
#include "ui/gfx/color_space.h"

namespace media {

// Described in ISO 23001-8:2016
class MEDIA_EXPORT VideoColorSpace {
 public:
  // Table 2
  enum class PrimaryID : uint8_t {
    INVALID = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    BT470M = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    FILM = 8,
    BT2020 = 9,
    SMPTEST428_1 = 10,
    SMPTEST431_2 = 11,
    SMPTEST432_1 = 12,
    EBU_3213_E = 22,
    kMaxValue = EBU_3213_E,
  };

  // Table 3
  enum class TransferID : uint8_t {
    INVALID = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    GAMMA22 = 4,
    GAMMA28 = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    LINEAR = 8,
    LOG = 9,
    LOG_SQRT = 10,
    IEC61966_2_4 = 11,
    BT1361_ECG = 12,
    IEC61966_2_1 = 13,
    BT2020_10 = 14,
    BT2020_12 = 15,
    SMPTEST2084 = 16,
    SMPTEST428_1 = 17,

    // Not yet standardized
    ARIB_STD_B67 = 18,  // AKA hybrid-log gamma, HLG.

    kMaxValue = ARIB_STD_B67,
  };

  // Table 4
  enum class MatrixID : uint8_t {
    RGB = 0,
    BT709 = 1,
    UNSPECIFIED = 2,
    FCC = 4,
    BT470BG = 5,
    SMPTE170M = 6,
    SMPTE240M = 7,
    YCOCG = 8,
    BT2020_NCL = 9,
    // NOTE: BT2020_CL is no longer supported (b/333906350).
    BT2020_CL = 10,
    YDZDX = 11,
    INVALID = 255,
    kMaxValue = INVALID,
  };

  VideoColorSpace();
  VideoColorSpace(int primaries,
                  int transfer,
                  int matrix,
                  gfx::ColorSpace::RangeID range);
  VideoColorSpace(PrimaryID primaries,
                  TransferID transfer,
                  MatrixID matrix,
                  gfx::ColorSpace::RangeID range);

  bool operator==(const VideoColorSpace& other) const;
  bool operator!=(const VideoColorSpace& other) const;

  // Returns true if all of the fields have a value other
  // than INVALID or UNSPECIFIED.
  bool IsSpecified() const;

  // These will return INVALID if the number you give it
  // is not a valid enum value.
  static PrimaryID GetPrimaryID(int primary);
  static TransferID GetTransferID(int transfer);
  static MatrixID GetMatrixID(int matrix);

  static VideoColorSpace REC709();
  static VideoColorSpace REC601();
  static VideoColorSpace JPEG();

  gfx::ColorSpace ToGfxColorSpace() const;

  // Similar to ToGfxColorSpace(), but attempts to guess a gfx::ColorSpace from
  // a fully or partially specified VideoColorSpace. E.g., a completely invalid
  // VideoColorSpace will return a BT.709 gfx::ColorSpace.
  gfx::ColorSpace GuessGfxColorSpace() const;

  std::string ToString() const;

  static VideoColorSpace FromGfxColorSpace(const gfx::ColorSpace& color_space);

  // Note, these are public variables.
  PrimaryID primaries = PrimaryID::INVALID;
  TransferID transfer = TransferID::INVALID;
  MatrixID matrix = MatrixID::INVALID;
  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;

 private:
  gfx::ColorSpace ToGfxColorSpaceInternal(bool allow_guessing) const;
};

}  // namespace media

#endif  // MEDIA_BASE_VIDEO_COLOR_SPACE_H_
