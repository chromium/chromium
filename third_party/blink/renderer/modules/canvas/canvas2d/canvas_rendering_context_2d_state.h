// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_STATE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/clip_list.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class BaseRenderingContext2D;
class CanvasRenderingContext2D;
class CanvasStyle;
class CSSValue;
class Element;

class CanvasRenderingContext2DState final
    : public GarbageCollected<CanvasRenderingContext2DState>,
      public FontSelectorClient {
  USING_GARBAGE_COLLECTED_MIXIN(CanvasRenderingContext2DState);

 public:
  enum ClipListCopyMode { kCopyClipList, kDontCopyClipList };

  CanvasRenderingContext2DState();
  CanvasRenderingContext2DState(const CanvasRenderingContext2DState&,
                                ClipListCopyMode);
  ~CanvasRenderingContext2DState() override;

  void Trace(blink::Visitor*) override;

  enum PaintType {
    kFillPaintType,
    kStrokePaintType,
    kImagePaintType,
  };

  static CanvasRenderingContext2DState* Create(
      const CanvasRenderingContext2DState& other,
      ClipListCopyMode mode) {
    return MakeGarbageCollected<CanvasRenderingContext2DState>(other, mode);
  }

  // FontSelectorClient implementation
  void FontsNeedUpdate(FontSelector*) override;

  bool HasUnrealizedSaves() const { return unrealized_save_count_; }
  void Save() { ++unrealized_save_count_; }
  void Restore() { --unrealized_save_count_; }
  void ResetUnrealizedSaveCount() { unrealized_save_count_ = 0; }

  void SetLineDash(const Vector<double>&);
  const Vector<double>& LineDash() const { return line_dash_; }

  void SetShouldAntialias(bool);
  bool ShouldAntialias() const;

  void SetLineDashOffset(double);
  double LineDashOffset() const { return line_dash_offset_; }

  // setTransform returns true iff transform is invertible;
  void SetTransform(const AffineTransform&);
  void ResetTransform();
  AffineTransform Transform() const { return transform_; }
  bool IsTransformInvertible() const { return is_transform_invertible_; }

  void ClipPath(const SkPath&, AntiAliasingMode);
  bool HasClip() const { return has_clip_; }
  bool HasComplexClip() const { return has_complex_clip_; }
  void PlaybackClips(cc::PaintCanvas* canvas) const {
    clip_list_.Playback(canvas);
  }
  const SkPath& GetCurrentClipPath() const {
    return clip_list_.GetCurrentClipPath();
  }

  void SetFont(const Font&, FontSelector*);
  const Font& GetFont() const;
  bool HasRealizedFont() const { return realized_font_; }
  void SetUnparsedFont(const String& font) { unparsed_font_ = font; }
  const String& UnparsedFont() const { return unparsed_font_; }

  void SetFontForFilter(const Font& font) { font_for_filter_ = font; }

  void SetFilter(const CSSValue*);
  void SetUnparsedFilter(const String& filter_string) {
    unparsed_filter_ = filter_string;
  }
  const String& UnparsedFilter() const { return unparsed_filter_; }
  sk_sp<PaintFilter> GetFilter(Element*,
                               IntSize canvas_size,
                               CanvasRenderingContext2D*) const;
  sk_sp<PaintFilter> GetFilterForOffscreenCanvas(IntSize canvas_size,
                                                 BaseRenderingContext2D*) const;
  bool HasFilterForOffscreenCanvas(IntSize canvas_size,
                                   BaseRenderingContext2D*) const;
  bool HasFilter(Element*,
                 IntSize canvas_size,
                 CanvasRenderingContext2D*) const;
  void ClearResolvedFilter() const;

  void SetStrokeStyle(CanvasStyle*);
  CanvasStyle* StrokeStyle() const { return stroke_style_.Get(); }

  void SetFillStyle(CanvasStyle*);
  CanvasStyle* FillStyle() const { return fill_style_.Get(); }

  CanvasStyle* Style(PaintType) const;

  bool HasPattern() const;

  // Only to be used if the CanvasRenderingContext2DState has Pattern
  bool PatternIsAccelerated() const;

  enum Direction { kDirectionInherit, kDirectionRTL, kDirectionLTR };

  void SetDirection(Direction direction) { direction_ = direction; }
  Direction GetDirection() const { return direction_; }

  void SetTextAlign(TextAlign align) { text_align_ = align; }
  TextAlign GetTextAlign() const { return text_align_; }

  void SetTextBaseline(TextBaseline baseline) { text_baseline_ = baseline; }
  TextBaseline GetTextBaseline() const { return text_baseline_; }

  void SetLineWidth(double line_width) {
    stroke_flags_.setStrokeWidth(clampTo<float>(line_width));
  }
  double LineWidth() const { return stroke_flags_.getStrokeWidth(); }

  void SetLineCap(LineCap line_cap) {
    stroke_flags_.setStrokeCap(static_cast<PaintFlags::Cap>(line_cap));
  }
  LineCap GetLineCap() const {
    return static_cast<LineCap>(stroke_flags_.getStrokeCap());
  }

  void SetLineJoin(LineJoin line_join) {
    stroke_flags_.setStrokeJoin(static_cast<PaintFlags::Join>(line_join));
  }
  LineJoin GetLineJoin() const {
    return static_cast<LineJoin>(stroke_flags_.getStrokeJoin());
  }

  void SetMiterLimit(double miter_limit) {
    stroke_flags_.setStrokeMiter(clampTo<float>(miter_limit));
  }
  double MiterLimit() const { return stroke_flags_.getStrokeMiter(); }

  void SetShadowOffsetX(double);
  void SetShadowOffsetY(double);
  const FloatSize& ShadowOffset() const { return shadow_offset_; }

  void SetShadowBlur(double);
  double ShadowBlur() const { return shadow_blur_; }

  void SetShadowColor(SkColor);
  SkColor ShadowColor() const { return shadow_color_; }

  void SetGlobalAlpha(double);
  double GlobalAlpha() const { return global_alpha_; }

  void SetGlobalComposite(SkBlendMode);
  SkBlendMode GlobalComposite() const;

  void SetImageSmoothingEnabled(bool);
  bool ImageSmoothingEnabled() const;
  void SetImageSmoothingQuality(const String&);
  String ImageSmoothingQuality() const;

  void SetUnparsedStrokeColor(const String& color) {
    unparsed_stroke_color_ = color;
  }
  const String& UnparsedStrokeColor() const { return unparsed_stroke_color_; }

  void SetUnparsedFillColor(const String& color) {
    unparsed_fill_color_ = color;
  }
  const String& UnparsedFillColor() const { return unparsed_fill_color_; }

  bool ShouldDrawShadows() const;

  enum ImageType { kNoImage, kOpaqueImage, kNonOpaqueImage };

  // If paint will not be used for painting a bitmap, set bitmapOpacity to
  // Opaque.
  const PaintFlags* GetFlags(PaintType, ShadowMode, ImageType = kNoImage) const;

 private:
  void UpdateLineDash() const;
  void UpdateStrokeStyle() const;
  void UpdateFillStyle() const;
  void UpdateFilterQuality() const;
  void UpdateFilterQualityWithSkFilterQuality(const SkFilterQuality&) const;
  void ShadowParameterChanged();
  SkDrawLooper* EmptyDrawLooper() const;
  SkDrawLooper* ShadowOnlyDrawLooper() const;
  SkDrawLooper* ShadowAndForegroundDrawLooper() const;
  sk_sp<PaintFilter> ShadowOnlyImageFilter() const;
  sk_sp<PaintFilter> ShadowAndForegroundImageFilter() const;

  unsigned unrealized_save_count_;

  String unparsed_stroke_color_;
  String unparsed_fill_color_;
  Member<CanvasStyle> stroke_style_;
  Member<CanvasStyle> fill_style_;

  mutable PaintFlags stroke_flags_;
  mutable PaintFlags fill_flags_;
  mutable PaintFlags image_flags_;

  FloatSize shadow_offset_;
  double shadow_blur_;
  SkColor shadow_color_;
  mutable sk_sp<SkDrawLooper> empty_draw_looper_;
  mutable sk_sp<SkDrawLooper> shadow_only_draw_looper_;
  mutable sk_sp<SkDrawLooper> shadow_and_foreground_draw_looper_;
  mutable sk_sp<PaintFilter> shadow_only_image_filter_;
  mutable sk_sp<PaintFilter> shadow_and_foreground_image_filter_;

  double global_alpha_;
  AffineTransform transform_;
  Vector<double> line_dash_;
  double line_dash_offset_;

  String unparsed_font_;
  Font font_;
  Font font_for_filter_;

  String unparsed_filter_;
  Member<const CSSValue> filter_value_;
  mutable sk_sp<PaintFilter> resolved_filter_;

  // Text state.
  TextAlign text_align_;
  TextBaseline text_baseline_;
  Direction direction_;

  bool realized_font_ : 1;
  bool is_transform_invertible_ : 1;
  bool has_clip_ : 1;
  bool has_complex_clip_ : 1;
  mutable bool fill_style_dirty_ : 1;
  mutable bool stroke_style_dirty_ : 1;
  mutable bool line_dash_dirty_ : 1;

  bool image_smoothing_enabled_;
  SkFilterQuality image_smoothing_quality_;

  ClipList clip_list_;

  DISALLOW_COPY_AND_ASSIGN(CanvasRenderingContext2DState);
};

}  // namespace blink

#endif
