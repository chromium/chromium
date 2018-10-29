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

#include "third_party/blink/renderer/core/css/css_computed_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property.h"
#include "third_party/blink/renderer/core/svg/svg_animated_color.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_list.h"
#include "third_party/blink/renderer/core/svg/svg_number.h"
#include "third_party/blink/renderer/core/svg/svg_string.h"
#include "third_party/blink/renderer/core/xlink_names.h"

namespace blink {

namespace {

bool IsTargetAttributeCSSProperty(const SVGElement& target_element,
                                  const QualifiedName& attribute_name) {
  return SVGElement::IsAnimatableCSSProperty(attribute_name) ||
         target_element.IsPresentationAttribute(attribute_name);
}

String ComputeCSSPropertyValue(SVGElement* element, CSSPropertyID id) {
  DCHECK(element);
  // TODO(fs): StyleEngine doesn't support document without a frame.
  // Refer to comment in Element::computedStyle.
  DCHECK(element->InActiveDocument());

  // Don't include any properties resulting from CSS Transitions/Animations or
  // SMIL animations, as we want to retrieve the "base value".
  element->SetUseOverrideComputedStyle(true);
  String value =
      CSSComputedStyleDeclaration::Create(element)->GetPropertyValue(id);
  element->SetUseOverrideComputedStyle(false);
  return value;
}

AnimatedPropertyValueType PropertyValueType(const QualifiedName& attribute_name,
                                            const String& value) {
  DEFINE_STATIC_LOCAL(const AtomicString, inherit, ("inherit"));
  if (value.IsEmpty() || value != inherit ||
      !SVGElement::IsAnimatableCSSProperty(attribute_name))
    return kRegularPropertyValue;
  return kInheritValue;
}

QualifiedName ConstructQualifiedName(const SVGElement& svg_element,
                                     const AtomicString& attribute_name) {
  if (attribute_name.IsEmpty())
    return AnyQName();
  if (!attribute_name.Contains(':'))
    return QualifiedName(g_null_atom, attribute_name, g_null_atom);

  AtomicString prefix;
  AtomicString local_name;
  if (!Document::ParseQualifiedName(attribute_name, prefix, local_name,
                                    IGNORE_EXCEPTION_FOR_TESTING))
    return AnyQName();

  const AtomicString& namespace_uri = svg_element.lookupNamespaceURI(prefix);
  if (namespace_uri.IsEmpty())
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

SVGAnimateElement::SVGAnimateElement(const QualifiedName& tag_name,
                                     Document& document)
    : SVGAnimationElement(tag_name, document),
      type_(kAnimatedUnknown),
      css_property_id_(CSSPropertyInvalid),
      from_property_value_type_(kRegularPropertyValue),
      to_property_value_type_(kRegularPropertyValue),
      attribute_type_(kAttributeTypeAuto) {}

SVGAnimateElement* SVGAnimateElement::Create(Document& document) {
  return new SVGAnimateElement(svg_names::kAnimateTag, document);
}

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
    AnimationAttributeChanged();
    return;
  }
  if (params.name == svg_names::kAttributeNameAttr) {
    SetAttributeName(ConstructQualifiedName(*this, params.new_value));
    AnimationAttributeChanged();
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
      css_property_id_ = CSSPropertyInvalid;
    }
  } else {
    type_ = SVGElement::AnimatedPropertyTypeForCSSAttribute(AttributeName());
    css_property_id_ = type_ != kAnimatedUnknown
                           ? cssPropertyID(AttributeName().LocalName())
                           : CSSPropertyInvalid;
  }
  // Blacklist <script> targets here for now to prevent unpleasantries. This
  // also disallows the perfectly "valid" animation of 'className' on said
  // element. If SVGScriptElement.href is transitioned off of SVGAnimatedHref,
  // this can be removed.
  if (IsSVGScriptElement(*targetElement())) {
    type_ = kAnimatedUnknown;
    css_property_id_ = CSSPropertyInvalid;
  }
  DCHECK(type_ != kAnimatedPoint && type_ != kAnimatedStringList &&
         type_ != kAnimatedTransform && type_ != kAnimatedTransformList);
}

