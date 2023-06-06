// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/properties/css_direction_aware_resolver.h"
#include "third_party/blink/renderer/core/css/properties/css_unresolved_property.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ComputedStyle;
class CrossThreadStyleValue;
class ExecutionContext;
class LayoutObject;

class CORE_EXPORT CSSProperty : public CSSUnresolvedProperty {
 public:
  using Flags = uint64_t;

  static const CSSProperty& Get(CSSPropertyID id) {
    DCHECK(id != CSSPropertyID::kInvalid);
    DCHECK(id <= kLastCSSProperty);  // last property id
    return To<CSSProperty>(*GetPropertyInternal(id));
  }

  static bool IsShorthand(const CSSPropertyName&);
  static bool IsRepeated(const CSSPropertyName&);

  // For backwards compatibility when passing around CSSUnresolvedProperty
  // references. In case we need to call a function that hasn't been converted
  // to using property classes yet.
  CSSPropertyID PropertyID() const {
    return static_cast<CSSPropertyID>(property_id_);
  }
  virtual CSSPropertyName GetCSSPropertyName() const {
    return CSSPropertyName(PropertyID());
  }
  virtual bool HasEqualCSSPropertyName(const CSSProperty& other) const;

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
  bool IsAnimationProperty() const { return flags_ & kAnimation; }
  bool SupportsIncrementalStyle() const {
    return flags_ & kSupportsIncrementalStyle;
  }
  bool IsIdempotent() const { return flags_ & kIdempotent; }
  bool AcceptsNumericLiteral() const { return flags_ & kAcceptsNumericLiteral; }
  bool IsValidForFirstLetter() const { return flags_ & kValidForFirstLetter; }
  bool IsValidForFirstLine() const { return flags_ & kValidForFirstLine; }
  bool IsValidForCue() const { return flags_ & kValidForCue; }
  bool IsValidForMarker() const { return flags_ & kValidForMarker; }
  bool IsValidForFormattedText() const {
    return flags_ & kValidForFormattedText;
  }
  bool IsValidForFormattedTextRun() const {
    return flags_ & kValidForFormattedTextRun;
  }
  bool IsValidForKeyframe() const { return flags_ & kValidForKeyframe; }
  bool IsValidForPositionFallback() const {
    return flags_ & kValidForPositionFallback;
  }
  bool IsSurrogate() const { return flags_ & kSurrogate; }
  bool AffectsFont() const { return flags_ & kAffectsFont; }
  bool IsBackground() const { return flags_ & kBackground; }
  bool IsBorder() const { return flags_ & kBorder; }
  bool IsBorderRadius() const { return flags_ & kBorderRadius; }
  bool IsInLogicalPropertyGroup() const {
    return flags_ & kInLogicalPropertyGroup;
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

  const CSSProperty& ResolveDirectionAwareProperty(
      TextDirection direction,
      WritingMode writing_mode) const {
    if (!IsInLogicalPropertyGroup()) {
      // Avoid the potentially expensive virtual function call.
      return *this;
    } else {
      return ResolveDirectionAwarePropertyInternal(direction, writing_mode);
    }
  }

  virtual const CSSProperty& ResolveDirectionAwarePropertyInternal(
      TextDirection,
      WritingMode) const {
    return *this;
  }
  virtual bool IsInSameLogicalPropertyGroupWithDifferentMappingLogic(
      CSSPropertyID) const {
    return false;
  }
  virtual const CSSProperty* GetVisitedProperty() const { return nullptr; }
  virtual const CSSProperty* GetUnvisitedProperty() const { return nullptr; }

  virtual const CSSProperty* SurrogateFor(TextDirection, WritingMode) const {
    return nullptr;
  }

  static void FilterWebExposedCSSPropertiesIntoVector(
      const ExecutionContext*,
      const CSSPropertyID*,
      wtf_size_t length,
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
    // Animation properties have this flag set. (I.e. longhands of the
    // 'animation' and 'transition' shorthands).
    kAnimation = 1 << 9,
    // https://drafts.csswg.org/css-pseudo-4/#first-letter-styling
    kValidForFirstLetter = 1 << 10,
    // https://w3c.github.io/webvtt/#the-cue-pseudo-element
    kValidForCue = 1 << 11,
    // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo
    kValidForMarker = 1 << 12,
    // A surrogate is a (non-alias) property which acts like another property,
    // for example -webkit-writing-mode is a surrogate for writing-mode, and
    // inline-size is a surrogate for either width or height.
    kSurrogate = 1 << 13,
    kAffectsFont = 1 << 14,
    // If the author specifies any background, border or border-radius property
    // on an UI element, the native appearance must be disabled.
    kBackground = 1 << 15,
    kBorder = 1 << 16,
    kBorderRadius = 1 << 17,
    // Similar to the list at
    // https://drafts.csswg.org/css-pseudo-4/#highlight-styling, with some
    // differences for compatibility reasons.
    kValidForHighlightLegacy = 1 << 18,
    // https://drafts.csswg.org/css-logical/#logical-property-group
    kInLogicalPropertyGroup = 1 << 19,
    // https://drafts.csswg.org/css-pseudo-4/#first-line-styling
    kValidForFirstLine = 1 << 20,
    // The property participates in paired cascade, such that when encountered
    // in highlight styles, we make all other highlight color properties default
    // to initial, rather than the UA default.
    // https://drafts.csswg.org/css-pseudo-4/#highlight-cascade
    kHighlightColors = 1 << 21,
    kVisitedHighlightColors = 1 << 22,
    // See supports_incremental_style in css_properties.json5.
    kSupportsIncrementalStyle = 1 << 23,
    // See idempotent in css_properties.json5.
    kIdempotent = 1 << 24,
    // Set if the css property can apply to the experiemental canvas
    // formatted text API to render multiline text in canvas.
    // https://github.com/WICG/canvas-formatted-text
    kValidForFormattedText = 1 << 25,
    kValidForFormattedTextRun = 1 << 26,
    // See overlapping in css_properties.json5.
    kOverlapping = 1 << 27,
    // See legacy_overlapping in css_properties.json5.
    kLegacyOverlapping = 1 << 28,
    // See valid_for_keyframes in css_properties.json5
    kValidForKeyframe = 1 << 29,
    // See valid_for_position_fallback in css_properties.json5
    kValidForPositionFallback = 1 << 30,
    // https://drafts.csswg.org/css-pseudo-4/#highlight-styling
    kValidForHighlight = 1ull << 31,
    // See accepts_numeric_literal in css_properties.json5.
    kAcceptsNumericLiteral = 1ull << 32,
  };

  constexpr CSSProperty(CSSPropertyID property_id,
                        Flags flags,
                        char repetition_separator)
      : property_id_(static_cast<uint16_t>(property_id)),
        repetition_separator_(repetition_separator),
        flags_(flags) {}

  enum class ValueMode {
    kNormal,
    // https://drafts.csswg.org/css-variables/#animation-tainted
    kAnimated,
  };

 private:
  static constexpr size_t kPropertyIdBits = 16;
  uint64_t property_id_ : kPropertyIdBits;  // NOLINT(runtime/bitfields)
  uint64_t repetition_separator_ : 8;       // NOLINT(runtime/bitfields)
  uint64_t flags_ : 40;                     // NOLINT(runtime/bitfields)

  // Make sure we have room for all valid CSSPropertyIDs.
  // (Using bit fields here reduces CSSProperty size from 24 to 16
  // bytes, and we have many of them that are frequently accessed
  // during style application.)
  static_assert(kPropertyIdBits >= kCSSPropertyIDBitLength);
};
static_assert(sizeof(CSSProperty) <= 16);

template <>
struct DowncastTraits<CSSProperty> {
  static bool AllowFrom(const CSSUnresolvedProperty& unresolved) {
    return unresolved.IsResolvedProperty();
  }
};

CORE_EXPORT const CSSProperty& GetCSSPropertyVariable();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PROPERTIES_CSS_PROPERTY_H_
