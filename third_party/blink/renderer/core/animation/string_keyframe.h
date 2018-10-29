// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_STRING_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_STRING_KEYFRAME_H_

#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"

#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class StyleSheetContents;

// An implementation of Keyframe used for CSS Animations, web-animations, and
// the HTML <marquee> element.
//
// A StringKeyframe instance supports an arbitrary number of (property, value)
// pairs. The properties can be CSS properties or SVG attributes, mapping to
// CSSValue or plain String values respectively. CSS properties added to a
// StringKeyframe are expanded to shorthand and de-duplicated, with newer
// properties replacing older ones. SVG attributes are similarly de-duplicated.
//
// TODO(smcgruer): By the spec, a StringKeyframe should not de-duplicate or
// expand shorthand properties; that is done for computed keyframes.
class CORE_EXPORT StringKeyframe : public Keyframe {
 public:
  static StringKeyframe* Create() { return new StringKeyframe; }

  MutableCSSPropertyValueSet::SetResult SetCSSPropertyValue(
      const AtomicString& property_name,
      const PropertyRegistry*,
      const String& value,
      SecureContextMode,
      StyleSheetContents*);
  MutableCSSPropertyValueSet::SetResult SetCSSPropertyValue(
      CSSPropertyID,
      const String& value,
      SecureContextMode,
      StyleSheetContents*);
  void SetCSSPropertyValue(const CSSProperty&, const CSSValue&);
  void SetPresentationAttributeValue(const CSSProperty&,
                                     const String& value,
                                     SecureContextMode,
                                     StyleSheetContents*);
  void SetSVGAttributeValue(const QualifiedName&, const String& value);

  const CSSValue& CssPropertyValue(const PropertyHandle& property) const {
    int index = -1;
    if (property.IsCSSCustomProperty())
      index =
          css_property_map_->FindPropertyIndex(property.CustomPropertyName());
    else
      index = css_property_map_->FindPropertyIndex(
          property.GetCSSProperty().PropertyID());
    CHECK_GE(index, 0);
    return css_property_map_->PropertyAt(static_cast<unsigned>(index)).Value();
  }

  const CSSValue& PresentationAttributeValue(
      const CSSProperty& property) const {
    int index =
        presentation_attribute_map_->FindPropertyIndex(property.PropertyID());
    CHECK_GE(index, 0);
    return presentation_attribute_map_->PropertyAt(static_cast<unsigned>(index))
        .Value();
  }

  String SvgPropertyValue(const QualifiedName& attribute_name) const {
    return svg_attribute_map_.at(&attribute_name);
  }

  PropertyHandleSet Properties() const override;

  bool HasCssProperty() const;

  void AddKeyframePropertiesToV8Object(V8ObjectBuilder&) const override;

  void Trace(Visitor*) override;

  class CSSPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static CSSPropertySpecificKeyframe* Create(
        double offset,
        scoped_refptr<TimingFunction> easing,
        const CSSValue* value,
        EffectModel::CompositeOperation composite) {
      return new CSSPropertySpecificKeyframe(offset, std::move(easing), value,
                                             composite);
    }

    const CSSValue* Value() const { return value_.Get(); }

    bool PopulateAnimatableValue(const PropertyHandle&,
                                 Element&,
                                 const ComputedStyle& base_style,
                                 const ComputedStyle* parent_style) const final;
    const AnimatableValue* GetAnimatableValue() const final {
      return animatable_value_cache_;
    }

    bool IsNeutral() const final { return !value_; }
    Keyframe::PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final;

    void Trace(Visitor*) override;

   private:
    CSSPropertySpecificKeyframe(double offset,
                                scoped_refptr<TimingFunction> easing,
                                const CSSValue* value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value) {}

    Keyframe::PropertySpecificKeyframe* CloneWithOffset(
        double offset) const override;
    bool IsCSSPropertySpecificKeyframe() const override { return true; }

    Member<const CSSValue> value_;
    mutable Member<AnimatableValue> animatable_value_cache_;
  };

  class SVGPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    static SVGPropertySpecificKeyframe* Create(
        double offset,
        scoped_refptr<TimingFunction> easing,
        const String& value,
        EffectModel::CompositeOperation composite) {
      return new SVGPropertySpecificKeyframe(offset, std::move(easing), value,
                                             composite);
    }

    const String& Value() const { return value_; }

    PropertySpecificKeyframe* CloneWithOffset(double offset) const final;

    const AnimatableValue* GetAnimatableValue() const final { return nullptr; }

    bool IsNeutral() const final { return value_.IsNull(); }
    PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final;

   private:
    SVGPropertySpecificKeyframe(double offset,
                                scoped_refptr<TimingFunction> easing,
                                const String& value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value) {}

    bool IsSVGPropertySpecificKeyframe() const override { return true; }

    String value_;
  };

 protected:
  StringKeyframe()
      : css_property_map_(
            MutableCSSPropertyValueSet::Create(kHTMLStandardMode)),
        presentation_attribute_map_(
            MutableCSSPropertyValueSet::Create(kHTMLStandardMode)) {}

 private:
  StringKeyframe(const StringKeyframe& copy_from);

  Keyframe* Clone() const override;
  Keyframe::PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
      const PropertyHandle&,
      EffectModel::CompositeOperation effect_composite,
      double offset) const override;

  bool IsStringKeyframe() const override { return true; }

  Member<MutableCSSPropertyValueSet> css_property_map_;
  Member<MutableCSSPropertyValueSet> presentation_attribute_map_;
  HashMap<const QualifiedName*, String> svg_attribute_map_;
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

DEFINE_TYPE_CASTS(StringKeyframe,
                  Keyframe,
                  value,
                  value->IsStringKeyframe(),
                  value.IsStringKeyframe());
DEFINE_TYPE_CASTS(CSSPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->IsCSSPropertySpecificKeyframe(),
                  value.IsCSSPropertySpecificKeyframe());
DEFINE_TYPE_CASTS(SVGPropertySpecificKeyframe,
                  Keyframe::PropertySpecificKeyframe,
                  value,
                  value->IsSVGPropertySpecificKeyframe(),
                  value.IsSVGPropertySpecificKeyframe());

}  // namespace blink

#endif
