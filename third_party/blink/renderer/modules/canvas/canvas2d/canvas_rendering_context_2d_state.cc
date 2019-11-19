// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d_state.h"

#include <memory>
#include "third_party/blink/renderer/core/css/resolver/filter_operation_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/paint/filter_effect_builder.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/filter_operation.h"
#include "third_party/blink/renderer/core/svg/svg_filter_element.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_gradient.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_pattern.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_rendering_context_2d.h"
#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_style.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/graphics/draw_looper_builder.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"

static const char defaultFont[] = "10px sans-serif";
static const char defaultFilter[] = "none";

namespace blink {

CanvasRenderingContext2DState::CanvasRenderingContext2DState()
    : unrealized_save_count_(0),
      stroke_style_(CanvasStyle::CreateFromRGBA(SK_ColorBLACK)),
      fill_style_(CanvasStyle::CreateFromRGBA(SK_ColorBLACK)),
      shadow_blur_(0),
      shadow_color_(Color::kTransparent),
      global_alpha_(1),
      line_dash_offset_(0),
      unparsed_font_(defaultFont),
      unparsed_filter_(defaultFilter),
      text_align_(kStartTextAlign),
      text_baseline_(kAlphabeticTextBaseline),
      direction_(kDirectionInherit),
      realized_font_(false),
      is_transform_invertible_(true),
      has_clip_(false),
      has_complex_clip_(false),
      fill_style_dirty_(true),
      stroke_style_dirty_(true),
      line_dash_dirty_(false),
      image_smoothing_quality_(kLow_SkFilterQuality) {
  fill_flags_.setStyle(PaintFlags::kFill_Style);
  fill_flags_.setAntiAlias(true);
  image_flags_.setStyle(PaintFlags::kFill_Style);
  image_flags_.setAntiAlias(true);
  stroke_flags_.setStyle(PaintFlags::kStroke_Style);
  stroke_flags_.setStrokeWidth(1);
  stroke_flags_.setStrokeCap(PaintFlags::kButt_Cap);
  stroke_flags_.setStrokeMiter(10);
  stroke_flags_.setStrokeJoin(PaintFlags::kMiter_Join);
  stroke_flags_.setAntiAlias(true);
  SetImageSmoothingEnabled(true);
}

CanvasRenderingContext2DState::CanvasRenderingContext2DState(
    const CanvasRenderingContext2DState& other,
    ClipListCopyMode mode)
    : unrealized_save_count_(other.unrealized_save_count_),
      unparsed_stroke_color_(other.unparsed_stroke_color_),
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
      unparsed_filter_(other.unparsed_filter_),
      filter_value_(other.filter_value_),
      resolved_filter_(other.resolved_filter_),
      text_align_(other.text_align_),
      text_baseline_(other.text_baseline_),
      direction_(other.direction_),
      realized_font_(other.realized_font_),
      is_transform_invertible_(other.is_transform_invertible_),
      has_clip_(other.has_clip_),
      has_complex_clip_(other.has_complex_clip_),
      fill_style_dirty_(other.fill_style_dirty_),
      stroke_style_dirty_(other.stroke_style_dirty_),
      line_dash_dirty_(other.line_dash_dirty_),
      image_smoothing_enabled_(other.image_smoothing_enabled_),
      image_smoothing_quality_(other.image_smoothing_quality_) {
  if (mode == kCopyClipList) {
    clip_list_ = other.clip_list_;
  }
  if (realized_font_)
    font_.GetFontSelector()->RegisterForInvalidationCallbacks(this);
}

CanvasRenderingContext2DState::~CanvasRenderingContext2DState() = default;

void CanvasRenderingContext2DState::FontsNeedUpdate(
    FontSelector* font_selector) {
  DCHECK_EQ(font_selector, font_.GetFontSelector());
  DCHECK(realized_font_);

  font_.Update(font_selector);
  // FIXME: We only really need to invalidate the resolved filter if the font
  // update above changed anything and the filter uses font-dependent units.
  resolved_filter_.reset();
}

void CanvasRenderingContext2DState::Trace(blink::Visitor* visitor) {
  visitor->Trace(stroke_style_);
  visitor->Trace(fill_style_);
  visitor->Trace(filter_value_);
  FontSelectorClient::Trace(visitor);
}

void CanvasRenderingContext2DState::SetLineDashOffset(double offset) {
  line_dash_offset_ = clampTo<float>(offset);
  line_dash_dirty_ = true;
}

void CanvasRenderingContext2DState::SetLineDash(const Vector<double>& dash) {
  line_dash_ = dash;
  // Spec requires the concatenation of two copies the dash list when the
  // number of elements is odd
  if (dash.size() % 2)
    line_dash_.AppendVector(dash);
  // clamp the double values to float
  std::transform(line_dash_.begin(), line_dash_.end(), line_dash_.begin(),
                 [](double d) { return clampTo<float>(d); });

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
    std::copy(line_dash_.begin(), line_dash_.end(), line_dash.begin());
    stroke_flags_.setPathEffect(SkDashPathEffect::Make(
        line_dash.data(), line_dash.size(), line_dash_offset_));
  }

