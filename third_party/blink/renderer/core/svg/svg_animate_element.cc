/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/svg/svg_animate_element.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_effect_parameters.h"
#include "third_party/blink/renderer/core/svg/animation/smil_animation_value.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_angle.h"
#include "third_party/blink/renderer/core/svg/svg_animated_color.h"
#include "third_party/blink/renderer/core/svg/svg_boolean.h"
#include "third_party/blink/renderer/core/svg/svg_integer.h"
#include "third_party/blink/renderer/core/svg/svg_integer_optional_integer.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_number.h"
#include "third_party/blink/renderer/core/svg/svg_number_list.h"
#include "third_party/blink/renderer/core/svg/svg_number_optional_number.h"
#include "third_party/blink/renderer/core/svg/svg_path.h"
#include "third_party/blink/renderer/core/svg/svg_point_list.h"
#include "third_party/blink/renderer/core/svg/svg_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_rect.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"
#include "third_party/blink/renderer/core/svg/svg_set_element.h"
#include "third_party/blink/renderer/core/svg/svg_string.h"
#include "third_party/blink/renderer/core/xlink_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

String ComputeCSSPropertyValue(SVGElement* element, CSSPropertyID id) {
  DCHECK(element);
  // TODO(fs): StyleEngine doesn't support document without a frame.
  // Refer to comment in Element::computedStyle.
  DCHECK(element->InActiveDocument());

  element->GetDocument().UpdateStyleAndLayoutTreeForElement(
      element, DocumentUpdateReason::kSMILAnimation);

  // Don't include any properties resulting from CSS Transitions/Animations or
  // SMIL animations, as we want to retrieve the "base value".
  const ComputedStyle* style = element->BaseComputedStyleForSMIL();
  if (!style)
    return "";
  const CSSValue* value = CSSProperty::Get(id).CSSValueFromComputedStyle(
      *style, element->GetLayoutObject(), false, CSSValuePhase::kResolvedValue);
  return value ? value->CssText() : "";
}

AnimatedPropertyValueType PropertyValueType(const QualifiedName& attribute_name,
                                            const String& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, inherit, ("inherit"));
  if (value.empty() || value != inherit ||
      !SVGElement::IsAnimatableCSSProperty(attribute_name))
    return kRegularPropertyValue;
  return kInheritValue;
}

QualifiedName ConstructQualifiedName(const SVGElement& svg_element,
                                     const AtomicString& attribute_name) {
  if (attribute_name.empty())
    return AnyQName();
  if (!attribute_name.Contains(':'))
    return QualifiedName(attribute_name);

  AtomicString prefix;
  AtomicString local_name;
  if (!Document::ParseQualifiedName(attribute_name, prefix, local_name,
                                    IGNORE_EXCEPTION_FOR_TESTING))
    return AnyQName();

  const AtomicString& namespace_uri = svg_element.lookupNamespaceURI(prefix);
  if (namespace_uri.empty())
    return AnyQName();

  QualifiedName resolved_attr_name(g_null_atom, local_name, namespace_uri);
  // "Animation elements treat attributeName='xlink:href' as being an alias
  // for targetting the 'href' attribute."
  // https://svgwg.org/svg2-draft/types.html#__svg__SVGURIReference__href
  if (resolved_attr_name == xlink_names::kHrefAttr)
    return svg_names::kHrefAttr;
  return resolved_attr_name;
}

}  // unnamed namespace

SVGAnimateElement::SVGAnimateElement(Document& document)
    : SVGAnimateElement(svg_names::kAnimateTag, document) {}

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tag_name,
                                     Document& document)
    : SVGAnimationElement(tag_name, document),
      attribute_name_(AnyQName()),
      type_(kAnimatedUnknown),
      css_property_id_(CSSPropertyID::kInvalid),
      from_property_value_type_(kRegularPropertyValue),
      to_property_value_type_(kRegularPropertyValue),
      attribute_type_(kAttributeTypeAuto) {}

SVGAnimateElement::~SVGAnimateElement() = default;

bool SVGAnimateElement::IsSVGAnimationAttributeSettingJavaScriptURL(
    const Attribute& attribute) const {
  if ((attribute.GetName() == svg_names::kFromAttr ||
       attribute.GetName() == svg_names::kToAttr) &&
      AttributeValueIsJavaScriptURL(attribute))
    return true;

  if (attribute.GetName() == svg_names::kValuesAttr) {
    Vector<String> parts;
    if (!ParseValues(attribute.Value(), parts)) {
      // Assume the worst.
      return true;
    }
    for (const auto& part : parts) {
      if (ProtocolIsJavaScript(part))
        return true;
    }
  }

  return SVGSMILElement::IsSVGAnimationAttributeSettingJavaScriptURL(attribute);
}

