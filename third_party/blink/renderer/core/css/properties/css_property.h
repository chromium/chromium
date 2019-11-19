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
class LayoutObject;
class SVGComputedStyle;

class CORE_EXPORT CSSProperty : public CSSUnresolvedProperty {
 public:
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
  static void FilterWebExposedCSSPropertiesIntoVector(
      const CSSPropertyID*,
      size_t length,
      Vector<const CSSProperty*>&);

 protected:
  enum Flag : uint16_t {
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
    kIsAffectedByForcedColors = 1 << 9
  };

  constexpr CSSProperty(CSSPropertyID property_id,
                        uint16_t flags,
                        char repetition_separator)
      : CSSUnresolvedProperty(),
        property_id_(property_id),
        flags_(flags),
        repetition_separator_(repetition_separator) {}

 private:
  CSSPropertyID property_id_;
  uint16_t flags_;
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