  line_dash_dirty_ = false;
}

void CanvasRenderingContext2DState::SetStrokeStyle(CanvasStyle* style) {
  stroke_style_ = style;
  stroke_style_dirty_ = true;
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
  stroke_flags_.setColor(
      ScaleAlpha(stroke_style_->PaintColor(), global_alpha_));
  stroke_style_dirty_ = false;
}

void CanvasRenderingContext2DState::UpdateFillStyle() const {
  if (!fill_style_dirty_)
    return;

  DCHECK(fill_style_);
  fill_style_->ApplyToFlags(fill_flags_);
  fill_flags_.setColor(ScaleAlpha(fill_style_->PaintColor(), global_alpha_));
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

void CanvasRenderingContext2DState::SetFont(const Font& font,
                                            FontSelector* selector) {
  font_ = font;
  font_.Update(selector);
  realized_font_ = true;
  if (selector)
    selector->RegisterForInvalidationCallbacks(this);
}

const Font& CanvasRenderingContext2DState::GetFont() const {
  DCHECK(realized_font_);
  return font_;
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

sk_sp<PaintFilter> CanvasRenderingContext2DState::GetFilterForOffscreenCanvas(
    IntSize canvas_size,
    BaseRenderingContext2D* context) const {
  if (!filter_value_)
    return nullptr;

  if (resolved_filter_)
    return resolved_filter_;

  FilterOperations operations =
      FilterOperationResolver::CreateOffscreenFilterOperations(
          *filter_value_, font_for_filter_);

  // We can't reuse m_fillFlags and m_strokeFlags for the filter, since these
  // incorporate the global alpha, which isn't applicable here.
  PaintFlags fill_flags_for_filter;
  fill_style_->ApplyToFlags(fill_flags_for_filter);
  fill_flags_for_filter.setColor(fill_style_->PaintColor());
  PaintFlags stroke_flags_for_filter;
  stroke_style_->ApplyToFlags(stroke_flags_for_filter);
  stroke_flags_for_filter.setColor(stroke_style_->PaintColor());

  FilterEffectBuilder filter_effect_builder(
      FloatRect((FloatPoint()), FloatSize(canvas_size)),
      1.0f,  // Deliberately ignore zoom on the canvas element.
      &fill_flags_for_filter, &stroke_flags_for_filter);

  FilterEffect* last_effect = filter_effect_builder.BuildFilterEffect(
      operations, !context->OriginClean());
  if (last_effect) {
    // TODO(chrishtr): Taint the origin if needed. crbug.com/792506.
    resolved_filter_ =
        paint_filter_builder::Build(last_effect, kInterpolationSpaceSRGB);
  }

  return resolved_filter_;
}

sk_sp<PaintFilter> CanvasRenderingContext2DState::GetFilter(
    Element* style_resolution_host,
    IntSize canvas_size,
    CanvasRenderingContext2D* context) const {
  if (!filter_value_)
    return nullptr;

  // StyleResolverState cannot be used in frame-less documents.
  if (!style_resolution_host->GetDocument().GetFrame())
    return nullptr;

  if (!resolved_filter_) {
    // Update the filter value to the proper base URL if needed.
    if (filter_value_->MayContainUrl()) {
      style_resolution_host->GetDocument().UpdateStyleAndLayout();
      filter_value_->ReResolveUrl(style_resolution_host->GetDocument());
    }

    scoped_refptr<ComputedStyle> filter_style = ComputedStyle::Create();
    // Must set font in case the filter uses any font-relative units (em, ex)
    // If font_for_filter_ was never set (ie frame-less documents) use base font
    if (LIKELY(font_for_filter_.GetFontSelector())) {
      filter_style->SetFont(font_for_filter_);
    } else {
      const ComputedStyle* computed_style =
          style_resolution_host->GetDocument().GetComputedStyle();
      if (computed_style) {
        filter_style->SetFont(computed_style->GetFont());
      } else {
        return nullptr;
      }
    }
    StyleResolverState resolver_state(style_resolution_host->GetDocument(),
                                      *style_resolution_host,
                                      filter_style.get(), filter_style.get());
    resolver_state.SetStyle(filter_style);

    StyleBuilder::ApplyProperty(GetCSSPropertyFilter(), resolver_state,
                                *filter_value_);
    resolver_state.LoadPendingResources();

    // We can't reuse m_fillFlags and m_strokeFlags for the filter, since these
    // incorporate the global alpha, which isn't applicable here.
    PaintFlags fill_flags_for_filter;
    fill_style_->ApplyToFlags(fill_flags_for_filter);
    fill_flags_for_filter.setColor(fill_style_->PaintColor());
    PaintFlags stroke_flags_for_filter;
    stroke_style_->ApplyToFlags(stroke_flags_for_filter);
    stroke_flags_for_filter.setColor(stroke_style_->PaintColor());

    FilterEffectBuilder filter_effect_builder(
        FloatRect((FloatPoint()), FloatSize(canvas_size)),
        1.0f,  // Deliberately ignore zoom on the canvas element.
        &fill_flags_for_filter, &stroke_flags_for_filter);

    FilterEffect* last_effect = filter_effect_builder.BuildFilterEffect(
        filter_style->Filter(), !context->OriginClean());
    if (last_effect) {
      resolved_filter_ =
          paint_filter_builder::Build(last_effect, kInterpolationSpaceSRGB);
      if (resolved_filter_) {
        context->UpdateFilterReferences(filter_style->Filter());
        if (last_effect->OriginTainted())
          context->SetOriginTainted();
      }
    }
  }

  return resolved_filter_;
}

bool CanvasRenderingContext2DState::HasFilterForOffscreenCanvas(
    IntSize canvas_size,
    BaseRenderingContext2D* context) const {
  // Checking for a non-null m_filterValue isn't sufficient, since this value
  // might refer to a non-existent filter.
  return !!GetFilterForOffscreenCanvas(canvas_size, context);
}

bool CanvasRenderingContext2DState::HasFilter(
    Element* style_resolution_host,
    IntSize canvas_size,
    CanvasRenderingContext2D* context) const {
  // Checking for a non-null m_filterValue isn't sufficient, since this value
  // might refer to a non-existent filter.
  return !!GetFilter(style_resolution_host, canvas_size, context);
}

void CanvasRenderingContext2DState::ClearResolvedFilter() const {
  resolved_filter_.reset();
}

SkDrawLooper* CanvasRenderingContext2DState::EmptyDrawLooper() const {
  if (!empty_draw_looper_)
    empty_draw_looper_ = DrawLooperBuilder().DetachDrawLooper();

  return empty_draw_looper_.get();
}

SkDrawLooper* CanvasRenderingContext2DState::ShadowOnlyDrawLooper() const {
  if (!shadow_only_draw_looper_) {
    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(shadow_offset_, clampTo<float>(shadow_blur_),
                                  shadow_color_,
                                  DrawLooperBuilder::kShadowIgnoresTransforms,
                                  DrawLooperBuilder::kShadowRespectsAlpha);
    shadow_only_draw_looper_ = draw_looper_builder.DetachDrawLooper();
  }
  return shadow_only_draw_looper_.get();
}

SkDrawLooper* CanvasRenderingContext2DState::ShadowAndForegroundDrawLooper()
    const {
  if (!shadow_and_foreground_draw_looper_) {
    DrawLooperBuilder draw_looper_builder;
    draw_looper_builder.AddShadow(shadow_offset_, clampTo<float>(shadow_blur_),
                                  shadow_color_,
                                  DrawLooperBuilder::kShadowIgnoresTransforms,
                                  DrawLooperBuilder::kShadowRespectsAlpha);
    draw_looper_builder.AddUnmodifiedContent();
    shadow_and_foreground_draw_looper_ = draw_looper_builder.DetachDrawLooper();
  }
  return shadow_and_foreground_draw_looper_.get();
}

sk_sp<PaintFilter> CanvasRenderingContext2DState::ShadowOnlyImageFilter()
    const {
  if (!shadow_only_image_filter_) {
    const auto sigma = BlurRadiusToStdDev(shadow_blur_);
    shadow_only_image_filter_ = sk_make_sp<DropShadowPaintFilter>(
        shadow_offset_.Width(), shadow_offset_.Height(), sigma, sigma,
        shadow_color_, SkDropShadowImageFilter::kDrawShadowOnly_ShadowMode,
        nullptr);
  }
  return shadow_only_image_filter_;
}

sk_sp<PaintFilter>
CanvasRenderingContext2DState::ShadowAndForegroundImageFilter() const {
  if (!shadow_and_foreground_image_filter_) {
    const auto sigma = BlurRadiusToStdDev(shadow_blur_);
    shadow_and_foreground_image_filter_ = sk_make_sp<DropShadowPaintFilter>(
        shadow_offset_.Width(), shadow_offset_.Height(), sigma, sigma,
        shadow_color_,
        SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode, nullptr);
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
  shadow_offset_.SetWidth(clampTo<float>(x));
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowOffsetY(double y) {
  shadow_offset_.SetHeight(clampTo<float>(y));
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowBlur(double shadow_blur) {
  shadow_blur_ = clampTo<float>(shadow_blur);
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetShadowColor(SkColor shadow_color) {
  shadow_color_ = shadow_color;
  ShadowParameterChanged();
}

void CanvasRenderingContext2DState::SetFilter(const CSSValue* filter_value) {
  filter_value_ = filter_value;
  resolved_filter_.reset();
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
    image_smoothing_quality_ = kLow_SkFilterQuality;
  } else if (quality_string == "medium") {
    image_smoothing_quality_ = kMedium_SkFilterQuality;
  } else if (quality_string == "high") {
    image_smoothing_quality_ = kHigh_SkFilterQuality;
  } else {
    return;
  }
  UpdateFilterQuality();
}

String CanvasRenderingContext2DState::ImageSmoothingQuality() const {
  switch (image_smoothing_quality_) {
    case kLow_SkFilterQuality:
      return "low";
    case kMedium_SkFilterQuality:
      return "medium";
    case kHigh_SkFilterQuality:
      return "high";
    default:
      NOTREACHED();
      return "low";
  }
}

void CanvasRenderingContext2DState::UpdateFilterQuality() const {
  if (!image_smoothing_enabled_) {
    UpdateFilterQualityWithSkFilterQuality(kNone_SkFilterQuality);
  } else {
    UpdateFilterQualityWithSkFilterQuality(image_smoothing_quality_);
  }
}

void CanvasRenderingContext2DState::UpdateFilterQualityWithSkFilterQuality(
    const SkFilterQuality& filter_quality) const {
  stroke_flags_.setFilterQuality(filter_quality);
  fill_flags_.setFilterQuality(filter_quality);
  image_flags_.setFilterQuality(filter_quality);
}

bool CanvasRenderingContext2DState::ShouldDrawShadows() const {
  return AlphaChannel(shadow_color_) &&
         (shadow_blur_ || !shadow_offset_.IsZero());
}

const PaintFlags* CanvasRenderingContext2DState::GetFlags(
    PaintType paint_type,
    ShadowMode shadow_mode,
    ImageType image_type) const {
  PaintFlags* flags;
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
      FALLTHROUGH;
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
    flags->setLooper(sk_ref_sp(EmptyDrawLooper()));  // draw nothing
    flags->setImageFilter(nullptr);
    return flags;
  }

  if (shadow_mode == kDrawShadowOnly) {
    if (image_type == kNonOpaqueImage || filter_value_) {
      flags->setLooper(nullptr);
      flags->setImageFilter(ShadowOnlyImageFilter());
      return flags;
    }
    flags->setLooper(sk_ref_sp(ShadowOnlyDrawLooper()));
    flags->setImageFilter(nullptr);
    return flags;
  }

  DCHECK(shadow_mode == kDrawShadowAndForeground);
  if (image_type == kNonOpaqueImage) {
    flags->setLooper(nullptr);
    flags->setImageFilter(ShadowAndForegroundImageFilter());
    return flags;
  }
  flags->setLooper(sk_ref_sp(ShadowAndForegroundDrawLooper()));
  flags->setImageFilter(nullptr);
  return flags;
}

bool CanvasRenderingContext2DState::HasPattern() const {
  return FillStyle() && FillStyle()->GetCanvasPattern() &&
         FillStyle()->GetCanvasPattern()->GetPattern();
}

// Only to be used if the CanvasRenderingContext2DState has Pattern
bool CanvasRenderingContext2DState::PatternIsAccelerated() const {
  DCHECK(HasPattern());
  return FillStyle()->GetCanvasPattern()->GetPattern()->IsTextureBacked();
}

}  // namespace blink
