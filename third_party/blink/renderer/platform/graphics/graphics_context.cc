/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"

#include <memory>
#include <optional>

#include "base/logging.h"
#include "build/build_config.h"
#include "cc/paint/color_filter.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/platform/fonts/plain_text_painter.h"
#include "third_party/blink/renderer/platform/geometry/contoured_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/geometry/path.h"
#include "third_party/blink/renderer/platform/geometry/path_builder.h"
#include "third_party/blink/renderer/platform/geometry/skia_geometry_utils.h"
#include "third_party/blink/renderer/platform/geometry/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/platform_focus_ring.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/text/text_run.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/skia/include/core/SkAnnotation.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "third_party/skia/include/utils/SkNullCanvas.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

// To avoid conflicts with the DrawText macro from the Windows SDK...
#undef DrawText

namespace blink {

namespace {

SkColor4f DarkModeColor(GraphicsContext& context,
                        const SkColor4f& color,
                        const AutoDarkMode& auto_dark_mode) {
  if (auto_dark_mode.enabled) {
    return context.GetDarkModeFilter()->InvertColorIfNeeded(
        color, auto_dark_mode.role,
        SkColor4f::FromColor(auto_dark_mode.contrast_color));
  }
  return color;
}

InterpolationQuality ComputeInterpolationQuality(const gfx::SizeF& src,
                                                 const gfx::SizeF& dest,
                                                 bool is_data_complete) {
  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  const gfx::SizeF diff(std::abs(dest.width() - src.width()),
                        std::abs(dest.height() - src.height()));
  const bool width_nearly_equal =
      diff.width() < std::numeric_limits<float>::epsilon();
  const bool height_nearly_equal =
      diff.height() < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (width_nearly_equal && height_nearly_equal) {
    return kInterpolationNone;
  }

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  static constexpr int kSmallImageSizeThreshold = 8;
  if (src.width() <= kSmallImageSizeThreshold ||
      src.height() <= kSmallImageSizeThreshold ||
      dest.width() <= kSmallImageSizeThreshold ||
      dest.height() <= kSmallImageSizeThreshold) {
    // Small image detected.

    auto nearly_integral = [](float value) {
      return std::abs(value - std::floor(value)) <
             std::numeric_limits<float>::epsilon();
    };

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!nearly_integral(dest.width()) &&
         src.width() > 1 + std::numeric_limits<float>::epsilon()) ||
        (!nearly_integral(dest.height()) &&
         src.height() > 1 + std::numeric_limits<float>::epsilon())) {
      return kInterpolationLow;
    }

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return kInterpolationNone;
  }

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  static constexpr float kLargeStretch = 3.0f;
  if (src.height() * kLargeStretch <= dest.height() ||
      src.width() * kLargeStretch <= dest.width()) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (width_nearly_equal || height_nearly_equal) {
      return kInterpolationNone;
    }

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return kInterpolationLow;
  }

  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  static constexpr float kFractionalChangeThreshold = 0.025f;
  if ((diff.width() / src.width() < kFractionalChangeThreshold) &&
      (diff.height() / src.height() < kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return kInterpolationNone;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!is_data_complete) {
    return kInterpolationLow;
  }

  // Everything else gets resampled at default quality.
  return GetDefaultInterpolationQuality();
}

}  // namespace

// Helper class that copies |flags| only when dark mode is enabled.
//
// TODO(gilmanmh): Investigate removing const from |flags| in the calling
// methods and modifying the variable directly instead of copying it.
class GraphicsContext::DarkModeFlags final {
  STACK_ALLOCATED();

