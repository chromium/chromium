/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/page/drag_image.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/string_truncator.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

const float kDragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float kDragLabelBorderY = 2;
const float kLabelBorderYOffset = 2;

const float kMaxDragLabelWidth = 300;
const float kMaxDragLabelStringWidth =
    (kMaxDragLabelWidth - 2 * kDragLabelBorderX);

const float kDragLinkLabelFontSize = 11;
const float kDragLinkUrlFontSize = 10;

}  // anonymous namespace

gfx::Vector2dF DragImage::ClampedImageScale(const gfx::Size& image_size,
                                            const gfx::Size& size,
                                            const gfx::Size& max_size) {
  // Non-uniform scaling for size mapping.
  gfx::Vector2dF image_scale(
      static_cast<float>(size.width()) / image_size.width(),
      static_cast<float>(size.height()) / image_size.height());

  // Uniform scaling for clamping.
  const float clamp_scale_x =
      size.width() > max_size.width()
          ? static_cast<float>(max_size.width()) / size.width()
          : 1;
  const float clamp_scale_y =
      size.height() > max_size.height()
          ? static_cast<float>(max_size.height()) / size.height()
          : 1;
  image_scale.Scale(std::min(clamp_scale_x, clamp_scale_y));

  return image_scale;
}

std::unique_ptr<DragImage> DragImage::Create(
    Image* image,
    RespectImageOrientationEnum should_respect_image_orientation,
    InterpolationQuality interpolation_quality,
    float opacity,
    gfx::Vector2dF image_scale) {
  if (!image)
    return nullptr;

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!paint_image)
    return nullptr;

  ImageOrientation orientation;
  auto* bitmap_image = DynamicTo<BitmapImage>(image);
  if (should_respect_image_orientation == kRespectImageOrientation &&
      bitmap_image)
    orientation = bitmap_image->CurrentFrameOrientation();

  SkBitmap bm;
  paint_image = Image::ResizeAndOrientImage(
      paint_image, orientation, image_scale, opacity, interpolation_quality,
      SkColorSpace::MakeSRGB());
  if (!paint_image || !paint_image.GetSwSkImage()->asLegacyBitmap(&bm))
    return nullptr;

  return base::WrapUnique(new DragImage(bm, interpolation_quality));
}

static Font DeriveDragLabelFont(int size, FontSelectionValue font_weight) {
  const AtomicString& family =
      LayoutThemeFontProvider::SystemFontFamily(CSSValueID::kNone);

  FontDescription description;
  description.SetFamily(
      FontFamily(family, FontFamily::InferredTypeFor(family)));
  description.SetWeight(font_weight);
  description.SetSpecifiedSize(size);
  description.SetComputedSize(size);
  Font result(description);
  return result;
}

