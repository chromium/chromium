// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/css/css_numeric_literal_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/geometry/length.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"

static const char defaultFont[] = "10px sans-serif";
static const char defaultFilter[] = "none";
static const char defaultSpacing[] = "0px";

namespace blink {

// Convert CSS Length String to a number with unit, ex: "2em" to
// |number_spacing| = 2 and |unit| = CSSPrimitiveValue::UnitType::kEm. It
// returns true if the conversion succeeded; false otherwise.
bool StringToNumWithUnit(String spacing,
                         float* number_spacing,
                         CSSPrimitiveValue::UnitType* unit) {
  CSSTokenizer tokenizer(spacing);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range(tokens);
  // If we failed to parse token, return immediately.
  if (range.AtEnd())
    return false;

  const CSSParserToken* result = range.begin();
  range.Consume();
  // If there is more than 1 dimension token or |spacing| is not a valid
  // dimension token, or unit is not a valid CSS length unit, return
  // immediately.
  if (!range.AtEnd() || result->GetType() != kDimensionToken ||
      !CSSPrimitiveValue::IsLength(result->GetUnitType())) {
    return false;
  }
  *number_spacing = result->NumericValue();
  *unit = result->GetUnitType();
  return true;
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState()
    : stroke_style_(MakeGarbageCollected<CanvasStyle>(Color::kBlack)),
      fill_style_(MakeGarbageCollected<CanvasStyle>(Color::kBlack)),
      shadow_blur_(0.0),
      shadow_color_(Color::kTransparent),
      global_alpha_(1.0),
      line_dash_offset_(0.0),
      unparsed_font_(defaultFont),
      unparsed_css_filter_(defaultFilter),
      text_align_(kStartTextAlign),
      parsed_letter_spacing_(defaultSpacing),
      parsed_word_spacing_(defaultSpacing),
      realized_font_(false),
      is_transform_invertible_(true),
      has_clip_(false),
      has_complex_clip_(false),
      letter_spacing_is_set_(false),
      word_spacing_is_set_(false),
      fill_style_dirty_(true),
      stroke_style_dirty_(true),
      line_dash_dirty_(false),
      image_smoothing_quality_(cc::PaintFlags::FilterQuality::kLow) {
  fill_flags_.setStyle(cc::PaintFlags::kFill_Style);
  fill_flags_.setAntiAlias(true);
  image_flags_.setStyle(cc::PaintFlags::kFill_Style);
  image_flags_.setAntiAlias(true);
  stroke_flags_.setStyle(cc::PaintFlags::kStroke_Style);
  stroke_flags_.setStrokeWidth(1);
  stroke_flags_.setStrokeCap(cc::PaintFlags::kButt_Cap);
  stroke_flags_.setStrokeMiter(10);
  stroke_flags_.setStrokeJoin(cc::PaintFlags::kMiter_Join);
  stroke_flags_.setAntiAlias(true);
  SetImageSmoothingEnabled(true);
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState(
    const CanvasRenderingContext2DState& other,
    ClipListCopyMode mode,
    SaveType save_type)
    : unparsed_stroke_color_(other.unparsed_stroke_color_),
      unparsed_fill_color_(other.unparsed_fill_color_),
      stroke_style_(other.stroke_style_),
      fill_style_(other.fill_style_),
      stroke_flags_(other.stroke_flags_),
      fill_flags_(other.fill_flags_),
      image_flags_(other.image_flags_),
      shadow_offset_(other.shadow_offset_),
      shadow_blur_(other.shadow_blur_),
      shadow_color_(other.shadow_color_),
      empty_draw_looper_(other.empty_draw_looper_),
      shadow_only_draw_looper_(other.shadow_only_draw_looper_),
      shadow_and_foreground_draw_looper_(
          other.shadow_and_foreground_draw_looper_),
      shadow_only_image_filter_(other.shadow_only_image_filter_),
      shadow_and_foreground_image_filter_(
          other.shadow_and_foreground_image_filter_),
      global_alpha_(other.global_alpha_),
      transform_(other.transform_),
      line_dash_(other.line_dash_),
      line_dash_offset_(other.line_dash_offset_),
      unparsed_font_(other.unparsed_font_),
      font_(other.font_),
      font_for_filter_(other.font_for_filter_),
      filter_state_(other.filter_state_),
      canvas_filter_(other.canvas_filter_),
      unparsed_css_filter_(other.unparsed_css_filter_),
      css_filter_value_(other.css_filter_value_),
      resolved_filter_(other.resolved_filter_),
      text_align_(other.text_align_),
      text_baseline_(other.text_baseline_),
      direction_(other.direction_),
      letter_spacing_(other.letter_spacing_),
      letter_spacing_unit_(other.letter_spacing_unit_),
      word_spacing_(other.word_spacing_),
      word_spacing_unit_(other.word_spacing_unit_),
      text_rendering_mode_(other.text_rendering_mode_),
      font_kerning_(other.font_kerning_),
      font_stretch_(other.font_stretch_),
      font_variant_caps_(other.font_variant_caps_),
      realized_font_(other.realized_font_),
      is_transform_invertible_(other.is_transform_invertible_),
      has_clip_(other.has_clip_),
      has_complex_clip_(other.has_complex_clip_),
      letter_spacing_is_set_(other.letter_spacing_is_set_),
      word_spacing_is_set_(other.word_spacing_is_set_),
      fill_style_dirty_(other.fill_style_dirty_),
      stroke_style_dirty_(other.stroke_style_dirty_),
      line_dash_dirty_(other.line_dash_dirty_),
      image_smoothing_enabled_(other.image_smoothing_enabled_),
      image_smoothing_quality_(other.image_smoothing_quality_),
      save_type_(save_type) {
  if (mode == kCopyClipList) {
    clip_list_ = other.clip_list_;
  }
  stroke_style_->MarkShared(PassKey());
  fill_style_->MarkShared(PassKey());
  // Since FontSelector is weakly persistent with |font_|, the memory may be
  // freed even |font_| is valid.
  if (realized_font_ && font_.GetFontSelector())
    font_.GetFontSelector()->RegisterForInvalidationCallbacks(this);
  ValidateFilterState();
}

CanvasRenderingContext2DState::~CanvasRenderingContext2DState() = default;

void CanvasRenderingContext2DState::FontsNeedUpdate(FontSelector* font_selector,
                                                    FontInvalidationReason) {
  DCHECK_EQ(font_selector, font_.GetFontSelector());
  DCHECK(realized_font_);

  // |font_| will revalidate its FontFallbackList on demand. We don't need to
  // manually reset the Font object here.

  // FIXME: We only really need to invalidate the resolved filter if the font
  // update above changed anything and the filter uses font-dependent units.
  ClearResolvedFilter();
}

void CanvasRenderingContext2DState::Trace(Visitor* visitor) const {
  visitor->Trace(stroke_style_);
  visitor->Trace(fill_style_);
  visitor->Trace(css_filter_value_);
  visitor->Trace(canvas_filter_);
  FontSelectorClient::Trace(visitor);
}

void CanvasRenderingContext2DState::SetLineDashOffset(double offset) {
  line_dash_offset_ = ClampTo<float>(offset);
  line_dash_dirty_ = true;
}

void CanvasRenderingContext2DState::SetLineDash(const Vector<double>& dash) {
  line_dash_ = dash;
  // Spec requires the concatenation of two copies the dash list when the
  // number of elements is odd
  if (dash.size() % 2)
    line_dash_.AppendVector(dash);
  // clamp the double values to float
  base::ranges::transform(line_dash_, line_dash_.begin(),
                          [](double d) { return ClampTo<float>(d); });

  line_dash_dirty_ = true;
}

static bool HasANonZeroElement(const Vector<double>& line_dash) {
  for (double dash : line_dash) {
    if (dash != 0.0)
      return true;
  }
  return false;
}

void CanvasRenderingContext2DState::UpdateLineDash() const {
  if (!line_dash_dirty_)
    return;

  if (!HasANonZeroElement(line_dash_)) {
    stroke_flags_.setPathEffect(nullptr);
  } else {
    Vector<float> line_dash(line_dash_.size());
    base::ranges::copy(line_dash_, line_dash.begin());
    stroke_flags_.setPathEffect(SkDashPathEffect::Make(
        line_dash.data(), line_dash.size(), line_dash_offset_));
  }

  line_dash_dirty_ = false;
}

void CanvasRenderingContext2DState::SetStrokeColor(Color color) {
  if (stroke_style_->IsEquivalentColor(color)) {
    return;
  }

  if (stroke_style_->is_shared()) {
    SetStrokeStyle(MakeGarbageCollected<CanvasStyle>(color));
    return;
  }

  stroke_style_dirty_ = true;
  stroke_style_->SetColor(PassKey(), color);
}

void CanvasRenderingContext2DState::SetStrokePattern(CanvasPattern* pattern) {
  if (stroke_style_->IsEquivalentPattern(pattern)) {
    // Even though the pointer value hasn't changed, the contents of the pattern
    // may have. For this reason the style is marked dirty.
    stroke_style_dirty_ = true;
    return;
  }

  if (stroke_style_->is_shared()) {
    SetStrokeStyle(MakeGarbageCollected<CanvasStyle>(pattern));
    return;
  }

  stroke_style_->SetPattern(PassKey(), pattern);
  stroke_style_dirty_ = true;
}

void CanvasRenderingContext2DState::SetStrokeGradient(
    CanvasGradient* gradient) {
  if (stroke_style_->IsEquivalentGradient(gradient)) {
    // Even though the pointer value hasn't changed, the contents of the
    // gradient may have. For this reason the style is marked dirty.
    stroke_style_dirty_ = true;
    return;
  }

  if (stroke_style_->is_shared()) {
    SetStrokeStyle(MakeGarbageCollected<CanvasStyle>(gradient));
    return;
  }

  stroke_style_->SetGradient(PassKey(), gradient);
  stroke_style_dirty_ = true;
}

void CanvasRenderingContext2DState::SetStrokeStyle(CanvasStyle* style) {
  stroke_style_ = style;
  stroke_style_dirty_ = true;
}

void CanvasRenderingContext2DState::SetFillColor(Color color) {
  if (fill_style_->IsEquivalentColor(color)) {
    return;
  }

  if (fill_style_->is_shared()) {
    SetFillStyle(MakeGarbageCollected<CanvasStyle>(color));
    return;
  }

  fill_style_dirty_ = true;
  fill_style_->SetColor(PassKey(), color);
}

void CanvasRenderingContext2DState::SetFillPattern(CanvasPattern* pattern) {
  if (fill_style_->IsEquivalentPattern(pattern)) {
    // Even though the pointer value hasn't changed, the contents of the pattern
    // may have. For this reason the style is marked dirty.
    fill_style_dirty_ = true;
    return;
  }

  if (fill_style_->is_shared()) {
    SetFillStyle(MakeGarbageCollected<CanvasStyle>(pattern));
    return;
  }

  fill_style_dirty_ = true;
  fill_style_->SetPattern(PassKey(), pattern);
}

void CanvasRenderingContext2DState::SetFillGradient(CanvasGradient* gradient) {
  if (fill_style_->IsEquivalentGradient(gradient)) {
    // Even though the pointer value hasn't changed, the contents of the
    // gradient may have. For this reason the style is marked dirty.
    fill_style_dirty_ = true;
    return;
  }

  if (fill_style_->is_shared()) {
    SetFillStyle(MakeGarbageCollected<CanvasStyle>(gradient));
    return;
  }

  fill_style_dirty_ = true;
  fill_style_->SetGradient(PassKey(), gradient);
}

void CanvasRenderingContext2DState::SetFillStyle(CanvasStyle* style) {
  fill_style_ = style;
  fill_style_dirty_ = true;
}

void CanvasRenderingContext2DState::UpdateStrokeStyle() const {
  if (!stroke_style_dirty_)
    return;

  DCHECK(stroke_style_);
  stroke_style_->ApplyToFlags(stroke_flags_);
  Color stroke_flag_color = stroke_style_->PaintColor();
  stroke_flags_.setColor(
      stroke_flag_color.CombineWithAlpha(global_alpha_).toSkColor4f());
  stroke_style_dirty_ = false;
}

void CanvasRenderingContext2DState::UpdateFillStyle() const {
  if (!fill_style_dirty_)
    return;

  DCHECK(fill_style_);
  fill_style_->ApplyToFlags(fill_flags_);
  Color fill_flag_color = fill_style_->PaintColor();
  fill_flags_.setColor(
      fill_flag_color.CombineWithAlpha(global_alpha_).toSkColor4f());
  fill_style_dirty_ = false;
}

CanvasStyle* CanvasRenderingContext2DState::Style(PaintType paint_type) const {
  switch (paint_type) {
    case kFillPaintType:
      return FillStyle();
    case kStrokePaintType:
      return StrokeStyle();
    case kImagePaintType:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

void CanvasRenderingContext2DState::SetShouldAntialias(bool should_antialias) {
  fill_flags_.setAntiAlias(should_antialias);
  stroke_flags_.setAntiAlias(should_antialias);
  image_flags_.setAntiAlias(should_antialias);
}

bool CanvasRenderingContext2DState::ShouldAntialias() const {
  DCHECK(fill_flags_.isAntiAlias() == stroke_flags_.isAntiAlias() &&
         fill_flags_.isAntiAlias() == image_flags_.isAntiAlias());
  return fill_flags_.isAntiAlias();
}

void CanvasRenderingContext2DState::SetGlobalAlpha(double alpha) {
  global_alpha_ = alpha;
  stroke_style_dirty_ = true;
  fill_style_dirty_ = true;
  image_flags_.setColor(ScaleAlpha(SK_ColorBLACK, alpha));
}

void CanvasRenderingContext2DState::ClipPath(
    const SkPath& path,
    AntiAliasingMode anti_aliasing_mode) {
  clip_list_.ClipPath(path, anti_aliasing_mode,
                      AffineTransformToSkMatrix(transform_));
  has_clip_ = true;
  if (!path.isRect(nullptr))
    has_complex_clip_ = true;
}

void CanvasRenderingContext2DState::SetFont(
    const FontDescription& passed_font_description,
    FontSelector* selector) {
  FontDescription font_description = passed_font_description;
  font_description.SetSubpixelAscentDescent(true);

  CSSToLengthConversionData conversion_data = CSSToLengthConversionData();
  Font font = Font();
  auto const font_size = CSSToLengthConversionData::FontSizes(
      font_description.ComputedSize(), font_description.ComputedSize(), &font,
      1.0f /*Deliberately ignore zoom on the canvas element*/);
  conversion_data.SetFontSizes(font_size);

  // If wordSpacing is set in CanvasRenderingContext2D, then update the
  // information in fontDescription.
  if (word_spacing_is_set_) {
    // Convert word spacing to pixel length and set it in font_description.
    float word_spacing_in_pixel =
        conversion_data.ZoomedComputedPixels(word_spacing_, word_spacing_unit_);
    font_description.SetWordSpacing(word_spacing_in_pixel);
  }

  // If wordSpacing is set in CanvasRenderingContext2D, then update the
  // information in fontDescription.
  if (letter_spacing_is_set_) {
    // Convert letter spacing to pixel length and set it in font_description.
    float letter_spacing_in_pixel = conversion_data.ZoomedComputedPixels(
        letter_spacing_, letter_spacing_unit_);
    font_description.SetLetterSpacing(letter_spacing_in_pixel);
  }

  font_ = Font(font_description, selector);
  realized_font_ = true;
  if (selector)
    selector->RegisterForInvalidationCallbacks(this);
}

bool CanvasRenderingContext2DState::IsFontDirtyForFilter() const {
  // Indicates if the font has changed since the last time the filter was set.
  if (!HasRealizedFont())
    return true;
  return GetFont() != font_for_filter_;
}

const Font& CanvasRenderingContext2DState::GetFont() const {
  DCHECK(realized_font_);
  return font_;
}

const FontDescription& CanvasRenderingContext2DState::GetFontDescription()
    const {
  DCHECK(realized_font_);
  return font_.GetFontDescription();
}

void CanvasRenderingContext2DState::SetFontKerning(
    FontDescription::Kerning font_kerning,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontDescription font_description(GetFontDescription());
  font_description.SetKerning(font_kerning);
  font_kerning_ = font_kerning;
  SetFont(font_description, selector);
}

void CanvasRenderingContext2DState::SetFontStretch(
    FontSelectionValue font_stretch,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontDescription font_description(GetFontDescription());
  font_description.SetStretch(font_stretch);
  font_stretch_ = font_stretch;
  SetFont(font_description, selector);
}

void CanvasRenderingContext2DState::SetFontVariantCaps(
    FontDescription::FontVariantCaps font_variant_caps,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontDescription font_description(GetFontDescription());
  font_description.SetVariantCaps(font_variant_caps);
  font_variant_caps_ = font_variant_caps;
  SetFont(font_description, selector);
}

void CanvasRenderingContext2DState::SetTransform(
    const AffineTransform& transform) {
  is_transform_invertible_ = transform.IsInvertible();
  transform_ = transform;
}

void CanvasRenderingContext2DState::ResetTransform() {
  transform_.MakeIdentity();
  is_transform_invertible_ = true;
}

void CanvasRenderingContext2DState::ValidateFilterState() const {
#if DCHECK_IS_ON()
  switch (filter_state_) {
    case FilterState::kNone:
      DCHECK(!resolved_filter_);
      DCHECK(!css_filter_value_);
      DCHECK(!canvas_filter_);
      break;
    case FilterState::kUnresolved:
    case FilterState::kInvalid:
      DCHECK(!resolved_filter_);
      DCHECK(css_filter_value_ || canvas_filter_);
      break;
    case FilterState::kResolved:
      DCHECK(resolved_filter_);
      DCHECK(css_filter_value_ || canvas_filter_);
      break;
    default:
      NOTREACHED();
  }
#endif
}

sk_sp<PaintFilter> CanvasRenderingContext2DState::GetFilterForOffscreenCanvas(
    gfx::Size canvas_size,
    BaseRenderingContext2D* context) {
  ValidateFilterState();
  if (filter_state_ != FilterState::kUnresolved)
    return resolved_filter_;

  FilterOperations operations;
  if (canvas_filter_) {
    operations = canvas_filter_->Operations();
  } else {
    operations = FilterOperationResolver::CreateOffscreenFilterOperations(
        *css_filter_value_, font_for_filter_);
  }

  // We can't reuse m_fillFlags and m_strokeFlags for the filter, since these
  // incorporate the global alpha, which isn't applicable here.
  cc::PaintFlags fill_flags_for_filter;
  fill_style_->ApplyToFlags(fill_flags_for_filter);
  fill_flags_for_filter.setColor(fill_style_->PaintColor().toSkColor4f());
  cc::PaintFlags stroke_flags_for_filter;
  stroke_style_->ApplyToFlags(stroke_flags_for_filter);
  stroke_flags_for_filter.setColor(stroke_style_->PaintColor().toSkColor4f());

  FilterEffectBuilder filter_effect_builder(
      gfx::RectF(gfx::SizeF(canvas_size)),
      1.0f,  // Deliberately ignore zoom on the canvas element.
      &fill_flags_for_filter, &stroke_flags_for_filter);

  FilterEffect* last_effect = filter_effect_builder.BuildFilterEffect(
      operations, !context->OriginClean());
  if (last_effect) {
    // TODO(chrishtr): Taint the origin if needed. crbug.com/792506.
    resolved_filter_ =
        paint_filter_builder::Build(last_effect, kInterpolationSpaceSRGB);
  }

  filter_state_ =
      resolved_filter_ ? FilterState::kResolved : FilterState::kInvalid;
  ValidateFilterState();
  return resolved_filter_;
}

sk_sp<PaintFilter> CanvasRenderingContext2DState::GetFilter(
    Element* style_resolution_host,
    gfx::Size canvas_size,
    CanvasRenderingContext2D* context) {
  // TODO(1189879): Investigate refactoring all filter logic into the
  // CanvasFilterOperationResolver class
  ValidateFilterState();

  if (filter_state_ != FilterState::kUnresolved)
    return resolved_filter_;

  FilterOperations operations;
  if (canvas_filter_) {
    operations = canvas_filter_->Operations();
  } else {
    Document& document = style_resolution_host->GetDocument();

    // StyleResolver cannot be used in frame-less documents.
    if (!document.GetFrame())
      return nullptr;
    // Update the filter value to the proper base URL if needed.
    if (css_filter_value_->MayContainUrl()) {
      document.UpdateStyleAndLayout(DocumentUpdateReason::kCanvas);
      css_filter_value_->ReResolveUrl(document);
    }

    const Font* font = &font_for_filter_;

    // Must set font in case the filter uses any font-relative units (em, ex)
    // If font_for_filter_ was never set (ie frame-less documents) use base font
    if (UNLIKELY(!font_for_filter_.GetFontSelector())) {
      if (const ComputedStyle* computed_style = document.GetComputedStyle()) {
        font = &computed_style->GetFont();
      } else {
        return nullptr;
      }
    }

    DCHECK(font);

    operations = document.GetStyleResolver().ComputeFilterOperations(
        style_resolution_host, *font, *css_filter_value_);
  }

  // We can't reuse m_fillFlags and m_strokeFlags for the filter, since these
  // incorporate the global alpha, which isn't applicable here.
  cc::PaintFlags fill_flags_for_filter;
  fill_style_->ApplyToFlags(fill_flags_for_filter);
  fill_flags_for_filter.setColor(fill_style_->PaintColor().toSkColor4f());
  cc::PaintFlags stroke_flags_for_filter;
  stroke_style_->ApplyToFlags(stroke_flags_for_filter);
  stroke_flags_for_filter.setColor(stroke_style_->PaintColor().toSkColor4f());

  FilterEffectBuilder filter_effect_builder(
      gfx::RectF(gfx::SizeF(canvas_size)),
      1.0f,  // Deliberately ignore zoom on the canvas element.
      &fill_flags_for_filter, &stroke_flags_for_filter);

  FilterEffect* last_effect = filter_effect_builder.BuildFilterEffect(
      operations, !context->OriginClean());
  if (last_effect) {
    resolved_filter_ =
        paint_filter_builder::Build(last_effect, kInterpolationSpaceSRGB);
    if (resolved_filter_) {
      context->UpdateFilterReferences(operations);
      if (last_effect->OriginTainted())
        context->SetOriginTainted();
    }
  }

  filter_state_ =
      resolved_filter_ ? FilterState::kResolved : FilterState::kInvalid;
  ValidateFilterState();
  return resolved_filter_;
}

void CanvasRenderingContext2DState::ClearResolvedFilter() {
  resolved_filter_.reset();
  filter_state_ = (canvas_filter_ || css_filter_value_)
                      ? FilterState::kUnresolved
                      : FilterState::kNone;
  ValidateFilterState();
}

sk_sp<SkDrawLooper>& CanvasRenderingContext2DState::EmptyDrawLooper() const {
  if (!empty_draw_looper_)
    empty_draw_looper_ = DrawLooperBuilder().DetachDrawLooper();

  return empty_draw_looper_;
}

sk_sp<SkDrawLooper>& CanvasRenderingContext2DState::ShadowOnlyDrawLooper()
    const {
  if (!shadow_only_draw_looper_) {
    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(shadow_offset_, ClampTo<float>(shadow_blur_),
                                  shadow_color_,
                                  DrawLooperBuilder::kShadowIgnoresTransforms,
                                  DrawLooperBuilder::kShadowRespectsAlpha);
    shadow_only_draw_looper_ = draw_looper_builder.DetachDrawLooper();
  }
  return shadow_only_draw_looper_;
}

sk_sp<SkDrawLooper>&
CanvasRenderingContext2DState::ShadowAndForegroundDrawLooper() const {
  if (!shadow_and_foreground_draw_looper_) {
    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(shadow_offset_, ClampTo<float>(shadow_blur_),
                                  shadow_color_,
                                  DrawLooperBuilder::kShadowIgnoresTransforms,
                                  DrawLooperBuilder::kShadowRespectsAlpha);
    draw_looper_builder.AddUnmodifiedContent();
    shadow_and_foreground_draw_looper_ = draw_looper_builder.DetachDrawLooper();
  }
  return shadow_and_foreground_draw_looper_;
}

sk_sp<PaintFilter>& CanvasRenderingContext2DState::ShadowOnlyImageFilter()
    const {
  using ShadowMode = DropShadowPaintFilter::ShadowMode;
  if (!shadow_only_image_filter_) {
    const auto sigma = BlurRadiusToStdDev(shadow_blur_);
    shadow_only_image_filter_ = sk_make_sp<DropShadowPaintFilter>(
        shadow_offset_.x(), shadow_offset_.y(), sigma, sigma,
        shadow_color_.toSkColor4f(), ShadowMode::kDrawShadowOnly, nullptr);
  }
  return shadow_only_image_filter_;
}

sk_sp<PaintFilter>&
CanvasRenderingContext2DState::ShadowAndForegroundImageFilter() const {
  using ShadowMode = DropShadowPaintFilter::ShadowMode;
  if (!shadow_and_foreground_image_filter_) {
    const auto sigma = BlurRadiusToStdDev(shadow_blur_);
    // TODO(crbug/1308932): Remove FromColor and make all SkColor4f.
    shadow_and_foreground_image_filter_ = sk_make_sp<DropShadowPaintFilter>(
        shadow_offset_.x(), shadow_offset_.y(), sigma, sigma,
        shadow_color_.toSkColor4f(), ShadowMode::kDrawShadowAndForeground,
        nullptr);
  }
  return shadow_and_foreground_image_filter_;
}

void CanvasRenderingContext2DState::ShadowParameterChanged() {
  shadow_only_draw_looper_.reset();
  shadow_and_foreground_draw_looper_.reset();
  shadow_only_image_filter_.reset();
  shadow_and_foreground_image_filter_.reset();
}

void CanvasRenderingContext2DState::SetShadowOffsetX(double x) {
  shadow_offset_.set_x(ClampTo<float>(x));
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowOffsetY(double y) {
  shadow_offset_.set_y(ClampTo<float>(y));
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowBlur(double shadow_blur) {
  shadow_blur_ = ClampTo<float>(shadow_blur);
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowColor(Color shadow_color) {
  shadow_color_ = shadow_color;
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetCSSFilter(const CSSValue* filter_value) {
  css_filter_value_ = filter_value;
  canvas_filter_ = nullptr;
  ClearResolvedFilter();
}

void CanvasRenderingContext2DState::SetCanvasFilter(
    CanvasFilter* canvas_filter) {
  canvas_filter_ = canvas_filter;
  css_filter_value_ = nullptr;
  ClearResolvedFilter();
}

void CanvasRenderingContext2DState::SetGlobalComposite(SkBlendMode mode) {
  stroke_flags_.setBlendMode(mode);
  fill_flags_.setBlendMode(mode);
  image_flags_.setBlendMode(mode);
}

SkBlendMode CanvasRenderingContext2DState::GlobalComposite() const {
  return stroke_flags_.getBlendMode();
}

void CanvasRenderingContext2DState::SetImageSmoothingEnabled(bool enabled) {
  image_smoothing_enabled_ = enabled;
  UpdateFilterQuality();
}

bool CanvasRenderingContext2DState::ImageSmoothingEnabled() const {
  return image_smoothing_enabled_;
}

void CanvasRenderingContext2DState::SetImageSmoothingQuality(
    const String& quality_string) {
  if (quality_string == "low") {
    image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kLow;
  } else if (quality_string == "medium") {
    image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kMedium;
  } else if (quality_string == "high") {
    image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kHigh;
  } else {
    return;
  }
  UpdateFilterQuality();
}

String CanvasRenderingContext2DState::ImageSmoothingQuality() const {
  switch (image_smoothing_quality_) {
    case cc::PaintFlags::FilterQuality::kLow:
      return "low";
    case cc::PaintFlags::FilterQuality::kMedium:
      return "medium";
    case cc::PaintFlags::FilterQuality::kHigh:
      return "high";
    default:
      NOTREACHED();
      return "low";
  }
}

void CanvasRenderingContext2DState::UpdateFilterQuality() const {
  if (!image_smoothing_enabled_) {
    UpdateFilterQuality(cc::PaintFlags::FilterQuality::kNone);
  } else {
    UpdateFilterQuality(image_smoothing_quality_);
  }
}

void CanvasRenderingContext2DState::UpdateFilterQuality(
    cc::PaintFlags::FilterQuality filter_quality) const {
  stroke_flags_.setFilterQuality(filter_quality);
  fill_flags_.setFilterQuality(filter_quality);
  image_flags_.setFilterQuality(filter_quality);
}

const cc::PaintFlags* CanvasRenderingContext2DState::GetFlags(
    PaintType paint_type,
    ShadowMode shadow_mode,
    ImageType image_type) const {
  cc::PaintFlags* flags;
  switch (paint_type) {
    case kStrokePaintType:
      UpdateLineDash();
      UpdateStrokeStyle();
      flags = &stroke_flags_;
      break;
    default:
      NOTREACHED();
      // no break on purpose: flags needs to be assigned to avoid compiler warning
      // about uninitialized variable.
      [[fallthrough]];
    case kFillPaintType:
      UpdateFillStyle();
      flags = &fill_flags_;
      break;
    case kImagePaintType:
      flags = &image_flags_;
      break;
  }

  if ((!ShouldDrawShadows() && shadow_mode == kDrawShadowAndForeground) ||
      shadow_mode == kDrawForegroundOnly) {
    flags->setLooper(nullptr);
    flags->setImageFilter(nullptr);
    return flags;
  }

  if (!ShouldDrawShadows() && shadow_mode == kDrawShadowOnly) {
    flags->setLooper(EmptyDrawLooper());  // draw nothing
    flags->setImageFilter(nullptr);
    return flags;
  }

  if (shadow_mode == kDrawShadowOnly) {
    if (image_type == kNonOpaqueImage || css_filter_value_) {
      flags->setLooper(nullptr);
      flags->setImageFilter(ShadowOnlyImageFilter());
      return flags;
    }
    flags->setLooper(ShadowOnlyDrawLooper());
    flags->setImageFilter(nullptr);
    return flags;
  }

  DCHECK(shadow_mode == kDrawShadowAndForeground);
  if (image_type == kNonOpaqueImage) {
    flags->setLooper(nullptr);
    flags->setImageFilter(ShadowAndForegroundImageFilter());
    return flags;
  }
  flags->setLooper(ShadowAndForegroundDrawLooper());
  flags->setImageFilter(nullptr);
  return flags;
}

bool CanvasRenderingContext2DState::HasPattern(PaintType paint_type) const {
  return Style(paint_type) && Style(paint_type)->GetCanvasPattern() &&
         Style(paint_type)->GetCanvasPattern()->GetPattern();
}

// Only to be used if the CanvasRenderingContext2DState has Pattern
bool CanvasRenderingContext2DState::PatternIsAccelerated(
    PaintType paint_type) const {
  DCHECK(HasPattern(paint_type));
  return Style(paint_type)->GetCanvasPattern()->GetPattern()->IsTextureBacked();
}

void CanvasRenderingContext2DState::SetLetterSpacing(
    const String& letter_spacing) {
  DCHECK(realized_font_);
  if (!letter_spacing_is_set_)
    letter_spacing_is_set_ = true;
  if (parsed_letter_spacing_ == letter_spacing)
    return;
  float num_spacing;
  CSSPrimitiveValue::UnitType unit;
  if (!StringToNumWithUnit(letter_spacing, &num_spacing, &unit))
    return;

  if (unit == letter_spacing_unit_ && num_spacing == letter_spacing_)
    return;

  letter_spacing_unit_ = unit;
  letter_spacing_ = num_spacing;
  StringBuilder builder;
  builder.AppendNumber(num_spacing);
  builder.Append(CSSPrimitiveValue::UnitTypeToString(unit));
  parsed_letter_spacing_ = builder.ToString();
  if (font_.GetFontSelector())
    SetFont(GetFontDescription(), font_.GetFontSelector());
}

void CanvasRenderingContext2DState::SetWordSpacing(const String& word_spacing) {
  DCHECK(realized_font_);
  if (!word_spacing_is_set_)
    word_spacing_is_set_ = true;
  if (parsed_word_spacing_ == word_spacing)
    return;
  float num_spacing;
  CSSPrimitiveValue::UnitType unit;
  if (!StringToNumWithUnit(word_spacing, &num_spacing, &unit))
    return;

  if (unit == word_spacing_unit_ && num_spacing == word_spacing_)
    return;

  word_spacing_unit_ = unit;
  word_spacing_ = num_spacing;
  StringBuilder builder;
  builder.AppendNumber(num_spacing);
  builder.Append(CSSPrimitiveValue::UnitTypeToString(unit));
  parsed_word_spacing_ = builder.ToString();
  if (font_.GetFontSelector())
    SetFont(GetFontDescription(), font_.GetFontSelector());
}

void CanvasRenderingContext2DState::SetTextRendering(
    TextRenderingMode text_rendering,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontDescription font_description(GetFontDescription());
  font_description.SetTextRendering(text_rendering);
  text_rendering_mode_ = text_rendering;
  SetFont(font_description, selector);
}

}  // namespace blink
