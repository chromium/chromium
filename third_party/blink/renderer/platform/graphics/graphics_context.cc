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
#include "third_party/blink/renderer/platform/fonts/text_run_paint_info.h"
#include "third_party/blink/renderer/platform/geometry/float_rounded_rect.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/graphics/stroke_data.h"
#include "third_party/blink/renderer/platform/graphics/styled_stroke_data.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
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

static void EnforceDotsAtEndpoints(GraphicsContext& context,
                                   gfx::PointF& p1,
                                   gfx::PointF& p2,
                                   const int path_length,
                                   const int width,
                                   const cc::PaintFlags& flags,
                                   const bool is_vertical_line,
                                   const AutoDarkMode& auto_dark_mode) {
  // For narrow lines, we always want integral dot and dash sizes, and start
  // and end points, to prevent anti-aliasing from erasing the dot effect.
  // For 1-pixel wide lines, we must make one end a dash. Otherwise we have
  // a little more scope to distribute the error. But we never want to reduce
  // the size of the end dots because doing so makes corners of all-dotted
  // paths look odd.
  //
  // There is no way to give custom start and end dash sizes or gaps to Skia,
  // so if we need non-uniform gaps we need to draw the start, and maybe the
  // end dot ourselves, and move the line start (and end) to the start/end of
  // the second dot.
  DCHECK_LE(width, 3);  // Width is max 3 according to StrokeIsDashed
  int mod_4 = path_length % 4;
  int mod_6 = path_length % 6;
  // New start dot to be explicitly drawn, if needed, and the amount to grow the
  // start dot and the offset for first gap.
  bool use_start_dot = false;
  int start_dot_growth = 0;
  int start_line_offset = 0;
  // New end dot to be explicitly drawn, if needed, and the amount to grow the
  // second dot.
  bool use_end_dot = false;
  int end_dot_growth = 0;
  if ((width == 1 && path_length % 2 == 0) || (width == 3 && mod_6 == 0)) {
    // Cases where we add one pixel to the first dot.
    use_start_dot = true;
    start_dot_growth = 1;
    start_line_offset = 1;
  }
  if ((width == 2 && (mod_4 == 0 || mod_4 == 1)) ||
      (width == 3 && (mod_6 == 1 || mod_6 == 2))) {
    // Cases where we drop 1 pixel from the start gap
    use_start_dot = true;
    start_line_offset = -1;
  }
  if ((width == 2 && mod_4 == 0) || (width == 3 && mod_6 == 1)) {
    // Cases where we drop 1 pixel from the end gap
    use_end_dot = true;
  }
  if ((width == 2 && mod_4 == 3) ||
      (width == 3 && (mod_6 == 4 || mod_6 == 5))) {
    // Cases where we add 1 pixel to the start gap
    use_start_dot = true;
    start_line_offset = 1;
  }
  if (width == 3 && mod_6 == 5) {
    // Case where we add 1 pixel to the end gap and leave the end
    // dot the same size.
    use_end_dot = true;
  } else if (width == 3 && mod_6 == 0) {
    // Case where we add one pixel gap and one pixel to the dot at the end
    use_end_dot = true;
    end_dot_growth = 1;  // Moves the larger end pt for this case
  }

  if (use_start_dot || use_end_dot) {
    cc::PaintFlags fill_flags;
    fill_flags.setColor(flags.getColor4f());
    if (use_start_dot) {
      SkRect start_dot;
      if (is_vertical_line) {
        start_dot.setLTRB(p1.x() - width / 2, p1.y(),
                          p1.x() + width - width / 2,
                          p1.y() + width + start_dot_growth);
        p1.set_y(p1.y() + (2 * width + start_line_offset));
      } else {
        start_dot.setLTRB(p1.x(), p1.y() - width / 2,
                          p1.x() + width + start_dot_growth,
                          p1.y() + width - width / 2);
        p1.set_x(p1.x() + (2 * width + start_line_offset));
      }
      context.DrawRect(start_dot, fill_flags, auto_dark_mode);
    }
    if (use_end_dot) {
      SkRect end_dot;
      if (is_vertical_line) {
        end_dot.setLTRB(p2.x() - width / 2, p2.y() - width - end_dot_growth,
                        p2.x() + width - width / 2, p2.y());
        // Be sure to stop drawing before we get to the last dot
        p2.set_y(p2.y() - (width + end_dot_growth + 1));
      } else {
        end_dot.setLTRB(p2.x() - width - end_dot_growth, p2.y() - width / 2,
                        p2.x(), p2.y() + width - width / 2);
        // Be sure to stop drawing before we get to the last dot
        p2.set_x(p2.x() - (width + end_dot_growth + 1));
      }
      context.DrawRect(end_dot, fill_flags, auto_dark_mode);
    }
  }
}

