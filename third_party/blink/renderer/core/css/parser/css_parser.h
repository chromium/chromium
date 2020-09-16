// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_H_

#include <memory>
#include "third_party/blink/public/common/css/color_scheme.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"

namespace blink {

class Color;
class CSSParserObserver;
class CSSSelectorList;
class Element;
class ImmutableCSSPropertyValueSet;
class StyleRuleBase;
class StyleRuleKeyframe;
class StyleSheetContents;
class CSSValue;
class CSSPrimitiveValue;
enum class ParseSheetResult;
enum class SecureContextMode;

// This class serves as the public API for the css/parser subsystem
class CORE_EXPORT CSSParser {
  STATIC_ONLY(CSSParser);

 public:
  // As well as regular rules, allows @import and @namespace but not @charset
  static StyleRuleBase* ParseRule(const CSSParserContext*,
                                  StyleSheetContents*,
                                  const String&);

  static ParseSheetResult ParseSheet(
      const CSSParserContext*,
      StyleSheetContents*,
      const String&,
      CSSDeferPropertyParsing defer_property_parsing =
          CSSDeferPropertyParsing::kNo,
      bool allow_import_rules = true);
  static CSSSelectorList ParseSelector(const CSSParserContext*,
                                       StyleSheetContents*,
                                       const String&);
  static CSSSelectorList ParsePageSelector(const CSSParserContext&,
                                           StyleSheetContents*,
                                           const String&);
  static bool ParseDeclarationList(const CSSParserContext*,
                                   MutableCSSPropertyValueSet*,
                                   const String&);

  static MutableCSSPropertyValueSet::SetResult ParseValue(
      MutableCSSPropertyValueSet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important,
      SecureContextMode);
  static MutableCSSPropertyValueSet::SetResult ParseValue(
      MutableCSSPropertyValueSet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important,
      SecureContextMode,
      StyleSheetContents*);

  static MutableCSSPropertyValueSet::SetResult ParseValueForCustomProperty(
      MutableCSSPropertyValueSet*,
      const AtomicString& property_name,
      const String& value,
      bool important,
      SecureContextMode,
      StyleSheetContents*,
      bool is_animation_tainted);

  // This is for non-shorthands only
  static const CSSValue* ParseSingleValue(CSSPropertyID,
                                          const String&,
                                          const CSSParserContext*);

  static const CSSValue* ParseFontFaceDescriptor(CSSPropertyID,
                                                 const String&,
                                                 const CSSParserContext*);

  static ImmutableCSSPropertyValueSet* ParseInlineStyleDeclaration(
      const String&,
      Element*);
  static ImmutableCSSPropertyValueSet*
  ParseInlineStyleDeclaration(const String&, CSSParserMode, SecureContextMode);

  static std::unique_ptr<Vector<double>> ParseKeyframeKeyList(const String&);
  static StyleRuleKeyframe* ParseKeyframeRule(const CSSParserContext*,
                                              const String&);

  static bool ParseSupportsCondition(const String&, SecureContextMode);

  // The color will only be changed when string contains a valid CSS color, so
  // callers can set it to a default color and ignore the boolean result.
  static bool ParseColor(Color&, const String&, bool strict = false);
  static bool ParseSystemColor(Color&, const String&, ColorScheme color_scheme);

  static void ParseSheetForInspector(const CSSParserContext*,
                                     StyleSheetContents*,
                                     const String&,
                                     CSSParserObserver&);
  static void ParseDeclarationListForInspector(const CSSParserContext*,
                                               const String&,
                                               CSSParserObserver&);

  static CSSPrimitiveValue* ParseLengthPercentage(const String&,
                                                  const CSSParserContext*);

 private:
  static MutableCSSPropertyValueSet::SetResult ParseValue(
      MutableCSSPropertyValueSet*,
      CSSPropertyID unresolved_property,
      const String&,
      bool important,
      const CSSParserContext*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_CSS_PARSER_H_