Node::InsertionNotificationRequest SVGAnimateElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGAnimationElement::InsertedInto(root_parent);
  if (root_parent.isConnected()) {
    SetAttributeName(ConstructQualifiedName(
        *this, FastGetAttribute(svg_names::kAttributeNameAttr)));
  }
  return kInsertionDone;
}

void SVGAnimateElement::RemovedFrom(ContainerNode& root_parent) {
  if (root_parent.isConnected())
    SetAttributeName(AnyQName());
  SVGAnimationElement::RemovedFrom(root_parent);
}

void SVGAnimateElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kAttributeTypeAttr) {
    SetAttributeType(params.new_value);
    return;
  }
  if (params.name == svg_names::kAttributeNameAttr) {
    SetAttributeName(ConstructQualifiedName(*this, params.new_value));
    return;
  }
  SVGAnimationElement::ParseAttribute(params);
}

void SVGAnimateElement::ResolveTargetProperty() {
  DCHECK(targetElement());
  target_property_ = targetElement()->PropertyFromAttribute(AttributeName());
  if (target_property_) {
    type_ = target_property_->GetType();
    css_property_id_ = target_property_->CssPropertyId();

    // Only <animateTransform> is allowed to animate AnimatedTransformList.
    // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
    if (type_ == kAnimatedTransformList) {
      type_ = kAnimatedUnknown;
      css_property_id_ = CSSPropertyID::kInvalid;
    }
  } else {
    type_ = SVGElement::AnimatedPropertyTypeForCSSAttribute(AttributeName());
    css_property_id_ =
        type_ != kAnimatedUnknown
            ? CssPropertyID(targetElement()->GetExecutionContext(),
                            AttributeName().LocalName())
            : CSSPropertyID::kInvalid;
  }
  // Disallow <script> targets here for now to prevent unpleasantries. This
  // also disallows the perfectly "valid" animation of 'className' on said
  // element. If SVGScriptElement.href is transitioned off of SVGAnimatedHref,
  // this can be removed.
  if (IsA<SVGScriptElement>(*targetElement())) {
    type_ = kAnimatedUnknown;
    css_property_id_ = CSSPropertyID::kInvalid;
  }
  DCHECK(type_ != kAnimatedPoint && type_ != kAnimatedStringList &&
         type_ != kAnimatedTransform && type_ != kAnimatedTransformList);
}

void SVGAnimateElement::ClearTargetProperty() {
  target_property_ = nullptr;
  type_ = kAnimatedUnknown;
  css_property_id_ = CSSPropertyID::kInvalid;
}

void SVGAnimateElement::UpdateTargetProperty() {
  if (targetElement()) {
    ResolveTargetProperty();
  } else {
    ClearTargetProperty();
  }
}

bool SVGAnimateElement::HasValidAnimation() const {
  if (type_ == kAnimatedUnknown)
    return false;
  // Always animate CSS properties using the ApplyCSSAnimation code path,
  // regardless of the attributeType value.
  // If attributeType="CSS" and attributeName doesn't point to a CSS property,
  // ignore the animation.
  return IsAnimatingCSSProperty() || GetAttributeType() != kAttributeTypeCSS;
}

SVGPropertyBase* SVGAnimateElement::CreatePropertyForAttributeAnimation(
    const String& value) const {
  // SVG DOM animVal animation code-path.
  // TransformList must be animated via <animateTransform>, and its
  // {from,by,to} attribute values needs to be parsed w.r.t. its "type"
  // attribute. Spec:
  // http://www.w3.org/TR/SVG/single-page.html#animate-AnimateTransformElement
  DCHECK_NE(type_, kAnimatedTransformList);
  DCHECK(target_property_);
  return target_property_->BaseValueBase().CloneForAnimation(value);
}

