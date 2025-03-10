// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_PNG_PNG_IMAGE_SCALER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_PNG_PNG_IMAGE_SCALER_H_

#include <optional>

#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

// PNGImageScaler performs down sampling for PNGImageDecoder.
class PNGImageDecoder;

class PLATFORM_EXPORT PNGImageScaler final {
  USING_FAST_MALLOC(PNGImageScaler);

 public:
  // The provided decoder should already have been given enough data to compute
  // the source image size.
  explicit PNGImageScaler(PNGImageDecoder*);
  PNGImageScaler(const PNGImageScaler&) = delete;
  PNGImageScaler& operator=(const PNGImageScaler&) = delete;
  ~PNGImageScaler() = default;

  // True when the full-scaled decoded image would exceed the configured byte
  // limit.
  bool MustDownscale() const { return is_downscale_; }

  // For APNG, the frame size is a subset of the entire image size.
  gfx::Rect CalculateScaledFrameRect(const gfx::Rect& frame_rect) const;

  // Returns the corresponding destination y if the source y is the next row to
  // select pixels from, or null otherwise.
  std::optional<unsigned> CalculateScaledYIndex(unsigned origin_y_index) const;

  void DownscaleRowInPlace(unsigned char* row_buffer,
                           wtf_size_t row_buffer_width,
                           wtf_size_t scaled_rect_width);

  Vector<SkISize> SupportedDecodeSizes() const {
    return supported_decode_sizes_;
  }
  gfx::Size DecodedSize() const { return decoded_size_; }

 private:
  unsigned CalculateOriginXIndex(int scaled_x_index) const;

  raw_ptr<PNGImageDecoder> decoder_;
  bool is_downscale_ = false;
  Vector<SkISize> supported_decode_sizes_;
  gfx::Size decoded_size_;
  const gfx::Size origin_size_;

  unsigned width_numerator_ = 1;
  unsigned width_denominator_ = 1;
  unsigned height_numerator_ = 1;
  unsigned height_denominator_ = 1;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_PNG_PNG_IMAGE_SCALER_H_
