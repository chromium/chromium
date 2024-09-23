// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_input_helpers.h"

#include "third_party/blink/renderer/core/animation/property_handle.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/resolver/css_to_style_map.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/svg/animation/svg_smil_element.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const char kSVGPrefix[] = "svg-";
const unsigned kSVGPrefixLength = sizeof(kSVGPrefix) - 1;

static bool IsSVGPrefixed(const String& property) {
  return property.StartsWith(kSVGPrefix);
}

static String RemoveSVGPrefix(const String& property) {
  DCHECK(IsSVGPrefixed(property));
  return property.Substring(kSVGPrefixLength);
}

static String CSSPropertyToKeyframeAttribute(const CSSProperty& property) {
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kInvalid);
  DCHECK_NE(property.PropertyID(), CSSPropertyID::kVariable);

  switch (property.PropertyID()) {
    case CSSPropertyID::kFloat:
      return "cssFloat";
    case CSSPropertyID::kOffset:
      return "cssOffset";
    default:
      return property.GetJSPropertyName();
  }
}

static String PresentationAttributeToKeyframeAttribute(
    const CSSProperty& presentation_attribute) {
  StringBuilder builder;
  builder.Append(kSVGPrefix, kSVGPrefixLength);
  builder.Append(presentation_attribute.GetPropertyName());
  return builder.ToString();
}

CSSPropertyID AnimationInputHelpers::KeyframeAttributeToCSSProperty(
    const String& property,
    const Document& document) {
  if (CSSVariableParser::IsValidVariableName(property))
    return CSSPropertyID::kVariable;

  // Disallow prefixed properties.
  if (property[0] == '-')
    return CSSPropertyID::kInvalid;
  if (IsASCIIUpper(property[0]))
    return CSSPropertyID::kInvalid;
  if (property == "cssFloat")
    return CSSPropertyID::kFloat;
  if (property == "cssOffset")
    return CSSPropertyID::kOffset;

  StringBuilder builder;
  for (wtf_size_t i = 0; i < property.length(); ++i) {
    // Disallow hyphenated properties.
    if (property[i] == '-')
      return CSSPropertyID::kInvalid;
    if (IsASCIIUpper(property[i]))
      builder.Append('-');
    builder.Append(property[i]);
  }
  return CssPropertyID(document.GetExecutionContext(), builder.ToString());
}

CSSPropertyID AnimationInputHelpers::KeyframeAttributeToPresentationAttribute(
    const String& property,
    const Element* element) {
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled() || !element ||
      !element->IsSVGElement() || !IsSVGPrefixed(property))
    return CSSPropertyID::kInvalid;

  String unprefixed_property = RemoveSVGPrefix(property);
  if (SVGElement::IsAnimatableCSSProperty(
          QualifiedName(AtomicString(unprefixed_property)))) {
    return CssPropertyID(element->GetExecutionContext(), unprefixed_property);
  }
  return CSSPropertyID::kInvalid;
}

using AttributeNameMap = HashMap<QualifiedName, const QualifiedName*>;

