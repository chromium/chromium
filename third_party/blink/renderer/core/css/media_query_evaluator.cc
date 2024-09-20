/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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

#include "third_party/blink/renderer/core/css/media_query_evaluator.h"

#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/common/css/scripting.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_metric_builder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_surface.h"
#include "third_party/blink/public/mojom/device_posture/device_posture_provider.mojom-blink.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/core/css/css_container_values.h"
#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/css_resolution_units.h"
#include "third_party/blink/renderer/core/css/css_to_length_conversion_data.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/media_feature_names.h"
#include "third_party/blink/renderer/core/css/media_features.h"
#include "third_party/blink/renderer/core/css/media_list.h"
#include "third_party/blink/renderer/core/css/media_query.h"
#include "third_party/blink/renderer/core/css/media_values.h"
#include "third_party/blink/renderer/core/css/media_values_dynamic.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/media_type_names.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "ui/base/mojom/window_show_state.mojom-blink.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

namespace {

template <class T>
void MaybeRecordMediaFeatureValue(
    const MediaValues& media_values,
    const IdentifiableSurface::MediaFeatureName feature_name,
    T value) {
  Document* document = nullptr;
  if ((document = media_values.GetDocument()) &&
      (IdentifiabilityStudySettings::Get()->ShouldSampleType(
          IdentifiableSurface::Type::kMediaFeature)) &&
      !document->WasMediaFeatureEvaluated(static_cast<int>(feature_name))) {
    IdentifiableSurface surface = IdentifiableSurface::FromTypeAndToken(
        IdentifiableSurface::Type::kMediaFeature,
        IdentifiableToken(feature_name));

    IdentifiabilityMetricBuilder(document->UkmSourceID())
        .Add(surface, IdentifiableToken(value))
        .Record(document->UkmRecorder());
    document->SetMediaFeatureEvaluated(static_cast<int>(feature_name));
  }
}

KleeneValue KleeneOr(KleeneValue a, KleeneValue b) {
  switch (a) {
    case KleeneValue::kTrue:
      return KleeneValue::kTrue;
    case KleeneValue::kFalse:
      return b;
    case KleeneValue::kUnknown:
      return (b == KleeneValue::kTrue) ? KleeneValue::kTrue
                                       : KleeneValue::kUnknown;
  }
}

KleeneValue KleeneAnd(KleeneValue a, KleeneValue b) {
  switch (a) {
    case KleeneValue::kTrue:
      return b;
    case KleeneValue::kFalse:
      return KleeneValue::kFalse;
    case KleeneValue::kUnknown:
      return (b == KleeneValue::kFalse) ? KleeneValue::kFalse
                                        : KleeneValue::kUnknown;
  }
}

}  // namespace

using mojom::blink::DevicePostureType;
using mojom::blink::HoverType;
using mojom::blink::PointerType;

using EvalFunc = bool (*)(const MediaQueryExpValue&,
                          MediaQueryOperator,
                          const MediaValues&);
using FunctionMap = HashMap<StringImpl*, EvalFunc>;
static FunctionMap* g_function_map;

MediaQueryEvaluator::MediaQueryEvaluator(const char* accepted_media_type)
    : media_type_(accepted_media_type) {}

MediaQueryEvaluator::MediaQueryEvaluator(LocalFrame* frame)
    : media_values_(MediaValues::CreateDynamicIfFrameExists(frame)) {}

MediaQueryEvaluator::MediaQueryEvaluator(const MediaValues* container_values)
    : media_values_(container_values) {}

MediaQueryEvaluator::~MediaQueryEvaluator() = default;

void MediaQueryEvaluator::Trace(Visitor* visitor) const {
  visitor->Trace(media_values_);
}

const String MediaQueryEvaluator::MediaType() const {
  // If a static mediaType was given by the constructor, we use it here.
  if (!media_type_.empty()) {
    return media_type_;
  }
  // Otherwise, we get one from mediaValues (which may be dynamic or cached).
  if (media_values_) {
    return media_values_->MediaType();
  }
  return g_null_atom;
}

bool MediaQueryEvaluator::MediaTypeMatch(
    const String& media_type_to_match) const {
  return media_type_to_match.empty() ||
         EqualIgnoringASCIICase(media_type_to_match, media_type_names::kAll) ||
         EqualIgnoringASCIICase(media_type_to_match, MediaType());
}

static bool ApplyRestrictor(MediaQuery::RestrictorType r, KleeneValue value) {
  if (value == KleeneValue::kUnknown) {
    return false;
  }
  if (r == MediaQuery::RestrictorType::kNot) {
    return value == KleeneValue::kFalse;
  }
  return value == KleeneValue::kTrue;
}

bool MediaQueryEvaluator::Eval(const MediaQuery& query) const {
  return Eval(query, nullptr /* result_flags */);
}

bool MediaQueryEvaluator::Eval(const MediaQuery& query,
                               MediaQueryResultFlags* result_flags) const {
  if (!MediaTypeMatch(query.MediaType())) {
    return ApplyRestrictor(query.Restrictor(), KleeneValue::kFalse);
  }
  if (!query.ExpNode()) {
    return ApplyRestrictor(query.Restrictor(), KleeneValue::kTrue);
  }
  return ApplyRestrictor(query.Restrictor(),
                         Eval(*query.ExpNode(), result_flags));
}

bool MediaQueryEvaluator::Eval(const MediaQuerySet& query_set) const {
  return Eval(query_set, nullptr /* result_flags */);
}

bool MediaQueryEvaluator::Eval(const MediaQuerySet& query_set,
                               MediaQueryResultFlags* result_flags) const {
  const HeapVector<Member<const MediaQuery>>& queries = query_set.QueryVector();
  if (!queries.size()) {
    return true;  // Empty query list evaluates to true.
  }

  // Iterate over queries, stop if any of them eval to true (OR semantics).
  bool result = false;
  for (wtf_size_t i = 0; i < queries.size() && !result; ++i) {
    result = Eval(*queries[i], result_flags);
  }

  return result;
}

KleeneValue MediaQueryEvaluator::Eval(const MediaQueryExpNode& node) const {
  return Eval(node, nullptr /* result_flags */);
}

