// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {

// The maximum ratio of image size to screen size that is considered an icon.
constexpr float kMaxIconRatio = 0.13f;
constexpr int kMaxImageLength = 50;
// Images with either dimension less than this value are considered separators.
constexpr int kMaxImageSeparatorLength = 8;

// We need to do image classification first before calling
// DarkModeFilter::GenerateImageFilter.
DarkModeFilter::ImageType GetImageType(float dest_to_device_ratio,
                                       const gfx::Rect& dest_rect,
                                       const gfx::Rect& src_rect) {
  // TODO: Use a viewport relative threshold for the size check instead of
  // absolute threshold.
  if (dest_to_device_ratio <= kMaxIconRatio ||
      (dest_rect.width() <= kMaxImageLength &&
       dest_rect.height() <= kMaxImageLength))
    return DarkModeFilter::ImageType::kIcon;

  if (src_rect.width() <= kMaxImageSeparatorLength ||
      src_rect.height() <= kMaxImageSeparatorLength)
    return DarkModeFilter::ImageType::kSeparator;

  return DarkModeFilter::ImageType::kPhoto;
}

float GetRatio(const display::ScreenInfo& screen_info,
               const gfx::RectF& dest_rect) {
  const gfx::SizeF& device_rect = gfx::ScaleSize(
      gfx::SizeF(screen_info.rect.size()), screen_info.device_scale_factor);

  return std::max(dest_rect.width() / device_rect.width(),
                  dest_rect.height() / device_rect.height());
}

}  // namespace

// static
ImageAutoDarkMode ImageClassifierHelper::GetImageAutoDarkMode(
    LocalFrame& local_frame,
    const ComputedStyle& style,
    const gfx::RectF& dest_rect,
    const gfx::RectF& src_rect,
    DarkModeFilter::ElementRole role) {
  if (!style.ForceDark())
    return ImageAutoDarkMode::Disabled();

  const display::ScreenInfo& screen_info =
      local_frame.GetChromeClient().GetScreenInfo(local_frame);

  return ImageAutoDarkMode(role, style.ForceDark(),
                           GetImageType(GetRatio(screen_info, dest_rect),
                                        gfx::ToEnclosingRect(dest_rect),
                                        gfx::ToEnclosingRect(src_rect)));
}

// static
DarkModeFilter::ImageType ImageClassifierHelper::GetImageTypeForTesting(
    display::ScreenInfo& screen_info,
    const gfx::RectF& dest_rect,
    const gfx::RectF& src_rect) {
  return GetImageType(GetRatio(screen_info, dest_rect),
                      gfx::ToEnclosingRect(dest_rect),
                      gfx::ToEnclosingRect(src_rect));
}

}  // namespace blink
