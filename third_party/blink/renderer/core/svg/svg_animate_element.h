/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_ELEMENT_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/svg/properties/svg_property_info.h"
#include "third_party/blink/renderer/core/svg/svg_animation_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// If we have 'inherit' as animation value, we need to grab the value
// during the animation since the value can be animated itself.
enum AnimatedPropertyValueType { kRegularPropertyValue, kInheritValue };

struct ParsedAnimationValue {
  STACK_ALLOCATED();

 public:
  SVGPropertyBase* property;
  AnimatedPropertyValueType property_value_type;
  SVGParseStatus status;
};

class CORE_EXPORT SVGAnimateElement : public SVGAnimationElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SVGAnimateElement(Document&);
  SVGAnimateElement(const QualifiedName&, Document&);
  ~SVGAnimateElement() override;

  void Trace(Visitor*) const override;

  bool IsSVGAnimationAttributeSettingJavaScriptURL(
      const Attribute&) const override;

  const QualifiedName& AttributeName() const { return attribute_name_; }
  AnimatedPropertyType GetAnimatedPropertyTypeForTesting() const {
    return type_;
  }
  bool AnimatedPropertyTypeSupportsAddition() const;

 protected:
  void WillChangeAnimationTarget() final;
  void DidChangeAnimationTarget() final;

  bool HasValidAnimation() const override;

  SMILAnimationValue CreateAnimationValue() const final;
  void ClearAnimationValue() final;

  AnimationMode CalculateAnimationMode() override;
  void UpdateKeyframeValues(const Keyframe& keyframe) override;
  bool CalculateFromAndToValues(const String& from_string,
                                const String& to_string) final;
  bool CalculateFromAndByValues(const String& from_string,
                                const String& by_string) final;
  bool CalculateValues(const Vector<String>& values) final;
  wtf_size_t ValuesCount() const final {
    DCHECK_EQ(GetAnimationMode(), kValuesAnimation);
    return values_.size();
  }
  void CalculateAnimationValue(SMILAnimationValue&,
                               float percentage,
                               unsigned repeat_count) const final;
  void ApplyResultsToTarget(const SMILAnimationValue&) final;
  float CalculateDistance(const Keyframe&) const final;

  void ParseAttribute(const AttributeModificationParams&) override;

  void SetAttributeName(const QualifiedName&);

  enum AttributeType {
    kAttributeTypeCSS,
    kAttributeTypeXML,
    kAttributeTypeAuto
  };
  AttributeType GetAttributeType() const { return attribute_type_; }

  FRIEND_TEST_ALL_PREFIXES(UnsafeSVGAttributeSanitizationTest,
                           stringsShouldNotSupportAddition);

 private:
  void SetAttributeType(const AtomicString&);

  InsertionNotificationRequest InsertedInto(ContainerNode&) final;
  void RemovedFrom(ContainerNode&) final;

  virtual void ResolveTargetProperty();
  void ClearTargetProperty();
  void UpdateTargetProperty();

  void WillChangeAnimatedType();
  void DidChangeAnimatedType();

  void ClearValues();

  virtual SVGPropertyBase* CreateUnderlyingValueForAnimation() const;
  virtual ParsedAnimationValue ParseValue(const String&) const;
  SVGPropertyBase* CreateUnderlyingValueForAttributeAnimation() const;
  std::pair<SVGPropertyBase*, SVGParseStatus>
  CreatePropertyForAttributeAnimation(const String&) const;
  SVGPropertyBase* CreatePropertyForCSSAnimation(const CSSValue* value) const;
  std::pair<SVGPropertyBase*, SVGParseStatus> CreatePropertyForCSSAnimation(
      const String&) const;

  SVGPropertyBase* AdjustForInheritance(SVGPropertyBase*,
                                        AnimatedPropertyValueType) const;

  Member<SVGPropertyBase> from_property_;
  Member<SVGPropertyBase> to_property_;

 protected:
  Member<const SVGAnimatedPropertyBase> target_property_;
  QualifiedName attribute_name_;
  AnimatedPropertyType type_;
  CSSPropertyID css_property_id_;

  bool IsAnimatingSVGDom() const { return target_property_ != nullptr; }
  bool IsAnimatingCSSProperty() const {
    return css_property_id_ != CSSPropertyID::kInvalid;
  }

 private:
  AnimatedPropertyValueType from_property_value_type_;
  AnimatedPropertyValueType to_property_value_type_;
  HeapVector<Member<SVGPropertyBase>> values_;
  std::vector<bool> values_is_inherit_
      ALLOW_DISCOURAGED_TYPE("More space-efficient than Vector<bool>");
  AttributeType attribute_type_;
};

inline bool IsSVGAnimateElement(const SVGElement& element) {
  return element.HasTagName(svg_names::kAnimateTag) ||
         element.HasTagName(svg_names::kAnimateTransformTag) ||
         element.HasTagName(svg_names::kSetTag);
}

template <>
struct DowncastTraits<SVGAnimateElement> {
  static bool AllowFrom(const Node& node) {
    auto* element = DynamicTo<SVGElement>(node);
    return element && IsSVGAnimateElement(*element);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_ANIMATE_ELEMENT_H_