KleeneValue MediaQueryEvaluator::Eval(
    const MediaQueryExpNode& node,
    MediaQueryResultFlags* result_flags) const {
  if (auto* n = DynamicTo<MediaQueryNestedExpNode>(node)) {
    return Eval(n->Operand(), result_flags);
  }
  if (auto* n = DynamicTo<MediaQueryFunctionExpNode>(node)) {
    return Eval(n->Operand(), result_flags);
  }
  if (auto* n = DynamicTo<MediaQueryNotExpNode>(node)) {
    return EvalNot(n->Operand(), result_flags);
  }
  if (auto* n = DynamicTo<MediaQueryAndExpNode>(node)) {
    return EvalAnd(n->Left(), n->Right(), result_flags);
  }
  if (auto* n = DynamicTo<MediaQueryOrExpNode>(node)) {
    return EvalOr(n->Left(), n->Right(), result_flags);
  }
  if (IsA<MediaQueryUnknownExpNode>(node)) {
    return KleeneValue::kUnknown;
  }
  return EvalFeature(To<MediaQueryFeatureExpNode>(node), result_flags);
}

KleeneValue MediaQueryEvaluator::EvalNot(
    const MediaQueryExpNode& operand_node,
    MediaQueryResultFlags* result_flags) const {
  switch (Eval(operand_node, result_flags)) {
    case KleeneValue::kTrue:
      return KleeneValue::kFalse;
    case KleeneValue::kFalse:
      return KleeneValue::kTrue;
    case KleeneValue::kUnknown:
      return KleeneValue::kUnknown;
  }
}

KleeneValue MediaQueryEvaluator::EvalAnd(
    const MediaQueryExpNode& left_node,
    const MediaQueryExpNode& right_node,
    MediaQueryResultFlags* result_flags) const {
  KleeneValue left = Eval(left_node, result_flags);
  // Short-circuiting before calling Eval on |right_node| prevents
  // unnecessary entries in |results|.
  if (left == KleeneValue::kFalse) {
    return left;
  }
  return KleeneAnd(left, Eval(right_node, result_flags));
}

KleeneValue MediaQueryEvaluator::EvalOr(
    const MediaQueryExpNode& left_node,
    const MediaQueryExpNode& right_node,
    MediaQueryResultFlags* result_flags) const {
  KleeneValue left = Eval(left_node, result_flags);
  // Short-circuiting before calling Eval on |right_node| prevents
  // unnecessary entries in |results|.
  if (left == KleeneValue::kTrue) {
    return left;
  }
  return KleeneOr(left, Eval(right_node, result_flags));
}

bool MediaQueryEvaluator::DidResultsChange(
    const HeapVector<MediaQuerySetResult>& result_flags) const {
  for (const auto& result : result_flags) {
    if (result.Result() != Eval(result.MediaQueries())) {
      return true;
    }
  }
  return false;
}

// As per
// https://w3c.github.io/csswg-drafts/mediaqueries/#false-in-the-negative-range
static bool HandleNegativeMediaFeatureValue(MediaQueryOperator op) {
  switch (op) {
    case MediaQueryOperator::kLe:
    case MediaQueryOperator::kLt:
    case MediaQueryOperator::kEq:
    case MediaQueryOperator::kNone:
      return false;
    case MediaQueryOperator::kGt:
    case MediaQueryOperator::kGe:
      return true;
  }
}

template <typename T>
bool CompareValue(T actual_value, T query_value, MediaQueryOperator op) {
  if (query_value < T(0)) {
    return HandleNegativeMediaFeatureValue(op);
  }
  switch (op) {
    case MediaQueryOperator::kGe:
      return actual_value >= query_value;
    case MediaQueryOperator::kLe:
      return actual_value <= query_value;
    case MediaQueryOperator::kEq:
    case MediaQueryOperator::kNone:
      return actual_value == query_value;
    case MediaQueryOperator::kLt:
      return actual_value < query_value;
    case MediaQueryOperator::kGt:
      return actual_value > query_value;
  }
  return false;
}

bool CompareDoubleValue(double actual_value,
                        double query_value,
                        MediaQueryOperator op) {
  if (query_value < 0) {
    return HandleNegativeMediaFeatureValue(op);
  }
  const double precision = LayoutUnit::Epsilon();
  switch (op) {
    case MediaQueryOperator::kGe:
      return actual_value >= (query_value - precision);
    case MediaQueryOperator::kLe:
      return actual_value <= (query_value + precision);
    case MediaQueryOperator::kEq:
    case MediaQueryOperator::kNone:
      return std::abs(actual_value - query_value) <= precision;
    case MediaQueryOperator::kLt:
      return actual_value < query_value;
    case MediaQueryOperator::kGt:
      return actual_value > query_value;
  }
  return false;
}

static bool CompareAspectRatioValue(const MediaQueryExpValue& value,
                                    int width,
                                    int height,
                                    MediaQueryOperator op,
                                    const MediaValues& media_values) {
  if (value.IsRatio()) {
    return CompareDoubleValue(
        static_cast<double>(width) * value.Denominator(media_values),
        static_cast<double>(height) * value.Numerator(media_values), op);
  }
  return false;
}

static bool NumberValue(const MediaQueryExpValue& value,
                        float& result,
                        const MediaValues& media_values) {
  if (value.IsNumber()) {
    result = ClampTo<float>(value.Value(media_values));
    return true;
  }
  return false;
}

static bool ColorMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator op,
                                  const MediaValues& media_values) {
  float number;
  int bits_per_component = media_values.ColorBitsPerComponent();
  MaybeRecordMediaFeatureValue(media_values,
                               IdentifiableSurface::MediaFeatureName::kColor,
                               bits_per_component);
  if (value.IsValid()) {
    return NumberValue(value, number, media_values) &&
           CompareValue(bits_per_component, static_cast<int>(number), op);
  }

  return bits_per_component != 0;
}

