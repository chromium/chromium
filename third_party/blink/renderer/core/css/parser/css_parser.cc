// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser.h"

#include <memory>

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/style_color.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

bool CSSParser::ParseDeclarationList(const CSSParserContext* context,
                                     MutableCSSPropertyValueSet* property_set,
                                     const String& declaration) {
  return CSSParserImpl::ParseDeclarationList(property_set, declaration,
                                             context);
}

StyleRuleBase* CSSParser::ParseNestedDeclarationsRule(
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    StringView text) {
  return CSSParserImpl::ParseNestedDeclarationsRule(
      context, nesting_type, parent_rule_for_nesting, text);
}

void CSSParser::ParseDeclarationListForInspector(
    const CSSParserContext* context,
    const String& declaration,
    CSSParserObserver& observer) {
  CSSParserImpl::ParseDeclarationListForInspector(declaration, context,
                                                  observer);
}

base::span<CSSSelector> CSSParser::ParseSelector(
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    bool is_within_scope,
    StyleSheetContents* style_sheet_contents,
    const String& selector,
    HeapVector<CSSSelector>& arena) {
  CSSParserTokenStream stream(selector);
  return CSSSelectorParser::ParseSelector(
      stream, context, nesting_type, parent_rule_for_nesting, is_within_scope,
      /* semicolon_aborts_nested_selector */ false, style_sheet_contents,
      arena);
}

CSSSelectorList* CSSParser::ParsePageSelector(
    const CSSParserContext& context,
    StyleSheetContents* style_sheet_contents,
    const String& selector) {
  CSSParserTokenStream stream(selector);
  CSSSelectorList* selector_list =
      CSSParserImpl::ParsePageSelector(stream, style_sheet_contents, context);
  if (!stream.AtEnd()) {
    // Extra tokens at end of selector.
    return nullptr;
  }
  return selector_list;
}

StyleRuleBase* CSSParser::ParseMarginRule(const CSSParserContext* context,
                                          StyleSheetContents* style_sheet,
                                          const String& rule) {
  return CSSParserImpl::ParseRule(rule, context, CSSNestingType::kNone,
                                  /*parent_rule_for_nesting=*/nullptr,
                                  style_sheet, CSSParserImpl::kPageMarginRules);
}

StyleRuleBase* CSSParser::ParseRule(const CSSParserContext* context,
                                    StyleSheetContents* style_sheet,
                                    CSSNestingType nesting_type,
                                    StyleRule* parent_rule_for_nesting,
                                    const String& rule) {
  return CSSParserImpl::ParseRule(rule, context, nesting_type,
                                  parent_rule_for_nesting, style_sheet,
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
    StringView string,
    bool important,
    const ExecutionContext* execution_context) {
  return ParseValue(
      declaration, unresolved_property, string, important,
      execution_context ? execution_context->GetSecureContextMode()
                        : SecureContextMode::kInsecureContext,
      static_cast<StyleSheetContents*>(nullptr), execution_context);
}

static inline const CSSParserContext* GetParserContext(
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet,
    const ExecutionContext* execution_context,
    CSSParserMode parser_mode) {
  if (style_sheet) {
    if (style_sheet->ParserContext()->GetMode() == parser_mode) {
      // We can reuse this, to save on the construction.
      return style_sheet->ParserContext();
    } else {
      // This can happen when parsing e.g. SVG attributes in the context of
      // an HTML document.
      CSSParserContext* mutable_context =
          MakeGarbageCollected<CSSParserContext>(style_sheet->ParserContext());
      mutable_context->SetMode(parser_mode);
      return mutable_context;
    }
  } else if (IsA<LocalDOMWindow>(execution_context)) {
    // Create parser context using document if it exists so it can check for
    // origin trial enabled property/value.
    CSSParserContext* mutable_context = MakeGarbageCollected<CSSParserContext>(
        *To<LocalDOMWindow>(execution_context)->document());
    mutable_context->SetMode(parser_mode);
    return mutable_context;
  } else {
    return MakeGarbageCollected<CSSParserContext>(parser_mode,
                                                  secure_context_mode);
  }
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    StringView string,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet,
    const ExecutionContext* execution_context) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  if (string.empty()) {
    return MutableCSSPropertyValueSet::kParseError;
  }

  CSSPropertyID resolved_property = ResolveCSSPropertyID(unresolved_property);
  CSSParserMode parser_mode = declaration->CssParserMode();
  const CSSParserContext* context = GetParserContext(
      secure_context_mode, style_sheet, execution_context, parser_mode);

  // See if this property has a specific fast-path parser.
  const CSSValue* value =
      CSSParserFastPaths::MaybeParseValue(resolved_property, string, context);
  if (value) {
    return declaration->SetLonghandProperty(CSSPropertyValue(
        CSSPropertyName(resolved_property), *value, important));
  }

  // OK, that didn't work (either the property doesn't have a fast path,
  // or the string is on some form that the fast-path parser doesn't support,
  // e.g. a parse error). See if the value we are looking for is a longhand;
  // if so, we can use a faster parsing function. In particular, we don't need
  // to set up a vector for the results, since there will be only one.
  //
  // We only allow this path in standards mode, which rules out situations
  // like @font-face parsing etc. (which have their own rules).
  const CSSProperty& property = CSSProperty::Get(resolved_property);
  if (parser_mode == kHTMLStandardMode && property.IsProperty() &&
      !property.IsShorthand()) {
    CSSParserTokenStream stream(string);
    value =
        CSSPropertyParser::ParseSingleValue(resolved_property, stream, context);
    if (value != nullptr) {
      return declaration->SetLonghandProperty(CSSPropertyValue(
          CSSPropertyName(resolved_property), *value, important));
    }
  }

  // OK, that didn't work either, so we'll need the full-blown parser.
  return ParseValue(declaration, unresolved_property, string, important,
                    context);
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValueForCustomProperty(
    MutableCSSPropertyValueSet* declaration,
    const AtomicString& property_name,
    StringView value,
    bool important,
    SecureContextMode secure_context_mode,
    StyleSheetContents* style_sheet,
    bool is_animation_tainted) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  DCHECK(CSSVariableParser::IsValidVariableName(property_name));
  if (value.empty()) {
    return MutableCSSPropertyValueSet::kParseError;
  }
  CSSParserMode parser_mode = declaration->CssParserMode();
  CSSParserContext* context;
  if (style_sheet) {
    context =
        MakeGarbageCollected<CSSParserContext>(style_sheet->ParserContext());
    context->SetMode(parser_mode);
  } else {
    context = MakeGarbageCollected<CSSParserContext>(parser_mode,
                                                     secure_context_mode);
  }
  return CSSParserImpl::ParseVariableValue(declaration, property_name, value,
                                           important, context,
                                           is_animation_tainted);
}