const AttributeNameMap& GetSupportedAttributes() {
  DEFINE_STATIC_LOCAL(AttributeNameMap, supported_attributes, ());
  if (supported_attributes.empty()) {
    // Fill the set for the first use.
    // Animatable attributes from http://www.w3.org/TR/SVG/attindex.html
    const auto attributes = std::to_array<const QualifiedName*>({
        &html_names::kClassAttr,
        &svg_names::kAmplitudeAttr,
        &svg_names::kAzimuthAttr,
        &svg_names::kBaseFrequencyAttr,
        &svg_names::kBiasAttr,
        &svg_names::kClipPathUnitsAttr,
        &svg_names::kCxAttr,
        &svg_names::kCyAttr,
        &svg_names::kDAttr,
        &svg_names::kDiffuseConstantAttr,
        &svg_names::kDivisorAttr,
        &svg_names::kDxAttr,
        &svg_names::kDyAttr,
        &svg_names::kEdgeModeAttr,
        &svg_names::kElevationAttr,
        &svg_names::kExponentAttr,
        &svg_names::kFilterUnitsAttr,
        &svg_names::kFxAttr,
        &svg_names::kFyAttr,
        &svg_names::kGradientTransformAttr,
        &svg_names::kGradientUnitsAttr,
        &svg_names::kHeightAttr,
        &svg_names::kHrefAttr,
        &svg_names::kIn2Attr,
        &svg_names::kInAttr,
        &svg_names::kInterceptAttr,
        &svg_names::kK1Attr,
        &svg_names::kK2Attr,
        &svg_names::kK3Attr,
        &svg_names::kK4Attr,
        &svg_names::kKernelMatrixAttr,
        &svg_names::kKernelUnitLengthAttr,
        &svg_names::kLengthAdjustAttr,
        &svg_names::kLimitingConeAngleAttr,
        &svg_names::kMarkerHeightAttr,
        &svg_names::kMarkerUnitsAttr,
        &svg_names::kMarkerWidthAttr,
        &svg_names::kMaskContentUnitsAttr,
        &svg_names::kMaskUnitsAttr,
        &svg_names::kMethodAttr,
        &svg_names::kModeAttr,
        &svg_names::kNumOctavesAttr,
        &svg_names::kOffsetAttr,
        &svg_names::kOperatorAttr,
        &svg_names::kOrderAttr,
        &svg_names::kOrientAttr,
        &svg_names::kPathLengthAttr,
        &svg_names::kPatternContentUnitsAttr,
        &svg_names::kPatternTransformAttr,
        &svg_names::kPatternUnitsAttr,
        &svg_names::kPointsAtXAttr,
        &svg_names::kPointsAtYAttr,
        &svg_names::kPointsAtZAttr,
        &svg_names::kPointsAttr,
        &svg_names::kPreserveAlphaAttr,
        &svg_names::kPreserveAspectRatioAttr,
        &svg_names::kPrimitiveUnitsAttr,
        &svg_names::kRAttr,
        &svg_names::kRadiusAttr,
        &svg_names::kRefXAttr,
        &svg_names::kRefYAttr,
        &svg_names::kResultAttr,
        &svg_names::kRotateAttr,
        &svg_names::kRxAttr,
        &svg_names::kRyAttr,
        &svg_names::kScaleAttr,
        &svg_names::kSeedAttr,
        &svg_names::kSlopeAttr,
        &svg_names::kSpacingAttr,
        &svg_names::kSpecularConstantAttr,
        &svg_names::kSpecularExponentAttr,
        &svg_names::kSpreadMethodAttr,
        &svg_names::kStartOffsetAttr,
        &svg_names::kStdDeviationAttr,
        &svg_names::kStitchTilesAttr,
        &svg_names::kSurfaceScaleAttr,
        &svg_names::kTableValuesAttr,
        &svg_names::kTargetAttr,
        &svg_names::kTargetXAttr,
        &svg_names::kTargetYAttr,
        &svg_names::kTextLengthAttr,
        &svg_names::kTransformAttr,
        &svg_names::kTypeAttr,
        &svg_names::kValuesAttr,
        &svg_names::kViewBoxAttr,
        &svg_names::kWidthAttr,
        &svg_names::kX1Attr,
        &svg_names::kX2Attr,
        &svg_names::kXAttr,
        &svg_names::kXChannelSelectorAttr,
        &svg_names::kY1Attr,
        &svg_names::kY2Attr,
        &svg_names::kYAttr,
        &svg_names::kYChannelSelectorAttr,
        &svg_names::kZAttr,
    });
    for (const QualifiedName* attribute : attributes) {
      DCHECK(!SVGElement::IsAnimatableCSSProperty(*attribute));
      supported_attributes.Set(*attribute, attribute);
    }
  }
  return supported_attributes;
}

QualifiedName SvgAttributeName(const String& property) {
  DCHECK(!IsSVGPrefixed(property));
  return QualifiedName(AtomicString(property));
}

const QualifiedName* AnimationInputHelpers::KeyframeAttributeToSVGAttribute(
    const String& property,
    Element* element) {
  auto* svg_element = DynamicTo<SVGElement>(element);
  if (!RuntimeEnabledFeatures::WebAnimationsSVGEnabled() || !svg_element ||
      !IsSVGPrefixed(property))
    return nullptr;

  if (IsA<SVGSMILElement>(svg_element))
    return nullptr;

  String unprefixed_property = RemoveSVGPrefix(property);
  QualifiedName attribute_name = SvgAttributeName(unprefixed_property);
  const AttributeNameMap& supported_attributes = GetSupportedAttributes();
  auto iter = supported_attributes.find(attribute_name);
  if (iter == supported_attributes.end() ||
      !svg_element->PropertyFromAttribute(*iter->value))
    return nullptr;

  return iter->value;
}

scoped_refptr<TimingFunction> AnimationInputHelpers::ParseTimingFunction(
    const String& string,
    Document* document,
    ExceptionState& exception_state) {
  if (string.empty()) {
    exception_state.ThrowTypeError("Easing may not be the empty string");
    return nullptr;
  }

  // Fallback to an insecure parsing mode if we weren't provided with a
  // document.
  SecureContextMode secure_context_mode =
      document && document->GetExecutionContext()
          ? document->GetExecutionContext()->GetSecureContextMode()
          : SecureContextMode::kInsecureContext;
  const CSSValue* value = CSSParser::ParseSingleValue(
      CSSPropertyID::kTransitionTimingFunction, string,
      StrictCSSParserContext(secure_context_mode));
  const auto* value_list = DynamicTo<CSSValueList>(value);
  if (!value_list) {
    DCHECK(!value || value->IsCSSWideKeyword());
    exception_state.ThrowTypeError("'" + string +
                                   "' is not a valid value for easing");
    return nullptr;
  }
  if (value_list->length() > 1) {
    exception_state.ThrowTypeError("Easing may not be set to a list of values");
    return nullptr;
  }
  return CSSToStyleMap::MapAnimationTimingFunction(value_list->Item(0));
}

String AnimationInputHelpers::PropertyHandleToKeyframeAttribute(
    PropertyHandle property) {
  if (property.IsCSSProperty()) {
    return property.IsCSSCustomProperty()
               ? property.CustomPropertyName()
               : CSSPropertyToKeyframeAttribute(property.GetCSSProperty());
  }

  if (property.IsPresentationAttribute()) {
    return PresentationAttributeToKeyframeAttribute(
        property.PresentationAttribute());
  }

  DCHECK(property.IsSVGAttribute());
  return property.SvgAttribute().LocalName();
}

}  // namespace blink
