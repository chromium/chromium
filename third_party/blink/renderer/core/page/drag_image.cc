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
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_cache.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_metrics.h"
#include "third_party/blink/renderer/platform/fonts/string_truncator.h"
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_point.h"
#include "third_party/blink/renderer/platform/graphics/bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/drawing_recorder.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/bidi_text_run.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

FloatSize DragImage::ClampedImageScale(const IntSize& image_size,
                                       const IntSize& size,
                                       const IntSize& max_size) {
  // Non-uniform scaling for size mapping.
  FloatSize image_scale(
      static_cast<float>(size.Width()) / image_size.Width(),
      static_cast<float>(size.Height()) / image_size.Height());

  // Uniform scaling for clamping.
  const float clamp_scale_x =
      size.Width() > max_size.Width()
          ? static_cast<float>(max_size.Width()) / size.Width()
          : 1;
  const float clamp_scale_y =
      size.Height() > max_size.Height()
          ? static_cast<float>(max_size.Height()) / size.Height()
          : 1;
  image_scale.Scale(std::min(clamp_scale_x, clamp_scale_y));

  return image_scale;
}

std::unique_ptr<DragImage> DragImage::Create(
    Image* image,
    RespectImageOrientationEnum should_respect_image_orientation,
    float device_scale_factor,
    InterpolationQuality interpolation_quality,
    float opacity,
    FloatSize image_scale) {
  if (!image)
    return nullptr;

  PaintImage paint_image = image->PaintImageForCurrentFrame();
  if (!paint_image)
    return nullptr;

  ImageOrientation orientation;
  if (should_respect_image_orientation == kRespectImageOrientation &&
      image->IsBitmapImage())
    orientation = ToBitmapImage(image)->CurrentFrameOrientation();

  SkBitmap bm;
  paint_image = Image::ResizeAndOrientImage(
      paint_image, orientation, image_scale, opacity, interpolation_quality);
  if (!paint_image || !paint_image.GetSkImage()->asLegacyBitmap(&bm))
    return nullptr;

  return base::WrapUnique(
      new DragImage(bm, device_scale_factor, interpolation_quality));
}

static Font DeriveDragLabelFont(int size,
                                FontSelectionValue font_weight,
                                const FontDescription& system_font) {
  FontDescription description = system_font;
  description.SetWeight(font_weight);
  description.SetSpecifiedSize(size);
  description.SetComputedSize(size);
  Font result(description);
  result.Update(nullptr);
  return result;
}