static bool ColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  // FIXME: We currently assume that we do not support indexed displays, as it
  // is unknown how to retrieve the information if the display mode is indexed.
  // This matches Firefox.
  if (!value.IsValid()) {
    return false;
  }

  // Acording to spec, if the device does not use a color lookup table, the
  // value is zero.
  float number;
  return NumberValue(value, number, media_values) &&
         CompareValue(0, static_cast<int>(number), op);
}

static bool MonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  float number;
  int bits_per_component = media_values.MonochromeBitsPerComponent();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kMonochrome,
      bits_per_component);
  if (value.IsValid()) {
    return NumberValue(value, number, media_values) &&
           CompareValue(bits_per_component, static_cast<int>(number), op);
  }
  return bits_per_component != 0;
}

static bool DisplayModeMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator,
                                        const MediaValues& media_values) {
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that displayModeMediaFeature is enabled in the
  // browser.
  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  blink::mojom::DisplayMode mode = media_values.DisplayMode();

  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kDisplayMode, mode);

  switch (value.Id()) {
    case CSSValueID::kFullscreen:
      return mode == blink::mojom::DisplayMode::kFullscreen;
    case CSSValueID::kStandalone:
      return mode == blink::mojom::DisplayMode::kStandalone;
    case CSSValueID::kMinimalUi:
      return mode == blink::mojom::DisplayMode::kMinimalUi;
    case CSSValueID::kBrowser:
      return mode == blink::mojom::DisplayMode::kBrowser;
    case CSSValueID::kWindowControlsOverlay:
      return mode == blink::mojom::DisplayMode::kWindowControlsOverlay;
    case CSSValueID::kBorderless:
      return mode == blink::mojom::DisplayMode::kBorderless;
    case CSSValueID::kTabbed:
      return mode == blink::mojom::DisplayMode::kTabbed;
    case CSSValueID::kPictureInPicture:
      return mode == blink::mojom::DisplayMode::kPictureInPicture;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

// WindowShowState is mapped into a CSS media query value `display-state`.
static bool DisplayStateMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator,
                                         const MediaValues& media_values) {
  // No value = boolean context:
  // https://w3c.github.io/csswg-drafts/mediaqueries/#mq-boolean-context
  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  ui::mojom::blink::WindowShowState state = media_values.WindowShowState();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kDisplayState,
      state);

  switch (value.Id()) {
    case CSSValueID::kFullscreen:
      return state == ui::mojom::blink::WindowShowState::kFullscreen;
    case CSSValueID::kMaximized:
      return state == ui::mojom::blink::WindowShowState::kMaximized;
    case CSSValueID::kMinimized:
      return state == ui::mojom::blink::WindowShowState::kMinimized;
    case CSSValueID::kNormal:
      return state == ui::mojom::blink::WindowShowState::kDefault ||
             state == ui::mojom::blink::WindowShowState::kInactive ||
             state == ui::mojom::blink::WindowShowState::kNormal;
    default:
      NOTREACHED();
  }
}

static bool ResizableMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  // No value = boolean context:
  // https://w3c.github.io/csswg-drafts/mediaqueries/#mq-boolean-context
  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  bool resizable = media_values.Resizable();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kResizable,
      resizable);

  return (resizable && value.Id() == CSSValueID::kTrue) ||
         (!resizable && value.Id() == CSSValueID::kFalse);
}

static bool OrientationMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator,
                                        const MediaValues& media_values) {
  int width = *media_values.Width();
  int height = *media_values.Height();

  if (value.IsId()) {
    if (width > height) {  // Square viewport is portrait.
      MaybeRecordMediaFeatureValue(
          media_values, IdentifiableSurface::MediaFeatureName::kOrientation,
          CSSValueID::kLandscape);
      return CSSValueID::kLandscape == value.Id();
    }

    MaybeRecordMediaFeatureValue(
        media_values, IdentifiableSurface::MediaFeatureName::kOrientation,
        CSSValueID::kPortrait);
    return CSSValueID::kPortrait == value.Id();
  }

  // Expression (orientation) evaluates to true if width and height >= 0.
  return height >= 0 && width >= 0;
}

static bool AspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  double aspect_ratio =
      std::max(*media_values.Width(), *media_values.Height()) /
      std::min(*media_values.Width(), *media_values.Height());
  MaybeRecordMediaFeatureValue(
      media_values,
      IdentifiableSurface::MediaFeatureName::kAspectRatioNormalized,
      aspect_ratio);
  if (value.IsValid()) {
    return CompareAspectRatioValue(value, *media_values.Width(),
                                   *media_values.Height(), op, media_values);
  }

  // ({,min-,max-}aspect-ratio)
  // assume if we have a device, its aspect ratio is non-zero.
  return true;
}

static bool DeviceAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                              MediaQueryOperator op,
                                              const MediaValues& media_values) {
  if (value.IsValid()) {
    return CompareAspectRatioValue(value, media_values.DeviceWidth(),
                                   media_values.DeviceHeight(), op,
                                   media_values);
  }

  // ({,min-,max-}device-aspect-ratio)
  // assume if we have a device, its aspect ratio is non-zero.
  return true;
}

