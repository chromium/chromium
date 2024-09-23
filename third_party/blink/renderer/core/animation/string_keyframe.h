// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_STRING_KEYFRAME_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_STRING_KEYFRAME_H_

#include "third_party/blink/renderer/core/animation/keyframe.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

class CSSPropertyName;
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
class CORE_EXPORT StringKeyframe : public Keyframe {
 public:
  explicit StringKeyframe(const TreeScope* tree_scope = nullptr)
      : tree_scope_(tree_scope),
        presentation_attribute_map_(
            MakeGarbageCollected<MutableCSSPropertyValueSet>(
                kHTMLStandardMode)) {}
  StringKeyframe(const StringKeyframe& copy_from);

  MutableCSSPropertyValueSet::SetResult SetCSSPropertyValue(
      const AtomicString& custom_property_name,
      const String& value,
      SecureContextMode,
      StyleSheetContents*);
  MutableCSSPropertyValueSet::SetResult SetCSSPropertyValue(
      CSSPropertyID,
      const String& value,
      SecureContextMode,
      StyleSheetContents*);
  void SetCSSPropertyValue(const CSSPropertyName&, const CSSValue&);
  void RemoveCustomCSSProperty(const PropertyHandle& property);

  void SetPresentationAttributeValue(const CSSProperty&,
                                     const String& value,
                                     SecureContextMode,
                                     StyleSheetContents*);
  void SetSVGAttributeValue(const QualifiedName&, const String& value);

  const CSSValue& CssPropertyValue(const PropertyHandle& property) const {
    EnsureCssPropertyMap();
    int index = -1;
    if (property.IsCSSCustomProperty()) {
      index =
          css_property_map_->FindPropertyIndex(property.CustomPropertyName());
    } else {
      DCHECK(!property.GetCSSProperty().IsShorthand());
      index = css_property_map_->FindPropertyIndex(
          property.GetCSSProperty().PropertyID());
    }
    CHECK_GE(index, 0);
    return css_property_map_->PropertyAt(static_cast<unsigned>(index))
        .Value()
        .EnsureScopedValue(tree_scope_.Get());
  }

  const CSSValue& PresentationAttributeValue(
      const CSSProperty& property) const {
    int index =
        presentation_attribute_map_->FindPropertyIndex(property.PropertyID());
    CHECK_GE(index, 0);
    return presentation_attribute_map_->PropertyAt(static_cast<unsigned>(index))
        .Value()
        .EnsureScopedValue(tree_scope_.Get());
    ;
  }

  String SvgPropertyValue(const QualifiedName& attribute_name) const {
    return svg_attribute_map_.at(&attribute_name);
  }

  PropertyHandleSet Properties() const override;

  bool HasCssProperty() const;

  void AddKeyframePropertiesToV8Object(V8ObjectBuilder&,
                                       Element*) const override;

  Keyframe* Clone() const override;

  bool HasLogicalProperty() { return has_logical_property_; }

  bool SetLogicalPropertyResolutionContext(
      WritingDirectionMode writing_direction);

  void Trace(Visitor*) const override;

  class CSSPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    CSSPropertySpecificKeyframe(double offset,
                                scoped_refptr<TimingFunction> easing,
                                const CSSValue* value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value) {}

    const CSSValue* Value() const { return value_.Get(); }

    bool PopulateCompositorKeyframeValue(
        const PropertyHandle&,
        Element&,
        const ComputedStyle& base_style,
        const ComputedStyle* parent_style) const final;
    const CompositorKeyframeValue* GetCompositorKeyframeValue() const final {
      return compositor_keyframe_value_cache_.Get();
    }

    bool IsNeutral() const final { return !value_; }
    bool IsRevert() const final;
    bool IsRevertLayer() const final;
    Keyframe::PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final;

    void Trace(Visitor*) const override;

   private:
    Keyframe::PropertySpecificKeyframe* CloneWithOffset(
        double offset) const override;
    bool IsCSSPropertySpecificKeyframe() const override { return true; }