MutableCSSPropertyValueSet::SetResult CSSParser::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    StringView string,
    bool important,
    const CSSParserContext* context) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  return CSSParserImpl::ParseValue(declaration, unresolved_property, string,
                                   important, context);
}

const CSSValue* CSSParser::ParseSingleValue(CSSPropertyID property_id,
                                            const String& string,
                                            const CSSParserContext* context) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  if (string.empty()) {
    return nullptr;
  }
  if (CSSValue* value =
          CSSParserFastPaths::MaybeParseValue(property_id, string, context)) {
    return value;
  }
  CSSParserTokenStream stream(string);
  return CSSPropertyParser::ParseSingleValue(property_id, stream, context);
}

ImmutableCSSPropertyValueSet* CSSParser::ParseInlineStyleDeclaration(
    const String& style_string,
    Element* element) {
  return CSSParserImpl::ParseInlineStyleDeclaration(style_string, element);
}

ImmutableCSSPropertyValueSet* CSSParser::ParseInlineStyleDeclaration(
    const String& style_string,
    CSSParserMode parser_mode,
    SecureContextMode secure_context_mode,
    const Document* document) {
  return CSSParserImpl::ParseInlineStyleDeclaration(
      style_string, parser_mode, secure_context_mode, document);
}

std::unique_ptr<Vector<KeyframeOffset>> CSSParser::ParseKeyframeKeyList(
    const CSSParserContext* context,
    const String& key_list) {
  return CSSParserImpl::ParseKeyframeKeyList(context, key_list);
}

StyleRuleKeyframe* CSSParser::ParseKeyframeRule(const CSSParserContext* context,
                                                const String& rule) {
  StyleRuleBase* keyframe = CSSParserImpl::ParseRule(
      rule, context, CSSNestingType::kNone, /*parent_rule_for_nesting=*/nullptr,
      nullptr, CSSParserImpl::kKeyframeRules);
  return To<StyleRuleKeyframe>(keyframe);
}

String CSSParser::ParseCustomPropertyName(const String& name_text) {
  return CSSParserImpl::ParseCustomPropertyName(name_text);
}