static bool DynamicRangeMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator op,
                                         const MediaValues& media_values) {
  if (!value.IsId()) {
    return false;
  }

  switch (value.Id()) {
    case CSSValueID::kStandard:
      MaybeRecordMediaFeatureValue(
          media_values, IdentifiableSurface::MediaFeatureName::kDynamicRange,
          CSSValueID::kStandard);
      return true;

    case CSSValueID::kHigh:
      MaybeRecordMediaFeatureValue(
          media_values, IdentifiableSurface::MediaFeatureName::kDynamicRange,
          media_values.DeviceSupportsHDR());
      return media_values.DeviceSupportsHDR();

    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool VideoDynamicRangeMediaFeatureEval(const MediaQueryExpValue& value,
                                              MediaQueryOperator op,
                                              const MediaValues& media_values) {
  // For now, Chrome makes no distinction between video-dynamic-range and
  // dynamic-range
  return DynamicRangeMediaFeatureEval(value, op, media_values);
}

static bool EvalResolution(const MediaQueryExpValue& value,
                           MediaQueryOperator op,
                           const MediaValues& media_values) {
  // According to MQ4, only 'screen', 'print' and 'speech' may match.
  // FIXME: What should speech match?
  // https://www.w3.org/Style/CSS/Tracker/issues/348
  float actual_resolution = 0;

  // This checks the actual media type applied to the document, and we know
  // this method only got called if this media type matches the one defined
  // in the query. Thus, if if the document's media type is "print", the
  // media type of the query will either be "print" or "all".
  if (EqualIgnoringASCIICase(media_values.MediaType(),
                             media_type_names::kScreen)) {
    actual_resolution = ClampTo<float>(media_values.DevicePixelRatio());
  } else if (EqualIgnoringASCIICase(media_values.MediaType(),
                                    media_type_names::kPrint)) {
    // The resolution of images while printing should not depend on the DPI
    // of the screen. Until we support proper ways of querying this info
    // we use 300px which is considered minimum for current printers.
    actual_resolution = 300 / kCssPixelsPerInch;
  }

  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kResolution,
      actual_resolution);

  if (!value.IsValid()) {
    return !!actual_resolution;
  }

  if (value.IsNumber()) {
    return CompareValue(actual_resolution,
                        ClampTo<float>(value.Value(media_values)), op);
  }

  if (!value.IsResolution()) {
    return false;
  }

  double dppx_factor = CSSPrimitiveValue::ConversionToCanonicalUnitsScaleFactor(
      CSSPrimitiveValue::UnitType::kDotsPerPixel);
  float value_in_dppx = ClampTo<float>(value.Value(media_values) / dppx_factor);
  if (value.IsDotsPerCentimeter()) {
    // To match DPCM to DPPX values, we limit to 2 decimal points.
    // The https://drafts.csswg.org/css-values/#absolute-lengths recommends
    // "that the pixel unit refer to the whole number of device pixels that best
    // approximates the reference pixel". With that in mind, allowing 2 decimal
    // point precision seems appropriate.
    return CompareValue(floorf(0.5 + 100 * actual_resolution) / 100,
                        floorf(0.5 + 100 * value_in_dppx) / 100, op);
  }

  return CompareValue(actual_resolution, value_in_dppx, op);
}

static bool DevicePixelRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                             MediaQueryOperator op,
                                             const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedDevicePixelRatioMediaFeature);

  return (!value.IsValid() || value.IsNumber()) &&
         EvalResolution(value, op, media_values);
}

static bool ResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  return (!value.IsValid() || value.IsResolution()) &&
         EvalResolution(value, op, media_values);
}

static bool GridMediaFeatureEval(const MediaQueryExpValue& value,
                                 MediaQueryOperator op,
                                 const MediaValues& media_values) {
  // if output device is bitmap, grid: 0 == true
  // assume we have bitmap device
  float number;
  if (value.IsValid() && NumberValue(value, number, media_values)) {
    return CompareValue(static_cast<int>(number), 0, op);
  }
  return false;
}

static bool ComputeLength(const MediaQueryExpValue& value,
                          const MediaValues& media_values,
                          double& result) {
  if (value.IsNumber()) {
    result = ClampTo<int>(value.Value(media_values));
    return !media_values.StrictMode() || !result;
  }

  if (value.IsValue()) {
    result = value.Value(media_values);
    return true;
  }

  return false;
}

static bool ComputeLengthAndCompare(const MediaQueryExpValue& value,
                                    MediaQueryOperator op,
                                    const MediaValues& media_values,
                                    double compare_to_value) {
  double length;
  return ComputeLength(value, media_values, length) &&
         CompareDoubleValue(compare_to_value, length, op);
}

static bool DeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator op,
                                         const MediaValues& media_values) {
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values,
                                   media_values.DeviceHeight());
  }

  // ({,min-,max-}device-height)
  // assume if we have a device, assume non-zero
  return true;
}

static bool DeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values,
                                   media_values.DeviceWidth());
  }

  // ({,min-,max-}device-width)
  // assume if we have a device, assume non-zero
  return true;
}

static bool HeightMediaFeatureEval(const MediaQueryExpValue& value,
                                   MediaQueryOperator op,
                                   const MediaValues& media_values) {
  double height = *media_values.Height();
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values, height);
  }

  return height;
}

static bool WidthMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator op,
                                  const MediaValues& media_values) {
  double width = *media_values.Width();
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values, width);
  }

  return width;
}

static bool InlineSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator op,
                                       const MediaValues& media_values) {
  double size = *media_values.InlineSize();
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values, size);
  }

  return size;
}

static bool BlockSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator op,
                                      const MediaValues& media_values) {
  double size = *media_values.BlockSize();
  if (value.IsValid()) {
    return ComputeLengthAndCompare(value, op, media_values, size);
  }

  return size;
}

// Rest of the functions are trampolines which set the prefix according to the
// media feature expression used.

static bool MinColorMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return ColorMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxColorMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return ColorMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ColorIndexMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxColorIndexMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ColorIndexMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool MinMonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return MonochromeMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxMonochromeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return MonochromeMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool MinAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return AspectRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                     media_values);
}

static bool MaxAspectRatioMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return AspectRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                     media_values);
}

static bool MinDeviceAspectRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  return DeviceAspectRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                           media_values);
}

static bool MaxDeviceAspectRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  return DeviceAspectRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                           media_values);
}

static bool MinDevicePixelRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedMinDevicePixelRatioMediaFeature);

  return DevicePixelRatioMediaFeatureEval(value, MediaQueryOperator::kGe,
                                          media_values);
}

static bool MaxDevicePixelRatioMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedMaxDevicePixelRatioMediaFeature);

  return DevicePixelRatioMediaFeatureEval(value, MediaQueryOperator::kLe,
                                          media_values);
}

static bool MinHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  return HeightMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  return HeightMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return WidthMediaFeatureEval(value, MediaQueryOperator::kGe, media_values);
}

static bool MaxWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  return WidthMediaFeatureEval(value, MediaQueryOperator::kLe, media_values);
}

static bool MinBlockSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator,
                                         const MediaValues& media_values) {
  return BlockSizeMediaFeatureEval(value, MediaQueryOperator::kGe,
                                   media_values);
}

