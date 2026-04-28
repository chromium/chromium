// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"

#include "base/strings/stringprintf.h"

namespace media {

namespace {

VideoColorSpace::PrimaryID ToMediaPrimaryID(
    gfx::ColorSpace::PrimaryID primary_id) {
  switch (primary_id) {
    case gfx::ColorSpace::PrimaryID::BT709:
      return VideoColorSpace::PrimaryID::BT709;
    case gfx::ColorSpace::PrimaryID::BT470M:
      return VideoColorSpace::PrimaryID::BT470M;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      return VideoColorSpace::PrimaryID::BT470BG;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      return VideoColorSpace::PrimaryID::SMPTE170M;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      return VideoColorSpace::PrimaryID::SMPTE240M;
    case gfx::ColorSpace::PrimaryID::FILM:
      return VideoColorSpace::PrimaryID::FILM;
    case gfx::ColorSpace::PrimaryID::BT2020:
      return VideoColorSpace::PrimaryID::BT2020;
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
      return VideoColorSpace::PrimaryID::SMPTEST428_1;
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
      return VideoColorSpace::PrimaryID::SMPTEST431_2;
    case gfx::ColorSpace::PrimaryID::P3:
      return VideoColorSpace::PrimaryID::SMPTEST432_1;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      return VideoColorSpace::PrimaryID::EBU_3213_E;
    default:
      return VideoColorSpace::PrimaryID::INVALID;
  }
}

gfx::ColorSpace::PrimaryID ToGfxPrimaryID(
    VideoColorSpace::PrimaryID primary_id) {
  switch (primary_id) {
    case VideoColorSpace::PrimaryID::BT709:
      return gfx::ColorSpace::PrimaryID::BT709;
    case VideoColorSpace::PrimaryID::BT470M:
      return gfx::ColorSpace::PrimaryID::BT470M;
    case VideoColorSpace::PrimaryID::BT470BG:
      return gfx::ColorSpace::PrimaryID::BT470BG;
    case VideoColorSpace::PrimaryID::SMPTE170M:
      return gfx::ColorSpace::PrimaryID::SMPTE170M;
    case VideoColorSpace::PrimaryID::SMPTE240M:
      return gfx::ColorSpace::PrimaryID::SMPTE240M;
    case VideoColorSpace::PrimaryID::FILM:
      return gfx::ColorSpace::PrimaryID::FILM;
    case VideoColorSpace::PrimaryID::BT2020:
      return gfx::ColorSpace::PrimaryID::BT2020;
    case VideoColorSpace::PrimaryID::SMPTEST428_1:
      return gfx::ColorSpace::PrimaryID::SMPTEST428_1;
    case VideoColorSpace::PrimaryID::SMPTEST431_2:
      return gfx::ColorSpace::PrimaryID::SMPTEST431_2;
    case VideoColorSpace::PrimaryID::SMPTEST432_1:
      return gfx::ColorSpace::PrimaryID::P3;
    case VideoColorSpace::PrimaryID::EBU_3213_E:
      return gfx::ColorSpace::PrimaryID::EBU_3213_E;
    default:
      return gfx::ColorSpace::PrimaryID::INVALID;
  }
}

VideoColorSpace::TransferID ToMediaTransferID(
    gfx::ColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::BT709_APPLE:
      return VideoColorSpace::TransferID::BT709;
    case gfx::ColorSpace::TransferID::GAMMA22:
      return VideoColorSpace::TransferID::GAMMA22;
    case gfx::ColorSpace::TransferID::GAMMA28:
      return VideoColorSpace::TransferID::GAMMA28;
    case gfx::ColorSpace::TransferID::SMPTE170M:
      return VideoColorSpace::TransferID::SMPTE170M;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      return VideoColorSpace::TransferID::SMPTE240M;
    case gfx::ColorSpace::TransferID::LINEAR:
      return VideoColorSpace::TransferID::LINEAR;
    case gfx::ColorSpace::TransferID::LOG:
      return VideoColorSpace::TransferID::LOG;
    case gfx::ColorSpace::TransferID::LOG_SQRT:
      return VideoColorSpace::TransferID::LOG_SQRT;
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
      return VideoColorSpace::TransferID::IEC61966_2_4;
    case gfx::ColorSpace::TransferID::BT1361_ECG:
      return VideoColorSpace::TransferID::BT1361_ECG;
    case gfx::ColorSpace::TransferID::SRGB:
      return VideoColorSpace::TransferID::IEC61966_2_1;
    case gfx::ColorSpace::TransferID::BT2020_10:
      return VideoColorSpace::TransferID::BT2020_10;
    case gfx::ColorSpace::TransferID::BT2020_12:
      return VideoColorSpace::TransferID::BT2020_12;
    case gfx::ColorSpace::TransferID::PQ:
      return VideoColorSpace::TransferID::SMPTEST2084;
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
      return VideoColorSpace::TransferID::SMPTEST428_1;
    case gfx::ColorSpace::TransferID::HLG:
      return VideoColorSpace::TransferID::ARIB_STD_B67;
    default:
      return VideoColorSpace::TransferID::INVALID;
  }
}

gfx::ColorSpace::TransferID ToGfxTransferID(
    VideoColorSpace::TransferID transfer_id) {
  switch (transfer_id) {
    case VideoColorSpace::TransferID::BT709:
      return gfx::ColorSpace::TransferID::BT709;
    case VideoColorSpace::TransferID::GAMMA22:
      return gfx::ColorSpace::TransferID::GAMMA22;
    case VideoColorSpace::TransferID::GAMMA28:
      return gfx::ColorSpace::TransferID::GAMMA28;
    case VideoColorSpace::TransferID::SMPTE170M:
      return gfx::ColorSpace::TransferID::SMPTE170M;
    case VideoColorSpace::TransferID::SMPTE240M:
      return gfx::ColorSpace::TransferID::SMPTE240M;
    case VideoColorSpace::TransferID::LINEAR:
      return gfx::ColorSpace::TransferID::LINEAR;
    case VideoColorSpace::TransferID::LOG:
      return gfx::ColorSpace::TransferID::LOG;
    case VideoColorSpace::TransferID::LOG_SQRT:
      return gfx::ColorSpace::TransferID::LOG_SQRT;
    case VideoColorSpace::TransferID::IEC61966_2_4:
      return gfx::ColorSpace::TransferID::IEC61966_2_4;
    case VideoColorSpace::TransferID::BT1361_ECG:
      return gfx::ColorSpace::TransferID::BT1361_ECG;
    case VideoColorSpace::TransferID::IEC61966_2_1:
      return gfx::ColorSpace::TransferID::SRGB;
    case VideoColorSpace::TransferID::BT2020_10:
      return gfx::ColorSpace::TransferID::BT2020_10;
    case VideoColorSpace::TransferID::BT2020_12:
      return gfx::ColorSpace::TransferID::BT2020_12;
    case VideoColorSpace::TransferID::SMPTEST2084:
      return gfx::ColorSpace::TransferID::PQ;
    case VideoColorSpace::TransferID::SMPTEST428_1:
      return gfx::ColorSpace::TransferID::SMPTEST428_1;
    case VideoColorSpace::TransferID::ARIB_STD_B67:
      return gfx::ColorSpace::TransferID::HLG;
    default:
      return gfx::ColorSpace::TransferID::INVALID;
  }
}

VideoColorSpace::MatrixID ToMediaMatrixID(gfx::ColorSpace::MatrixID matrix_id) {
  switch (matrix_id) {
    case gfx::ColorSpace::MatrixID::RGB:
      return VideoColorSpace::MatrixID::RGB;
    case gfx::ColorSpace::MatrixID::BT709:
      return VideoColorSpace::MatrixID::BT709;
    case gfx::ColorSpace::MatrixID::FCC:
      return VideoColorSpace::MatrixID::FCC;
    case gfx::ColorSpace::MatrixID::BT470BG:
      return VideoColorSpace::MatrixID::BT470BG;
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      return VideoColorSpace::MatrixID::SMPTE170M;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      return VideoColorSpace::MatrixID::SMPTE240M;
    case gfx::ColorSpace::MatrixID::YCOCG:
      return VideoColorSpace::MatrixID::YCOCG;
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
      return VideoColorSpace::MatrixID::BT2020_NCL;
    case gfx::ColorSpace::MatrixID::YDZDX:
      return VideoColorSpace::MatrixID::YDZDX;
    case gfx::ColorSpace::MatrixID::GBR:
      // gfx::ColorSpace uses GBR for the identity matrix (no color transform).
      // VideoColorSpace calls this RGB. They represent the same thing.
      return VideoColorSpace::MatrixID::RGB;
    default:
      return VideoColorSpace::MatrixID::INVALID;
  }
}

gfx::ColorSpace::MatrixID ToGfxMatrixID(VideoColorSpace::MatrixID matrix_id) {
  switch (matrix_id) {
    case VideoColorSpace::MatrixID::RGB:
      return gfx::ColorSpace::MatrixID::GBR;
    case VideoColorSpace::MatrixID::BT709:
      return gfx::ColorSpace::MatrixID::BT709;
    case VideoColorSpace::MatrixID::FCC:
      return gfx::ColorSpace::MatrixID::FCC;
    case VideoColorSpace::MatrixID::BT470BG:
      return gfx::ColorSpace::MatrixID::BT470BG;
    case VideoColorSpace::MatrixID::SMPTE170M:
      return gfx::ColorSpace::MatrixID::SMPTE170M;
    case VideoColorSpace::MatrixID::SMPTE240M:
      return gfx::ColorSpace::MatrixID::SMPTE240M;
    case VideoColorSpace::MatrixID::YCOCG:
      return gfx::ColorSpace::MatrixID::YCOCG;
    case VideoColorSpace::MatrixID::BT2020_NCL:
      return gfx::ColorSpace::MatrixID::BT2020_NCL;
    case VideoColorSpace::MatrixID::YDZDX:
      return gfx::ColorSpace::MatrixID::YDZDX;
    default:
      return gfx::ColorSpace::MatrixID::INVALID;
  }
}

}  // namespace

