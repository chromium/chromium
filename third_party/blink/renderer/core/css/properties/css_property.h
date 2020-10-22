// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_

#include <memory>
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ComputedStyle;
class CrossThreadStyleValue;
class ExecutionContext;
class LayoutObject;
class SVGComputedStyle;

class CORE_EXPORT CSSProperty : public CSSUnresolvedProperty {
 public:
  using Flags = uint32_t;

  static const CSSProperty& Get(CSSPropertyID);

  // For backwards compatibility when passing around CSSUnresolvedProperty
  // references. In case we need to call a function that hasn't been converted
  // to using property classes yet.
  CSSPropertyID PropertyID() const { return property_id_; }
  virtual CSSPropertyName GetCSSPropertyName() const {
    return CSSPropertyName(PropertyID());
  }

  bool IDEquals(CSSPropertyID id) const { return PropertyID() == id; }
  bool IsResolvedProperty() const override { return true; }

  Flags GetFlags() const { return flags_; }
  bool IsInterpolable() const { return flags_ & kInterpolable; }
  bool IsCompositableProperty() const { return flags_ & kCompositableProperty; }
  bool IsDescriptor() const { return flags_ & kDescriptor; }
  bool IsProperty() const { return flags_ & kProperty; }
  bool IsShorthand() const { return flags_ & kShorthand; }
  bool IsLonghand() const { return flags_ & kLonghand; }
  bool IsInherited() const { return flags_ & kInherited; }
  bool IsVisited() const { return flags_ & kVisited; }
  bool IsInternal() const { return flags_ & kInternal; }
  bool IsAffectedByForcedColors() const {
    return flags_ & kIsAffectedByForcedColors;
  }
  bool IsValidForFirstLetter() const { return flags_ & kValidForFirstLetter; }
  bool IsValidForCue() const { return flags_ & kValidForCue; }
  bool IsValidForMarker() const { return flags_ & kValidForMarker; }
  bool IsSurrogate() const { return flags_ & kSurrogate; }
  bool AffectsFont() const { return flags_ & kAffectsFont; }
  bool IsBackground() const { return flags_ & kBackground; }
  bool IsBorder() const { return flags_ & kBorder; }
  bool IsComputedValueComparable() const {
    return flags_ & kComputedValueComparable;
  }
  bool TakesTreeScopedValue() const { return flags_ & kTreeScopedValue; }

  bool IsRepeated() const { return repetition_separator_ != '\0'; }
  char RepetitionSeparator() const { return repetition_separator_; }

  virtual bool IsAffectedByAll() const {
    return IsWebExposed() && IsProperty();
  }
  virtual bool IsLayoutDependentProperty() const { return false; }
  virtual bool IsLayoutDependent(const ComputedStyle* style,
                                 LayoutObject* layout_object) const {
    return false;
  }

  virtual bool ComputedValuesEqual(const ComputedStyle&,
                                   const ComputedStyle&) const {
    // May only be called if IsComputedValueComparable() is true.
    NOTREACHED();
    return false;
  }

  virtual const CSSValue* CSSValueFromComputedStyleInternal(
      const ComputedStyle&,
      const SVGComputedStyle&,
      const LayoutObject*,
      bool allow_visited_style) const {
    return nullptr;
  }
  const CSSValue* CSSValueFromComputedStyle(const ComputedStyle&,
                                            const LayoutObject*,
                                            bool allow_visited_style) const;
  std::unique_ptr<CrossThreadStyleValue> CrossThreadStyleValueFromComputedStyle(
      const ComputedStyle& computed_style,
      const LayoutObject* layout_object,
      bool allow_visited_style) const;
  virtual const CSSProperty& ResolveDirectionAwareProperty(TextDirection,
                                                           WritingMode) const {
    return *this;
  }
  virtual const CSSProperty* GetVisitedProperty() const { return nullptr; }
  virtual const CSSProperty* GetUnvisitedProperty() const { return nullptr; }

  virtual const CSSProperty* SurrogateFor(TextDirection, WritingMode) const {
    return nullptr;
  }

  static void FilterWebExposedCSSPropertiesIntoVector(
      const ExecutionContext*,
      const CSSPropertyID*,
      size_t length,
      Vector<const CSSProperty*>&);

  enum Flag : Flags {
    kInterpolable = 1 << 0,
    kCompositableProperty = 1 << 1,
    kDescriptor = 1 << 2,
    kProperty = 1 << 3,
    kShorthand = 1 << 4,
    kLonghand = 1 << 5,
    kInherited = 1 << 6,
    // Visited properties are internal counterparts to properties that
    // are permitted in :visited styles. They are used to handle and store the
    // computed value as seen by painting (as opposed to the computed value
    // seen by CSSOM, which is represented by the unvisited property).
    kVisited = 1 << 7,
    kInternal = 1 << 8,
    kIsAffectedByForcedColors = 1 << 9,
    // Animation properties have this flag set. (I.e. longhands of the
    // 'animation' and 'transition' shorthands).
    kAnimation = 1 << 10,
    // https://drafts.csswg.org/css-pseudo-4/#first-letter-styling
    kValidForFirstLetter = 1 << 11,
    // https://w3c.github.io/webvtt/#the-cue-pseudo-element
    kValidForCue = 1 << 12,
    // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo
    kValidForMarker = 1 << 13,
    // A surrogate is a (non-alias) property which acts like another property,
    // for example -webkit-writing-mode is a surrogate for writing-mode, and
    // inline-size is a surrogate for either width or height.
    kSurrogate = 1 << 14,
    kAffectsFont = 1 << 15,
    // If the author specifies any background or border property on an UI
    // element, the native appearance must be disabled.
    kBackground = 1 << 16,
    kBorder = 1 << 17,
    // Set if ComputedValuesEqual is implemented for the given CSSProperty.
    kComputedValueComparable = 1 << 18,
    // Set if the property values are tree-scoped references.
    kTreeScopedValue = 1 << 19,
  };

  constexpr CSSProperty(CSSPropertyID property_id,
                        Flags flags,
                        char repetition_separator)
      : CSSUnresolvedProperty(),
        property_id_(property_id),
        flags_(flags),
        repetition_separator_(repetition_separator) {}

 private:
  CSSPropertyID property_id_;
  Flags flags_;
  char repetition_separator_;
};

template <>
struct DowncastTraits<CSSProperty> {
  static bool AllowFrom(const CSSUnresolvedProperty& unresolved) {
    return unresolved.IsResolvedProperty();
  }
};

CORE_EXPORT const CSSProperty& GetCSSPropertyVariable();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