static bool MaxBlockSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator,
                                         const MediaValues& media_values) {
  return BlockSizeMediaFeatureEval(value, MediaQueryOperator::kLe,
                                   media_values);
}

static bool MinInlineSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return InlineSizeMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxInlineSizeMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return InlineSizeMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool MinDeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  return DeviceHeightMediaFeatureEval(value, MediaQueryOperator::kGe,
                                      media_values);
}

static bool MaxDeviceHeightMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  return DeviceHeightMediaFeatureEval(value, MediaQueryOperator::kLe,
                                      media_values);
}

static bool MinDeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return DeviceWidthMediaFeatureEval(value, MediaQueryOperator::kGe,
                                     media_values);
}

static bool MaxDeviceWidthMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  return DeviceWidthMediaFeatureEval(value, MediaQueryOperator::kLe,
                                     media_values);
}

static bool MinResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ResolutionMediaFeatureEval(value, MediaQueryOperator::kGe,
                                    media_values);
}

static bool MaxResolutionMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  return ResolutionMediaFeatureEval(value, MediaQueryOperator::kLe,
                                    media_values);
}

static bool Transform3dMediaFeatureEval(const MediaQueryExpValue& value,
                                        MediaQueryOperator op,
                                        const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefixedTransform3dMediaFeature);

  bool return_value_if_no_parameter;
  int have3d_rendering;

  bool three_d_enabled = media_values.ThreeDEnabled();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kTransform3d,
      three_d_enabled);

  return_value_if_no_parameter = three_d_enabled;
  have3d_rendering = three_d_enabled ? 1 : 0;

  if (value.IsValid()) {
    float number;
    return NumberValue(value, number, media_values) &&
           CompareValue(have3d_rendering, static_cast<int>(number), op);
  }
  return return_value_if_no_parameter;
}

static bool HoverMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator,
                                  const MediaValues& media_values) {
  HoverType hover = media_values.PrimaryHoverType();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kHover, hover);

  if (!value.IsValid()) {
    return hover != HoverType::kHoverNone;
  }

  if (!value.IsId()) {
    return false;
  }

  return (hover == HoverType::kHoverNone && value.Id() == CSSValueID::kNone) ||
         (hover == HoverType::kHoverHoverType &&
          value.Id() == CSSValueID::kHover);
}

static bool AnyHoverMediaFeatureEval(const MediaQueryExpValue& value,
                                     MediaQueryOperator,
                                     const MediaValues& media_values) {
  int available_hover_types = media_values.AvailableHoverTypes();
  MaybeRecordMediaFeatureValue(media_values,
                               IdentifiableSurface::MediaFeatureName::kAnyHover,
                               available_hover_types);

  if (!value.IsValid()) {
    return available_hover_types & ~static_cast<int>(HoverType::kHoverNone);
  }

  if (!value.IsId()) {
    return false;
  }

  switch (value.Id()) {
    case CSSValueID::kNone:
      return available_hover_types & static_cast<int>(HoverType::kHoverNone);
    case CSSValueID::kHover:
      return available_hover_types &
             static_cast<int>(HoverType::kHoverHoverType);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool OriginTrialTestMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  // The test feature only supports a 'no-value' parsing. So if we've gotten
  // to this point it will always match.
  DCHECK(!value.IsValid());
  return true;
}

static bool PointerMediaFeatureEval(const MediaQueryExpValue& value,
                                    MediaQueryOperator,
                                    const MediaValues& media_values) {
  PointerType pointer = media_values.PrimaryPointerType();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kPointer, pointer);

  if (!value.IsValid()) {
    return pointer != PointerType::kPointerNone;
  }

  if (!value.IsId()) {
    return false;
  }

  return (pointer == PointerType::kPointerNone &&
          value.Id() == CSSValueID::kNone) ||
         (pointer == PointerType::kPointerCoarseType &&
          value.Id() == CSSValueID::kCoarse) ||
         (pointer == PointerType::kPointerFineType &&
          value.Id() == CSSValueID::kFine);
}

static bool PrefersReducedMotionMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersReducedMotionMediaFeature);

  MaybeRecordMediaFeatureValue(
      media_values,
      IdentifiableSurface::MediaFeatureName::kPrefersReducedMotion,
      media_values.PrefersReducedMotion());

  // If the value is not valid, this was passed without an argument. In that
  // case, it implicitly resolves to 'reduce'.
  if (!value.IsValid()) {
    return media_values.PrefersReducedMotion();
  }

  if (!value.IsId()) {
    return false;
  }

  return (value.Id() == CSSValueID::kNoPreference) ^
         media_values.PrefersReducedMotion();
}

static bool PrefersReducedDataMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersReducedDataMediaFeature);
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kPrefersReducedData,
      media_values.PrefersReducedData());

  if (!value.IsValid()) {
    return media_values.PrefersReducedData();
  }

  if (!value.IsId()) {
    return false;
  }

  return (value.Id() == CSSValueID::kNoPreference) ^
         media_values.PrefersReducedData();
}

static bool PrefersReducedTransparencyMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersReducedTransparencyMediaFeature);
  MaybeRecordMediaFeatureValue(
      media_values,
      IdentifiableSurface::MediaFeatureName::kPrefersReducedTransparency,
      media_values.PrefersReducedTransparency());

  if (!value.IsValid()) {
    return media_values.PrefersReducedTransparency();
  }

  if (!value.IsId()) {
    return false;
  }

  return (value.Id() == CSSValueID::kNoPreference) ^
         media_values.PrefersReducedTransparency();
}