void SVGAnimateElement::ClearTargetProperty() {
  target_property_ = nullptr;
  type_ = kAnimatedUnknown;
  css_property_id_ = CSSPropertyInvalid;
}

AnimatedPropertyType SVGAnimateElement::GetAnimatedPropertyType() {
  if (!targetElement())
    return kAnimatedUnknown;
  ResolveTargetProperty();
  return type_;
}

bool SVGAnimateElement::HasValidTarget() {
  if (!SVGAnimationElement::HasValidTarget())
    return false;
  if (AttributeName() == AnyQName())
    return false;
  ResolveTargetProperty();
  if (type_ == kAnimatedUnknown)
    return false;
  // Always animate CSS properties using the ApplyCSSAnimation code path,
  // regardless of the attributeType value.
  // If attributeType="CSS" and attributeName doesn't point to a CSS property,
  // ignore the animation.
  return IsTargetAttributeCSSProperty(*targetElement(), AttributeName()) ||
         GetAttributeType() != kAttributeTypeCSS;
}

bool SVGAnimateElement::ShouldApplyAnimation(
    const SVGElement& target_element,
    const QualifiedName& attribute_name) {
  return target_element.parentNode() && HasValidTarget();
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
  return target_property_->CurrentValueBase()->CloneForAnimation(value);
}