 public:
  // This helper's lifetime should never exceed |flags|'.
  DarkModeFlags(GraphicsContext* context,
                const AutoDarkMode& auto_dark_mode,
                const cc::PaintFlags& flags) {
    if (auto_dark_mode.enabled) {
      dark_mode_flags_ = context->GetDarkModeFilter()->ApplyToFlagsIfNeeded(
          flags, auto_dark_mode.role,
          SkColor4f::FromColor(auto_dark_mode.contrast_color));
      if (dark_mode_flags_) {
        flags_ = &dark_mode_flags_.value();
        return;
      }
    }
    flags_ = &flags;
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  operator const cc::PaintFlags&() const { return *flags_; }

 private:
  const cc::PaintFlags* flags_;
  std::optional<cc::PaintFlags> dark_mode_flags_;
};

GraphicsContext::GraphicsContext(PaintController& paint_controller)
    : paint_controller_(paint_controller) {
  // FIXME: Do some tests to determine how many states are typically used, and
  // allocate several here.
  paint_state_stack_.push_back(std::make_unique<GraphicsContextState>());
  paint_state_ = paint_state_stack_.back().get();
}

GraphicsContext::~GraphicsContext() {
#if DCHECK_IS_ON()
  if (!disable_destruction_checks_) {
    DCHECK(!paint_state_index_);
    DCHECK(!paint_state_->SaveCount());
    DCHECK(!layer_count_);
    DCHECK(!SaveCount());
  }
#endif
}

void GraphicsContext::CopyConfigFrom(GraphicsContext& other) {
  SetPrintingMetafile(other.printing_metafile_);
  SetPaintPreviewTracker(other.paint_preview_tracker_);
  SetPrinting(other.printing_);
  SetPrintingInternalHeadersAndFooters(
      other.printing_internal_headers_and_footers_);
}

DarkModeFilter* GraphicsContext::GetDarkModeFilter() {
  if (!dark_mode_filter_) {
    dark_mode_filter_ =
        std::make_unique<DarkModeFilter>(GetCurrentDarkModeSettings());
  }
  return dark_mode_filter_.get();
}

DarkModeFilter* GraphicsContext::GetDarkModeFilterForImage(
    const ImageAutoDarkMode& auto_dark_mode) {
  if (!auto_dark_mode.enabled)
    return nullptr;
  DarkModeFilter* dark_mode_filter = GetDarkModeFilter();
  if (!dark_mode_filter->ShouldApplyFilterToImage(auto_dark_mode.image_type))
    return nullptr;
  return dark_mode_filter;
}

void GraphicsContext::UpdateDarkModeSettingsForTest(
    const DarkModeSettings& settings) {
  dark_mode_filter_ = std::make_unique<DarkModeFilter>(settings);
}

void GraphicsContext::Save() {
  paint_state_->IncrementSaveCount();

  DCHECK(canvas_);
  canvas_->save();
}

void GraphicsContext::Restore() {
  if (!paint_state_index_ && !paint_state_->SaveCount()) {
    DLOG(ERROR) << "ERROR void GraphicsContext::restore() stack is empty";
    return;
  }

  if (paint_state_->SaveCount()) {
    paint_state_->DecrementSaveCount();
  } else {
    paint_state_index_--;
    paint_state_ = paint_state_stack_[paint_state_index_].get();
  }

  DCHECK(canvas_);
  canvas_->restore();
}

#if DCHECK_IS_ON()
unsigned GraphicsContext::SaveCount() const {
  // Each m_paintStateStack entry implies an additional save op
  // (on top of its own saveCount), except for the first frame.
  unsigned count = paint_state_index_;
  DCHECK_GE(paint_state_stack_.size(), paint_state_index_);
  for (unsigned i = 0; i <= paint_state_index_; ++i)
    count += paint_state_stack_[i]->SaveCount();

  return count;
}
#endif

void GraphicsContext::SetInDrawingRecorder(bool val) {
  // Nested drawing recorders are not allowed.
  DCHECK(!val || !in_drawing_recorder_);
  in_drawing_recorder_ = val;
}

void GraphicsContext::SetDOMNodeId(DOMNodeId new_node_id) {
  DCHECK(NeedsDOMNodeId());
  if (canvas_)
    canvas_->setNodeId(new_node_id);

  dom_node_id_ = new_node_id;
}

DOMNodeId GraphicsContext::GetDOMNodeId() const {
  DCHECK(NeedsDOMNodeId());
  return dom_node_id_;
}

void GraphicsContext::SetDrawLooper(sk_sp<cc::DrawLooper> draw_looper) {
  MutableState()->SetDrawLooper(std::move(draw_looper));
}

void GraphicsContext::Concat(const SkM44& matrix) {
  DCHECK(canvas_);
  canvas_->concat(matrix);
}

void GraphicsContext::BeginLayer(float opacity) {
  DCHECK(canvas_);
  canvas_->saveLayerAlphaf(opacity);

#if DCHECK_IS_ON()
  ++layer_count_;
#endif
}

void GraphicsContext::BeginLayer(SkBlendMode xfermode) {
  cc::PaintFlags flags;
  flags.setBlendMode(xfermode);
  BeginLayer(flags);
}

void GraphicsContext::BeginLayer(sk_sp<cc::ColorFilter> color_filter,
                                 const SkBlendMode* blend_mode) {
  cc::PaintFlags flags;
  flags.setColorFilter(std::move(color_filter));
  if (blend_mode) {
    flags.setBlendMode(*blend_mode);
  }
  BeginLayer(flags);
}

void GraphicsContext::BeginLayer(sk_sp<PaintFilter> image_filter) {
  cc::PaintFlags flags;
  flags.setImageFilter(std::move(image_filter));
  BeginLayer(flags);
}

void GraphicsContext::BeginLayer(const cc::PaintFlags& flags) {
  DCHECK(canvas_);
  canvas_->saveLayer(flags);

#if DCHECK_IS_ON()
  ++layer_count_;
#endif
}

void GraphicsContext::EndLayer() {
  DCHECK(canvas_);
  canvas_->restore();

#if DCHECK_IS_ON()
  DCHECK_GT(layer_count_--, 0);
#endif
}

void GraphicsContext::BeginRecording() {
  DCHECK(!canvas_);
  canvas_ = paint_recorder_.beginRecording();
  if (printing_metafile_)
    canvas_->SetPrintingMetafile(printing_metafile_);
  if (paint_preview_tracker_)
    canvas_->SetPaintPreviewTracker(paint_preview_tracker_);
}

PaintRecord GraphicsContext::EndRecording() {
  canvas_->SetPrintingMetafile(nullptr);
  canvas_ = nullptr;
  return paint_recorder_.finishRecordingAsPicture();
}

void GraphicsContext::DrawRecord(PaintRecord record) {
  if (record.empty()) {
    return;
  }

  DCHECK(canvas_);
  canvas_->drawPicture(std::move(record));
}

void GraphicsContext::DrawFocusRingPath(const SkPath& path,
                                        const Color& color,
                                        float width,
                                        float corner_radius,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawPlatformFocusRing(
      path, canvas_, DarkModeColor(*this, color.toSkColor4f(), auto_dark_mode),
      width, corner_radius);
}

void GraphicsContext::DrawFocusRingRect(const SkRRect& rrect,
                                        const Color& color,
                                        float width,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawPlatformFocusRing(
      rrect, canvas_, DarkModeColor(*this, color.toSkColor4f(), auto_dark_mode),
      width);
}

void GraphicsContext::SetPrinting(bool printing) {
  printing_ = printing;
}

void GraphicsContext::SetPrintingInternalHeadersAndFooters(
    bool printing_internal_headers_and_footers) {
  printing_internal_headers_and_footers_ =
      printing_internal_headers_and_footers;
}

void GraphicsContext::DrawText(const Font& font,
                               const TextFragmentPaintInfo& text_info,
                               const gfx::PointF& point,
                               const cc::PaintFlags& flags,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  DarkModeFlags dark_mode_flags(this, auto_dark_mode, flags);
  if (sk_sp<SkTextBlob> text_blob = paint_controller_.CachedTextBlob()) {
    canvas_->drawTextBlob(text_blob, point.x(), point.y(), node_id,
                          dark_mode_flags);
    return;
  }
  font.DrawText(canvas_, text_info, point, node_id, dark_mode_flags,
                printing_ ? Font::DrawType::kGlyphsAndClusters
                          : Font::DrawType::kGlyphsOnly);
}

template <typename DrawTextFunc>
void GraphicsContext::DrawTextPasses(const DrawTextFunc& draw_text) {
  TextDrawingModeFlags mode_flags = TextDrawingMode();

  if (ImmutableState()->GetTextPaintOrder() == kFillStroke) {
    if (mode_flags & kTextModeFill) {
      draw_text(ImmutableState()->FillFlags());
    }
  }

  if ((mode_flags & kTextModeStroke) && StrokeThickness() > 0) {
    cc::PaintFlags stroke_flags(ImmutableState()->StrokeFlags());
    if (mode_flags & kTextModeFill) {
      // shadow was already applied during fill pass
      stroke_flags.setLooper(nullptr);
    }
    draw_text(stroke_flags);
  }

  if (ImmutableState()->GetTextPaintOrder() == kStrokeFill) {
    if (mode_flags & kTextModeFill) {
      draw_text(ImmutableState()->FillFlags());
    }
  }
}

void GraphicsContext::DrawText(const Font& font,
                               const TextFragmentPaintInfo& text_info,
                               const gfx::PointF& point,
                               DOMNodeId node_id,
                               const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses([&](const cc::PaintFlags& flags) {
    DrawText(font, text_info, point, flags, node_id, auto_dark_mode);
  });
}

void GraphicsContext::DrawEmphasisMarks(const Font& font,
                                        const TextFragmentPaintInfo& text_info,
                                        const AtomicString& mark,
                                        const gfx::PointF& point,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses([&](const cc::PaintFlags& flags) {
    font.DrawEmphasisMarks(canvas_, text_info, mark, point,
                           DarkModeFlags(this, auto_dark_mode, flags));
  });
}

void GraphicsContext::DrawBidiText(const Font& font,
                                   const TextRun& run,
                                   const gfx::PointF& point,
                                   const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses([&](const cc::PaintFlags& flags) {
    if (PlainTextPainter::Shared().DrawWithBidiReorder(
            run, 0, run.length(), font, Font::kDoNotPaintIfFontNotReady,
            *canvas_, point, DarkModeFlags(this, auto_dark_mode, flags),
            printing_ ? Font::DrawType::kGlyphsAndClusters
                      : Font::DrawType::kGlyphsOnly)) {
      paint_controller_.SetTextPainted();
    }
  });
}

void GraphicsContext::DrawImage(
    Image& image,
    Image::ImageDecodingMode decode_mode,
    const ImageAutoDarkMode& auto_dark_mode,
    const ImagePaintTimingInfo& paint_timing_info,
    const gfx::RectF& dest,
    const gfx::RectF* src_ptr,
    SkBlendMode op,
    RespectImageOrientationEnum should_respect_image_orientation,
    Image::ImageClampingMode clamping_mode) {
  const gfx::RectF src = src_ptr ? *src_ptr : gfx::RectF(image.Rect());
  cc::PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  image_flags.setColor(SkColors::kBlack);

  SkSamplingOptions sampling = ComputeSamplingOptions(image, dest, src);
  DarkModeFilter* dark_mode_filter = GetDarkModeFilterForImage(auto_dark_mode);
  ImageDrawOptions draw_options(dark_mode_filter, sampling,
                                should_respect_image_orientation, clamping_mode,
                                decode_mode, auto_dark_mode.enabled,
                                paint_timing_info.image_may_be_lcp_candidate);
  image.Draw(canvas_, image_flags, dest, src, draw_options);
  SetImagePainted(paint_timing_info.report_paint_timing);
}
void GraphicsContext::DrawImageRRect(
    Image& image,
    Image::ImageDecodingMode decode_mode,
    const ImageAutoDarkMode& auto_dark_mode,
    const ImagePaintTimingInfo& paint_timing_info,
    const FloatRoundedRect& dest,
    const gfx::RectF& src_rect,
    SkBlendMode op,
    RespectImageOrientationEnum respect_orientation,
    Image::ImageClampingMode clamping_mode) {
  if (!dest.IsRounded()) {
    DrawImage(image, decode_mode, auto_dark_mode, paint_timing_info,
              dest.Rect(), &src_rect, op, respect_orientation, clamping_mode);
    return;
  }

  DCHECK(dest.IsRenderable());

  const gfx::RectF visible_src =
      IntersectRects(src_rect, gfx::RectF(image.Rect()));
  if (dest.IsEmpty() || visible_src.IsEmpty())
    return;

  SkSamplingOptions sampling =
      ComputeSamplingOptions(image, dest.Rect(), src_rect);
  cc::PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  image_flags.setColor(SkColors::kBlack);

  DarkModeFilter* dark_mode_filter = GetDarkModeFilterForImage(auto_dark_mode);
  ImageDrawOptions draw_options(dark_mode_filter, sampling, respect_orientation,
                                clamping_mode, decode_mode,
                                auto_dark_mode.enabled,
                                paint_timing_info.image_may_be_lcp_candidate);

  bool use_shader = (visible_src == src_rect) &&
                    (respect_orientation == kDoNotRespectImageOrientation ||
                     image.HasDefaultOrientation());
  if (use_shader) {
    const SkMatrix local_matrix = SkMatrix::RectToRect(
        gfx::RectFToSkRect(visible_src), gfx::RectFToSkRect(dest.Rect()));
    use_shader =
        image.ApplyShader(image_flags, local_matrix, src_rect, draw_options);
  }

  if (use_shader) {
    // Temporarily set filter-quality for the shader. <reed>
    // Should be replaced with explicit sampling parameter passed to
    // ApplyShader()
    image_flags.setFilterQuality(
        ComputeFilterQuality(image, dest.Rect(), src_rect));
    // Shader-based fast path.
    canvas_->drawRRect(SkRRect(dest), image_flags);
  } else {
    // Clip-based fallback.
    PaintCanvasAutoRestore auto_restore(canvas_, true);
    canvas_->clipRRect(SkRRect(dest), image_flags.isAntiAlias());
    image.Draw(canvas_, image_flags, dest.Rect(), src_rect, draw_options);
  }

  SetImagePainted(paint_timing_info.report_paint_timing);
}

void GraphicsContext::SetImagePainted(bool report_paint_timing) {
  if (!report_paint_timing) {
    return;
  }

  paint_controller_.SetImagePainted();
}

cc::PaintFlags::FilterQuality GraphicsContext::ComputeFilterQuality(
    Image& image,
    const gfx::RectF& dest,
    const gfx::RectF& src) const {
  InterpolationQuality resampling;
  if (printing_) {
    resampling = kInterpolationNone;
  } else if (image.IsLazyDecoded()) {
    resampling = GetDefaultInterpolationQuality();
  } else {
    resampling = ComputeInterpolationQuality(src.size(), dest.size(),
                                             image.FirstFrameIsComplete());

    if (resampling == kInterpolationNone) {
      // FIXME: This is to not break tests (it results in the filter bitmap flag
      // being set to true). We need to decide if we respect InterpolationNone
      // being returned from computeInterpolationQuality.
      resampling = kInterpolationLow;
    }
  }
  return static_cast<cc::PaintFlags::FilterQuality>(
      std::min(resampling, ImageInterpolationQuality()));
}

void GraphicsContext::DrawImageTiled(
    Image& image,
    const gfx::RectF& dest_rect,
    const ImageTilingInfo& tiling_info,
    const ImageAutoDarkMode& auto_dark_mode,
    const ImagePaintTimingInfo& paint_timing_info,
    SkBlendMode op,
    RespectImageOrientationEnum respect_orientation) {
  cc::PaintFlags image_flags = ImmutableState()->FillFlags();
  image_flags.setBlendMode(op);
  SkSamplingOptions sampling = ImageSamplingOptions();
  DarkModeFilter* dark_mode_filter = GetDarkModeFilterForImage(auto_dark_mode);
  ImageDrawOptions draw_options(dark_mode_filter, sampling, respect_orientation,
                                Image::kClampImageToSourceRect,
                                Image::kSyncDecode, auto_dark_mode.enabled,
                                paint_timing_info.image_may_be_lcp_candidate);

  image.DrawPattern(*this, image_flags, dest_rect, tiling_info, draw_options);
  SetImagePainted(paint_timing_info.report_paint_timing);
}

void GraphicsContext::DrawLine(const gfx::PointF& from,
                               const gfx::PointF& to,
                               const cc::PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawLine(from.x(), from.y(), to.x(), to.y(),
                    DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawOval(const SkRect& oval,
                               const cc::PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawOval(oval, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawPath(const SkPath& path,
                               const cc::PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawPath(path, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawRect(const SkRect& rect,
                               const cc::PaintFlags& flags,
                               const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawRect(rect, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::DrawRRect(const SkRRect& rrect,
                                const cc::PaintFlags& flags,
                                const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);
  canvas_->drawRRect(rrect, DarkModeFlags(this, auto_dark_mode, flags));
}

void GraphicsContext::FillPath(const Path& path_to_fill,
                               const AutoDarkMode& auto_dark_mode) {
  if (path_to_fill.IsEmpty())
    return;

  DrawPath(path_to_fill.GetSkPath(), ImmutableState()->FillFlags(),
           auto_dark_mode);
}

void GraphicsContext::StrokePath(const Path& path_to_stroke,
                                 const AutoDarkMode& auto_dark_mode) {
  if (path_to_stroke.IsEmpty()) {
    return;
  }

  DrawPath(path_to_stroke.GetSkPath(), ImmutableState()->StrokeFlags(),
           auto_dark_mode);
}

void GraphicsContext::FillRect(const gfx::Rect& rect,
                               const AutoDarkMode& auto_dark_mode) {
  FillRect(gfx::RectF(rect), auto_dark_mode);
}

void GraphicsContext::FillRect(const gfx::Rect& rect,
                               const Color& color,
                               const AutoDarkMode& auto_dark_mode,
                               SkBlendMode xfer_mode) {
  FillRect(gfx::RectF(rect), color, auto_dark_mode, xfer_mode);
}

void GraphicsContext::FillRect(const gfx::RectF& rect,
                               const AutoDarkMode& auto_dark_mode) {
  DrawRect(gfx::RectFToSkRect(rect), ImmutableState()->FillFlags(),
           auto_dark_mode);
}

void GraphicsContext::FillRect(const gfx::RectF& rect,
                               const Color& color,
                               const AutoDarkMode& auto_dark_mode,
                               SkBlendMode xfer_mode) {
  cc::PaintFlags flags = ImmutableState()->FillFlags();
  flags.setColor(color.toSkColor4f());
  flags.setBlendMode(xfer_mode);

  DrawRect(gfx::RectFToSkRect(rect), flags, auto_dark_mode);
}

void GraphicsContext::FillContouredRect(const ContouredRect& crect,
                                        const Color& color,
                                        const AutoDarkMode& auto_dark_mode) {
  if (crect.HasRoundCurvature()) {
    FillRoundedRect(crect.AsRoundedRect(), color, auto_dark_mode);
    return;
  }
  const cc::PaintFlags& fill_flags = ImmutableState()->FillFlags();
  Path path = crect.GetPath();
  const SkColor4f sk_color = color.toSkColor4f();
  if (sk_color == fill_flags.getColor4f()) {
    DrawPath(path.GetSkPath(), fill_flags, auto_dark_mode);
    return;
  }

  cc::PaintFlags flags = fill_flags;
  flags.setColor(sk_color);

  DrawPath(path.GetSkPath(), flags, auto_dark_mode);
}

void GraphicsContext::FillRoundedRect(const FloatRoundedRect& rrect,
                                      const Color& color,
                                      const AutoDarkMode& auto_dark_mode) {
  if (!rrect.IsRounded() || !rrect.IsRenderable()) {
    FillRect(rrect.Rect(), color, auto_dark_mode);
    return;
  }

  const cc::PaintFlags& fill_flags = ImmutableState()->FillFlags();
  const SkColor4f sk_color = color.toSkColor4f();
  if (sk_color == fill_flags.getColor4f()) {
    DrawRRect(SkRRect(rrect), fill_flags, auto_dark_mode);
    return;
  }

  cc::PaintFlags flags = fill_flags;
  flags.setColor(sk_color);

  DrawRRect(SkRRect(rrect), flags, auto_dark_mode);
}

namespace {

bool IsSimpleDRRect(const FloatRoundedRect& outer,
                    const FloatRoundedRect& inner) {
  // A DRRect is "simple" (i.e. can be drawn as a rrect stroke) if
  //   1) all sides have the same width
  const gfx::Vector2dF stroke_size =
      inner.Rect().origin() - outer.Rect().origin();
  if (!WebCoreFloatNearlyEqual(stroke_size.AspectRatio(), 1) ||
      !WebCoreFloatNearlyEqual(stroke_size.x(),
                               outer.Rect().right() - inner.Rect().right()) ||
      !WebCoreFloatNearlyEqual(stroke_size.y(),
                               outer.Rect().bottom() - inner.Rect().bottom())) {
    return false;
  }

  const auto& is_simple_corner = [&stroke_size](const gfx::SizeF& outer,
                                                const gfx::SizeF& inner) {
    // trivial/zero-radius corner
    if (outer.IsZero() && inner.IsZero())
      return true;

    // and
    //   2) all corners are isotropic
    // and
    //   3) the inner radii are not constrained
    return WebCoreFloatNearlyEqual(outer.width(), outer.height()) &&
           WebCoreFloatNearlyEqual(inner.width(), inner.height()) &&
           WebCoreFloatNearlyEqual(outer.width(),
                                   inner.width() + stroke_size.x());
  };

  const auto& o_radii = outer.GetRadii();
  const auto& i_radii = inner.GetRadii();

  return is_simple_corner(o_radii.TopLeft(), i_radii.TopLeft()) &&
         is_simple_corner(o_radii.TopRight(), i_radii.TopRight()) &&
         is_simple_corner(o_radii.BottomRight(), i_radii.BottomRight()) &&
         is_simple_corner(o_radii.BottomLeft(), i_radii.BottomLeft());
}

}  // anonymous namespace

void GraphicsContext::FillDRRect(const FloatRoundedRect& outer,
                                 const FloatRoundedRect& inner,
                                 const Color& color,
                                 const AutoDarkMode& auto_dark_mode) {
  DCHECK(canvas_);

  const cc::PaintFlags& fill_flags = ImmutableState()->FillFlags();
  const SkColor4f sk_color = color.toSkColor4f();
  if (!IsSimpleDRRect(outer, inner)) {
    if (sk_color == fill_flags.getColor4f()) {
      canvas_->drawDRRect(SkRRect(outer), SkRRect(inner),
                          DarkModeFlags(this, auto_dark_mode, fill_flags));
    } else {
      cc::PaintFlags flags(fill_flags);
      flags.setColor(sk_color);
      canvas_->drawDRRect(SkRRect(outer), SkRRect(inner),
                          DarkModeFlags(this, auto_dark_mode, flags));
    }
    return;
  }

  // We can draw this as a stroked rrect.
  float stroke_width = inner.Rect().x() - outer.Rect().x();
  SkRRect stroke_r_rect(outer);
  stroke_r_rect.inset(stroke_width / 2, stroke_width / 2);

  cc::PaintFlags stroke_flags(fill_flags);
  stroke_flags.setColor(sk_color);
  stroke_flags.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags.setStrokeWidth(stroke_width);

  canvas_->drawRRect(stroke_r_rect,
                     DarkModeFlags(this, auto_dark_mode, stroke_flags));
}

void GraphicsContext::FillRectWithContouredHole(
    const gfx::RectF& rect,
    const ContouredRect& contoured_hole_rect,
    const Color& color,
    const AutoDarkMode& auto_dark_mode) {
  cc::PaintFlags flags(ImmutableState()->FillFlags());
  flags.setColor(color.toSkColor4f());
  const DarkModeFlags dark_mode_flags(this, auto_dark_mode, flags);
  if (contoured_hole_rect.HasRoundCurvature()) {
    canvas_->drawDRRect(SkRRect::MakeRect(gfx::RectFToSkRect(rect)),
                        SkRRect(contoured_hole_rect.AsRoundedRect()),
                        dark_mode_flags);
  } else {
    SkPath path;
    CHECK(Op(SkPath::Rect(gfx::RectFToSkRect(rect)),
             contoured_hole_rect.GetPath().GetSkPath(), kDifference_SkPathOp,
             &path));
    canvas_->drawPath(path, dark_mode_flags);
  }
}

void GraphicsContext::FillEllipse(const gfx::RectF& ellipse,
                                  const AutoDarkMode& auto_dark_mode) {
  DrawOval(gfx::RectFToSkRect(ellipse), ImmutableState()->FillFlags(),
           auto_dark_mode);
}

void GraphicsContext::StrokeEllipse(const gfx::RectF& ellipse,
                                    const AutoDarkMode& auto_dark_mode) {
  DrawOval(gfx::RectFToSkRect(ellipse), ImmutableState()->StrokeFlags(),
           auto_dark_mode);
}

void GraphicsContext::StrokeRect(const gfx::RectF& rect,
                                 const AutoDarkMode& auto_dark_mode) {
  const cc::PaintFlags& flags = ImmutableState()->StrokeFlags();
  // strokerect has special rules for CSS when the rect is degenerate:
  // if width==0 && height==0, do nothing
  // if width==0 || height==0, then just draw line for the other dimension
  SkRect r = gfx::RectFToSkRect(rect);
  bool valid_w = r.width() > 0;
  bool valid_h = r.height() > 0;
  if (valid_w && valid_h) {
    DrawRect(r, flags, auto_dark_mode);
  } else if (valid_w || valid_h) {
    // we are expected to respect the lineJoin, so we can't just call
    // drawLine -- we have to create a path that doubles back on itself.
    const SkPath path = SkPathBuilder()
                            .moveTo(r.fLeft, r.fTop)
                            .lineTo(r.fRight, r.fBottom)
                            .close()
                            .detach();
    DrawPath(path, flags, auto_dark_mode);
  }
}

void GraphicsContext::ClipContouredRect(const ContouredRect& contoured_rect,
                                        SkClipOp clip_op,
                                        AntiAliasingMode should_antialias) {
  if (!contoured_rect.IsRounded()) {
    ClipRect(gfx::RectFToSkRect(contoured_rect.Rect()), should_antialias,
             clip_op);
    return;
  }

  if (contoured_rect.HasRoundCurvature()) {
    ClipRRect(SkRRect(contoured_rect.AsRoundedRect()), should_antialias,
              clip_op);
    return;
  }

  ClipPath(contoured_rect.GetPath().GetSkPath(), should_antialias, clip_op);
}

void GraphicsContext::ClipOutContouredRect(const ContouredRect& rect) {
  ClipContouredRect(rect, SkClipOp::kDifference);
}

void GraphicsContext::ClipRect(const SkRect& rect,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipRect(rect, op, aa == kAntiAliased);
}

void GraphicsContext::ClipPath(const SkPath& path,
                               AntiAliasingMode aa,
                               SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipPath(path, op, aa == kAntiAliased);
}

void GraphicsContext::ClipRRect(const SkRRect& rect,
                                AntiAliasingMode aa,
                                SkClipOp op) {
  DCHECK(canvas_);
  canvas_->clipRRect(rect, op, aa == kAntiAliased);
}

void GraphicsContext::Translate(float x, float y) {
  DCHECK(canvas_);

  if (!x && !y)
    return;

  canvas_->translate(ClampNonFiniteToZero(x), ClampNonFiniteToZero(y));
}

void GraphicsContext::Scale(float x, float y) {
  DCHECK(canvas_);
  canvas_->scale(ClampNonFiniteToZero(x), ClampNonFiniteToZero(y));
}

void GraphicsContext::SetURLForRect(const KURL& link,
                                    const gfx::Rect& dest_rect) {
  DCHECK(canvas_);

  sk_sp<SkData> url(SkData::MakeWithCString(link.GetString().Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::kUrl,
                    gfx::RectToSkRect(dest_rect), std::move(url));
}

void GraphicsContext::SetURLFragmentForRect(const String& dest_name,
                                            const gfx::Rect& rect) {
  DCHECK(canvas_);

  sk_sp<SkData> sk_dest_name(SkData::MakeWithCString(dest_name.Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::kLinkToDestination,
                    gfx::RectToSkRect(rect), std::move(sk_dest_name));
}

void GraphicsContext::SetURLDestinationLocation(const String& name,
                                                const gfx::Point& location) {
  DCHECK(canvas_);

  // Paint previews don't make use of linked destinations.
  if (paint_preview_tracker_)
    return;

  SkRect rect = SkRect::MakeXYWH(location.x(), location.y(), 0, 0);
  sk_sp<SkData> sk_name(SkData::MakeWithCString(name.Utf8().c_str()));
  canvas_->Annotate(cc::PaintCanvas::AnnotationType::kNameDestination, rect,
                    std::move(sk_name));
}

void GraphicsContext::ConcatCTM(const AffineTransform& affine) {
  Concat(affine.ToSkM44());
}

}  // namespace blink