static bool AnyPointerMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator,
                                       const MediaValues& media_values) {
  int available_pointers = media_values.AvailablePointerTypes();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kAnyPointer,
      available_pointers);

  if (!value.IsValid()) {
    return available_pointers & ~static_cast<int>(PointerType::kPointerNone);
  }

  if (!value.IsId()) {
    return false;
  }

  switch (value.Id()) {
    case CSSValueID::kCoarse:
      return available_pointers &
             static_cast<int>(PointerType::kPointerCoarseType);
    case CSSValueID::kFine:
      return available_pointers &
             static_cast<int>(PointerType::kPointerFineType);
    case CSSValueID::kNone:
      return available_pointers & static_cast<int>(PointerType::kPointerNone);
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool ScanMediaFeatureEval(const MediaQueryExpValue& value,
                                 MediaQueryOperator,
                                 const MediaValues& media_values) {
  MaybeRecordMediaFeatureValue(media_values,
                               IdentifiableSurface::MediaFeatureName::kScan,
                               media_values.MediaType().Utf8());

  // Scan only applies to 'tv' media.
  if (!EqualIgnoringASCIICase(media_values.MediaType(),
                              media_type_names::kTv)) {
    return false;
  }

  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  // If a platform interface supplies progressive/interlace info for TVs in the
  // future, it needs to be handled here. For now, assume a modern TV with
  // progressive display.
  return (value.Id() == CSSValueID::kProgressive);
}

static bool ColorGamutMediaFeatureEval(const MediaQueryExpValue& value,
                                       MediaQueryOperator,
                                       const MediaValues& media_values) {
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that colorGamutMediaFeature is enabled in the
  // browser.
  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  DCHECK(value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3 ||
         value.Id() == CSSValueID::kRec2020);

  ColorSpaceGamut gamut = media_values.ColorGamut();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kColorGamut, gamut);

  switch (gamut) {
    case ColorSpaceGamut::kUnknown:
    case ColorSpaceGamut::kLessThanNTSC:
    case ColorSpaceGamut::NTSC:
    case ColorSpaceGamut::SRGB:
      return value.Id() == CSSValueID::kSRGB;
    case ColorSpaceGamut::kAlmostP3:
    case ColorSpaceGamut::P3:
    case ColorSpaceGamut::kAdobeRGB:
    case ColorSpaceGamut::kWide:
      return value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3;
    case ColorSpaceGamut::BT2020:
    case ColorSpaceGamut::kProPhoto:
    case ColorSpaceGamut::kUltraWide:
      return value.Id() == CSSValueID::kSRGB || value.Id() == CSSValueID::kP3 ||
             value.Id() == CSSValueID::kRec2020;
    case ColorSpaceGamut::kEnd:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  // This is for some compilers that do not understand that it can't be reached.
  NOTREACHED_IN_MIGRATION();
  return false;
}

static bool PrefersColorSchemeMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersColorSchemeMediaFeature);

  auto preferred_scheme = media_values.GetPreferredColorScheme();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kPrefersColorScheme,
      preferred_scheme);

  if (!value.IsValid()) {
    return true;
  }

  if (!value.IsId()) {
    return false;
  }

  return (preferred_scheme == mojom::blink::PreferredColorScheme::kDark &&
          value.Id() == CSSValueID::kDark) ||
         (preferred_scheme == mojom::blink::PreferredColorScheme::kLight &&
          value.Id() == CSSValueID::kLight);
}

static bool PrefersContrastMediaFeatureEval(const MediaQueryExpValue& value,
                                            MediaQueryOperator,
                                            const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kPrefersContrastMediaFeature);

  auto preferred_contrast = media_values.GetPreferredContrast();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kPrefersContrast,
      preferred_contrast);

  if (!value.IsValid()) {
    return preferred_contrast != mojom::blink::PreferredContrast::kNoPreference;
  }

  if (!value.IsId()) {
    return false;
  }

  switch (value.Id()) {
    case CSSValueID::kMore:
      return preferred_contrast == mojom::blink::PreferredContrast::kMore;
    case CSSValueID::kLess:
      return preferred_contrast == mojom::blink::PreferredContrast::kLess;
    case CSSValueID::kNoPreference:
      return preferred_contrast ==
             mojom::blink::PreferredContrast::kNoPreference;
    case CSSValueID::kCustom:
      return preferred_contrast == mojom::blink::PreferredContrast::kCustom;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool ForcedColorsMediaFeatureEval(const MediaQueryExpValue& value,
                                         MediaQueryOperator,
                                         const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kForcedColorsMediaFeature);

  ForcedColors forced_colors = media_values.GetForcedColors();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kForcedColors,
      forced_colors);

  if (!value.IsValid()) {
    return forced_colors != ForcedColors::kNone;
  }

  if (!value.IsId()) {
    return false;
  }

  // Check the forced colors against value.Id().
  return (forced_colors == ForcedColors::kNone &&
          value.Id() == CSSValueID::kNone) ||
         (forced_colors != ForcedColors::kNone &&
          value.Id() == CSSValueID::kActive);
}

static bool NavigationControlsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator,
    const MediaValues& media_values) {
  NavigationControls navigation_controls = media_values.GetNavigationControls();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kNavigationControls,
      navigation_controls);

  if (!value.IsValid()) {
    return navigation_controls != NavigationControls::kNone;
  }

  if (!value.IsId()) {
    return false;
  }

  // Check the navigation controls against value.Id().
  return (navigation_controls == NavigationControls::kNone &&
          value.Id() == CSSValueID::kNone) ||
         (navigation_controls == NavigationControls::kBackButton &&
          value.Id() == CSSValueID::kBackButton);
}

static bool HorizontalViewportSegmentsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator op,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kViewportSegmentsMediaFeature);
  UseCounter::Count(media_values.GetDocument(), WebFeature::kFoldableAPIs);
  int horizontal_viewport_segments =
      media_values.GetHorizontalViewportSegments();

  MaybeRecordMediaFeatureValue(
      media_values,
      IdentifiableSurface::MediaFeatureName::kHorizontalViewportSegments,
      horizontal_viewport_segments);

  if (!value.IsValid()) {
    return true;
  }

  float number;
  return NumberValue(value, number, media_values) &&
         CompareValue(horizontal_viewport_segments, static_cast<int>(number),
                      op);
}