void GraphicsContext::DrawLine(const gfx::Point& point1,
                               const gfx::Point& point2,
                               const StyledStrokeData& styled_stroke,
                               const AutoDarkMode& auto_dark_mode,
                               bool is_text_line,
                               const cc::PaintFlags* paint_flags) {
  DCHECK(canvas_);

  StrokeStyle pen_style = styled_stroke.Style();
  if (pen_style == kNoStroke)
    return;

  gfx::PointF p1 = gfx::PointF(point1);
  gfx::PointF p2 = gfx::PointF(point2);
  bool is_vertical_line = (p1.x() == p2.x());
  int width = roundf(styled_stroke.Thickness());

  // We know these are vertical or horizontal lines, so the length will just
  // be the sum of the displacement component vectors give or take 1 -
  // probably worth the speed up of no square root, which also won't be exact.
  gfx::Vector2dF disp = p2 - p1;
  int length = SkScalarRoundToInt(disp.x() + disp.y());
  cc::PaintFlags flags =
      paint_flags ? *paint_flags : ImmutableState()->StrokeFlags();
  styled_stroke.SetupPaint(&flags, {length, width, false});

  if (pen_style == kDottedStroke) {
    if (StyledStrokeData::StrokeIsDashed(width, pen_style)) {
      // When the length of the line is an odd multiple of the width, things
      // work well because we get dots at each end of the line, but if the
      // length is anything else, we get gaps or partial dots at the end of the
      // line. Fix that by explicitly enforcing full dots at the ends of lines.
      // Note that we don't enforce end points when it's text line as enforcing
      // is to improve border line quality.
      if (!is_text_line) {
        EnforceDotsAtEndpoints(*this, p1, p2, length, width, flags,
                               is_vertical_line, auto_dark_mode);
      }
    } else {
      // We draw thick dotted lines with 0 length dash strokes and round
      // endcaps, producing circles. The endcaps extend beyond the line's
      // endpoints, so move the start and end in.
      if (is_vertical_line) {
        p1.set_y(p1.y() + width / 2.f);
        p2.set_y(p2.y() - width / 2.f);
      } else {
        p1.set_x(p1.x() + width / 2.f);
        p2.set_x(p2.x() - width / 2.f);
      }
    }
  }

  AdjustLineToPixelBoundaries(p1, p2, width);
  DrawLine(p1, p2, flags, auto_dark_mode);
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

template <typename TextPaintInfo>
void GraphicsContext::DrawEmphasisMarksInternal(
    const Font& font,
    const TextPaintInfo& text_info,
    const AtomicString& mark,
    const gfx::PointF& point,
    const AutoDarkMode& auto_dark_mode) {
  DrawTextPasses([&](const cc::PaintFlags& flags) {
    font.DrawEmphasisMarks(canvas_, text_info, mark, point,
                           DarkModeFlags(this, auto_dark_mode, flags));
  });
}

void GraphicsContext::DrawEmphasisMarks(const Font& font,
                                        const TextRunPaintInfo& text_info,
                                        const AtomicString& mark,
                                        const gfx::PointF& point,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawEmphasisMarksInternal(font, text_info, mark, point, auto_dark_mode);
}

void GraphicsContext::DrawEmphasisMarks(const Font& font,
                                        const TextFragmentPaintInfo& text_info,
                                        const AtomicString& mark,
                                        const gfx::PointF& point,
                                        const AutoDarkMode& auto_dark_mode) {
  DrawEmphasisMarksInternal(font, text_info, mark, point, auto_dark_mode);
}

void GraphicsContext::DrawBidiText(
    const Font& font,
    const TextRunPaintInfo& run_info,
    const gfx::PointF& point,
    const AutoDarkMode& auto_dark_mode,
    Font::CustomFontNotReadyAction custom_font_not_ready_action) {
  DrawTextPasses([&](const cc::PaintFlags& flags) {
    if (font.DrawBidiText(canvas_, run_info, point,
                          custom_font_not_ready_action,
                          DarkModeFlags(this, auto_dark_mode, flags),
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
  } else if (image.CurrentFrameIsLazyDecoded()) {
    resampling = kInterpolationDefault;
  } else {
    resampling = ComputeInterpolationQuality(
        SkScalarToFloat(src.width()), SkScalarToFloat(src.height()),
        SkScalarToFloat(dest.width()), SkScalarToFloat(dest.height()),
        image.CurrentFrameIsComplete());

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

void GraphicsContext::FillRectWithRoundedHole(
    const gfx::RectF& rect,
    const FloatRoundedRect& rounded_hole_rect,
    const Color& color,
    const AutoDarkMode& auto_dark_mode) {
  cc::PaintFlags flags(ImmutableState()->FillFlags());
  flags.setColor(color.toSkColor4f());
  canvas_->drawDRRect(SkRRect::MakeRect(gfx::RectFToSkRect(rect)),
                      SkRRect(rounded_hole_rect),
                      DarkModeFlags(this, auto_dark_mode, flags));
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
    SkPath path;
    path.moveTo(r.fLeft, r.fTop);
    path.lineTo(r.fRight, r.fBottom);
    path.close();
    DrawPath(path, flags, auto_dark_mode);
  }
}

void GraphicsContext::ClipRoundedRect(const FloatRoundedRect& rrect,
                                      SkClipOp clip_op,
                                      AntiAliasingMode should_antialias) {
  if (!rrect.IsRounded()) {
    ClipRect(gfx::RectFToSkRect(rrect.Rect()), should_antialias, clip_op);
    return;
  }

  ClipRRect(SkRRect(rrect), should_antialias, clip_op);
}

void GraphicsContext::ClipOutRoundedRect(const FloatRoundedRect& rect) {
  ClipRoundedRect(rect, SkClipOp::kDifference);
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

  canvas_->translate(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
}

void GraphicsContext::Scale(float x, float y) {
  DCHECK(canvas_);
  canvas_->scale(WebCoreFloatToSkScalar(x), WebCoreFloatToSkScalar(y));
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
  Concat(AffineTransformToSkM44(affine));
}

void GraphicsContext::AdjustLineToPixelBoundaries(gfx::PointF& p1,
                                                  gfx::PointF& p2,
                                                  float stroke_width) {
  // For odd widths, we add in 0.5 to the appropriate x/y so that the float
  // arithmetic works out.  For example, with a border width of 3, painting will
  // pass us (y1+y2)/2, e.g., (50+53)/2 = 103/2 = 51 when we want 51.5.  It is
  // always true that an even width gave us a perfect position, but an odd width
  // gave us a position that is off by exactly 0.5.
  if (static_cast<int>(stroke_width) % 2) {  // odd
    if (p1.x() == p2.x()) {
      // We're a vertical line.  Adjust our x.
      p1.set_x(p1.x() + 0.5f);
      p2.set_x(p2.x() + 0.5f);
    } else {
      // We're a horizontal line. Adjust our y.
      p1.set_y(p1.y() + 0.5f);
      p2.set_y(p2.y() + 0.5f);
    }
  }
}

}  // namespace blink
