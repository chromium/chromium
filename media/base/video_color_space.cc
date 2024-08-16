// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_color_space.h"

#include "base/strings/stringprintf.h"

namespace media {

VideoColorSpace::PrimaryID VideoColorSpace::GetPrimaryID(int primary) {
  if (primary < 1 || primary > 22 || primary == 3)
    return PrimaryID::INVALID;
  if (primary > 12 && primary < 22)
    return PrimaryID::INVALID;
  return static_cast<PrimaryID>(primary);
}

VideoColorSpace::TransferID VideoColorSpace::GetTransferID(int transfer) {
  if (transfer < 1 || transfer > 18 || transfer == 3)
    return TransferID::INVALID;
  return static_cast<TransferID>(transfer);
}

VideoColorSpace::MatrixID VideoColorSpace::GetMatrixID(int matrix) {
  if (matrix < 0 || matrix > 11 || matrix == 3)
    return MatrixID::INVALID;
  return static_cast<MatrixID>(matrix);
}

VideoColorSpace::VideoColorSpace() = default;

VideoColorSpace::VideoColorSpace(PrimaryID primaries,
                                 TransferID transfer,
                                 MatrixID matrix,
                                 gfx::ColorSpace::RangeID range)
    : primaries(primaries), transfer(transfer), matrix(matrix), range(range) {}

VideoColorSpace::VideoColorSpace(int primaries,
                                 int transfer,
                                 int matrix,
                                 gfx::ColorSpace::RangeID range)
    : primaries(GetPrimaryID(primaries)),
      transfer(GetTransferID(transfer)),
      matrix(GetMatrixID(matrix)),
      range(range) {}

bool VideoColorSpace::operator==(const VideoColorSpace& other) const {
  return primaries == other.primaries && transfer == other.transfer &&
         matrix == other.matrix && range == other.range;
}

bool VideoColorSpace::operator!=(const VideoColorSpace& other) const {
  return primaries != other.primaries || transfer != other.transfer ||
         matrix != other.matrix || range != other.range;
}

bool VideoColorSpace::IsSpecified() const {
  return primaries != PrimaryID::INVALID &&
         primaries != PrimaryID::UNSPECIFIED &&
         transfer != TransferID::INVALID &&
         transfer != TransferID::UNSPECIFIED && matrix != MatrixID::INVALID &&
         matrix != MatrixID::UNSPECIFIED &&
         range != gfx::ColorSpace::RangeID::INVALID;
}

gfx::ColorSpace VideoColorSpace::ToGfxColorSpace() const {
  return ToGfxColorSpaceInternal(/*allow_guessing=*/false);
}

gfx::ColorSpace VideoColorSpace::GuessGfxColorSpace() const {
  return ToGfxColorSpaceInternal(/*allow_guessing=*/true);
}

std::string VideoColorSpace::ToString() const {
  return base::StringPrintf("{primary=%d, transfer=%d, matrix=%d, range=%d}",
                            static_cast<int>(primaries),
                            static_cast<int>(transfer),
                            static_cast<int>(matrix), static_cast<int>(range));
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
  VideoColorSpace::PrimaryID primaries = VideoColorSpace::PrimaryID::INVALID;
  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
      primaries = VideoColorSpace::PrimaryID::BT709;
      break;
    case gfx::ColorSpace::PrimaryID::BT470M:
      primaries = VideoColorSpace::PrimaryID::BT470M;
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      primaries = VideoColorSpace::PrimaryID::BT470BG;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      primaries = VideoColorSpace::PrimaryID::SMPTE170M;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      primaries = VideoColorSpace::PrimaryID::SMPTE240M;
      break;
    case gfx::ColorSpace::PrimaryID::FILM:
      primaries = VideoColorSpace::PrimaryID::FILM;
      break;
    case gfx::ColorSpace::PrimaryID::BT2020:
      primaries = VideoColorSpace::PrimaryID::BT2020;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
      primaries = VideoColorSpace::PrimaryID::SMPTEST428_1;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
      primaries = VideoColorSpace::PrimaryID::SMPTEST431_2;
      break;
    case gfx::ColorSpace::PrimaryID::P3:
      primaries = VideoColorSpace::PrimaryID::SMPTEST432_1;
      break;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      primaries = VideoColorSpace::PrimaryID::EBU_3213_E;
      break;
    default:
      break;
  }