// static
std::unique_ptr<DragImage> DragImage::Create(const KURL& url,
                                             const String& in_label,
                                             float device_scale_factor) {
  const Font label_font =
      DeriveDragLabelFont(kDragLinkLabelFontSize, kBoldWeightValue);
  const SimpleFontData* label_font_data = label_font.PrimaryFont();
  DCHECK(label_font_data);
  const Font url_font =
      DeriveDragLabelFont(kDragLinkUrlFontSize, kNormalWeightValue);
  const SimpleFontData* url_font_data = url_font.PrimaryFont();
  DCHECK(url_font_data);

  if (!label_font_data || !url_font_data)
    return nullptr;

  FontCachePurgePreventer font_cache_purge_preventer;

  bool draw_url_string = true;
  bool clip_url_string = false;
  bool clip_label_string = false;
  float max_drag_label_string_width_dip =
      kMaxDragLabelStringWidth / device_scale_factor;

  String url_string = url.GetString();
  String label = in_label.StripWhiteSpace();
  if (label.empty()) {
    draw_url_string = false;
    label = url_string;
  }

  // First step is drawing the link drag image width.
  TextRun label_run(label.Impl());
  TextRun url_run(url_string.Impl());
  gfx::Size label_size(label_font.Width(label_run),
                       label_font_data->GetFontMetrics().Ascent() +
                           label_font_data->GetFontMetrics().Descent());

  if (label_size.width() > max_drag_label_string_width_dip) {
    label_size.set_width(max_drag_label_string_width_dip);
    clip_label_string = true;
  }

  gfx::Size url_string_size;
  gfx::Size image_size(label_size.width() + kDragLabelBorderX * 2,
                       label_size.height() + kDragLabelBorderY * 2);

  if (draw_url_string) {
    url_string_size.set_width(url_font.Width(url_run));
    url_string_size.set_height(url_font_data->GetFontMetrics().Ascent() +
                               url_font_data->GetFontMetrics().Descent());
    image_size.set_height(image_size.height() + url_string_size.height());
    if (url_string_size.width() > max_drag_label_string_width_dip) {
      image_size.set_width(max_drag_label_string_width_dip);
      clip_url_string = true;
    } else {
      image_size.set_width(
          std::max(label_size.width(), url_string_size.width()) +
          kDragLabelBorderX * 2);
    }
  }

  // We now know how big the image needs to be, so we create and
  // fill the background
  gfx::Size scaled_image_size =
      gfx::ScaleToFlooredSize(image_size, device_scale_factor);
  // TODO(fserb): are we sure this should be software?
  std::unique_ptr<CanvasResourceProvider> resource_provider(
      CanvasResourceProvider::CreateBitmapProvider(
          SkImageInfo::MakeN32Premul(scaled_image_size.width(),
                                     scaled_image_size.height()),
          cc::PaintFlags::FilterQuality::kLow,
          CanvasResourceProvider::ShouldInitialize::kNo));
  if (!resource_provider)
    return nullptr;

  resource_provider->Canvas().scale(device_scale_factor, device_scale_factor);

  const float kDragLabelRadius = 5;

  gfx::Rect rect(image_size);
  cc::PaintFlags background_paint;
  background_paint.setColor(SkColorSetRGB(140, 140, 140));
  background_paint.setAntiAlias(true);
  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(image_size.width(), image_size.height()),
                  kDragLabelRadius, kDragLabelRadius);
  resource_provider->Canvas().drawRRect(rrect, background_paint);

  // Draw the text
  cc::PaintFlags text_paint;
  if (draw_url_string) {
    if (clip_url_string)
      url_string = StringTruncator::CenterTruncate(
          url_string, image_size.width() - (kDragLabelBorderX * 2.0f),
          url_font);
    gfx::PointF text_pos(
        kDragLabelBorderX,
        image_size.height() -
            (kLabelBorderYOffset + url_font_data->GetFontMetrics().Descent()));
    TextRun text_run(url_string);
    url_font.DrawText(&resource_provider->Canvas(), TextRunPaintInfo(text_run),
                      text_pos, device_scale_factor, text_paint);
  }

  if (clip_label_string) {
    label = StringTruncator::RightTruncate(
        label, image_size.width() - (kDragLabelBorderX * 2.0f), label_font);
  }

  TextRun text_run(label);
  text_run.SetDirectionFromText();
  gfx::Point text_pos(
      kDragLabelBorderX,
      kDragLabelBorderY + label_font.GetFontDescription().ComputedPixelSize());
  if (text_run.Direction() == TextDirection::kRtl) {
    float text_width = label_font.Width(text_run);
    int available_width = image_size.width() - kDragLabelBorderX * 2;
    text_pos.set_x(available_width - ceilf(text_width));
  }
  label_font.DrawBidiText(&resource_provider->Canvas(),
                          TextRunPaintInfo(text_run), gfx::PointF(text_pos),
                          Font::kDoNotPaintIfFontNotReady, text_paint);

  scoped_refptr<StaticBitmapImage> image =
      resource_provider->Snapshot(FlushReason::kNon2DCanvas);
  return DragImage::Create(image.get(), kRespectImageOrientation);
}

DragImage::DragImage(const SkBitmap& bitmap,
                     InterpolationQuality interpolation_quality)
    : bitmap_(bitmap), interpolation_quality_(interpolation_quality) {}

DragImage::~DragImage() = default;

void DragImage::Scale(float scale_x, float scale_y) {
  skia::ImageOperations::ResizeMethod resize_method =
      interpolation_quality_ == kInterpolationNone
          ? skia::ImageOperations::RESIZE_BOX
          : skia::ImageOperations::RESIZE_LANCZOS3;
  int image_width = scale_x * bitmap_.width();
  int image_height = scale_y * bitmap_.height();
  bitmap_ = skia::ImageOperations::Resize(bitmap_, resize_method, image_width,
                                          image_height);
}

}  // namespace blink
