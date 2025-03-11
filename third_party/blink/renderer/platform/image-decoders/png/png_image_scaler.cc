// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_scaler.h"

#include <optional>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/gfx/geometry/size.h"

namespace {
gfx::Size CalculateScaledSize(const gfx::Size& src_size, float scale_factor) {
  // Compute src aspect ratio, swapping axes if necessary so ratio is >= 1.
  float src_numerator = src_size.width();
  float src_denominator = src_size.height();
  const bool portrait = src_denominator > src_numerator;
  if (portrait) {
    std::swap(src_numerator, src_denominator);
  }

  // Floor smaller dimension to ensure ratio of dest to src area is clamped by
  // the scale factor instead of potentially being slightly over it.
  const int dest_denominator = base::ClampFloor(src_denominator * scale_factor);
  // Finish computing scaled aspect ratio and use it to return a size.
  const int dest_numerator =
      base::ClampRound(dest_denominator * src_numerator / src_denominator);
  return portrait ? gfx::Size(dest_denominator, dest_numerator)
                  : gfx::Size(dest_numerator, dest_denominator);
}
}  // namespace

namespace blink {

PNGImageScaler::PNGImageScaler(PNGImageDecoder* decoder) : decoder_(decoder) {
  CHECK(decoder_->IsSizeAvailable());
  gfx::Size origin_size = decoder_->Size();

  // TODO(crbug.com/381913638, crbug.com/381969445): Support APNG and interlaced
  // PNGs in the future.
  if (decoder_->IsInterlaced() || decoder_->IsAnimated()) {
    decoded_size_.SetSize(origin_size.width(), origin_size.height());
    is_downscale_ = false;
    return;
  }

  float scale_factor = 1.0f;
  for (gfx::Size current_size = origin_size;
       current_size.width() > 2 && current_size.height() > 2;
       scale_factor /= 2, current_size =
                              CalculateScaledSize(origin_size, scale_factor)) {
    if (current_size.GetArea() * decoder_->GetDecodedBytesPerPixel() >
        decoder_->GetMaxDecodedBytes()) {
      continue;
    }
    supported_decode_sizes_.insert(
        0, SkISize::Make(current_size.width(), current_size.height()));
  }

  if (!supported_decode_sizes_.empty()) {
    auto largest_supported_size = supported_decode_sizes_.back();
    decoded_size_.SetSize(largest_supported_size.width(),
                          largest_supported_size.height());
  } else {
    decoded_size_.SetSize(origin_size.width(), origin_size.height());
  }

  is_downscale_ = origin_size != decoded_size_;

  // To align the image borders between origin and target, the scale factor
  // should be (original height - 1)/(target height - 1), not (original
  // height)/(target height). Numerator and denominator should larger than 1.
  width_numerator_ = std::max(static_cast<unsigned>(origin_size.width()) - 1,
                              (width_numerator_));
  height_numerator_ = std::max(static_cast<unsigned>(origin_size.height()) - 1,
                               (height_numerator_));
  width_denominator_ = std::max(
      static_cast<unsigned>(decoded_size_.width()) - 1, (width_denominator_));
  height_denominator_ = std::max(
      static_cast<unsigned>(decoded_size_.height()) - 1, (height_denominator_));
}

std::optional<unsigned> PNGImageScaler::CalculateScaledYIndex(
    unsigned src_y) const {
  if (src_y == 0) {
    return 0;
  }

  CHECK_GT(height_numerator_, 0u);
  CHECK_GT(height_denominator_, 0u);

  // Given a src y and the fixed ratio k of original height to decoded height,
  // we want to know if that src y is the appropriate nearest neighbor for some
  // dest y (and if so, what that dest y is). This will be true precisely when
  // there is an integer dest y that satisfies src_y = round(k * dest_y) for
  // this src y.

  // This can be equivalently written as: (src_y - 0.5)/k <= y_dest < (src_y +
  // 0.5)/k.

  // Since k >= 1 (because the decoded size is no larger than the original
  // size), this leaves a range of at most 1 that the dest y can fall within.
  // This integer, if it exists, must be the ceil of the lower bound; and we can
  // check whether it's a valid candidate by ensuring that is less than the
  // upper bound.

  unsigned candidate_dest_y = base::ClampCeil(
      ((src_y - 0.5) * height_denominator_) / height_numerator_);

  double upper_bound =
      ((src_y + 0.5) * height_denominator_) / height_numerator_;

  return candidate_dest_y < upper_bound ? std::make_optional(candidate_dest_y)
                                        : std::nullopt;
}

gfx::Rect PNGImageScaler::CalculateScaledFrameRect(
    const gfx::Rect& frame_rect) const {
  // TODO(crbug.com/381969445): For a single frame image, the rect is same as
  // the entire image. So we only need to return the decoded size. But for APNG,
  // the rect may be a part of the image. And we need to calculate the scaled
  // rect once we supput APNG.
  return gfx::Rect(decoded_size_);
}

// TODO(crbug.com/383157202): We are using nearest neighbor for downscaling
// currently, but we should improve the algorithm in the future, like using
// bilinear/bicubic.
void PNGImageScaler::DownscaleRowInPlace(unsigned char* row_buffer,
                                         wtf_size_t row_buffer_width,
                                         wtf_size_t scaled_rect_width) {
  wtf_size_t bytes = decoder_->GetBytesPerRawPixel();

  // SAFETY: The caller must ensure the buffer contains pixels spanning the
  // whole original width.
  UNSAFE_BUFFERS(base::span row(row_buffer, row_buffer_width * bytes));

  for (wtf_size_t offset = 0; offset < scaled_rect_width; ++offset) {
    const int source_x = CalculateOriginXIndex(offset);

    auto source_pixel = row.subspan(source_x * bytes, bytes);
    auto target_pixel = row.subspan(offset * bytes, bytes);
    std::ranges::copy(source_pixel, target_pixel.begin());
  }
}

unsigned PNGImageScaler::CalculateOriginXIndex(int scaled_x_index) const {
  return base::ClampRound(
      static_cast<double>(width_numerator_ * scaled_x_index) /
      width_denominator_);
}

}  // namespace blink
