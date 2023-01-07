// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/formatted_text/formatted_text_style.h"

#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"

namespace blink {

void FormattedTextStyle::SetStyle(const CSSParserContext* context,
                                  const String& style_text) {
  if (css_property_value_set_) {
    css_property_value_set_->Clear();
  } else {
    css_property_value_set_ =
        MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  }

  CSSParser::ParseDeclarationList(context, css_property_value_set_, style_text);
}

const CSSPropertyValueSet* FormattedTextStyle::GetCssPropertySet() const {
  return css_property_value_set_;
}

void FormattedTextStyle::Trace(Visitor* visitor) const {
  visitor->Trace(css_property_value_set_);
}

}  // namespace blink