SVGPropertyBase* SVGAnimateElement::CreatePropertyForCSSAnimation(
    const String& value) const {
  // CSS properties animation code-path.
  // Create a basic instance of the corresponding SVG property.
  // The instance will not have full context info. (e.g. SVGLengthMode)
  switch (type_) {
    case kAnimatedColor:
      return SVGColorProperty::Create(value);
    case kAnimatedNumber: {
      SVGNumber* property = SVGNumber::Create();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedLength: {
      SVGLength* property = SVGLength::Create();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedLengthList: {
      SVGLengthList* property = SVGLengthList::Create();
      property->SetValueAsString(value);
      return property;
    }
    case kAnimatedString: {
      SVGString* property = SVGString::Create();
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
  NOTREACHED();
  return nullptr;
}

SVGPropertyBase* SVGAnimateElement::CreatePropertyForAnimation(
    const String& value) const {
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
  // TODO(fs): At the moment the computed style gets returned as a String and
  // needs to get parsed again. In the future we might want to work with the
  // value type directly to avoid the String parsing.
  DCHECK(targetElement());
  Element* parent = targetElement()->parentElement();
  if (!parent || !parent->IsSVGElement())
    return property_value;
  SVGElement* svg_parent = ToSVGElement(parent);
  // Replace 'inherit' by its computed property value.
  String value = ComputeCSSPropertyValue(svg_parent, css_property_id_);
  return CreatePropertyForAnimation(value);
}

void SVGAnimateElement::CalculateAnimatedValue(float percentage,
                                               unsigned repeat_count,
                                               SVGSMILElement* result_element) {
  DCHECK(result_element);
  DCHECK(targetElement());
  if (!IsSVGAnimateElement(*result_element))
    return;

  DCHECK(percentage >= 0 && percentage <= 1);
  DCHECK_NE(GetAnimatedPropertyType(), kAnimatedUnknown);
  DCHECK(from_property_);
  DCHECK_EQ(from_property_->GetType(), GetAnimatedPropertyType());
  DCHECK(to_property_);

  SVGAnimateElement* result_animation_element =
      ToSVGAnimateElement(result_element);
  DCHECK(result_animation_element->animated_value_);
  DCHECK_EQ(result_animation_element->GetAnimatedPropertyType(),
            GetAnimatedPropertyType());

  if (IsSVGSetElement(*this))
    percentage = 1;

  if (GetCalcMode() == kCalcModeDiscrete)
    percentage = percentage < 0.5 ? 0 : 1;

  // Target element might have changed.
  SVGElement* target_element = this->targetElement();

  // Values-animation accumulates using the last values entry corresponding to
  // the end of duration time.
  SVGPropertyBase* animated_value = result_animation_element->animated_value_;
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

  animated_value->CalculateAnimatedValue(
      this, percentage, repeat_count, from_value, to_value,
      to_at_end_of_duration_value, target_element);
}

bool SVGAnimateElement::CalculateToAtEndOfDurationValue(
    const String& to_at_end_of_duration_string) {
  if (to_at_end_of_duration_string.IsEmpty())
    return false;
  to_at_end_of_duration_property_ =
      CreatePropertyForAnimation(to_at_end_of_duration_string);
  return true;
}

bool SVGAnimateElement::CalculateFromAndToValues(const String& from_string,
                                                 const String& to_string) {
  DCHECK(targetElement());
  from_property_ = CreatePropertyForAnimation(from_string);
  from_property_value_type_ = PropertyValueType(AttributeName(), from_string);
  to_property_ = CreatePropertyForAnimation(to_string);
  to_property_value_type_ = PropertyValueType(AttributeName(), to_string);
  return true;
}

bool SVGAnimateElement::CalculateFromAndByValues(const String& from_string,
                                                 const String& by_string) {
  DCHECK(targetElement());

  if (GetAnimationMode() == kByAnimation && !IsAdditive())
    return false;

  // from-by animation may only be used with attributes that support addition
  // (e.g. most numeric attributes).
  if (GetAnimationMode() == kFromByAnimation &&
      !AnimatedPropertyTypeSupportsAddition())
    return false;

  DCHECK(!IsSVGSetElement(*this));

  from_property_ = CreatePropertyForAnimation(from_string);
  from_property_value_type_ = PropertyValueType(AttributeName(), from_string);
  to_property_ = CreatePropertyForAnimation(by_string);
  to_property_value_type_ = PropertyValueType(AttributeName(), by_string);
  to_property_->Add(from_property_, targetElement());
  return true;
}

void SVGAnimateElement::ResetAnimatedType() {
  ResolveTargetProperty();

  SVGElement* target_element = this->targetElement();
  const QualifiedName& attribute_name = this->AttributeName();

  if (!ShouldApplyAnimation(*target_element, attribute_name))
    return;
  if (IsAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    animated_value_ = target_property_->CreateAnimatedValue();
    DCHECK_EQ(animated_value_->GetType(), type_);
    target_element->SetAnimatedAttribute(attribute_name, animated_value_);
    return;
  }
  DCHECK(IsAnimatingCSSProperty());
  // Presentation attributes which has an SVG DOM representation should use the
  // "SVG DOM" code-path (above.)
  DCHECK(SVGElement::IsAnimatableCSSProperty(attribute_name));

  // CSS properties animation code-path.
  String base_value = ComputeCSSPropertyValue(target_element, css_property_id_);
  animated_value_ = CreatePropertyForAnimation(base_value);
}

void SVGAnimateElement::ClearAnimatedType() {
  if (!animated_value_)
    return;

  // The animated property lock is held for the "result animation" (see
  // SMILTimeContainer::updateAnimations()) while we're processing an animation
  // group. We will very likely crash later if we clear the animated type while
  // the lock is held. See crbug.com/581546.
  DCHECK(!AnimatedTypeIsLocked());

  SVGElement* target_element = this->targetElement();
  if (!target_element) {
    animated_value_.Clear();
    return;
  }

  bool should_apply = ShouldApplyAnimation(*target_element, AttributeName());
  if (IsAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    if (should_apply) {
      MutableCSSPropertyValueSet* property_set =
          target_element->EnsureAnimatedSMILStyleProperties();
      if (property_set->RemoveProperty(css_property_id_)) {
        target_element->SetNeedsStyleRecalc(
            kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                   style_change_reason::kAnimation));
      }
    }
  }
  if (IsAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    target_element->ClearAnimatedAttribute(AttributeName());
    if (should_apply)
      target_element->InvalidateAnimatedAttribute(AttributeName());
  }

  animated_value_.Clear();
  ClearTargetProperty();
}

void SVGAnimateElement::ApplyResultsToTarget() {
  DCHECK_NE(GetAnimatedPropertyType(), kAnimatedUnknown);

  // Early exit if our animated type got destructed by a previous
  // endedActiveInterval().
  if (!animated_value_)
    return;

  if (!ShouldApplyAnimation(*targetElement(), AttributeName()))
    return;

  // We do update the style and the animation property independent of each
  // other.
  if (IsAnimatingCSSProperty()) {
    // CSS properties animation code-path.
    // Convert the result of the animation to a String and apply it as CSS
    // property on the target.
    MutableCSSPropertyValueSet* property_set =
        targetElement()->EnsureAnimatedSMILStyleProperties();
    if (property_set
            ->SetProperty(
                css_property_id_, animated_value_->ValueAsString(), false,
                targetElement()->GetDocument().GetSecureContextMode(), nullptr)
            .did_change) {
      targetElement()->SetNeedsStyleRecalc(
          kLocalStyleChange,
          StyleChangeReasonForTracing::Create(style_change_reason::kAnimation));
    }
  }
  if (IsAnimatingSVGDom()) {
    // SVG DOM animVal animation code-path.
    // At this point the SVG DOM values are already changed, unlike for CSS.
    // We only have to trigger update notifications here.
    targetElement()->InvalidateAnimatedAttribute(AttributeName());
  }
}

bool SVGAnimateElement::AnimatedPropertyTypeSupportsAddition() {
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties.
  switch (GetAnimatedPropertyType()) {
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

bool SVGAnimateElement::IsAdditive() {
  if (GetAnimationMode() == kByAnimation ||
      GetAnimationMode() == kFromByAnimation) {
    if (!AnimatedPropertyTypeSupportsAddition())
      return false;
  }

  return SVGAnimationElement::IsAdditive();
}

float SVGAnimateElement::CalculateDistance(const String& from_string,
                                           const String& to_string) {
  DCHECK(targetElement());
  // FIXME: A return value of float is not enough to support paced animations on
  // lists.
  SVGPropertyBase* from_value = CreatePropertyForAnimation(from_string);
  SVGPropertyBase* to_value = CreatePropertyForAnimation(to_string);
  return from_value->CalculateDistance(to_value, targetElement());
}

void SVGAnimateElement::WillChangeAnimationTarget() {
  SVGAnimationElement::WillChangeAnimationTarget();
  if (targetElement())
    ClearAnimatedType();
}

void SVGAnimateElement::DidChangeAnimationTarget() {
  SVGAnimationElement::DidChangeAnimationTarget();
  ResetAnimatedPropertyType();
}

void SVGAnimateElement::SetAttributeName(const QualifiedName& attribute_name) {
  WillChangeAnimationTarget();
  attribute_name_ = attribute_name;
  DidChangeAnimationTarget();
}

void SVGAnimateElement::SetAttributeType(const AtomicString& attribute_type) {
  WillChangeAnimationTarget();
  if (attribute_type == "CSS")
    attribute_type_ = kAttributeTypeCSS;
  else if (attribute_type == "XML")
    attribute_type_ = kAttributeTypeXML;
  else
    attribute_type_ = kAttributeTypeAuto;
  DidChangeAnimationTarget();
}

void SVGAnimateElement::ResetAnimatedPropertyType() {
  DCHECK(!animated_value_);
  InvalidatedValuesCache();
  from_property_.Clear();
  to_property_.Clear();
  to_at_end_of_duration_property_.Clear();
  ClearTargetProperty();
}

void SVGAnimateElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(from_property_);
  visitor->Trace(to_property_);
  visitor->Trace(to_at_end_of_duration_property_);
  visitor->Trace(animated_value_);
  visitor->Trace(target_property_);
  SVGAnimationElement::Trace(visitor);
}

}  // namespace blink
