// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"

#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "cc/paint/draw_looper.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/path_effect.h"
#include "third_party/blink/public/common/metrics/document_update_reason.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_font_stretch.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_text_rendering.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operations.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/base_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_filter.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/platform/fonts/font_selection_types.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/text_rendering_mode.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/interpolation_space.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_filter.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBlendMode.h"  // IWYU pragma: keep (for SkBlendMode)
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {
enum class FontInvalidationReason;
}  // namespace blink

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
  CSSParserTokenStream stream(spacing);
  // If we failed to parse token, return immediately.
  if (stream.AtEnd()) {
    return false;
  }

  // If there is more than 1 dimension token or |spacing| is not a valid
  // dimension token, or unit is not a valid CSS length unit, return
  // immediately.
  const CSSParserToken& result = stream.Peek();
  if (result.GetType() == kDimensionToken &&
      CSSPrimitiveValue::IsLength(result.GetUnitType())) {
    *number_spacing = result.NumericValue();
    *unit = result.GetUnitType();
    stream.Consume();
    return stream.AtEnd();
  }
  return false;
}

FontSelectionValue CanvasFontStretchToSelectionValue(
    V8CanvasFontStretch font_stretch) {
  FontSelectionValue stretch_value;
  switch (font_stretch.AsEnum()) {
    case (V8CanvasFontStretch::Enum::kUltraCondensed):
      stretch_value = kUltraCondensedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kExtraCondensed):
      stretch_value = kExtraCondensedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kCondensed):
      stretch_value = kCondensedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kSemiCondensed):
      stretch_value = kSemiCondensedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kNormal):
      stretch_value = kNormalWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kUltraExpanded):
      stretch_value = kUltraExpandedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kExtraExpanded):
      stretch_value = kExtraExpandedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kExpanded):
      stretch_value = kExpandedWidthValue;
      break;
    case (V8CanvasFontStretch::Enum::kSemiExpanded):
      stretch_value = kSemiExpandedWidthValue;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return stretch_value;
}

