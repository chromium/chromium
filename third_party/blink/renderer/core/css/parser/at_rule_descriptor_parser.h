// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_AT_RULE_DESCRIPTOR_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_AT_RULE_DESCRIPTOR_PARSER_H_

#include "third_party/blink/renderer/core/css/css_property_value.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptors.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class AtRuleDescriptorParser {
  STATIC_ONLY(AtRuleDescriptorParser);

 public:
  static bool ParseAtRule(AtRuleDescriptorID,
                          CSSParserTokenRange&,
                          const CSSParserContext&,
                          HeapVector<CSSPropertyValue, 256>&);
  static CSSValue* ParseFontFaceDescriptor(AtRuleDescriptorID,
                                           CSSParserTokenRange&,
                                           const CSSParserContext&);
  static CSSValue* ParseFontFaceDescriptor(AtRuleDescriptorID,
                                           const String& value,
                                           const CSSParserContext&);
  static CSSValue* ParseFontFaceDeclaration(CSSParserTokenRange&,
                                            const CSSParserContext&);
  static CSSValue* ParseAtPropertyDescriptor(AtRuleDescriptorID,
                                             CSSParserTokenRange&,
                                             const CSSParserContext&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_AT_RULE_DESCRIPTOR_PARSER_H_