VideoColorSpace::PrimaryID VideoColorSpace::GetPrimaryID(int primary) {
  if (primary < 1 || primary > 22 || primary == 3) {
    return PrimaryID::INVALID;
  }
  if (primary > 12 && primary < 22) {
    return PrimaryID::INVALID;
  }
  return static_cast<PrimaryID>(primary);
}

VideoColorSpace::TransferID VideoColorSpace::GetTransferID(int transfer) {
  if (transfer < 1 || transfer > 18 || transfer == 3) {
    return TransferID::INVALID;
  }
  return static_cast<TransferID>(transfer);
}

VideoColorSpace::MatrixID VideoColorSpace::GetMatrixID(int matrix) {
  if (matrix < 0 || matrix > 11 || matrix == 3 || matrix == 10) {
    // TODO(crbug.com/333906350): BT2020_CL (10) is no longer supported.
    return MatrixID::INVALID;
  }
  return static_cast<MatrixID>(matrix);
}

VideoColorSpace::VideoColorSpace() = default;

VideoColorSpace::VideoColorSpace(gfx::ColorSpace color_space,
                                 bool primaries_unspecified,
                                 bool transfer_unspecified,
                                 bool matrix_unspecified)
    : color_space_(color_space),
      primaries_unspecified_(primaries_unspecified),
      transfer_unspecified_(transfer_unspecified),
      matrix_unspecified_(matrix_unspecified) {}