    Member<const CSSValue> value_;
    mutable Member<CompositorKeyframeValue> compositor_keyframe_value_cache_;
  };

  class SVGPropertySpecificKeyframe
      : public Keyframe::PropertySpecificKeyframe {
   public:
    SVGPropertySpecificKeyframe(double offset,
                                scoped_refptr<TimingFunction> easing,
                                const String& value,
                                EffectModel::CompositeOperation composite)
        : Keyframe::PropertySpecificKeyframe(offset,
                                             std::move(easing),
                                             composite),
          value_(value) {}

    const String& Value() const { return value_; }

    PropertySpecificKeyframe* CloneWithOffset(double offset) const final;

    const CompositorKeyframeValue* GetCompositorKeyframeValue() const final {
      return nullptr;
    }

    bool IsNeutral() const final { return value_.IsNull(); }
    bool IsRevert() const final { return false; }
    bool IsRevertLayer() const final { return false; }
    PropertySpecificKeyframe* NeutralKeyframe(
        double offset,
        scoped_refptr<TimingFunction> easing) const final;

   private:
    bool IsSVGPropertySpecificKeyframe() const override { return true; }

    String value_;
  };

  class PropertyResolver : public GarbageCollected<PropertyResolver> {
   public:
    // Custom properties must use this version of the constructor.
    PropertyResolver(CSSPropertyID property_id, const CSSValue& css_value);

    // Shorthand and logical properties must use this version of the
    // constructor.
    PropertyResolver(const CSSProperty& property,
                     const MutableCSSPropertyValueSet* property_value_set,
                     bool is_logical);

    static PropertyResolver* CreateCustomVariableResolver(
        const CSSValue& css_value);

    bool IsValid() const;

    const CSSValue* CssValue();

    void AppendTo(MutableCSSPropertyValueSet* property_value_set,
                  WritingDirectionMode writing_direction);

    void SetProperty(MutableCSSPropertyValueSet* property_value_set,
                     CSSPropertyID property_id,
                     const CSSValue& value,
                     WritingDirectionMode writing_direction);

    static bool HasLowerPriority(PropertyResolver* first,
                                 PropertyResolver* second);

    // Helper methods for resolving longhand name collisions.
    // Longhands take priority over shorthands.
    // Physical properties take priority over logical.
    // Two shorthands with overlapping longhand properties are sorted based
    // on the number of longhand properties in their expansions.
    bool IsLogical() { return is_logical_; }
    bool IsShorthand() { return css_property_value_set_ != nullptr; }
    unsigned ExpansionCount() {
      return css_property_value_set_ ? css_property_value_set_->PropertyCount()
                                     : 1;
    }

    void Trace(Visitor* visitor) const;

   private:
    CSSPropertyID property_id_ = CSSPropertyID::kInvalid;
    Member<const CSSValue> css_value_ = nullptr;
    Member<ImmutableCSSPropertyValueSet> css_property_value_set_ = nullptr;
    bool is_logical_ = false;
  };

 private:
  Keyframe::PropertySpecificKeyframe* CreatePropertySpecificKeyframe(
      const PropertyHandle&,
      EffectModel::CompositeOperation effect_composite,
      double offset) const override;

  void InvalidateCssPropertyMap() { css_property_map_ = nullptr; }
  void EnsureCssPropertyMap() const;

  bool IsStringKeyframe() const override { return true; }

  // The tree scope for all the tree-scoped names and references in the
  // keyframe. Nullptr if there's no such tree scope (e.g., the keyframe is
  // created via JavaScript or defined by UA style sheet).
  WeakMember<const TreeScope> tree_scope_;

  // Mapping of unresolved properties to a their resolvers. A resolver knows
  // how to expand shorthands to their corresponding longhand property names,
  // convert logical to physical property names and compare precedence for
  // resolving longhand name collisions.  The resolver also knows how to
  // create serialized text for a shorthand, which is required for getKeyframes
  // calls.
  // See: https://w3.org/TR/web-animations-1/#keyframes-section
  HeapHashMap<PropertyHandle, Member<PropertyResolver>> input_properties_;

  // The resolved properties are computed from unresolved ones applying these
  // steps:
  //  1. Resolve conflicts when multiple properties map to same underlying
  //      one (e.g., margin, margin-top)
  //  2. Expand shorthands to longhands
  //  3. Expand logical properties to physical ones
  mutable Member<MutableCSSPropertyValueSet> css_property_map_;
  Member<MutableCSSPropertyValueSet> presentation_attribute_map_;
  HashMap<const QualifiedName*, String> svg_attribute_map_;

  // If the keyframes contain one or more logical properties, these need to be
  // remapped to physical properties when the writing mode or text direction
  // changes.
  bool has_logical_property_ = false;

  // The following member is required for mapping logical to physical
  // property names. Though the same for all keyframes within the same model,
  // we store the value here to facilitate lazy evaluation of the CSS
  // properties.
  WritingDirectionMode writing_direction_{WritingMode::kHorizontalTb,
                                          TextDirection::kLtr};
};

using CSSPropertySpecificKeyframe = StringKeyframe::CSSPropertySpecificKeyframe;
using SVGPropertySpecificKeyframe = StringKeyframe::SVGPropertySpecificKeyframe;

template <>
struct DowncastTraits<StringKeyframe> {
  static bool AllowFrom(const Keyframe& value) {
    return value.IsStringKeyframe();
  }
};
template <>
struct DowncastTraits<CSSPropertySpecificKeyframe> {
  static bool AllowFrom(const Keyframe::PropertySpecificKeyframe& value) {
    return value.IsCSSPropertySpecificKeyframe();
  }
};
template <>
struct DowncastTraits<SVGPropertySpecificKeyframe> {
  static bool AllowFrom(const Keyframe::PropertySpecificKeyframe& value) {
    return value.IsSVGPropertySpecificKeyframe();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_STRING_KEYFRAME_H_
