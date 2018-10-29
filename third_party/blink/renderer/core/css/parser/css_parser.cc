// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser.h"

#include <memory>
#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"

namespace blink {

using namespace cssvalue;

bool CSSParser::ParseDeclarationList(const CSSParserContext* context,
                                     MutableCSSPropertyValueSet* property_set,
                                     const String& declaration) {
  return CSSParserImpl::ParseDeclarationList(property_set, declaration,
                                             context);
}

void CSSParser::ParseDeclarationListForInspector(
    const CSSParserContext* context,
    const String& declaration,
    CSSParserObserver& observer) {
  CSSParserImpl::ParseDeclarationListForInspector(declaration, context,
                                                  observer);
}

CSSSelectorList CSSParser::ParseSelector(
    const CSSParserContext* context,
    StyleSheetContents* style_sheet_contents,
    const String& selector) {
  CSSTokenizer tokenizer(selector);
  const auto tokens = tokenizer.TokenizeToEOF();
  return CSSSelectorParser::ParseSelector(CSSParserTokenRange(tokens), context,
                                          style_sheet_contents);
}

CSSSelectorList CSSParser::ParsePageSelector(
    const CSSParserContext& context,
    StyleSheetContents* style_sheet_contents,
    const String& selector) {
  CSSTokenizer tokenizer(selector);
  const auto tokens = tokenizer.TokenizeToEOF();
  return CSSParserImpl::ParsePageSelector(CSSParserTokenRange(tokens),
                                          style_sheet_contents);
}

StyleRuleBase* CSSParser::ParseRule(const CSSParserContext* context,
                                    StyleSheetContents* style_sheet,
                                    const String& rule) {
  return CSSParserImpl::ParseRule(rule, context, style_sheet,
                                  CSSParserImpl::kAllowImportRules);
}

ParseSheetResult CSSParser::ParseSheet(
    const CSSParserContext* context,
    StyleSheetContents* style_sheet,
    const String& text,
    CSSDeferPropertyParsing defer_property_parsing,
    bool allow_import_rules) {
  return CSSParserImpl::ParseStyleSheet(
      text, context, style_sheet, defer_property_parsing, allow_import_rules);
}

void CSSParser::ParseSheetForInspector(const CSSParserContext* context,
                                       StyleSheetContents* style_sheet,
                                       const String& text,
                                       CSSParserObserver& observer) {
  return CSSParserImpl::ParseStyleSheetForInspector(text, context, style_sheet,
                                                    observer);
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    SecureContextMode secure_context_mode) {
  return ParseValue(declaration, unresolved_property, string, important,
                    secure_context_mode,
                    static_cast<StyleSheetContents*>(nullptr));
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet) {
  if (string.IsEmpty()) {
    bool did_parse = false;
    bool did_change = false;
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }

  CSSPropertyID resolved_property = resolveCSSPropertyID(unresolved_property);
  CSSParserMode parser_mode = declaration->CssParserMode();
  CSSValue* value = CSSParserFastPaths::MaybeParseValue(resolved_property,
                                                        string, parser_mode);
  if (value) {
    bool did_parse = true;
    bool did_change = declaration->SetProperty(CSSPropertyValue(
        CSSProperty::Get(resolved_property), *value, important));
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }
  CSSParserContext* context;
  if (style_sheet) {
    context = CSSParserContext::Create(style_sheet->ParserContext(), nullptr);
    context->SetMode(parser_mode);
  } else {
    context = CSSParserContext::Create(parser_mode, secure_context_mode);
  }
  return ParseValue(declaration, unresolved_property, string, important,
                    context);
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValueForCustomProperty(
    MutableCSSPropertyValueSet* declaration,
    const AtomicString& property_name,
    const PropertyRegistry* registry,
    const String& value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet,
    bool is_animation_tainted) {
  DCHECK(CSSVariableParser::IsValidVariableName(property_name));
  if (value.IsEmpty()) {
    bool did_parse = false;
    bool did_change = false;
    return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
  }
  CSSParserMode parser_mode = declaration->CssParserMode();
  CSSParserContext* context;
  if (style_sheet) {
    context = CSSParserContext::Create(style_sheet->ParserContext(), nullptr);
    context->SetMode(parser_mode);
  } else {
    context = CSSParserContext::Create(parser_mode, secure_context_mode);
  }
  return CSSParserImpl::ParseVariableValue(declaration, property_name, registry,
                                           value, important, context,
                                           is_animation_tainted);
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  return CSSParserImpl::ParseValue(declaration, unresolved_property, string,
                                   important, context);
}

const CSSValue* CSSParser::ParseSingleValue(CSSPropertyID property_id,
                                            const String& string,
                                            const CSSParserContext* context) {
  if (string.IsEmpty())
    return nullptr;
  if (CSSValue* value = CSSParserFastPaths::MaybeParseValue(property_id, string,
                                                            context->Mode()))
    return value;
  CSSTokenizer tokenizer(string);
  const auto tokens = tokenizer.TokenizeToEOF();
  return CSSPropertyParser::ParseSingleValue(
      property_id, CSSParserTokenRange(tokens), context);
}

ImmutableCSSPropertyValueSet* CSSParser::ParseInlineStyleDeclaration(
    const String& style_string,
    Element* element) {
  return CSSParserImpl::ParseInlineStyleDeclaration(style_string, element);
}

std::unique_ptr<Vector<double>> CSSParser::ParseKeyframeKeyList(
    const String& key_list) {
  return CSSParserImpl::ParseKeyframeKeyList(key_list);
}

StyleRuleKeyframe* CSSParser::ParseKeyframeRule(const CSSParserContext* context,
                                                const String& rule) {
  StyleRuleBase* keyframe = CSSParserImpl::ParseRule(
      rule, context, nullptr, CSSParserImpl::kKeyframeRules);
  return ToStyleRuleKeyframe(keyframe);
}

bool CSSParser::ParseSupportsCondition(const String& condition,
                                       SecureContextMode secure_context_mode) {
  CSSTokenizer tokenizer(condition);
  const auto tokens = tokenizer.TokenizeToEOF();
  CSSParserImpl parser(StrictCSSParserContext(secure_context_mode));
  return CSSSupportsParser::SupportsCondition(
             CSSParserTokenRange(tokens), parser,
             CSSSupportsParser::kForWindowCSS) == CSSSupportsParser::kSupported;
}

bool CSSParser::ParseColor(Color& color, const String& string, bool strict) {
  if (string.IsEmpty())
    return false;

  // The regular color parsers don't resolve named colors, so explicitly
  // handle these first.
  Color named_color;
  if (named_color.SetNamedColor(string)) {
    color = named_color;
    return true;
  }

  const CSSValue* value = CSSParserFastPaths::ParseColor(
      string, strict ? kHTMLStandardMode : kHTMLQuirksMode);
  // TODO(timloh): Why is this always strict mode?
  if (!value) {
    // NOTE(ikilpatrick): We will always parse color value in the insecure
    // context mode. If a function/unit/etc will require a secure context check
    // in the future, plumbing will need to be added.
    value = ParseSingleValue(
        CSSPropertyColor, string,
        StrictCSSParserContext(SecureContextMode::kInsecureContext));
  }

  if (!value || !value->IsColorValue())
    return false;
  color = ToCSSColorValue(*value).Value();
  return true;
}

bool CSSParser::ParseSystemColor(Color& color, const String& color_string) {
  CSSValueID id = CssValueKeywordID(color_string);
  if (!StyleColor::IsSystemColor(id))
    return false;

  color = LayoutTheme::GetTheme().SystemColor(id);
  return true;
}

const CSSValue* CSSParser::ParseFontFaceDescriptor(
    CSSPropertyID property_id,
    const String& property_value,
    const CSSParserContext* context) {
  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kCSSFontFaceRuleMode);
  CSSParser::ParseValue(style, property_id, property_value, true, context);
  const CSSValue* value = style->GetPropertyCSSValue(property_id);

  return value;
}

}  // namespace blink