VideoColorSpace::VideoColorSpace(PrimaryID primaries,
                                 TransferID transfer,
                                 MatrixID matrix,
                                 gfx::ColorSpace::RangeID range)
    : VideoColorSpace(gfx::ColorSpace(ToGfxPrimaryID(primaries),
                                      ToGfxTransferID(transfer),
                                      ToGfxMatrixID(matrix),
                                      range),
                      primaries == PrimaryID::UNSPECIFIED,
                      transfer == TransferID::UNSPECIFIED,
                      matrix == MatrixID::UNSPECIFIED) {}

VideoColorSpace::VideoColorSpace(int primaries,
                                 int transfer,
                                 int matrix,
                                 gfx::ColorSpace::RangeID range)
    : VideoColorSpace(GetPrimaryID(primaries),
                      GetTransferID(transfer),
                      GetMatrixID(matrix),
                      range) {}

bool VideoColorSpace::operator==(const VideoColorSpace& other) const {
  return color_space_ == other.color_space_ &&
         primaries_unspecified_ == other.primaries_unspecified_ &&
         transfer_unspecified_ == other.transfer_unspecified_ &&
         matrix_unspecified_ == other.matrix_unspecified_;
}

bool VideoColorSpace::operator!=(const VideoColorSpace& other) const {
  return !(*this == other);
}

bool VideoColorSpace::IsSpecified() const {
  return !primaries_unspecified_ && !transfer_unspecified_ &&
         !matrix_unspecified_ && color_space_.IsValid();
}