  VideoColorSpace::TransferID transfer = VideoColorSpace::TransferID::INVALID;
  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::BT709:
      transfer = VideoColorSpace::TransferID::BT709;
      break;
    case gfx::ColorSpace::TransferID::GAMMA22:
      transfer = VideoColorSpace::TransferID::GAMMA22;
      break;
    case gfx::ColorSpace::TransferID::GAMMA28:
      transfer = VideoColorSpace::TransferID::GAMMA28;
      break;
    case gfx::ColorSpace::TransferID::SMPTE170M:
      transfer = VideoColorSpace::TransferID::SMPTE170M;
      break;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      transfer = VideoColorSpace::TransferID::SMPTE240M;
      break;
    case gfx::ColorSpace::TransferID::LINEAR:
      transfer = VideoColorSpace::TransferID::LINEAR;
      break;
    case gfx::ColorSpace::TransferID::LOG:
      transfer = VideoColorSpace::TransferID::LOG;
      break;
    case gfx::ColorSpace::TransferID::LOG_SQRT:
      transfer = VideoColorSpace::TransferID::LOG_SQRT;
      break;
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
      transfer = VideoColorSpace::TransferID::IEC61966_2_4;
      break;
    case gfx::ColorSpace::TransferID::BT1361_ECG:
      transfer = VideoColorSpace::TransferID::BT1361_ECG;
      break;
    case gfx::ColorSpace::TransferID::SRGB:
      transfer = VideoColorSpace::TransferID::IEC61966_2_1;
      break;
    case gfx::ColorSpace::TransferID::BT2020_10:
      transfer = VideoColorSpace::TransferID::BT2020_10;
      break;
    case gfx::ColorSpace::TransferID::BT2020_12:
      transfer = VideoColorSpace::TransferID::BT2020_12;
      break;
    case gfx::ColorSpace::TransferID::PQ:
      transfer = VideoColorSpace::TransferID::SMPTEST2084;
      break;
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
      transfer = VideoColorSpace::TransferID::SMPTEST428_1;
      break;
    case gfx::ColorSpace::TransferID::HLG:
      transfer = VideoColorSpace::TransferID::ARIB_STD_B67;
      break;
    default:
      break;
  }

  VideoColorSpace::MatrixID matrix = VideoColorSpace::MatrixID::INVALID;
  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::RGB:
      matrix = VideoColorSpace::MatrixID::RGB;
      break;
    case gfx::ColorSpace::MatrixID::BT709:
      matrix = VideoColorSpace::MatrixID::BT709;
      break;
    case gfx::ColorSpace::MatrixID::FCC:
      matrix = VideoColorSpace::MatrixID::FCC;
      break;
    case gfx::ColorSpace::MatrixID::BT470BG:
      matrix = VideoColorSpace::MatrixID::BT470BG;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      matrix = VideoColorSpace::MatrixID::SMPTE170M;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      matrix = VideoColorSpace::MatrixID::SMPTE240M;
      break;
    case gfx::ColorSpace::MatrixID::YCOCG:
      matrix = VideoColorSpace::MatrixID::YCOCG;
      break;
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
      matrix = VideoColorSpace::MatrixID::BT2020_NCL;
      break;
    case gfx::ColorSpace::MatrixID::YDZDX:
      matrix = VideoColorSpace::MatrixID::YDZDX;
      break;
    default:
      break;
  }

  return VideoColorSpace(primaries, transfer, matrix, color_space.GetRangeID());
}

