// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_color_space.h"

#include "media/base/video_color_space.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_color_space_init.h"
#include "ui/gfx/color_space.h"

namespace blink {

// static
VideoColorSpace* VideoColorSpace::Create(const VideoColorSpaceInit* init) {
  return MakeGarbageCollected<VideoColorSpace>(init);
}

VideoColorSpace::VideoColorSpace(const VideoColorSpaceInit* init) {
  if (init->hasPrimaries())
    primaries_ = init->primaries();
  if (init->hasTransfer())
    transfer_ = init->transfer();
  if (init->hasMatrix())
    matrix_ = init->matrix();
  if (init->hasFullRange())
    full_range_ = init->fullRange();
}

VideoColorSpace::VideoColorSpace(const gfx::ColorSpace& color_space) {
  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
      primaries_ = V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kBt709);
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      primaries_ = V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kBt470Bg);
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      primaries_ =
          V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kSmpte170M);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::BT709:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kBt709);
      break;
    case gfx::ColorSpace::TransferID::SMPTE170M:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kSmpte170M);
      break;
    case gfx::ColorSpace::TransferID::SRGB:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kIec6196621);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::RGB:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kRgb);
      break;
    case gfx::ColorSpace::MatrixID::BT709:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kBt709);
      break;
    case gfx::ColorSpace::MatrixID::BT470BG:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kBt470Bg);
      break;
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      matrix_ = V8VideoMatrixCoefficients(
          V8VideoMatrixCoefficients::Enum::kSmpte170M);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.GetRangeID()) {
    case gfx::ColorSpace::RangeID::LIMITED:
      full_range_ = false;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      full_range_ = true;
      break;
    default:
      // Other values map to unspecified. We could probably map DERIVED to a
      // specific value, though.
      break;
  }
}

VideoColorSpace::VideoColorSpace(const media::VideoColorSpace& color_space) {
  switch (color_space.primaries) {
    case media::VideoColorSpace::PrimaryID::BT709:
      primaries_ = V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kBt709);
      break;
    case media::VideoColorSpace::PrimaryID::BT470BG:
      primaries_ = V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kBt470Bg);
      break;
    case media::VideoColorSpace::PrimaryID::SMPTE170M:
      primaries_ =
          V8VideoColorPrimaries(V8VideoColorPrimaries::Enum::kSmpte170M);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.transfer) {
    case media::VideoColorSpace::TransferID::BT709:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kBt709);
      break;
    case media::VideoColorSpace::TransferID::SMPTE170M:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kSmpte170M);
      break;
    case media::VideoColorSpace::TransferID::IEC61966_2_1:
      transfer_ = V8VideoTransferCharacteristics(
          V8VideoTransferCharacteristics::Enum::kIec6196621);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.matrix) {
    case media::VideoColorSpace::MatrixID::RGB:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kRgb);
      break;
    case media::VideoColorSpace::MatrixID::BT709:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kBt709);
      break;
    case media::VideoColorSpace::MatrixID::BT470BG:
      matrix_ =
          V8VideoMatrixCoefficients(V8VideoMatrixCoefficients::Enum::kBt470Bg);
      break;
    case media::VideoColorSpace::MatrixID::SMPTE170M:
      matrix_ = V8VideoMatrixCoefficients(
          V8VideoMatrixCoefficients::Enum::kSmpte170M);
      break;
    default:
      // Other values map to unspecified for now.
      break;
  }

  switch (color_space.range) {
    case gfx::ColorSpace::RangeID::LIMITED:
      full_range_ = false;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      full_range_ = true;
      break;
    default:
      // Other values map to unspecified. We could probably map DERIVED to a
      // specific value, though.
      break;
  }
}