SVGPropertyBase* SVGAnimateElement::CreateUnderlyingValueForAttributeAnimation()
    const {
  // SVG DOM animVal animation code-path.
  DCHECK_NE(type_, kAnimatedTransformList);
  DCHECK(target_property_);
  const SVGPropertyBase& base_value = target_property_->BaseValueBase();
  switch (base_value.GetType()) {
    case kAnimatedAngle:
      return To<SVGAngle>(base_value).Clone();
    case kAnimatedBoolean:
      return To<SVGBoolean>(base_value).Clone();
    case kAnimatedEnumeration:
      return To<SVGEnumeration>(base_value).Clone();
    case kAnimatedInteger:
      return To<SVGInteger>(base_value).Clone();
    case kAnimatedIntegerOptionalInteger:
      return To<SVGIntegerOptionalInteger>(base_value).Clone();
    case kAnimatedLength:
      return To<SVGLength>(base_value).Clone();
    case kAnimatedLengthList:
      return To<SVGLengthList>(base_value).Clone();
    case kAnimatedNumber:
      return To<SVGNumber>(base_value).Clone();
    case kAnimatedNumberList:
      return To<SVGNumberList>(base_value).Clone();
    case kAnimatedNumberOptionalNumber:
      return To<SVGNumberOptionalNumber>(base_value).Clone();
    case kAnimatedPath:
      return To<SVGPath>(base_value).Clone();
    case kAnimatedPoints:
      return To<SVGPointList>(base_value).Clone();
    case kAnimatedPreserveAspectRatio:
      return To<SVGPreserveAspectRatio>(base_value).Clone();
    case kAnimatedRect:
      return To<SVGRect>(base_value).Clone();
    case kAnimatedString:
      return To<SVGString>(base_value).Clone();

    // The following are either not animated or are not animated as
    // attributeType=XML. <animateTransform> handles the transform-list case.
    case kAnimatedUnknown:
    case kAnimatedColor:
    case kAnimatedPoint:
    case kAnimatedStringList:
    case kAnimatedTransform:
    case kAnimatedTransformList:
    default:
      NOTREACHED_IN_MIGRATION();
      return nullptr;
  }
}

