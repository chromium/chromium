// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/cssom/style_property_map.h"

namespace blink {

class CSSParserContext;
class FontDescription;

class CanvasFormattedTextStyle : public GarbageCollectedMixin {
  DISALLOW_NEW();

 public:
  explicit CanvasFormattedTextStyle() = default;

  const CSSPropertyValueSet* GetCssPropertySet() const;
  void SetStyle(const CSSParserContext* context, const String& style_text);

  void Trace(Visitor* visitor) const override;

 private:
  Member<MutableCSSPropertyValueSet> css_property_value_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CANVAS_CANVAS2D_CANVAS_FORMATTED_TEXT_STYLE_H_
