// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/canvas_formatted_text_style.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/properties/longhand.h"
#include "third_party/blink/renderer/core/css/resolver/font_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_adjuster.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_property_serializer.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"

namespace blink {

StylePropertyMap* CanvasFormattedTextStyle::styleMap() {
  if (!style_map_) {
    style_map_ = MakeGarbageCollected<CanvasFormattedTextStylePropertyMap>(
        is_text_run_, this);
  }
  return style_map_.Get();
}

const CSSPropertyValueSet* CanvasFormattedTextStyle::GetCssPropertySet() const {
  return style_map_ ? style_map_->GetCssPropertySet() : nullptr;
}

void CanvasFormattedTextStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_map_);
}

CanvasFormattedTextStylePropertyMap::CanvasFormattedTextStylePropertyMap(
    bool is_text_run,
    CanvasFormattedTextStyle* canvas_formatted_text_style)
    : is_text_run_(is_text_run) {
  css_property_value_set_ =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  canvas_formatted_text_style_ = canvas_formatted_text_style;
}

const CSSValue* CanvasFormattedTextStylePropertyMap::GetProperty(
    CSSPropertyID property_id) const {
  return css_property_value_set_->GetPropertyCSSValue(property_id);
}

const CSSValue* CanvasFormattedTextStylePropertyMap::GetCustomProperty(
    const AtomicString& property_name) const {
  // Custom properties or CSS variables are not supported for
  // CanvasFormattedText at this point.
  NOTREACHED();
  return nullptr;
}

void CanvasFormattedTextStylePropertyMap::ForEachProperty(
    const IterationCallback& callback) {
  for (unsigned i = 0; i < css_property_value_set_->PropertyCount(); i++) {
    const auto& property_reference = css_property_value_set_->PropertyAt(i);
    callback(property_reference.Name(), property_reference.Value());
  }
}

void CanvasFormattedTextStylePropertyMap::SetProperty(
    CSSPropertyID unresolved_property,
    const CSSValue& value) {
  const CSSProperty& prop = CSSProperty::Get(unresolved_property);
  if ((prop.IsValidForCanvasFormattedText() && !is_text_run_) ||
      (prop.IsValidForCanvasFormattedTextRun() && is_text_run_)) {
    css_property_value_set_->SetProperty(unresolved_property, value);
    if (canvas_formatted_text_style_)
      canvas_formatted_text_style_->SetNeedsStyleRecalc();
  }
}

bool CanvasFormattedTextStylePropertyMap::SetShorthandProperty(
    CSSPropertyID unresolved_property,
    const String& string,
    SecureContextMode secure_context) {
  MutableCSSPropertyValueSet::SetResult result =
      css_property_value_set_->SetProperty(unresolved_property, string, false,
                                           secure_context);
  if (canvas_formatted_text_style_ &&
      result != MutableCSSPropertyValueSet::kParseError)
    canvas_formatted_text_style_->SetNeedsStyleRecalc();
  return result != MutableCSSPropertyValueSet::kParseError;
}

void CanvasFormattedTextStylePropertyMap::SetCustomProperty(const AtomicString&,
                                                            const CSSValue&) {
  // Custom properties are not supported on CanvasFormattedText
  NOTREACHED();
}

void CanvasFormattedTextStylePropertyMap::RemoveProperty(
    CSSPropertyID property_id) {
  bool did_change = css_property_value_set_->RemoveProperty(property_id);
  if (did_change) {
    canvas_formatted_text_style_->SetNeedsStyleRecalc();
  }
}

void CanvasFormattedTextStylePropertyMap::RemoveCustomProperty(
    const AtomicString&) {
  // Custom properties are not supported on CanvasFormattedText
  NOTREACHED();
}

void CanvasFormattedTextStylePropertyMap::RemoveAllProperties() {
  css_property_value_set_->Clear();
  canvas_formatted_text_style_->SetNeedsStyleRecalc();
}

String CanvasFormattedTextStylePropertyMap::SerializationForShorthand(
    const CSSProperty& property) const {
  DCHECK(property.IsShorthand());
  return StylePropertySerializer(*css_property_value_set_)
      .SerializeShorthand(property.PropertyID());
}

}  // namespace blink