gfx::ColorSpace VideoColorSpace::ToGfxColorSpace() const {
  gfx::ColorSpace::PrimaryID primaries = gfx::ColorSpace::PrimaryID::INVALID;
  if (primaries_) {
    switch (primaries_->AsEnum()) {
      case V8VideoColorPrimaries::Enum::kBt709:
        primaries = gfx::ColorSpace::PrimaryID::BT709;
        break;
      case V8VideoColorPrimaries::Enum::kBt470Bg:
        primaries = gfx::ColorSpace::PrimaryID::BT470BG;
        break;
      case V8VideoColorPrimaries::Enum::kSmpte170M:
        primaries = gfx::ColorSpace::PrimaryID::SMPTE170M;
        break;
    }
  }

  gfx::ColorSpace::TransferID transfer = gfx::ColorSpace::TransferID::INVALID;
  if (transfer_) {
    switch (transfer_->AsEnum()) {
      case V8VideoTransferCharacteristics::Enum::kBt709:
        transfer = gfx::ColorSpace::TransferID::BT709;
        break;
      case V8VideoTransferCharacteristics::Enum::kSmpte170M:
        transfer = gfx::ColorSpace::TransferID::SMPTE170M;
        break;
      case V8VideoTransferCharacteristics::Enum::kIec6196621:
        transfer = gfx::ColorSpace::TransferID::SRGB;
        break;
    }
  }

  gfx::ColorSpace::MatrixID matrix = gfx::ColorSpace::MatrixID::INVALID;
  if (matrix_) {
    switch (matrix_->AsEnum()) {
      case V8VideoMatrixCoefficients::Enum::kRgb:
        matrix = gfx::ColorSpace::MatrixID::RGB;
        break;
      case V8VideoMatrixCoefficients::Enum::kBt709:
        matrix = gfx::ColorSpace::MatrixID::BT709;
        break;
      case V8VideoMatrixCoefficients::Enum::kBt470Bg:
        matrix = gfx::ColorSpace::MatrixID::BT470BG;
        break;
      case V8VideoMatrixCoefficients::Enum::kSmpte170M:
        matrix = gfx::ColorSpace::MatrixID::SMPTE170M;
        break;
    }
  }

  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;
  if (full_range_) {
    range = *full_range_ ? gfx::ColorSpace::RangeID::FULL
                         : gfx::ColorSpace::RangeID::LIMITED;
  }

  return gfx::ColorSpace(primaries, transfer, matrix, range);
}

media::VideoColorSpace VideoColorSpace::ToMediaColorSpace() const {
  media::VideoColorSpace::PrimaryID primaries =
      media::VideoColorSpace::PrimaryID::UNSPECIFIED;
  if (primaries_) {
    switch (primaries_->AsEnum()) {
      case V8VideoColorPrimaries::Enum::kBt709:
        primaries = media::VideoColorSpace::PrimaryID::BT709;
        break;
      case V8VideoColorPrimaries::Enum::kBt470Bg:
        primaries = media::VideoColorSpace::PrimaryID::BT470BG;
        break;
      case V8VideoColorPrimaries::Enum::kSmpte170M:
        primaries = media::VideoColorSpace::PrimaryID::SMPTE170M;
        break;
    }
  }

  media::VideoColorSpace::TransferID transfer =
      media::VideoColorSpace::TransferID::UNSPECIFIED;
  if (transfer_) {
    switch (transfer_->AsEnum()) {
      case V8VideoTransferCharacteristics::Enum::kBt709:
        transfer = media::VideoColorSpace::TransferID::BT709;
        break;
      case V8VideoTransferCharacteristics::Enum::kSmpte170M:
        transfer = media::VideoColorSpace::TransferID::SMPTE170M;
        break;
      case V8VideoTransferCharacteristics::Enum::kIec6196621:
        transfer = media::VideoColorSpace::TransferID::IEC61966_2_1;
        break;
    }
  }

  media::VideoColorSpace::MatrixID matrix =
      media::VideoColorSpace::MatrixID::UNSPECIFIED;
  if (matrix_) {
    switch (matrix_->AsEnum()) {
      case V8VideoMatrixCoefficients::Enum::kRgb:
        matrix = media::VideoColorSpace::MatrixID::RGB;
        break;
      case V8VideoMatrixCoefficients::Enum::kBt709:
        matrix = media::VideoColorSpace::MatrixID::BT709;
        break;
      case V8VideoMatrixCoefficients::Enum::kBt470Bg:
        matrix = media::VideoColorSpace::MatrixID::BT470BG;
        break;
      case V8VideoMatrixCoefficients::Enum::kSmpte170M:
        matrix = media::VideoColorSpace::MatrixID::SMPTE170M;
        break;
    }
  }

  gfx::ColorSpace::RangeID range = gfx::ColorSpace::RangeID::INVALID;
  if (full_range_) {
    range = *full_range_ ? gfx::ColorSpace::RangeID::FULL
                         : gfx::ColorSpace::RangeID::LIMITED;
  }

  return media::VideoColorSpace(primaries, transfer, matrix, range);
}

VideoColorSpaceInit* VideoColorSpace::toJSON() const {
  auto* init = MakeGarbageCollected<VideoColorSpaceInit>();
  if (primaries_)
    init->setPrimaries(*primaries_);
  if (transfer_)
    init->setTransfer(*transfer_);
  if (matrix_)
    init->setMatrix(*matrix_);
  if (full_range_)
    init->setFullRange(*full_range_);
  return init;
}

}  // namespace blink