static bool VerticalViewportSegmentsMediaFeatureEval(
    const MediaQueryExpValue& value,
    MediaQueryOperator op,
    const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kViewportSegmentsMediaFeature);
  UseCounter::Count(media_values.GetDocument(), WebFeature::kFoldableAPIs);
  int vertical_viewport_segments = media_values.GetVerticalViewportSegments();

  MaybeRecordMediaFeatureValue(
      media_values,
      IdentifiableSurface::MediaFeatureName::kVerticalViewportSegments,
      vertical_viewport_segments);

  if (!value.IsValid()) {
    return true;
  }

  float number;
  return NumberValue(value, number, media_values) &&
         CompareValue(vertical_viewport_segments, static_cast<int>(number), op);
}

static bool OverflowInlineMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  bool can_scroll = !EqualIgnoringASCIICase(media_values.MediaType(),
                                            media_type_names::kPrint);
  // No value = boolean context:
  // https://w3c.github.io/csswg-drafts/mediaqueries/#mq-boolean-context
  if (!value.IsValid()) {
    return can_scroll;
  }
  DCHECK(value.IsId());
  switch (value.Id()) {
    case CSSValueID::kNone:
      return !can_scroll;
    case CSSValueID::kScroll:
      return can_scroll;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool OverflowBlockMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  bool can_scroll = !EqualIgnoringASCIICase(media_values.MediaType(),
                                            media_type_names::kPrint);
  // No value = boolean context:
  // https://w3c.github.io/csswg-drafts/mediaqueries/#mq-boolean-context
  if (!value.IsValid()) {
    return true;
  }
  DCHECK(value.IsId());
  switch (value.Id()) {
    case CSSValueID::kNone:
      return false;
    case CSSValueID::kScroll:
      return can_scroll;
    case CSSValueID::kPaged:
      return !can_scroll;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool DevicePostureMediaFeatureEval(const MediaQueryExpValue& value,
                                          MediaQueryOperator,
                                          const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kDevicePostureMediaFeature);
  UseCounter::Count(media_values.GetDocument(), WebFeature::kFoldableAPIs);
  // isValid() is false if there is no parameter. Without parameter we should
  // return true to indicate that device posture is enabled in the
  // browser.
  if (!value.IsValid()) {
    return true;
  }

  DCHECK(value.IsId());

  DevicePostureType device_posture = media_values.GetDevicePosture();
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kDevicePosture,
      device_posture);

  switch (value.Id()) {
    case CSSValueID::kContinuous:
      return device_posture == DevicePostureType::kContinuous;
    case CSSValueID::kFolded:
      return device_posture == DevicePostureType::kFolded;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool UpdateMediaFeatureEval(const MediaQueryExpValue& value,
                                   MediaQueryOperator,
                                   const MediaValues& media_values) {
  bool can_update = !EqualIgnoringASCIICase(media_values.MediaType(),
                                            media_type_names::kPrint);
  // No value = boolean context:
  // https://w3c.github.io/csswg-drafts/mediaqueries/#mq-boolean-context
  if (!value.IsValid()) {
    return can_update;
  }
  const auto& device_update_ability_type =
      media_values.OutputDeviceUpdateAbilityType();
  DCHECK(value.IsId());
  switch (value.Id()) {
    case CSSValueID::kNone:
      return !can_update;
    case CSSValueID::kSlow:
      return can_update &&
             device_update_ability_type ==
                 mojom::blink::OutputDeviceUpdateAbilityType::kSlowType;
    case CSSValueID::kFast:
      return can_update &&
             device_update_ability_type ==
                 mojom::blink::OutputDeviceUpdateAbilityType::kFastType;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool StuckMediaFeatureEval(const MediaQueryExpValue& value,
                                  MediaQueryOperator op,
                                  const MediaValues& media_values) {
  if (!value.IsValid()) {
    return media_values.Stuck();
  }

  switch (value.Id()) {
    case CSSValueID::kNone:
      return media_values.StuckHorizontal() == ContainerStuckPhysical::kNo &&
             media_values.StuckVertical() == ContainerStuckPhysical::kNo;
    case CSSValueID::kTop:
      return media_values.StuckVertical() == ContainerStuckPhysical::kTop;
    case CSSValueID::kLeft:
      return media_values.StuckHorizontal() == ContainerStuckPhysical::kLeft;
    case CSSValueID::kBottom:
      return media_values.StuckVertical() == ContainerStuckPhysical::kBottom;
    case CSSValueID::kRight:
      return media_values.StuckHorizontal() == ContainerStuckPhysical::kRight;
    case CSSValueID::kBlockStart:
      return media_values.StuckBlock() == ContainerStuckLogical::kStart;
    case CSSValueID::kBlockEnd:
      return media_values.StuckBlock() == ContainerStuckLogical::kEnd;
    case CSSValueID::kInlineStart:
      return media_values.StuckInline() == ContainerStuckLogical::kStart;
    case CSSValueID::kInlineEnd:
      return media_values.StuckInline() == ContainerStuckLogical::kEnd;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool SnappedMediaFeatureEval(const MediaQueryExpValue& value,
                                    MediaQueryOperator op,
                                    const MediaValues& media_values) {
  if (!value.IsValid()) {
    return media_values.Snapped();
  }
  switch (value.Id()) {
    case CSSValueID::kNone:
      return !media_values.Snapped();
    case CSSValueID::kX:
      return media_values.SnappedX();
    case CSSValueID::kY:
      return media_values.SnappedY();
    case CSSValueID::kBlock:
      return media_values.SnappedBlock();
    case CSSValueID::kInline:
      return media_values.SnappedInline();
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static bool InvertedColorsMediaFeatureEval(const MediaQueryExpValue& value,
                                           MediaQueryOperator,
                                           const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kInvertedColorsMediaFeature);
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kInvertedColors,
      media_values.InvertedColors());

  if (!value.IsValid()) {
    return media_values.InvertedColors();
  }

  if (!value.IsId()) {
    return false;
  }

  return (value.Id() == CSSValueID::kNone) != media_values.InvertedColors();
}

static bool ScriptingMediaFeatureEval(const MediaQueryExpValue& value,
                                      MediaQueryOperator,
                                      const MediaValues& media_values) {
  UseCounter::Count(media_values.GetDocument(),
                    WebFeature::kScriptingMediaFeature);
  MaybeRecordMediaFeatureValue(
      media_values, IdentifiableSurface::MediaFeatureName::kScripting,
      media_values.GetScripting());

  if (!value.IsValid()) {
    return media_values.GetScripting() == Scripting::kEnabled;
  }

  if (!value.IsId()) {
    return false;
  }

  switch (value.Id()) {
    case CSSValueID::kNone:
      return media_values.GetScripting() == Scripting::kNone;
    case CSSValueID::kInitialOnly:
      return media_values.GetScripting() == Scripting::kInitialOnly;
    case CSSValueID::kEnabled:
      return media_values.GetScripting() == Scripting::kEnabled;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

static MediaQueryOperator ReverseOperator(MediaQueryOperator op) {
  switch (op) {
    case MediaQueryOperator::kNone:
    case MediaQueryOperator::kEq:
      return op;
    case MediaQueryOperator::kLt:
      return MediaQueryOperator::kGt;
    case MediaQueryOperator::kLe:
      return MediaQueryOperator::kGe;
    case MediaQueryOperator::kGt:
      return MediaQueryOperator::kLt;
    case MediaQueryOperator::kGe:
      return MediaQueryOperator::kLe;
  }

  NOTREACHED_IN_MIGRATION();
  return MediaQueryOperator::kNone;
}

void MediaQueryEvaluator::Init() {
  // Create the table.
  g_function_map = new FunctionMap;
#define ADD_TO_FUNCTIONMAP(constantPrefix, methodPrefix)   \
  g_function_map->Set(constantPrefix##MediaFeature.Impl(), \
                      methodPrefix##MediaFeatureEval);
  CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
}

KleeneValue MediaQueryEvaluator::EvalFeature(
    const MediaQueryFeatureExpNode& feature,
    MediaQueryResultFlags* result_flags) const {
  if (!media_values_ || !media_values_->HasValues()) {
    // media_values_ should only be nullptr when parsing UA stylesheets. The
    // only media queries we support in UA stylesheets are media type queries.
    // If HasValues() return false, it means the document frame is nullptr.
    NOTREACHED_IN_MIGRATION();
    return KleeneValue::kFalse;
  }

  if (!media_values_->Width().has_value() && feature.IsWidthDependent()) {
    return KleeneValue::kUnknown;
  }
  if (!media_values_->Height().has_value() && feature.IsHeightDependent()) {
    return KleeneValue::kUnknown;
  }
  if (!media_values_->InlineSize().has_value() &&
      feature.IsInlineSizeDependent()) {
    return KleeneValue::kUnknown;
  }
  if (!media_values_->BlockSize().has_value() &&
      feature.IsBlockSizeDependent()) {
    return KleeneValue::kUnknown;
  }

  if (CSSVariableParser::IsValidVariableName(feature.Name())) {
    return EvalStyleFeature(feature, result_flags);
  }

  DCHECK(g_function_map);

  // Call the media feature evaluation function. Assume no prefix and let
  // trampoline functions override the prefix if prefix is used.
  EvalFunc func = g_function_map->at(feature.Name().Impl());

  if (!func) {
    return KleeneValue::kFalse;
  }

  const auto& bounds = feature.Bounds();

  bool result = true;

  if (!bounds.IsRange() || bounds.right.IsValid()) {
    DCHECK((bounds.right.op == MediaQueryOperator::kNone) || bounds.IsRange());
    result &= func(bounds.right.value, bounds.right.op, *media_values_);
  }

  if (bounds.left.IsValid()) {
    DCHECK(bounds.IsRange());
    auto op = ReverseOperator(bounds.left.op);
    result &= func(bounds.left.value, op, *media_values_);
  }

  if (result_flags) {
    result_flags->is_viewport_dependent =
        result_flags->is_viewport_dependent || feature.IsViewportDependent();
    result_flags->is_device_dependent =
        result_flags->is_device_dependent || feature.IsDeviceDependent();
    result_flags->unit_flags |= feature.GetUnitFlags();
  }

  return result ? KleeneValue::kTrue : KleeneValue::kFalse;
}

KleeneValue MediaQueryEvaluator::EvalStyleFeature(
    const MediaQueryFeatureExpNode& feature,
    MediaQueryResultFlags* result_flags) const {
  if (!media_values_ || !media_values_->HasValues()) {
    NOTREACHED_IN_MIGRATION()
        << "media_values has to be initialized for style() container queries";
    return KleeneValue::kFalse;
  }

  const MediaQueryExpBounds& bounds = feature.Bounds();

  // Style features do not support the range syntax.
  DCHECK(!bounds.IsRange());
  DCHECK(bounds.right.op == MediaQueryOperator::kNone);

  Element* container = media_values_->ContainerElement();
  DCHECK(container);

  AtomicString property_name(feature.Name());
  bool explicit_value = bounds.right.value.IsValid();
  const CSSValue& query_specified = explicit_value
                                        ? bounds.right.value.GetCSSValue()
                                        : *CSSInitialValue::Create();

  if (query_specified.IsRevertValue() || query_specified.IsRevertLayerValue()) {
    return KleeneValue::kFalse;
  }

  const CSSValue* query_value = StyleResolver::ComputeValue(
      container, CSSPropertyName(property_name), query_specified);

  if (const auto* decl_value =
          DynamicTo<CSSUnparsedDeclarationValue>(query_value)) {
    CSSVariableData* query_computed =
        decl_value ? decl_value->VariableDataValue() : nullptr;
    CSSVariableData* computed =
        container->ComputedStyleRef().GetVariableData(property_name);

    if (computed == query_computed ||
        (computed && query_computed &&
         computed->EqualsIgnoringTaint(*query_computed))) {
      return KleeneValue::kTrue;
    }
    return KleeneValue::kFalse;
  }

  const CSSValue* computed_value =
      CustomProperty(property_name, *media_values_->GetDocument())
          .CSSValueFromComputedStyle(
              container->ComputedStyleRef(), nullptr /* layout_object */,
              false /* allow_visited_style */, CSSValuePhase::kComputedValue);
  if (base::ValuesEquivalent(query_value, computed_value) == explicit_value) {
    return KleeneValue::kTrue;
  }
  return KleeneValue::kFalse;
}

}  // namespace blink