gfx::ColorSpace VideoColorSpace::ToGfxColorSpaceInternal(
    bool allow_guessing) const {
  gfx::ColorSpace::PrimaryID primary_id = gfx::ColorSpace::PrimaryID::INVALID;
  gfx::ColorSpace::TransferID transfer_id =
      gfx::ColorSpace::TransferID::INVALID;
  gfx::ColorSpace::MatrixID matrix_id = gfx::ColorSpace::MatrixID::INVALID;
  gfx::ColorSpace::RangeID range_id = range;

  // Bitfield, note that guesses with higher values take precedence over
  // guesses with lower values.
  enum Guess {
    GUESS_BT709 = 1 << 4,
    GUESS_BT470M = 1 << 3,
    GUESS_BT470BG = 1 << 2,
    GUESS_SMPTE170M = 1 << 1,
    GUESS_SMPTE240M = 1 << 0,
  };

  uint32_t guess = 0;

  switch (primaries) {
    case PrimaryID::BT709:
      primary_id = gfx::ColorSpace::PrimaryID::BT709;
      guess |= GUESS_BT709;
      break;
    case PrimaryID::BT470M:
      primary_id = gfx::ColorSpace::PrimaryID::BT470M;
      guess |= GUESS_BT470M;
      break;
    case PrimaryID::BT470BG:
      primary_id = gfx::ColorSpace::PrimaryID::BT470BG;
      guess |= GUESS_BT470BG;
      break;
    case PrimaryID::SMPTE170M:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTE170M;
      guess |= GUESS_SMPTE170M;
      break;
    case PrimaryID::SMPTE240M:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTE240M;
      guess |= GUESS_SMPTE240M;
      break;
    case PrimaryID::FILM:
      primary_id = gfx::ColorSpace::PrimaryID::FILM;
      break;
    case PrimaryID::BT2020:
      primary_id = gfx::ColorSpace::PrimaryID::BT2020;
      break;
    case PrimaryID::SMPTEST428_1:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTEST428_1;
      break;
    case PrimaryID::SMPTEST431_2:
      primary_id = gfx::ColorSpace::PrimaryID::SMPTEST431_2;
      break;
    case PrimaryID::SMPTEST432_1:
      primary_id = gfx::ColorSpace::PrimaryID::P3;
      break;
    case PrimaryID::EBU_3213_E:
      primary_id = gfx::ColorSpace::PrimaryID::EBU_3213_E;
      break;
    case PrimaryID::INVALID:
    case PrimaryID::UNSPECIFIED:
      break;
  }

  switch (transfer) {
    case TransferID::BT709:
      transfer_id = gfx::ColorSpace::TransferID::BT709;
      guess |= GUESS_BT709;
      break;
    case TransferID::GAMMA22:
      transfer_id = gfx::ColorSpace::TransferID::GAMMA22;
      break;
    case TransferID::GAMMA28:
      transfer_id = gfx::ColorSpace::TransferID::GAMMA28;
      break;
    case TransferID::SMPTE170M:
      transfer_id = gfx::ColorSpace::TransferID::SMPTE170M;
      guess |= GUESS_SMPTE170M;
      break;
    case TransferID::SMPTE240M:
      transfer_id = gfx::ColorSpace::TransferID::SMPTE240M;
      guess |= GUESS_SMPTE240M;
      break;
    case TransferID::LINEAR:
      transfer_id = gfx::ColorSpace::TransferID::LINEAR;
      break;
    case TransferID::LOG:
      transfer_id = gfx::ColorSpace::TransferID::LOG;
      break;
    case TransferID::LOG_SQRT:
      transfer_id = gfx::ColorSpace::TransferID::LOG_SQRT;
      break;
    case TransferID::IEC61966_2_4:
      transfer_id = gfx::ColorSpace::TransferID::IEC61966_2_4;
      break;
    case TransferID::BT1361_ECG:
      transfer_id = gfx::ColorSpace::TransferID::BT1361_ECG;
      break;
    case TransferID::IEC61966_2_1:
      transfer_id = gfx::ColorSpace::TransferID::SRGB;
      break;
    case TransferID::BT2020_10:
      transfer_id = gfx::ColorSpace::TransferID::BT2020_10;
      break;
    case TransferID::BT2020_12:
      transfer_id = gfx::ColorSpace::TransferID::BT2020_12;
      break;
    case TransferID::SMPTEST2084:
      transfer_id = gfx::ColorSpace::TransferID::PQ;
      break;
    case TransferID::SMPTEST428_1:
      transfer_id = gfx::ColorSpace::TransferID::SMPTEST428_1;
      break;
    case TransferID::ARIB_STD_B67:
      transfer_id = gfx::ColorSpace::TransferID::HLG;
      break;
    case TransferID::INVALID:
    case TransferID::UNSPECIFIED:
      break;
  }

  switch (matrix) {
    case MatrixID::RGB:
      // RGB-encoded video actually puts the green in the Y channel,
      // the blue in the Cb (U) channel and the red in the Cr (V) channel.
      matrix_id = gfx::ColorSpace::MatrixID::GBR;
      break;
    case MatrixID::BT709:
      matrix_id = gfx::ColorSpace::MatrixID::BT709;
      guess |= GUESS_BT709;
      break;
    case MatrixID::FCC:
      matrix_id = gfx::ColorSpace::MatrixID::FCC;
      break;
    case MatrixID::BT470BG:
      matrix_id = gfx::ColorSpace::MatrixID::BT470BG;
      guess |= GUESS_BT470BG;
      break;
    case MatrixID::SMPTE170M:
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE170M;
      guess |= GUESS_SMPTE170M;
      break;
    case MatrixID::SMPTE240M:
      matrix_id = gfx::ColorSpace::MatrixID::SMPTE240M;
      guess |= GUESS_SMPTE240M;
      break;
    case MatrixID::YCOCG:
      matrix_id = gfx::ColorSpace::MatrixID::YCOCG;
      break;
    case MatrixID::BT2020_NCL:
      matrix_id = gfx::ColorSpace::MatrixID::BT2020_NCL;
      break;
    case MatrixID::YDZDX:
      matrix_id = gfx::ColorSpace::MatrixID::YDZDX;
      break;
    case MatrixID::BT2020_CL:
    case MatrixID::INVALID:
    case MatrixID::UNSPECIFIED:
      break;
  }

  if (!allow_guessing) {
    // For simplicity, when guessing is disabled return a fully invalid
    // gfx::ColorSpace if any individual parameter is unspecified.
    if (primary_id == gfx::ColorSpace::PrimaryID::INVALID ||
        transfer_id == gfx::ColorSpace::TransferID::INVALID ||
        matrix_id == gfx::ColorSpace::MatrixID::INVALID ||
        range_id == gfx::ColorSpace::RangeID::INVALID) {
      return gfx::ColorSpace();
    }
    return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
  }

  // Removes lowest bit until only a single bit remains.
  while (guess & (guess - 1)) {
    guess &= guess - 1;
  }
  if (!guess)
    guess = GUESS_BT709;

  if (primary_id == gfx::ColorSpace::PrimaryID::INVALID &&
      transfer_id == gfx::ColorSpace::TransferID::INVALID &&
      matrix_id == gfx::ColorSpace::MatrixID::INVALID &&
      range_id == gfx::ColorSpace::RangeID::INVALID) {
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
    }
  }

  if (range_id == gfx::ColorSpace::RangeID::INVALID) {
    range_id = gfx::ColorSpace::RangeID::DERIVED;
  }

  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

}  // namespace
