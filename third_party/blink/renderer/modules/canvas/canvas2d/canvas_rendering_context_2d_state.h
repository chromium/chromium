// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_STATE_H_

#include "base/types/pass_key.h"
#include "cc/paint/paint_flags.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/clip_list.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class BaseRenderingContext2D;
class CanvasRenderingContext2D;
class CanvasFilter;
class CanvasGradient;
class CanvasPattern;
class CSSValue;
class Element;

enum ShadowMode {
  kDrawShadowAndForeground,
  kDrawShadowOnly,
  kDrawForegroundOnly
};

class MODULES_EXPORT CanvasRenderingContext2DState final
    : public GarbageCollected<CanvasRenderingContext2DState>,
      public FontSelectorClient {
 public:
  enum ClipListCopyMode { kCopyClipList, kDontCopyClipList };
  // SaveType indicates whether the state was pushed to the state stack by Save
  // or by BeginLayer. The first state on the state stack, which is created in
  // the canvas constructor and not by Save or BeginLayer, has SaveType
  // kInitial. In some circumpstances we have to split an endlayer into two
  // 'states', we use the kExtraState for that.
  enum class SaveType {
    kSaveRestore,
    kBeginEndLayerOneSave,
    kBeginEndLayerTwoSaves,
    kInitial
  };

  CanvasRenderingContext2DState();
  CanvasRenderingContext2DState(const CanvasRenderingContext2DState&,
                                ClipListCopyMode,
                                SaveType);

  CanvasRenderingContext2DState(const CanvasRenderingContext2DState&) = delete;
  CanvasRenderingContext2DState& operator=(
      const CanvasRenderingContext2DState&) = delete;

  ~CanvasRenderingContext2DState() override;

  void Trace(Visitor*) const override;

  enum PaintType {
    kFillPaintType,
    kStrokePaintType,
    kImagePaintType,
  };

  enum ImageType { kNoImage, kOpaqueImage, kNonOpaqueImage };

  // FontSelectorClient implementation
  void FontsNeedUpdate(FontSelector*, FontInvalidationReason) override;

  void SetLineDash(const Vector<double>&);
  const Vector<double>& LineDash() const { return line_dash_; }

  void SetShouldAntialias(bool);
  bool ShouldAntialias() const;

  void SetLineDashOffset(double);
  double LineDashOffset() const { return line_dash_offset_; }

  void SetTransform(const AffineTransform&);
  void ResetTransform();
  AffineTransform GetTransform() const { return transform_; }
  bool IsTransformInvertible() const { return is_transform_invertible_; }

  void ClipPath(const SkPath&, AntiAliasingMode);
  bool HasClip() const { return has_clip_; }
  bool HasComplexClip() const { return has_complex_clip_; }
  void PlaybackClips(cc::PaintCanvas* canvas) const {
    clip_list_.Playback(canvas);
  }
  SkPath IntersectPathWithClip(const SkPath& path) const {
    return clip_list_.IntersectPathWithClip(path);
  }

  void SetFont(const FontDescription&, FontSelector*);
  bool IsFontDirtyForFilter() const;
  const Font& GetFont() const;
  const FontDescription& GetFontDescription() const;
  inline bool HasRealizedFont() const { return realized_font_; }
  void SetUnparsedFont(const String& font) { unparsed_font_ = font; }
  const String& UnparsedFont() const { return unparsed_font_; }

  void SetFontForFilter(const Font& font) { font_for_filter_ = font; }

  void SetCSSFilter(const CSSValue*);
  void SetUnparsedCSSFilter(const String& filter_string) {
    unparsed_css_filter_ = filter_string;
  }
  const String& UnparsedCSSFilter() const { return unparsed_css_filter_; }
  void SetCanvasFilter(CanvasFilter* filter_value);
  CanvasFilter* GetCanvasFilter() const { return canvas_filter_; }
  sk_sp<PaintFilter> GetFilter(Element*,
                               gfx::Size canvas_size,
                               CanvasRenderingContext2D*);
  sk_sp<PaintFilter> GetFilterForOffscreenCanvas(gfx::Size canvas_size,
                                                 BaseRenderingContext2D*);
  ALWAYS_INLINE bool IsFilterUnresolved() const {
    return filter_state_ == FilterState::kUnresolved;
  }
  ALWAYS_INLINE bool IsFilterResolved() const {
    return filter_state_ == FilterState::kResolved;
  }

  void ClearResolvedFilter();
  void ValidateFilterState() const;

  void SetLayerFilter(sk_sp<PaintFilter> filter) {
    layer_filter_ = std::move(filter);
  }
  sk_sp<PaintFilter> GetLayerFilter() const { return layer_filter_; }
  bool HasLayerFilter() const { return layer_filter_ != nullptr; }

  void SetStrokeColor(Color color) {
    if (stroke_style_.SetColor(color)) {
      stroke_style_.ApplyColorToFlags(stroke_flags_, global_alpha_);
    }
  }
  void SetStrokePattern(CanvasPattern* pattern) {
    stroke_style_.SetPattern(pattern);
  }
  void SetStrokeGradient(CanvasGradient* gradient) {
    stroke_style_.SetGradient(gradient);
  }
  const CanvasStyle& StrokeStyle() const { return stroke_style_; }

  void SetFillColor(Color color) {
    if (fill_style_.SetColor(color)) {
      fill_style_.ApplyColorToFlags(fill_flags_, global_alpha_);
    }
  }
  void SetFillPattern(CanvasPattern* pattern) {
    fill_style_.SetPattern(pattern);
  }
  void SetFillGradient(CanvasGradient* gradient) {
    fill_style_.SetGradient(gradient);
  }
  const CanvasStyle& FillStyle() const { return fill_style_; }

  // Prefer to use Style() over StrokeStyle() and FillStyle()
  // if properties of CanvasStyle are concerned
  const CanvasStyle& Style(PaintType type) const {
    // Using DCHECK below because this is a critical hotspot.
    DCHECK(type != kImagePaintType);
    return type == kStrokePaintType ? stroke_style_ : fill_style_;
  }

  // Check the pattern in StrokeStyle or FillStyle depending on the PaintType
  bool HasPattern(PaintType type) const {
    CanvasPattern* pattern = Style(type).GetCanvasPattern();
    return pattern != nullptr && pattern->GetPattern() != nullptr;
  }

  // Only to be used if the CanvasRenderingContext2DState has Pattern
  // Pattern is in either StrokeStyle or FillStyle depending on the PaintType
  bool PatternIsAccelerated(PaintType type) const {
    // Using DCHECK here because condition is somewhat tautological and
    // provides little added value to Release builds
    DCHECK(HasPattern(type));
    return Style(type).GetCanvasPattern()->GetPattern()->IsTextureBacked();
  }

  enum Direction { kDirectionInherit, kDirectionRTL, kDirectionLTR };

  void SetDirection(Direction direction) { direction_ = direction; }
  Direction GetDirection() const { return direction_; }

  void SetTextAlign(TextAlign align) { text_align_ = align; }
  TextAlign GetTextAlign() const { return text_align_; }

  void SetTextBaseline(TextBaseline baseline) { text_baseline_ = baseline; }
  TextBaseline GetTextBaseline() const { return text_baseline_; }

  void SetLetterSpacing(const String& letter_spacing);
  String GetLetterSpacing() const { return parsed_letter_spacing_; }

  void SetWordSpacing(const String& word_spacing);
  String GetWordSpacing() const { return parsed_word_spacing_; }

  void SetTextRendering(TextRenderingMode text_rendering,
                        FontSelector* selector);
  TextRenderingMode GetTextRendering() const { return text_rendering_mode_; }

  void SetFontKerning(FontDescription::Kerning font_kerning,
                      FontSelector* selector);
  FontDescription::Kerning GetFontKerning() const { return font_kerning_; }

  void SetFontStretch(FontSelectionValue font_stretch, FontSelector* selector);
  FontSelectionValue GetFontStretch() const { return font_stretch_; }

  void SetFontVariantCaps(FontDescription::FontVariantCaps font_kerning,
                          FontSelector* selector);
  FontDescription::FontVariantCaps GetFontVariantCaps() const {
    return font_variant_caps_;
  }

  void SetLineWidth(double line_width) {
    stroke_flags_.setStrokeWidth(ClampTo<float>(line_width));
  }
  double LineWidth() const { return stroke_flags_.getStrokeWidth(); }

  void SetLineCap(LineCap line_cap) {
    stroke_flags_.setStrokeCap(static_cast<cc::PaintFlags::Cap>(line_cap));
  }
  LineCap GetLineCap() const {
    return static_cast<LineCap>(stroke_flags_.getStrokeCap());
  }

  void SetLineJoin(LineJoin line_join) {
    stroke_flags_.setStrokeJoin(static_cast<cc::PaintFlags::Join>(line_join));
  }
  LineJoin GetLineJoin() const {
    return static_cast<LineJoin>(stroke_flags_.getStrokeJoin());
  }

  void SetMiterLimit(double miter_limit) {
    stroke_flags_.setStrokeMiter(ClampTo<float>(miter_limit));
  }
  double MiterLimit() const { return stroke_flags_.getStrokeMiter(); }

  void SetShadowOffsetX(double);
  void SetShadowOffsetY(double);
  const gfx::Vector2dF& ShadowOffset() const { return shadow_offset_; }

  void SetShadowBlur(double);
  double ShadowBlur() const { return shadow_blur_; }

  void SetShadowColor(Color);
  Color ShadowColor() const { return shadow_color_; }

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

  // If paint will not be used for painting a bitmap, set bitmapOpacity to
  // Opaque.
  const cc::PaintFlags* GetFlags(PaintType,
                                 ShadowMode,
                                 ImageType = kNoImage) const;

  SaveType GetSaveType() const { return save_type_; }
  bool IsLayerSaveType() const {
    return save_type_ == SaveType::kBeginEndLayerOneSave ||
           save_type_ == SaveType::kBeginEndLayerTwoSaves;
  }

  sk_sp<PaintFilter>& ShadowAndForegroundImageFilter() const;

 private:
  void UpdateLineDash() const;
  void UpdateFilterQuality() const;
  void UpdateFilterQuality(cc::PaintFlags::FilterQuality) const;
  void ShadowParameterChanged();
  sk_sp<SkDrawLooper>& EmptyDrawLooper() const;
  sk_sp<SkDrawLooper>& ShadowOnlyDrawLooper() const;
  sk_sp<SkDrawLooper>& ShadowAndForegroundDrawLooper() const;
  sk_sp<PaintFilter>& ShadowOnlyImageFilter() const;

  String unparsed_stroke_color_;
  String unparsed_fill_color_;
  CanvasStyle stroke_style_;
  CanvasStyle fill_style_;

  mutable cc::PaintFlags stroke_flags_;
  mutable cc::PaintFlags fill_flags_;
  mutable cc::PaintFlags image_flags_;

  gfx::Vector2dF shadow_offset_;
  double shadow_blur_;
  Color shadow_color_;
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

  enum class FilterState {
    kNone,
    kUnresolved,
    kResolved,
    kInvalid,
  };
  FilterState filter_state_ = FilterState::kNone;
  Member<CanvasFilter> canvas_filter_;
  String unparsed_css_filter_;
  Member<const CSSValue> css_filter_value_;
  sk_sp<PaintFilter> resolved_filter_;
  sk_sp<PaintFilter> layer_filter_;

  // Text state.
  TextAlign text_align_;
  TextBaseline text_baseline_{kAlphabeticTextBaseline};
  Direction direction_{kDirectionInherit};
  float letter_spacing_{0};
  CSSPrimitiveValue::UnitType letter_spacing_unit_{
      CSSPrimitiveValue::UnitType::kPixels};
  String parsed_letter_spacing_;

  float word_spacing_{0};
  CSSPrimitiveValue::UnitType word_spacing_unit_{
      CSSPrimitiveValue::UnitType::kPixels};
  String parsed_word_spacing_;
  TextRenderingMode text_rendering_mode_{TextRenderingMode::kAutoTextRendering};
  FontDescription::Kerning font_kerning_{FontDescription::kAutoKerning};
  FontSelectionValue font_stretch_{NormalWidthValue()};
  FontDescription::FontVariantCaps font_variant_caps_{
      FontDescription::kCapsNormal};

  bool realized_font_ : 1;
  bool is_transform_invertible_ : 1;
  bool has_clip_ : 1;
  bool has_complex_clip_ : 1;
  bool letter_spacing_is_set_ : 1;
  bool word_spacing_is_set_ : 1;
  mutable bool line_dash_dirty_ : 1;

  bool image_smoothing_enabled_;
  cc::PaintFlags::FilterQuality image_smoothing_quality_;

  ClipList clip_list_;

  const SaveType save_type_ = SaveType::kInitial;
};

ALWAYS_INLINE bool CanvasRenderingContext2DState::ShouldDrawShadows() const {
  return (!shadow_color_.IsFullyTransparent()) &&
         (shadow_blur_ || !shadow_offset_.IsZero());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_RENDERING_CONTEXT_2D_STATE_H_
