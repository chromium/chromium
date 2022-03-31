// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

namespace blink {

class CanvasFormattedTextStylePropertyMap;
class FontDescription;

class CanvasFormattedTextStyle : public GarbageCollectedMixin {
  DISALLOW_NEW();

 public:
  explicit CanvasFormattedTextStyle(bool is_text_run)
      : is_text_run_(is_text_run) {}
  StylePropertyMap* styleMap();

  virtual void SetNeedsStyleRecalc() = 0;
  const CSSPropertyValueSet* GetCssPropertySet() const;

  void Trace(Visitor* visitor) const override;

 private:
  bool is_text_run_;
  Member<CanvasFormattedTextStylePropertyMap> style_map_;
};

class CanvasFormattedTextStylePropertyMap final : public StylePropertyMap {
 public:
  explicit CanvasFormattedTextStylePropertyMap(bool is_text_run,
                                               CanvasFormattedTextStyle*);
  CanvasFormattedTextStylePropertyMap(
      const CanvasFormattedTextStylePropertyMap&) = delete;
  CanvasFormattedTextStylePropertyMap& operator=(
      const CanvasFormattedTextStylePropertyMap&) = delete;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(css_property_value_set_);
    visitor->Trace(canvas_formatted_text_style_);
    StylePropertyMap::Trace(visitor);
  }

  unsigned int size() const final {
    return css_property_value_set_->PropertyCount();
  }

  const CSSPropertyValueSet* GetCssPropertySet() const {
    return css_property_value_set_;
  }

 protected:
  const CSSValue* GetProperty(CSSPropertyID) const override;
  const CSSValue* GetCustomProperty(const AtomicString&) const override;
  void ForEachProperty(const IterationCallback&) override;
  void SetProperty(CSSPropertyID, const CSSValue&) override;
  bool SetShorthandProperty(CSSPropertyID,
                            const String&,
                            SecureContextMode) override;
  void SetCustomProperty(const AtomicString&, const CSSValue&) override;
  void RemoveProperty(CSSPropertyID) override;
  void RemoveCustomProperty(const AtomicString&) override;
  void RemoveAllProperties() final;

  String SerializationForShorthand(const CSSProperty&) const final;

 private:
  Member<MutableCSSPropertyValueSet> css_property_value_set_;
  WeakMember<CanvasFormattedTextStyle> canvas_formatted_text_style_;
  bool is_text_run_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_
