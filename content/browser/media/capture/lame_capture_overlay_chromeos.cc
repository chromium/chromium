// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/lame_capture_overlay_chromeos.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "content/browser/media/capture/lame_window_capturer_chromeos.h"
#include "media/base/video_frame.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

LameCaptureOverlayChromeOS::Owner::~Owner() = default;

LameCaptureOverlayChromeOS::LameCaptureOverlayChromeOS(
    Owner* owner,
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCaptureOverlay> receiver)
    : receiver_(this, std::move(receiver)) {
  if (owner) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &Owner::OnOverlayConnectionLost, base::Unretained(owner), this));
  }
}

LameCaptureOverlayChromeOS::~LameCaptureOverlayChromeOS() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void LameCaptureOverlayChromeOS::SetImageAndBounds(const SkBitmap& image,
                                                   const gfx::RectF& bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  image_ = image;
  bounds_ = bounds;
  cached_scaled_image_.reset();
}

void LameCaptureOverlayChromeOS::SetBounds(const gfx::RectF& bounds) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bounds != bounds_) {
    bounds_ = bounds;
    cached_scaled_image_.reset();
  }
}

namespace {

// Scales a |relative| rect having coordinates in the range [0.0,1.0) by the
// given |span|, snapping all coordinates to even numbers.
gfx::Rect ToAbsoluteBoundsForI420(const gfx::RectF& relative,
                                  const gfx::Rect& span) {
  const float absolute_left = std::fma(relative.x(), span.width(), span.x());
  const float absolute_top = std::fma(relative.y(), span.height(), span.y());
  const float absolute_right =
      std::fma(relative.right(), span.width(), span.x());
  const float absolute_bottom =
      std::fma(relative.bottom(), span.height(), span.y());

  // Compute the largest I420-friendly Rect that is fully-enclosed by the
  // absolute rect. Use saturated_cast<> to restrict all extreme results [and
  // Inf and NaN] to a safe range of integers.
  const int snapped_left =
      base::saturated_cast<int16_t>(std::ceil(absolute_left / 2.0f)) * 2;
  const int snapped_top =
      base::saturated_cast<int16_t>(std::ceil(absolute_top / 2.0f)) * 2;
  const int snapped_right =
      base::saturated_cast<int16_t>(std::floor(absolute_right / 2.0f)) * 2;
  const int snapped_bottom =
      base::saturated_cast<int16_t>(std::floor(absolute_bottom / 2.0f)) * 2;
  return gfx::Rect(snapped_left, snapped_top,
                   std::max(0, snapped_right - snapped_left),
                   std::max(0, snapped_bottom - snapped_top));
}

inline int alpha_blend(int alpha, int src, int dst) {
  alpha = (src * alpha + dst * (255 - alpha)) / 255;
  return base::ClampToRange(alpha, 0, 255);
}

}  // namespace

LameCaptureOverlayChromeOS::OnceRenderer
LameCaptureOverlayChromeOS::MakeRenderer(const gfx::Rect& region_in_frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (bounds_.IsEmpty() || image_.drawsNothing()) {
    return OnceRenderer();
  }

  // Determine the bounds of the overlay inside the video frame. The
  // calculations here align to the 2x2 pixel-quads to simplify later
  // implementation.
  const gfx::Rect bounds_in_frame =
      ToAbsoluteBoundsForI420(bounds_, region_in_frame);

  // Compute the region of the frame to be modified.
  gfx::Rect blit_rect = region_in_frame;
  blit_rect.Intersect(bounds_in_frame);
  // If the two rects didn't intersect at all (i.e., everything has been
  // clipped), punt.
  if (blit_rect.IsEmpty()) {
    return OnceRenderer();
  }

  if (cached_scaled_image_.drawsNothing() ||
      cached_scaled_image_.width() != bounds_in_frame.width() ||
      cached_scaled_image_.height() != bounds_in_frame.height()) {
    cached_scaled_image_ = skia::ImageOperations::Resize(
        image_, skia::ImageOperations::RESIZE_BETTER, bounds_in_frame.width(),
        bounds_in_frame.height());
  }

  // The following binds all state required to render the overlay on a
  // VideoFrame at a later time. This callback does not require
  // LameCaptureOverlayChromeOS to outlive it.
  return base::BindOnce(
      [](const SkBitmap& image, const gfx::Point& position,
         const gfx::Rect& rect, media::VideoFrame* frame) {
        DCHECK(frame);
        DCHECK_EQ(frame->format(), media::PIXEL_FORMAT_I420);
        DCHECK(frame->visible_rect().Contains(rect));

        // Render the overlay in the video frame. This loop also performs a
        // simple RGB→YUV color space conversion, with alpha-blended
        // compositing.
        for (int y = rect.y(); y < rect.bottom(); ++y) {
          const int source_row = y - position.y();
          uint8_t* const yplane =
              frame->visible_data(media::VideoFrame::kYPlane) +
              y * frame->stride(media::VideoFrame::kYPlane);
          uint8_t* const uplane =
              frame->visible_data(media::VideoFrame::kUPlane) +
              (y / 2) * frame->stride(media::VideoFrame::kUPlane);
          uint8_t* const vplane =
              frame->visible_data(media::VideoFrame::kVPlane) +
              (y / 2) * frame->stride(media::VideoFrame::kVPlane);
          for (int x = rect.x(); x < rect.right(); ++x) {
            const int source_col = x - position.x();
            const SkColor color = image.getColor(source_col, source_row);
            const int alpha = SkColorGetA(color);
            const int color_r = SkColorGetR(color);
            const int color_g = SkColorGetG(color);
            const int color_b = SkColorGetB(color);
            const int color_y =
                ((color_r * 66 + color_g * 129 + color_b * 25 + 128) >> 8) + 16;
            yplane[x] = alpha_blend(alpha, color_y, yplane[x]);

            // Only sample U and V at even coordinates.
            if ((x % 2 == 0) && (y % 2 == 0)) {
              const int color_u =
                  ((color_r * -38 + color_g * -74 + color_b * 112 + 128) >> 8) +
                  128;
              const int color_v =
                  ((color_r * 112 + color_g * -94 + color_b * -18 + 128) >> 8) +
                  128;
              uplane[x / 2] = alpha_blend(alpha, color_u, uplane[x / 2]);
              vplane[x / 2] = alpha_blend(alpha, color_v, vplane[x / 2]);
            }
          }
        }
      },
      cached_scaled_image_, bounds_in_frame.origin(), blit_rect);
}

}  // namespace content