bool CSSParser::ParseSupportsCondition(
    const String& condition,
    const ExecutionContext* execution_context) {
  // window.CSS.supports requires to parse as-if it was wrapped in parenthesis.
  String wrapped_condition = "(" + condition + ")";
  CSSParserTokenStream stream(wrapped_condition);
  DCHECK(execution_context);
  // Create parser context using document so it can check for origin trial
  // enabled property/value.
  CSSParserContext* context = MakeGarbageCollected<CSSParserContext>(
      *To<LocalDOMWindow>(execution_context)->document());
  // Override the parser mode interpreted from the document as the spec
  // https://quirks.spec.whatwg.org/#css requires quirky values and colors
  // must not be supported in CSS.supports() method.
  context->SetMode(kHTMLStandardMode);
  CSSParserImpl parser(context);
  CSSSupportsParser::Result result =
      CSSSupportsParser::ConsumeSupportsCondition(stream, parser);
  if (!stream.AtEnd()) {
    result = CSSSupportsParser::Result::kParseFailure;
  }

  return result == CSSSupportsParser::Result::kSupported;
}

bool CSSParser::ParseColor(Color& color, const String& string, bool strict) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  if (string.empty()) {
    return false;
  }

  // The regular color parsers don't resolve named colors, so explicitly
  // handle these first.
  Color named_color;
  if (named_color.SetNamedColor(string)) {
    color = named_color;
    return true;
  }

  switch (CSSParserFastPaths::ParseColor(
      string, strict ? kHTMLStandardMode : kHTMLQuirksMode, color)) {
    case ParseColorResult::kFailure:
      break;
    case ParseColorResult::kKeyword:
      return false;
    case ParseColorResult::kColor:
      return true;
  }

  // TODO(timloh): Why is this always strict mode?
  // NOTE(ikilpatrick): We will always parse color value in the insecure
  // context mode. If a function/unit/etc will require a secure context check
  // in the future, plumbing will need to be added.
  const CSSValue* value = ParseSingleValue(
      CSSPropertyID::kColor, string,
      StrictCSSParserContext(SecureContextMode::kInsecureContext));
  auto* color_value = DynamicTo<cssvalue::CSSColor>(value);
  if (!color_value) {
    return false;
  }

  color = color_value->Value();
  return true;
}

bool CSSParser::ParseSystemColor(Color& color,
                                 const String& color_string,
                                 mojom::blink::ColorScheme color_scheme,
                                 const ui::ColorProvider* color_provider,
                                 bool is_in_web_app_scope) {
  CSSValueID id = CssValueKeywordID(color_string);
  if (!StyleColor::IsSystemColorIncludingDeprecated(id)) {
    return false;
  }

  color = LayoutTheme::GetTheme().SystemColor(id, color_scheme, color_provider,
                                              is_in_web_app_scope);
  return true;
}

const CSSValue* CSSParser::ParseFontFaceDescriptor(
    CSSPropertyID property_id,
    const String& property_value,
    const CSSParserContext* context) {
  auto* style =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kCSSFontFaceRuleMode);
  CSSParser::ParseValue(style, property_id, property_value, true, context);
  const CSSValue* value = style->GetPropertyCSSValue(property_id);

  return value;
}

CSSPrimitiveValue* CSSParser::ParseLengthPercentage(
    const String& string,
    const CSSParserContext* context,
    CSSPrimitiveValue::ValueRange value_range) {
  if (string.empty() || !context) {
    return nullptr;
  }
  CSSParserTokenStream stream(string);
  // Trim whitespace from the string. It's only necessary to consume leading
  // whitespaces, since ConsumeLengthOrPercent always consumes trailing ones.
  stream.ConsumeWhitespace();
  CSSPrimitiveValue* parsed_value =
      css_parsing_utils::ConsumeLengthOrPercent(stream, *context, value_range);
  return stream.AtEnd() ? parsed_value : nullptr;
}

MutableCSSPropertyValueSet* CSSParser::ParseFont(
    const String& string,
    const ExecutionContext* execution_context) {
  DCHECK(ThreadState::Current()->IsAllocationAllowed());
  auto* set =
      MakeGarbageCollected<MutableCSSPropertyValueSet>(kHTMLStandardMode);
  ParseValue(set, CSSPropertyID::kFont, string, true /* important */,
             execution_context);
  if (set->IsEmpty()) {
    return nullptr;
  }
  const CSSValue* font_size =
      set->GetPropertyCSSValue(CSSPropertyID::kFontSize);
  if (!font_size || font_size->IsCSSWideKeyword()) {
    return nullptr;
  }
  if (font_size->IsPendingSubstitutionValue()) {
    return nullptr;
  }
  return set;
}

}  // namespace blink