std::unique_ptr<DragImage> DragImage::Create(const KURL& url,
                                             const String& in_label,
                                             const FontDescription& system_font,
                                             float device_scale_factor) {
  const Font label_font = DeriveDragLabelFont(kDragLinkLabelFontSize,
                                              BoldWeightValue(), system_font);
  const SimpleFontData* label_font_data = label_font.PrimaryFont();
  DCHECK(label_font_data);
  const Font url_font = DeriveDragLabelFont(kDragLinkUrlFontSize,
                                            NormalWeightValue(), system_font);
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
  if (label.IsEmpty()) {
    draw_url_string = false;
    label = url_string;
  }

  // First step is drawing the link drag image width.
  TextRun label_run(label.Impl());
  TextRun url_run(url_string.Impl());
  IntSize label_size(label_font.Width(label_run),
                     label_font_data->GetFontMetrics().Ascent() +
                         label_font_data->GetFontMetrics().Descent());

  if (label_size.Width() > max_drag_label_string_width_dip) {
    label_size.SetWidth(max_drag_label_string_width_dip);
    clip_label_string = true;
  }

  IntSize url_string_size;
  IntSize image_size(label_size.Width() + kDragLabelBorderX * 2,
                     label_size.Height() + kDragLabelBorderY * 2);

  if (draw_url_string) {
    url_string_size.SetWidth(url_font.Width(url_run));
    url_string_size.SetHeight(url_font_data->GetFontMetrics().Ascent() +
                              url_font_data->GetFontMetrics().Descent());
    image_size.SetHeight(image_size.Height() + url_string_size.Height());
    if (url_string_size.Width() > max_drag_label_string_width_dip) {
      image_size.SetWidth(max_drag_label_string_width_dip);
      clip_url_string = true;
    } else {
      image_size.SetWidth(
          std::max(label_size.Width(), url_string_size.Width()) +
          kDragLabelBorderX * 2);
    }
  }

  // We now know how big the image needs to be, so we create and
  // fill the background
  IntSize scaled_image_size = image_size;
  scaled_image_size.Scale(device_scale_factor);
  // TODO(fserb): are we sure this should be software?
  std::unique_ptr<CanvasResourceProvider> resource_provider(
      CanvasResourceProvider::Create(
          scaled_image_size,
          CanvasResourceProvider::ResourceUsage::kSoftwareResourceUsage,
          nullptr,  // context_provider_wrapper
          0,        // msaa_sample_count
          kLow_SkFilterQuality, CanvasColorParams(),
          CanvasResourceProvider::kDefaultPresentationMode,
          nullptr));  // canvas_resource_dispatcher
  if (!resource_provider)
    return nullptr;

  resource_provider->Canvas()->scale(device_scale_factor, device_scale_factor);

  const float kDragLabelRadius = 5;

  IntRect rect(IntPoint(), image_size);
  PaintFlags background_paint;
  background_paint.setColor(SkColorSetRGB(140, 140, 140));
  background_paint.setAntiAlias(true);
  SkRRect rrect;
  rrect.setRectXY(SkRect::MakeWH(image_size.Width(), image_size.Height()),
                  kDragLabelRadius, kDragLabelRadius);
  resource_provider->Canvas()->drawRRect(rrect, background_paint);

  // Draw the text
  PaintFlags text_paint;
  if (draw_url_string) {
    if (clip_url_string)
      url_string = StringTruncator::CenterTruncate(
          url_string, image_size.Width() - (kDragLabelBorderX * 2.0f),
          url_font);
    FloatPoint text_pos(
        kDragLabelBorderX,
        image_size.Height() -
            (kLabelBorderYOffset + url_font_data->GetFontMetrics().Descent()));
    TextRun text_run(url_string);
    url_font.DrawText(resource_provider->Canvas(), TextRunPaintInfo(text_run),
                      text_pos, device_scale_factor, text_paint);
  }

  if (clip_label_string)
    label = StringTruncator::RightTruncate(
        label, image_size.Width() - (kDragLabelBorderX * 2.0f), label_font);

  bool has_strong_directionality;
  TextRun text_run =
      TextRunWithDirectionality(label, &has_strong_directionality);
  IntPoint text_pos(
      kDragLabelBorderX,
      kDragLabelBorderY + label_font.GetFontDescription().ComputedPixelSize());
  if (has_strong_directionality &&
      text_run.Direction() == TextDirection::kRtl) {
    float text_width = label_font.Width(text_run);
    int available_width = image_size.Width() - kDragLabelBorderX * 2;
    text_pos.SetX(available_width - ceilf(text_width));
  }
  label_font.DrawBidiText(resource_provider->Canvas(),
                          TextRunPaintInfo(text_run), FloatPoint(text_pos),
                          Font::kDoNotPaintIfFontNotReady, device_scale_factor,
                          text_paint);

  scoped_refptr<StaticBitmapImage> image = resource_provider->Snapshot();
  return DragImage::Create(image.get(), kDoNotRespectImageOrientation,
                           device_scale_factor);
}

DragImage::DragImage(const SkBitmap& bitmap,
                     float resolution_scale,
                     InterpolationQuality interpolation_quality)
    : bitmap_(bitmap),
      resolution_scale_(resolution_scale),
      interpolation_quality_(interpolation_quality) {}

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