SVGPropertyBase* SVGAnimateElement::CreatePropertyForCSSAnimation(
    const String& value) const {
  // CSS properties animation code-path.
  // Create a basic instance of the corresponding SVG property.
  // The instance will not have full context info. (e.g. SVGLengthMode)
  switch (type_) {
    case kAnimatedColor:
      return MakeGarbageCollected<SVGColorProperty>(value);
    case kAnimatedNumber: {
      auto* property = MakeGarbageCollected<SVGNumber>();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedLength: {
      auto* property = MakeGarbageCollected<SVGLength>();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedLengthList: {
      auto* property = MakeGarbageCollected<SVGLengthList>();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedString: {
      auto* property = MakeGarbageCollected<SVGString>();
      property->SetValueAsString(value);
      return property;
    }
    // These types don't appear in the table in
    // SVGElement::animatedPropertyTypeForCSSAttribute() and thus don't need
    // support.
    case kAnimatedAngle:
    case kAnimatedBoolean:
    case kAnimatedEnumeration:
    case kAnimatedInteger:
    case kAnimatedIntegerOptionalInteger:
    case kAnimatedNumberList:
    case kAnimatedNumberOptionalNumber:
    case kAnimatedPath:
    case kAnimatedPoint:
    case kAnimatedPoints:
    case kAnimatedPreserveAspectRatio:
    case kAnimatedRect:
    case kAnimatedStringList:
    case kAnimatedTransform:
    case kAnimatedTransformList:
    case kAnimatedUnknown:
      break;
    default:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

SVGPropertyBase* SVGAnimateElement::ParseValue(const String& value) const {
  if (IsAnimatingSVGDom())
    return CreatePropertyForAttributeAnimation(value);
  DCHECK(IsAnimatingCSSProperty());
  return CreatePropertyForCSSAnimation(value);
}

SVGPropertyBase* SVGAnimateElement::AdjustForInheritance(
    SVGPropertyBase* property_value,
    AnimatedPropertyValueType value_type) const {
  if (value_type != kInheritValue)
    return property_value;
  DCHECK(IsAnimatingCSSProperty());
  // TODO(fs): At the moment the computed style gets returned as a String and
  // needs to get parsed again. In the future we might want to work with the
  // value type directly to avoid the String parsing.
  DCHECK(targetElement());
  Element* parent = targetElement()->parentElement();
  auto* svg_parent = DynamicTo<SVGElement>(parent);
  if (!svg_parent)
    return property_value;
  // Replace 'inherit' by its computed property value.
  String value = ComputeCSSPropertyValue(svg_parent, css_property_id_);
  return CreatePropertyForCSSAnimation(value);
}

static SVGPropertyBase* DiscreteSelectValue(AnimationMode animation_mode,
                                            float percentage,
                                            SVGPropertyBase* from,
                                            SVGPropertyBase* to) {
  if ((animation_mode == kFromToAnimation && percentage > 0.5) ||
      animation_mode == kToAnimation || percentage == 1) {
    return to;
  }
  return from;
}

void SVGAnimateElement::CalculateAnimationValue(
    SMILAnimationValue& animation_value,
    float percentage,
    unsigned repeat_count) const {
  DCHECK(targetElement());
  DCHECK(percentage >= 0 && percentage <= 1);
  DCHECK_NE(type_, kAnimatedUnknown);
  DCHECK(from_property_);
  DCHECK_EQ(from_property_->GetType(), type_);
  DCHECK(to_property_);

  DCHECK(animation_value.property_value);
  DCHECK_EQ(animation_value.property_value->GetType(), type_);

  // The semantics of the 'set' element is that it always (and only) sets the
  // 'to' value. (It is also always set as a 'to' animation and will thus never
  // be additive or cumulative.)
  if (IsA<SVGSetElement>(*this))
    percentage = 1;

  if (GetCalcMode() == kCalcModeDiscrete)
    percentage = percentage < 0.5 ? 0 : 1;

  // Values-animation accumulates using the last values entry corresponding to
  // the end of duration time.
  SVGPropertyBase* animated_value = animation_value.property_value;
  SVGPropertyBase* to_at_end_of_duration_value =
      to_at_end_of_duration_property_ ? to_at_end_of_duration_property_
                                      : to_property_;
  SVGPropertyBase* from_value = GetAnimationMode() == kToAnimation
                                    ? animated_value
                                    : from_property_.Get();
  SVGPropertyBase* to_value = to_property_;

  // Apply CSS inheritance rules.
  from_value = AdjustForInheritance(from_value, from_property_value_type_);
  to_value = AdjustForInheritance(to_value, to_property_value_type_);

  // If the animated type can only be animated discretely, then do that here,
  // replacing |result_element|s animated value.
  if (!AnimatedPropertyTypeSupportsAddition()) {
    animation_value.property_value = DiscreteSelectValue(
        GetAnimationMode(), percentage, from_value, to_value);
    return;
  }

  SMILAnimationEffectParameters parameters = ComputeEffectParameters();
  animated_value->CalculateAnimatedValue(
      parameters, percentage, repeat_count, from_value, to_value,
      to_at_end_of_duration_value, targetElement());
}

AnimationMode SVGAnimateElement::CalculateAnimationMode() {
  AnimationMode animation_mode = SVGAnimationElement::CalculateAnimationMode();
  if (animation_mode == kByAnimation || animation_mode == kFromByAnimation) {
    // by/from-by animation may only be used with attributes that support addition
    // (e.g. most numeric attributes).
    if (!AnimatedPropertyTypeSupportsAddition()) {
      return kNoAnimation;
    }
  }
  return animation_mode;
}

bool SVGAnimateElement::CalculateToAtEndOfDurationValue(
    const String& to_at_end_of_duration_string) {
  if (to_at_end_of_duration_string.empty())
    return false;
  to_at_end_of_duration_property_ = ParseValue(to_at_end_of_duration_string);
  return true;
}

void SVGAnimateElement::CalculateFromAndToValues(const String& from_string,
                                                 const String& to_string) {
  DCHECK(targetElement());
  from_property_ = ParseValue(from_string);
  from_property_value_type_ = PropertyValueType(AttributeName(), from_string);
  to_property_ = ParseValue(to_string);
  to_property_value_type_ = PropertyValueType(AttributeName(), to_string);
}

void SVGAnimateElement::CalculateFromAndByValues(const String& from_string,
                                                 const String& by_string) {
  DCHECK(targetElement());
  DCHECK(GetAnimationMode() == kByAnimation ||
         GetAnimationMode() == kFromByAnimation);
  DCHECK(AnimatedPropertyTypeSupportsAddition());
  DCHECK(!IsA<SVGSetElement>(*this));

  from_property_ = ParseValue(from_string);
  from_property_value_type_ = PropertyValueType(AttributeName(), from_string);
  to_property_ = ParseValue(by_string);
  to_property_value_type_ = PropertyValueType(AttributeName(), by_string);
  to_property_->Add(from_property_, targetElement());
}

SVGPropertyBase* SVGAnimateElement::CreateUnderlyingValueForAnimation() const {
  DCHECK(targetElement());
  if (IsAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    return CreateUnderlyingValueForAttributeAnimation();
  }
  DCHECK(IsAnimatingCSSProperty());
  // Presentation attributes that have an SVG DOM representation should use
  // the "SVG DOM" code-path (above.)
  DCHECK(SVGElement::IsAnimatableCSSProperty(AttributeName()));

  // CSS properties animation code-path.
  String base_value =
      ComputeCSSPropertyValue(targetElement(), css_property_id_);
  return CreatePropertyForCSSAnimation(base_value);
}

SMILAnimationValue SVGAnimateElement::CreateAnimationValue() const {
  SMILAnimationValue animation_value;
  animation_value.property_value = CreateUnderlyingValueForAnimation();
  return animation_value;
}

void SVGAnimateElement::ClearAnimationValue() {
  SVGElement* target_element = targetElement();
  DCHECK(target_element);

  // CSS properties animation code-path.
  if (IsAnimatingCSSProperty()) {
    MutableCSSPropertyValueSet* property_set =
        target_element->EnsureAnimatedSMILStyleProperties();
    if (property_set->RemoveProperty(css_property_id_)) {
      target_element->SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kAnimation));
    }
  }
  // SVG DOM animVal animation code-path.
  if (IsAnimatingSVGDom())
    target_element->ClearAnimatedAttribute(AttributeName());
}

void SVGAnimateElement::ApplyResultsToTarget(
    const SMILAnimationValue& animation_value) {
  DCHECK(animation_value.property_value);
  DCHECK(targetElement());
  DCHECK_NE(type_, kAnimatedUnknown);

  // We do update the style and the animation property independent of each
  // other.
  SVGElement* target_element = targetElement();
  SVGPropertyBase* animated_value = animation_value.property_value;

  // CSS properties animation code-path.
  if (IsAnimatingCSSProperty()) {
    // Convert the result of the animation to a String and apply it as CSS
    // property on the target_element.
    MutableCSSPropertyValueSet* properties =
        target_element->EnsureAnimatedSMILStyleProperties();
    auto animated_value_string = animated_value->ValueAsString();
    auto& document = target_element->GetDocument();
    auto set_result = properties->ParseAndSetProperty(
        css_property_id_, animated_value_string, false,
        document.GetExecutionContext()->GetSecureContextMode(),
        document.ElementSheet().Contents());
    if (set_result >= MutableCSSPropertyValueSet::kModifiedExisting) {
      target_element->SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kAnimation));
    }
  }
  // SVG DOM animVal animation code-path.
  if (IsAnimatingSVGDom())
    target_element->SetAnimatedAttribute(AttributeName(), animated_value);
}

bool SVGAnimateElement::AnimatedPropertyTypeSupportsAddition() const {
  DCHECK(targetElement());
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
  switch (type_) {
    case kAnimatedBoolean:
    case kAnimatedEnumeration:
    case kAnimatedPreserveAspectRatio:
    case kAnimatedString:
    case kAnimatedUnknown:
      return false;
    default:
      return true;
  }
}

float SVGAnimateElement::CalculateDistance(const String& from_string,
                                           const String& to_string) {
  DCHECK(targetElement());
  // FIXME: A return value of float is not enough to support paced animations on
  // lists.
  SVGPropertyBase* from_value = ParseValue(from_string);
  SVGPropertyBase* to_value = ParseValue(to_string);
  return from_value->CalculateDistance(to_value, targetElement());
}

void SVGAnimateElement::WillChangeAnimatedType() {
  UnregisterAnimation(attribute_name_);
  from_property_.Clear();
  to_property_.Clear();
  to_at_end_of_duration_property_.Clear();
}

void SVGAnimateElement::DidChangeAnimatedType() {
  UpdateTargetProperty();
  RegisterAnimation(attribute_name_);
}

void SVGAnimateElement::WillChangeAnimationTarget() {
  SVGAnimationElement::WillChangeAnimationTarget();
  WillChangeAnimatedType();
}

void SVGAnimateElement::DidChangeAnimationTarget() {
  DidChangeAnimatedType();
  SVGAnimationElement::DidChangeAnimationTarget();
}

void SVGAnimateElement::SetAttributeName(const QualifiedName& attribute_name) {
  if (attribute_name == attribute_name_)
    return;
  WillChangeAnimatedType();
  attribute_name_ = attribute_name;
  DidChangeAnimatedType();
  AnimationAttributeChanged();
}

void SVGAnimateElement::SetAttributeType(
    const AtomicString& attribute_type_string) {
  AttributeType attribute_type = kAttributeTypeAuto;
  if (attribute_type_string == "CSS")
    attribute_type = kAttributeTypeCSS;
  else if (attribute_type_string == "XML")
    attribute_type = kAttributeTypeXML;
  if (attribute_type == attribute_type_)
    return;
  WillChangeAnimatedType();
  attribute_type_ = attribute_type;
  DidChangeAnimatedType();
  AnimationAttributeChanged();
}

void SVGAnimateElement::Trace(Visitor* visitor) const {
  visitor->Trace(from_property_);
  visitor->Trace(to_property_);
  visitor->Trace(to_at_end_of_duration_property_);
  visitor->Trace(target_property_);
  SVGAnimationElement::Trace(visitor);
}

}  // namespace blink