TextRenderingMode CanvasTextRenderingToTextRendering(
    V8CanvasTextRendering text_rendering) {
  TextRenderingMode text_rendering_mode;
  switch (text_rendering.AsEnum()) {
    case (V8CanvasTextRendering::Enum::kAuto):
      text_rendering_mode = TextRenderingMode::kAutoTextRendering;
      break;
    case (V8CanvasTextRendering::Enum::kOptimizeSpeed):
      text_rendering_mode = TextRenderingMode::kAutoTextRendering;
      break;
    case (V8CanvasTextRendering::Enum::kOptimizeLegibility):
      text_rendering_mode = TextRenderingMode::kAutoTextRendering;
      break;
    case (V8CanvasTextRendering::Enum::kGeometricPrecision):
      text_rendering_mode = TextRenderingMode::kAutoTextRendering;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  return text_rendering_mode;
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState()
    : shadow_blur_(0.0),
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
      line_dash_dirty_(other.line_dash_dirty_),
      image_smoothing_enabled_(other.image_smoothing_enabled_),
      image_smoothing_quality_(other.image_smoothing_quality_),
      save_type_(save_type) {
  if (mode == kCopyClipList) {
    clip_list_ = other.clip_list_;
  }
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
  visitor->Trace(font_);
  visitor->Trace(font_for_filter_);
  visitor->Trace(canvas_filter_);
  visitor->Trace(unparsed_stroke_color_);
  visitor->Trace(unparsed_fill_color_);
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

ALWAYS_INLINE void CanvasRenderingContext2DState::UpdateLineDash() const {
  if (!line_dash_dirty_) [[likely]] {
    return;
  }
  if (!HasANonZeroElement(line_dash_)) {
    stroke_flags_.setPathEffect(nullptr);
  } else {
    Vector<float> line_dash(line_dash_.size());
    base::ranges::copy(line_dash_, line_dash.begin());
    stroke_flags_.setPathEffect(cc::PathEffect::MakeDash(
        line_dash.data(), line_dash.size(), line_dash_offset_));
  }
  line_dash_dirty_ = false;
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
  stroke_style_.ApplyToFlags(stroke_flags_, global_alpha_);
  fill_style_.ApplyToFlags(fill_flags_, global_alpha_);
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

  // After the font changed value, the new font needs to follow the text
  // properties set for the context, ref:
  // https://html.spec.whatwg.org/multipage/canvas.html#text-preparation-algorithm
  // However, FontVariantCaps and FontStretch can be set with the font. It's
  // ambiguous if the values are left intentionally out to use default.
  // It's suggest to always use the values from font setter,
  // ref: https://github.com/whatwg/html/issues/8103.

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
  font_description.SetKerning(font_kerning_);
  font_description.SetTextRendering(
      CanvasTextRenderingToTextRendering(text_rendering_mode_));
  font_variant_caps_ = font_description.VariantCaps();
  std::optional<blink::V8CanvasFontStretch> font_value =
      V8CanvasFontStretch::Create(
          FontDescription::ToString(font_description.Stretch()).LowerASCII());
  if (font_value.has_value()) {
    font_stretch_ = *font_value;
  } else {
    NOTREACHED();
  }
  SetFontInternal(font_description, selector);
}

void CanvasRenderingContext2DState::SetFontInternal(
    const FontDescription& passed_font_description,
    FontSelector* selector) {
  FontDescription font_description = passed_font_description;
  font_description.SetSubpixelAscentDescent(true);

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
  SetFontInternal(font_description, selector);
}

void CanvasRenderingContext2DState::SetFontStretch(
    V8CanvasFontStretch font_stretch,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontSelectionValue stretch_value =
      CanvasFontStretchToSelectionValue(font_stretch);
  FontDescription font_description(GetFontDescription());
  font_description.SetStretch(stretch_value);
  font_stretch_ = font_stretch;
  SetFontInternal(font_description, selector);
}

void CanvasRenderingContext2DState::SetFontVariantCaps(
    FontDescription::FontVariantCaps font_variant_caps,
    FontSelector* selector) {
  DCHECK(realized_font_);
  FontDescription font_description(GetFontDescription());
  font_description.SetVariantCaps(font_variant_caps);
  font_variant_caps_ = font_variant_caps;
  SetFontInternal(font_description, selector);
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
      NOTREACHED_IN_MIGRATION();
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
  fill_style_.ApplyToFlags(fill_flags_for_filter, 1.0f);
  cc::PaintFlags stroke_flags_for_filter;
  stroke_style_.ApplyToFlags(stroke_flags_for_filter, 1.0f);

  const gfx::SizeF canvas_viewport(canvas_size);
  FilterEffectBuilder filter_effect_builder(
      gfx::RectF(canvas_viewport), canvas_viewport,
      1.0f,  // Deliberately ignore zoom on the canvas element.
      Color::kBlack, mojom::blink::ColorScheme::kLight, &fill_flags_for_filter,
      &stroke_flags_for_filter);

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
    if (!font_for_filter_.GetFontSelector()) [[unlikely]] {
      if (LayoutView* layout_view = document.GetLayoutView()) {
        font = &layout_view->StyleRef().GetFont();
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
  fill_style_.ApplyToFlags(fill_flags_for_filter, 1.0f);
  cc::PaintFlags stroke_flags_for_filter;
  stroke_style_.ApplyToFlags(stroke_flags_for_filter, 1.0f);

  const gfx::SizeF canvas_viewport(canvas_size);
  FilterEffectBuilder filter_effect_builder(
      gfx::RectF(canvas_viewport), canvas_viewport,
      1.0f,  // Deliberately ignore zoom on the canvas element.
      Color::kBlack, mojom::blink::ColorScheme::kLight, &fill_flags_for_filter,
      &stroke_flags_for_filter);

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

sk_sp<cc::DrawLooper>& CanvasRenderingContext2DState::EmptyDrawLooper() const {
  if (!empty_draw_looper_)
    empty_draw_looper_ = DrawLooperBuilder().DetachDrawLooper();

  return empty_draw_looper_;
}

sk_sp<cc::DrawLooper>& CanvasRenderingContext2DState::ShadowOnlyDrawLooper()
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

sk_sp<cc::DrawLooper>&
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
    const V8ImageSmoothingQuality& quality) {
  switch (quality.AsEnum()) {
    case V8ImageSmoothingQuality::Enum::kLow:
      image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kLow;
      UpdateFilterQuality();
      return;
    case V8ImageSmoothingQuality::Enum::kMedium:
      image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kMedium;
      UpdateFilterQuality();
      return;
    case V8ImageSmoothingQuality::Enum::kHigh:
      image_smoothing_quality_ = cc::PaintFlags::FilterQuality::kHigh;
      UpdateFilterQuality();
      return;
  }
  NOTREACHED();
}

V8ImageSmoothingQuality CanvasRenderingContext2DState::ImageSmoothingQuality()
    const {
  switch (image_smoothing_quality_) {
    case cc::PaintFlags::FilterQuality::kNone:
    case cc::PaintFlags::FilterQuality::kLow:
      return V8ImageSmoothingQuality(V8ImageSmoothingQuality::Enum::kLow);
    case cc::PaintFlags::FilterQuality::kMedium:
      return V8ImageSmoothingQuality(V8ImageSmoothingQuality::Enum::kMedium);
    case cc::PaintFlags::FilterQuality::kHigh:
      return V8ImageSmoothingQuality(V8ImageSmoothingQuality::Enum::kHigh);
  }
  NOTREACHED();
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
      stroke_style_.SyncFlags(stroke_flags_, global_alpha_);
      flags = &stroke_flags_;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      // no break on purpose: flags needs to be assigned to avoid compiler
      // warning about uninitialized variable.
      [[fallthrough]];
    case kFillPaintType:
      fill_style_.SyncFlags(fill_flags_, global_alpha_);
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
  // Convert letter spacing to pixel length and set it in font_description.
  FontDescription font_description(GetFontDescription());
  CSSToLengthConversionData conversion_data = CSSToLengthConversionData();
  auto const font_size = CSSToLengthConversionData::FontSizes(
      font_description.ComputedSize(), font_description.ComputedSize(), &font_,
      1.0f /*Deliberately ignore zoom on the canvas element*/);
  conversion_data.SetFontSizes(font_size);
  float letter_spacing_in_pixel =
      conversion_data.ZoomedComputedPixels(num_spacing, unit);

  font_description.SetLetterSpacing(letter_spacing_in_pixel);
  if (font_.GetFontSelector())
    SetFontInternal(font_description, font_.GetFontSelector());
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
  // Convert letter spacing to pixel length and set it in font_description.
  FontDescription font_description(GetFontDescription());
  CSSToLengthConversionData conversion_data = CSSToLengthConversionData();
  auto const font_size = CSSToLengthConversionData::FontSizes(
      font_description.ComputedSize(), font_description.ComputedSize(), &font_,
      1.0f /*Deliberately ignore zoom on the canvas element*/);
  conversion_data.SetFontSizes(font_size);
  float word_spacing_in_pixel =
      conversion_data.ZoomedComputedPixels(num_spacing, unit);

  font_description.SetWordSpacing(word_spacing_in_pixel);
  if (font_.GetFontSelector())
    SetFontInternal(font_description, font_.GetFontSelector());
}

void CanvasRenderingContext2DState::SetTextRendering(
    V8CanvasTextRendering text_rendering,
    FontSelector* selector) {
  DCHECK(realized_font_);
  TextRenderingMode text_rendering_mode =
      CanvasTextRenderingToTextRendering(text_rendering);
  FontDescription font_description(GetFontDescription());
  font_description.SetTextRendering(text_rendering_mode);
  text_rendering_mode_ = text_rendering;
  SetFontInternal(font_description, selector);
}

}  // namespace blink
