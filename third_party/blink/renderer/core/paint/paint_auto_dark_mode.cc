// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace blink {

namespace {
// Images with both width and height smaller than this value are considered
// icons.
constexpr int kMaxImageLength = 64;
// Images with either dimension less than this value are considered separators.
constexpr int kMaxImageSeparatorLength = 8;

// We need to do image classification first before calling
// DarkModeFilter::GenerateImageFilter.
DarkModeFilter::ImageType GetImageType(const gfx::Rect& dest_rect,
                                       const gfx::Rect& src_rect) {
  if (dest_rect.width() <= kMaxImageLength &&
      dest_rect.height() <= kMaxImageLength) {
    return DarkModeFilter::ImageType::kIcon;
  }

  if (src_rect.width() <= kMaxImageSeparatorLength ||
      src_rect.height() <= kMaxImageSeparatorLength)
    return DarkModeFilter::ImageType::kSeparator;

  return DarkModeFilter::ImageType::kPhoto;
}

// Classifies an image after undoing the frame's layout zoom factor.
// |dest_rect| comes from layout geometry and includes layout zoom (page zoom
// and potentially DSF) and CSS zoom. Undo only layout zoom so page zoom and
// DSF do not affect classification, while CSS zoom still does. |src_rect| is
// derived from the image's intrinsic pixel size and is already
// zoom-independent, so it must be left untouched.
DarkModeFilter::ImageType GetImageTypeWithZoom(float zoom,
                                               const gfx::RectF& dest_rect,
                                               const gfx::RectF& src_rect) {
  gfx::RectF unzoomed_dest_rect = dest_rect;
  if (zoom > 0.f && zoom != 1.f) {
    unzoomed_dest_rect.Scale(1.f / zoom);
  }

  return GetImageType(gfx::ToEnclosingRect(unzoomed_dest_rect),
                      gfx::ToEnclosingRect(src_rect));
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

  const float layout_zoom = local_frame.LayoutZoomFactor();
  return ImageAutoDarkMode(
      role, style.ForceDark(),
      GetImageTypeWithZoom(layout_zoom, dest_rect, src_rect));
}

// static
DarkModeFilter::ImageType ImageClassifierHelper::GetSVGDocumentType(
    LocalFrame& local_frame,
    const gfx::Rect& size) {
  // |size| includes the layout zoom factor (page zoom and DSF). Undo it so the
  // size threshold matches bitmap images, whose classification is unaffected by
  // page zoom and DSF.
  const float layout_zoom = local_frame.LayoutZoomFactor();
  const float unzoomed_width =
      layout_zoom > 0.f ? size.width() / layout_zoom : size.width();
  const float unzoomed_height =
      layout_zoom > 0.f ? size.height() / layout_zoom : size.height();
  if (unzoomed_width <= kMaxImageLength && unzoomed_height <= kMaxImageLength) {
    return DarkModeFilter::ImageType::kIcon;
  }

  return DarkModeFilter::ImageType::kPhoto;
}

// static
DarkModeFilter::ImageType ImageClassifierHelper::GetImageTypeForTesting(
    const gfx::RectF& dest_rect,
    const gfx::RectF& src_rect,
    float zoom) {
  return GetImageTypeWithZoom(zoom, dest_rect, src_rect);
}

}  // namespace blink