VideoColorSpace::PrimaryID VideoColorSpace::primaries() const {
  if (primaries_unspecified_) {
    return PrimaryID::UNSPECIFIED;
  }
  return ToMediaPrimaryID(color_space_.GetPrimaryID());
}

VideoColorSpace::TransferID VideoColorSpace::transfer() const {
  if (transfer_unspecified_) {
    return TransferID::UNSPECIFIED;
  }
  return ToMediaTransferID(color_space_.GetTransferID());
}

VideoColorSpace::MatrixID VideoColorSpace::matrix() const {
  if (matrix_unspecified_) {
    return MatrixID::UNSPECIFIED;
  }
  return ToMediaMatrixID(color_space_.GetMatrixID());
}

bool VideoColorSpace::IsHDR() const {
  if (transfer_unspecified_) {
    return false;
  }
  return color_space_.IsHDR();
}

gfx::ColorSpace VideoColorSpace::ToGfxColorSpace() const {
  return color_space_;
}

gfx::ColorSpace VideoColorSpace::GuessGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primary_id = ToGfxPrimaryID(primaries());
  gfx::ColorSpace::TransferID transfer_id = ToGfxTransferID(transfer());
  gfx::ColorSpace::MatrixID matrix_id = ToGfxMatrixID(matrix());
  gfx::ColorSpace::RangeID range_id = range();

  // Bitfield, note that guesses with higher values take precedence over
  // guesses with lower values.
  enum Guess {
    GUESS_BT709 = 1 << 5,
    GUESS_BT2020 = 1 << 4,
    GUESS_BT470M = 1 << 3,
    GUESS_BT470BG = 1 << 2,
    GUESS_SMPTE170M = 1 << 1,
    GUESS_SMPTE240M = 1 << 0,
  };

  uint32_t guess = 0;

  switch (primaries()) {
    case PrimaryID::BT709:
      guess |= GUESS_BT709;
      break;
    case PrimaryID::BT470M:
      guess |= GUESS_BT470M;
      break;
    case PrimaryID::BT470BG:
      guess |= GUESS_BT470BG;
      break;
    case PrimaryID::SMPTE170M:
      guess |= GUESS_SMPTE170M;
      break;
    case PrimaryID::SMPTE240M:
      guess |= GUESS_SMPTE240M;
      break;
    case PrimaryID::BT2020:
      guess |= GUESS_BT2020;
      break;
    case PrimaryID::FILM:
    case PrimaryID::SMPTEST428_1:
    case PrimaryID::SMPTEST431_2:
    case PrimaryID::SMPTEST432_1:
    case PrimaryID::EBU_3213_E:
    case PrimaryID::INVALID:
    case PrimaryID::UNSPECIFIED:
      break;
  }

  switch (transfer()) {
    case TransferID::BT709:
      guess |= GUESS_BT709;
      break;
    case TransferID::SMPTE170M:
      guess |= GUESS_SMPTE170M;
      break;
    case TransferID::SMPTE240M:
      guess |= GUESS_SMPTE240M;
      break;
    case TransferID::BT2020_10:
    case TransferID::BT2020_12:
    case TransferID::SMPTEST2084:
    case TransferID::ARIB_STD_B67:
      guess |= GUESS_BT2020;
      break;
    case TransferID::GAMMA22:
    case TransferID::GAMMA28:
    case TransferID::LINEAR:
    case TransferID::LOG:
    case TransferID::LOG_SQRT:
    case TransferID::IEC61966_2_4:
    case TransferID::BT1361_ECG:
    case TransferID::IEC61966_2_1:
    case TransferID::SMPTEST428_1:
    case TransferID::INVALID:
    case TransferID::UNSPECIFIED:
      break;
  }

  switch (matrix()) {
    case MatrixID::BT709:
      guess |= GUESS_BT709;
      break;
    case MatrixID::BT470BG:
      guess |= GUESS_BT470BG;
      break;
    case MatrixID::SMPTE170M:
      guess |= GUESS_SMPTE170M;
      break;
    case MatrixID::SMPTE240M:
      guess |= GUESS_SMPTE240M;
      break;
    case MatrixID::BT2020_NCL:
      guess |= GUESS_BT2020;
      break;
    case MatrixID::RGB:
    case MatrixID::FCC:
    case MatrixID::YCOCG:
    case MatrixID::YDZDX:
    case MatrixID::BT2020_CL:
    case MatrixID::INVALID:
    case MatrixID::UNSPECIFIED:
      break;
  }

  // Isolates the highest precedence guess.
  while (guess & (guess - 1)) {
    guess &= guess - 1;
  }
  if (!guess)
    guess = GUESS_BT709;

  // If no fields were specified at all, default the range to LIMITED so the
  // final guess falls through to a plain BT.709 limited-range color space.
  if (color_space_ == gfx::ColorSpace()) {
    range_id = gfx::ColorSpace::RangeID::LIMITED;
  }

  if (primary_id == gfx::ColorSpace::PrimaryID::INVALID) {
    switch (guess) {
      case GUESS_BT709:
        primary_id = gfx::ColorSpace::PrimaryID::BT709;
        break;
      case GUESS_BT470M:
        primary_id = gfx::ColorSpace::PrimaryID::BT470M;
        break;
      case GUESS_BT470BG:
        primary_id = gfx::ColorSpace::PrimaryID::BT470BG;
        break;
      case GUESS_SMPTE170M:
        primary_id = gfx::ColorSpace::PrimaryID::SMPTE170M;
        break;
      case GUESS_SMPTE240M:
        primary_id = gfx::ColorSpace::PrimaryID::SMPTE240M;
        break;
      case GUESS_BT2020:
        primary_id = gfx::ColorSpace::PrimaryID::BT2020;
        break;
    }
  }

  if (transfer_id == gfx::ColorSpace::TransferID::INVALID) {
    switch (guess) {
      case GUESS_BT709:
        transfer_id = gfx::ColorSpace::TransferID::BT709;
        break;
      case GUESS_BT470M:
      case GUESS_BT470BG:
      case GUESS_SMPTE170M:
        transfer_id = gfx::ColorSpace::TransferID::SMPTE170M;
        break;
      case GUESS_SMPTE240M:
        transfer_id = gfx::ColorSpace::TransferID::SMPTE240M;
        break;
      case GUESS_BT2020:
        transfer_id = gfx::ColorSpace::TransferID::BT2020_10;
        break;
    }
  }

  if (matrix_id == gfx::ColorSpace::MatrixID::INVALID) {
    switch (guess) {
      case GUESS_BT709:
        matrix_id = gfx::ColorSpace::MatrixID::BT709;
        break;
      case GUESS_BT470M:
      case GUESS_BT470BG:
      case GUESS_SMPTE170M:
        matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
        break;
      case GUESS_SMPTE240M:
        matrix_id = gfx::ColorSpace::MatrixID::SMPTE240M;
        break;
      case GUESS_BT2020:
        matrix_id = gfx::ColorSpace::MatrixID::BT2020_NCL;
        break;
    }
  }

  if (range_id == gfx::ColorSpace::RangeID::INVALID) {
    bool guessed_bt709 = (primary_id == gfx::ColorSpace::PrimaryID::BT709 &&
                          transfer_id == gfx::ColorSpace::TransferID::BT709 &&
                          matrix_id == gfx::ColorSpace::MatrixID::BT709);
    range_id = guessed_bt709 ? gfx::ColorSpace::RangeID::LIMITED
                             : gfx::ColorSpace::RangeID::DERIVED;
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

std::string VideoColorSpace::ToString() const {
  return base::StringPrintf(
      "{primary=%d, transfer=%d, matrix=%d, range=%d}",
      static_cast<int>(primaries()), static_cast<int>(transfer()),
      static_cast<int>(matrix()), static_cast<int>(range()));
}

VideoColorSpace VideoColorSpace::REC709() {
  return VideoColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                         gfx::ColorSpace::RangeID::LIMITED);
}

VideoColorSpace VideoColorSpace::REC601() {
  return VideoColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                         MatrixID::SMPTE170M,
                         gfx::ColorSpace::RangeID::LIMITED);
}

VideoColorSpace VideoColorSpace::JPEG() {
  // TODO(ccameron): Determine which primaries and transfer function were
  // intended here.
  return VideoColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1,
                         MatrixID::SMPTE170M, gfx::ColorSpace::RangeID::FULL);
}

// static
VideoColorSpace VideoColorSpace::FromGfxColorSpace(
    const gfx::ColorSpace& color_space) {
  return VideoColorSpace(color_space, /*primaries_unspecified=*/false,
                         /*transfer_unspecified=*/false,
                         /*matrix_unspecified=*/false);
}

}  // namespace media
