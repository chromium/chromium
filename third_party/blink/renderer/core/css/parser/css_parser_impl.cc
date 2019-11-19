// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include <bitset>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_selector.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

namespace {

// This may still consume tokens if it fails
AtomicString ConsumeStringOrURI(CSSParserTokenStream& stream) {
  const CSSParserToken& token = stream.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken)
    return stream.ConsumeIncludingWhitespace().Value().ToAtomicString();

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url"))
    return AtomicString();

  CSSParserTokenStream::BlockGuard guard(stream);
  const CSSParserToken& uri = stream.ConsumeIncludingWhitespace();
  if (uri.GetType() == kBadStringToken || !stream.UncheckedAtEnd())
    return AtomicString();
  DCHECK_EQ(uri.GetType(), kStringToken);
  return uri.Value().ToAtomicString();
}

}  // namespace

CSSParserImpl::CSSParserImpl(const CSSParserContext* context,
                             StyleSheetContents* style_sheet)
    : context_(context), style_sheet_(style_sheet), observer_(nullptr) {}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    const String& string,
    bool important,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = StyleRule::kStyle;
  if (declaration->CssParserMode() == kCSSViewportRuleMode)
    rule_type = StyleRule::kViewport;
  else if (declaration->CssParserMode() == kCSSFontFaceRuleMode)
    rule_type = StyleRule::kFontFace;
  CSSTokenizer tokenizer(string);
  // TODO(crbug.com/661854): Use streams instead of ranges
  parser.ConsumeDeclarationValue(CSSParserTokenRange(tokenizer.TokenizeToEOF()),
                                 unresolved_property, important, rule_type);
  bool did_parse = false;
  bool did_change = false;
  if (!parser.parsed_properties_.IsEmpty()) {
    did_parse = true;
    did_change = declaration->AddParsedProperties(parser.parsed_properties_);
  }
  return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseVariableValue(
    MutableCSSPropertyValueSet* declaration,
    const AtomicString& property_name,
    const String& value,
    bool important,
    const CSSParserContext* context,
    bool is_animation_tainted) {
  CSSParserImpl parser(context);
  CSSTokenizer tokenizer(value);
  // TODO(crbug.com/661854): Use streams instead of ranges
  const auto tokens = tokenizer.TokenizeToEOF();
  const CSSParserTokenRange range(tokens);
  parser.ConsumeVariableValue(range, property_name, important,
                              is_animation_tainted);
  bool did_parse = false;
  bool did_change = false;
  if (!parser.parsed_properties_.IsEmpty()) {
    did_parse = true;
    did_change = declaration->AddParsedProperties(parser.parsed_properties_);
  }
  return MutableCSSPropertyValueSet::SetResult{did_parse, did_change};
}

static inline void FilterProperties(
    bool important,
    const HeapVector<CSSPropertyValue, 256>& input,
    HeapVector<CSSPropertyValue, 256>& output,
    wtf_size_t& unused_entries,
    std::bitset<numCSSProperties>& seen_properties,
    HashSet<AtomicString>& seen_custom_properties) {
  // Add properties in reverse order so that highest priority definitions are
  // reached first. Duplicate definitions can then be ignored when found.
  for (wtf_size_t i = input.size(); i--;) {
    const CSSPropertyValue& property = input[i];
    if (property.IsImportant() != important)
      continue;
    if (property.Id() == CSSPropertyID::kVariable) {
      const AtomicString& name =
          To<CSSCustomPropertyDeclaration>(property.Value())->GetName();
      if (seen_custom_properties.Contains(name))
        continue;
      seen_custom_properties.insert(name);
    } else {
      const unsigned property_id_index = GetCSSPropertyIDIndex(property.Id());
      if (seen_properties.test(property_id_index))
        continue;
      seen_properties.set(property_id_index);
    }
    output[--unused_entries] = property;
  }
}

static ImmutableCSSPropertyValueSet* CreateCSSPropertyValueSet(
    HeapVector<CSSPropertyValue, 256>& parsed_properties,
    CSSParserMode mode) {
  std::bitset<numCSSProperties> seen_properties;
  wtf_size_t unused_entries = parsed_properties.size();
  HeapVector<CSSPropertyValue, 256> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;

  FilterProperties(true, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);

  ImmutableCSSPropertyValueSet* result = ImmutableCSSPropertyValueSet::Create(
      results.data() + unused_entries, results.size() - unused_entries, mode);
  parsed_properties.clear();
  return result;
}

ImmutableCSSPropertyValueSet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    Element* element) {
  Document& document = element->GetDocument();
  auto* context = MakeGarbageCollected<CSSParserContext>(
      document.ElementSheet().Contents()->ParserContext(), &document);
  CSSParserMode mode = element->IsHTMLElement() && !document.InQuirksMode()
                           ? kHTMLStandardMode
                           : kHTMLQuirksMode;
  context->SetMode(mode);
  CSSParserImpl parser(context, document.ElementSheet().Contents());
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, mode);
}

ImmutableCSSPropertyValueSet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    CSSParserMode parser_mode,
    SecureContextMode secure_context_mode) {
  auto* context =
      MakeGarbageCollected<CSSParserContext>(parser_mode, secure_context_mode);
  CSSParserImpl parser(context);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, parser_mode);
}

bool CSSParserImpl::ParseDeclarationList(
    MutableCSSPropertyValueSet* declaration,
    const String& string,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = StyleRule::kStyle;
  if (declaration->CssParserMode() == kCSSViewportRuleMode)
    rule_type = StyleRule::kViewport;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, rule_type);
  if (parser.parsed_properties_.IsEmpty())
    return false;

  std::bitset<numCSSProperties> seen_properties;
  wtf_size_t unused_entries = parser.parsed_properties_.size();
  HeapVector<CSSPropertyValue, 256> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;
  FilterProperties(true, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  if (unused_entries)
    results.EraseAt(0, unused_entries);
  return declaration->AddParsedProperties(results);
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        StyleSheetContents* style_sheet,
                                        AllowedRulesType allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  stream.ConsumeWhitespace();
  if (stream.UncheckedAtEnd())
    return nullptr;  // Parse error, empty rule
  StyleRuleBase* rule;
  if (stream.UncheckedPeek().GetType() == kAtKeywordToken)
    rule = parser.ConsumeAtRule(stream, allowed_rules);
  else
    rule = parser.ConsumeQualifiedRule(stream, allowed_rules);
  if (!rule)
    return nullptr;  // Parse error, failed to consume rule
  stream.ConsumeWhitespace();
  if (!rule || !stream.UncheckedAtEnd())
    return nullptr;  // Parse error, trailing garbage
  return rule;
}

ParseSheetResult CSSParserImpl::ParseStyleSheet(
    const String& string,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet,
    CSSDeferPropertyParsing defer_property_parsing,
    bool allow_import_rules) {
  TRACE_EVENT_BEGIN2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                     "baseUrl", context->BaseURL().GetString().Utf8(), "mode",
                     context->Mode());

  TRACE_EVENT_BEGIN0("blink,blink_style",
                     "CSSParserImpl::parseStyleSheet.parse");
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  CSSParserImpl parser(context, style_sheet);
  if (defer_property_parsing == CSSDeferPropertyParsing::kYes) {
    parser.lazy_state_ = MakeGarbageCollected<CSSLazyParsingState>(
        context, string, parser.style_sheet_);
  }
  ParseSheetResult result = ParseSheetResult::kSucceeded;
  bool first_rule_valid = parser.ConsumeRuleList(
      stream, kTopLevelRuleList,
      [&style_sheet, &result, allow_import_rules](StyleRuleBase* rule) {
        if (rule->IsCharsetRule())
          return;
        if (rule->IsImportRule() && !allow_import_rules) {
          result = ParseSheetResult::kHasUnallowedImportRule;
          return;
        }
        style_sheet->ParserAppendRule(rule);
      });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
  TRACE_EVENT_END0("blink,blink_style", "CSSParserImpl::parseStyleSheet.parse");

  TRACE_EVENT_END2("blink,blink_style", "CSSParserImpl::parseStyleSheet",
                   "tokenCount", tokenizer.TokenCount(), "length",
                   string.length());
  return result;
}

CSSSelectorList CSSParserImpl::ParsePageSelector(
    CSSParserTokenRange range,
    StyleSheetContents* style_sheet) {
  // We only support a small subset of the css-page spec.
  range.ConsumeWhitespace();
  AtomicString type_selector;
  if (range.Peek().GetType() == kIdentToken)
    type_selector = range.Consume().Value().ToAtomicString();

  AtomicString pseudo;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    if (range.Peek().GetType() != kIdentToken)
      return CSSSelectorList();
    pseudo = range.Consume().Value().ToAtomicString();
  }

  range.ConsumeWhitespace();
  if (!range.AtEnd())
    return CSSSelectorList();  // Parse error; extra tokens in @page selector

  std::unique_ptr<CSSParserSelector> selector;
  if (!type_selector.IsNull() && pseudo.IsNull()) {
    selector = std::make_unique<CSSParserSelector>(
        QualifiedName(g_null_atom, type_selector, g_star_atom));
  } else {
    selector = std::make_unique<CSSParserSelector>();
    if (!pseudo.IsNull()) {
      selector->SetMatch(CSSSelector::kPagePseudoClass);
      selector->UpdatePseudoPage(pseudo.LowerASCII());
      if (selector->GetPseudoType() == CSSSelector::kPseudoUnknown)
        return CSSSelectorList();
    }
    if (!type_selector.IsNull()) {
      selector->PrependTagSelector(
          QualifiedName(g_null_atom, type_selector, g_star_atom));
    }
  }

  selector->SetForPage();
  Vector<std::unique_ptr<CSSParserSelector>> selector_vector;
  selector_vector.push_back(std::move(selector));
  CSSSelectorList selector_list =
      CSSSelectorList::AdoptSelectorVector(selector_vector);
  return selector_list;
}

std::unique_ptr<Vector<double>> CSSParserImpl::ParseKeyframeKeyList(
    const String& key_list) {
  CSSTokenizer tokenizer(key_list);
  // TODO(crbug.com/661854): Use streams instead of ranges
  return ConsumeKeyframeKeyList(CSSParserTokenRange(tokenizer.TokenizeToEOF()));
}

bool CSSParserImpl::SupportsDeclaration(CSSParserTokenRange& range) {
  DCHECK(parsed_properties_.IsEmpty());
  // Even though we might use an observer here, this is just to test if we
  // successfully parse the range, so we can pass RangeOffset::Ignore() here
  // and temporarily remove the observer.
  CSSParserObserver* observer_copy = observer_;
  observer_ = nullptr;
  ConsumeDeclaration(range, RangeOffset::Ignore(), StyleRule::kStyle);
  observer_ = observer_copy;

  bool result = !parsed_properties_.IsEmpty();
  parsed_properties_.clear();
  return result;
}

void CSSParserImpl::ParseDeclarationListForInspector(
    const String& declaration,
    const CSSParserContext* context,
    CSSParserObserver& observer) {
  CSSParserImpl parser(context);
  parser.observer_ = &observer;
  CSSTokenizer tokenizer(declaration);
  observer.StartRuleHeader(StyleRule::kStyle, 0);
  observer.EndRuleHeader(1);
  CSSParserTokenStream stream(tokenizer);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
}

void CSSParserImpl::ParseStyleSheetForInspector(const String& string,
                                                const CSSParserContext* context,
                                                StyleSheetContents* style_sheet,
                                                CSSParserObserver& observer) {
  CSSParserImpl parser(context, style_sheet);
  parser.observer_ = &observer;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  bool first_rule_valid = parser.ConsumeRuleList(
      stream, kTopLevelRuleList, [&style_sheet](StyleRuleBase* rule) {
        if (rule->IsCharsetRule())
          return;
        style_sheet->ParserAppendRule(rule);
      });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
}

CSSPropertyValueSet* CSSParserImpl::ParseDeclarationListForLazyStyle(
    const String& string,
    wtf_size_t offset,
    const CSSParserContext* context) {
  CSSTokenizer tokenizer(string, offset);
  CSSParserTokenStream stream(tokenizer);
  CSSParserTokenStream::BlockGuard guard(stream);
  CSSParserImpl parser(context);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, context->Mode());
}

static CSSParserImpl::AllowedRulesType ComputeNewAllowedRules(
    CSSParserImpl::AllowedRulesType allowed_rules,
    StyleRuleBase* rule) {
  if (!rule || allowed_rules == CSSParserImpl::kKeyframeRules ||
      allowed_rules == CSSParserImpl::kFontFeatureRules ||
      allowed_rules == CSSParserImpl::kNoRules)
    return allowed_rules;
  DCHECK_LE(allowed_rules, CSSParserImpl::kRegularRules);
  if (rule->IsCharsetRule() || rule->IsImportRule())
    return CSSParserImpl::kAllowImportRules;
  if (rule->IsNamespaceRule())
    return CSSParserImpl::kAllowNamespaceRules;
  return CSSParserImpl::kRegularRules;
}

template <typename T>
bool CSSParserImpl::ConsumeRuleList(CSSParserTokenStream& stream,
                                    RuleListType rule_list_type,
                                    const T callback) {
  AllowedRulesType allowed_rules = kRegularRules;
  switch (rule_list_type) {
    case kTopLevelRuleList:
      allowed_rules = kAllowCharsetRules;
      break;
    case kRegularRuleList:
      allowed_rules = kRegularRules;
      break;
    case kKeyframesRuleList:
      allowed_rules = kKeyframeRules;
      break;
    case kFontFeatureRuleList:
      allowed_rules = kFontFeatureRules;
      break;
    default:
      NOTREACHED();
  }

  bool seen_rule = false;
  bool first_rule_valid = false;
  while (!stream.AtEnd()) {
    StyleRuleBase* rule;
    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
        stream.UncheckedConsume();
        continue;
      case kAtKeywordToken:
        rule = ConsumeAtRule(stream, allowed_rules);
        break;
      case kCDOToken:
      case kCDCToken:
        if (rule_list_type == kTopLevelRuleList) {
          stream.UncheckedConsume();
          continue;
        }
        FALLTHROUGH;
      default:
        rule = ConsumeQualifiedRule(stream, allowed_rules);
        break;
    }
    if (!seen_rule) {
      seen_rule = true;
      first_rule_valid = rule;
    }
    if (rule) {
      allowed_rules = ComputeNewAllowedRules(allowed_rules, rule);
      callback(rule);
    }
  }

  return first_rule_valid;
}

StyleRuleBase* CSSParserImpl::ConsumeAtRule(CSSParserTokenStream& stream,
                                            AllowedRulesType allowed_rules) {
  DCHECK_EQ(stream.Peek().GetType(), kAtKeywordToken);
  const StringView name = stream.ConsumeIncludingWhitespace().Value();
  const CSSAtRuleID id = CssAtRuleID(name);

  // @import rules have a URI component that is not technically part of the
  // prelude.
  AtomicString import_prelude_uri;
  if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport)
    import_prelude_uri = ConsumeStringOrURI(stream);

  stream.EnsureLookAhead();
  const wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserTokenRange prelude =
      stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
  const RangeOffset prelude_offset(prelude_offset_start,
                                   stream.LookAheadOffset());

  if (id != kCSSAtRuleInvalid && context_->IsUseCounterRecordingEnabled())
    CountAtRule(context_, id);

  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!stream.UncheckedAtEnd())
      stream.UncheckedConsume();  // kSemicolonToken

    if (allowed_rules == kAllowCharsetRules && id == kCSSAtRuleCharset)
      return ConsumeCharsetRule(prelude);
    if (allowed_rules <= kAllowImportRules && id == kCSSAtRuleImport) {
      return ConsumeImportRule(std::move(import_prelude_uri), prelude,
                               prelude_offset);
    }
    if (allowed_rules <= kAllowNamespaceRules && id == kCSSAtRuleNamespace)
      return ConsumeNamespaceRule(prelude);
    return nullptr;  // Parse error, unrecognised at-rule without block
  }

  CSSParserTokenStream::BlockGuard guard(stream);

  if (allowed_rules == kKeyframeRules)
    return nullptr;  // Parse error, no at-rules supported inside @keyframes
  // Parse error, no at-rules currently supported inside @font-feature-values
  if (allowed_rules == kFontFeatureRules)
    return nullptr;
  if (allowed_rules == kNoRules)
    return nullptr;  // Parse error, no at-rules with blocks supported inside
                     // declaration lists

  DCHECK_LE(allowed_rules, kRegularRules);

  switch (id) {
    case kCSSAtRuleMedia:
      return ConsumeMediaRule(prelude, prelude_offset, stream);
    case kCSSAtRuleSupports:
      return ConsumeSupportsRule(prelude, prelude_offset, stream);
    case kCSSAtRuleViewport:
      return ConsumeViewportRule(prelude, prelude_offset, stream);
    case kCSSAtRuleFontFace:
      return ConsumeFontFaceRule(prelude, prelude_offset, stream);
    case kCSSAtRuleWebkitKeyframes:
      return ConsumeKeyframesRule(true, prelude, prelude_offset, stream);
    case kCSSAtRuleKeyframes:
      return ConsumeKeyframesRule(false, prelude, prelude_offset, stream);
    case kCSSAtRulePage:
      return ConsumePageRule(prelude, prelude_offset, stream);
    case kCSSAtRuleProperty:
      return ConsumePropertyRule(prelude, prelude_offset, stream);
    default:
      return nullptr;  // Parse error, unrecognised at-rule with block
  }
}

StyleRuleBase* CSSParserImpl::ConsumeQualifiedRule(
    CSSParserTokenStream& stream,
    AllowedRulesType allowed_rules) {
  if (allowed_rules <= kRegularRules) {
    return ConsumeStyleRule(stream);
  }

  if (allowed_rules == kKeyframeRules) {
    stream.EnsureLookAhead();
    const wtf_size_t prelude_offset_start = stream.LookAheadOffset();
    const CSSParserTokenRange prelude =
        stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
    const RangeOffset prelude_offset(prelude_offset_start,
                                     stream.LookAheadOffset());

    if (stream.AtEnd())
      return nullptr;  // Parse error, EOF instead of qualified rule block

    CSSParserTokenStream::BlockGuard guard(stream);
    StyleRuleKeyframe* keyframe_style_rule =
        ConsumeKeyframeStyleRule(prelude, prelude_offset, stream);
    if (keyframe_style_rule)
      context_->ReportLayoutAnimationsViolationIfNeeded(*keyframe_style_rule);
    return keyframe_style_rule;
  }
  if (allowed_rules == kFontFeatureRules) {
    stream.ConsumeWhitespace();
    if (stream.AtEnd())
      return nullptr;  // Parse error, EOF instead of qualified rule block
    bool prelude_invalid = false;
    stream.EnsureLookAhead();
    if (stream.UncheckedPeek().GetType() != kLeftBraceToken) {
      prelude_invalid = true;
      while (!stream.AtEnd() &&
             stream.UncheckedPeek().GetType() != kLeftBraceToken)
        stream.UncheckedConsumeComponentValue();
      if (stream.AtEnd())
        return nullptr;
    }

    CSSParserTokenStream::BlockGuard guard(stream);
    if (prelude_invalid)
      return nullptr;
    ConsumeDeclarationList(stream, StyleRule::kFontFace);
    return MakeGarbageCollected<StyleRuleFontFace>(
        CreateCSSPropertyValueSet(parsed_properties_, kCSSFontFaceRuleMode));
  }

  NOTREACHED();
  return nullptr;
}

// This may still consume tokens if it fails
static AtomicString ConsumeStringOrURI(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken)
    return range.ConsumeIncludingWhitespace().Value().ToAtomicString();

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url"))
    return AtomicString();

  CSSParserTokenRange contents = range.ConsumeBlock();
  const CSSParserToken& uri = contents.ConsumeIncludingWhitespace();
  if (uri.GetType() == kBadStringToken || !contents.AtEnd())
    return AtomicString();
  DCHECK_EQ(uri.GetType(), kStringToken);
  return uri.Value().ToAtomicString();
}

StyleRuleCharset* CSSParserImpl::ConsumeCharsetRule(
    CSSParserTokenRange prelude) {
  const CSSParserToken& string = prelude.ConsumeIncludingWhitespace();
  if (string.GetType() != kStringToken || !prelude.AtEnd())
    return nullptr;  // Parse error, expected a single string
  return MakeGarbageCollected<StyleRuleCharset>();
}

StyleRuleImport* CSSParserImpl::ConsumeImportRule(
    AtomicString uri,
    CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset) {
  if (uri.IsNull())
    return nullptr;  // Parse error, expected string or URI

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kImport, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(prelude_offset.end);
    observer_->EndRuleBody(prelude_offset.end);
  }

  return MakeGarbageCollected<StyleRuleImport>(
      uri, MediaQueryParser::ParseMediaQuerySet(prelude),
      context_->IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse);
}

StyleRuleNamespace* CSSParserImpl::ConsumeNamespaceRule(
    CSSParserTokenRange prelude) {
  AtomicString namespace_prefix;
  if (prelude.Peek().GetType() == kIdentToken)
    namespace_prefix =
        prelude.ConsumeIncludingWhitespace().Value().ToAtomicString();

  AtomicString uri(ConsumeStringOrURI(prelude));
  if (uri.IsNull() || !prelude.AtEnd())
    return nullptr;  // Parse error, expected string or URI

  return MakeGarbageCollected<StyleRuleNamespace>(namespace_prefix, uri);
}

StyleRuleMedia* CSSParserImpl::ConsumeMediaRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  HeapVector<Member<StyleRuleBase>> rules;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kMedia, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(block.Offset());
  }

  if (style_sheet_)
    style_sheet_->SetHasMediaQueries();

  const auto media = MediaQueryParser::ParseMediaQuerySet(prelude);

  ConsumeRuleList(block, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(block.Offset());

  return MakeGarbageCollected<StyleRuleMedia>(media, rules);
}

StyleRuleSupports* CSSParserImpl::ConsumeSupportsRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  CSSSupportsParser::SupportsResult supported =
      CSSSupportsParser::SupportsCondition(prelude, *this,
                                           CSSSupportsParser::kForAtRule);
  if (supported == CSSSupportsParser::kInvalid)
    return nullptr;  // Parse error, invalid @supports condition

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kSupports, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(block.Offset());
  }

  const auto prelude_serialized = prelude.Serialize().StripWhiteSpace();

  HeapVector<Member<StyleRuleBase>> rules;
  ConsumeRuleList(block, kRegularRuleList,
                  [&rules](StyleRuleBase* rule) { rules.push_back(rule); });

  if (observer_)
    observer_->EndRuleBody(block.Offset());

  return MakeGarbageCollected<StyleRuleSupports>(prelude_serialized, supported,
                                                 rules);
}

StyleRuleViewport* CSSParserImpl::ConsumeViewportRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  // Allow @viewport rules from UA stylesheets even if the feature is disabled.
  if (!RuntimeEnabledFeatures::CSSViewportEnabled() &&
      !IsUASheetBehavior(context_->Mode()))
    return nullptr;

  if (!prelude.AtEnd())
    return nullptr;  // Parser error; @viewport prelude should be empty

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kViewport, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(prelude_offset.end);
    observer_->EndRuleBody(prelude_offset.end);
  }

  if (style_sheet_)
    style_sheet_->SetHasViewportRule();

  ConsumeDeclarationList(block, StyleRule::kViewport);
  return MakeGarbageCollected<StyleRuleViewport>(
      CreateCSSPropertyValueSet(parsed_properties_, kCSSViewportRuleMode));
}

StyleRuleFontFace* CSSParserImpl::ConsumeFontFaceRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& stream) {
  if (!prelude.AtEnd())
    return nullptr;  // Parse error; @font-face prelude should be empty

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFace, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(prelude_offset.end);
    observer_->EndRuleBody(prelude_offset.end);
  }

  if (style_sheet_)
    style_sheet_->SetHasFontFaceRule();

  ConsumeDeclarationList(stream, StyleRule::kFontFace);
  return MakeGarbageCollected<StyleRuleFontFace>(
      CreateCSSPropertyValueSet(parsed_properties_, kCSSFontFaceRuleMode));
}

StyleRuleKeyframes* CSSParserImpl::ConsumeKeyframesRule(
    bool webkit_prefixed,
    CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;  // Parse error; expected single non-whitespace token in
                     // @keyframes header

  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
  } else if (name_token.GetType() == kStringToken && webkit_prefixed) {
    context_->Count(WebFeature::kQuotedKeyframesRule);
    name = name_token.Value().ToString();
  } else {
    return nullptr;  // Parse error; expected ident token in @keyframes header
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframes, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(block.Offset());
  }

  auto* keyframe_rule = MakeGarbageCollected<StyleRuleKeyframes>();
  ConsumeRuleList(
      block, kKeyframesRuleList, [keyframe_rule](StyleRuleBase* keyframe) {
        keyframe_rule->ParserAppendKeyframe(To<StyleRuleKeyframe>(keyframe));
      });
  keyframe_rule->SetName(name);
  keyframe_rule->SetVendorPrefixed(webkit_prefixed);

  if (observer_)
    observer_->EndRuleBody(block.Offset());

  return keyframe_rule;
}

StyleRulePage* CSSParserImpl::ConsumePageRule(const CSSParserTokenRange prelude,
                                              const RangeOffset& prelude_offset,
                                              CSSParserTokenStream& block) {
  CSSSelectorList selector_list = ParsePageSelector(prelude, style_sheet_);
  if (!selector_list.IsValid())
    return nullptr;  // Parse error, invalid @page selector

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPage, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
  }

  ConsumeDeclarationList(block, StyleRule::kStyle);

  return MakeGarbageCollected<StyleRulePage>(
      std::move(selector_list),
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleProperty* CSSParserImpl::ConsumePropertyRule(
    CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  if (!RuntimeEnabledFeatures::CSSVariables2AtPropertyEnabled())
    return nullptr;

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd())
    return nullptr;
  if (!CSSVariableParser::IsValidVariableName(name_token))
    return nullptr;
  String name = name_token.Value().ToString();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kProperty, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
  }

  ConsumeDeclarationList(block, StyleRule::kProperty);
  return StyleRuleProperty::Create(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleKeyframe* CSSParserImpl::ConsumeKeyframeStyleRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  std::unique_ptr<Vector<double>> key_list = ConsumeKeyframeKeyList(prelude);
  if (!key_list)
    return nullptr;

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframe, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
  }

  ConsumeDeclarationList(block, StyleRule::kKeyframe);

  return MakeGarbageCollected<StyleRuleKeyframe>(
      std::move(key_list),
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRule* CSSParserImpl::ConsumeStyleRule(CSSParserTokenStream& stream) {
  if (observer_)
    observer_->StartRuleHeader(StyleRule::kStyle, stream.LookAheadOffset());

  // Parse the prelude of the style rule
  CSSSelectorList selector_list = CSSSelectorParser::ConsumeSelector(
      stream, context_, style_sheet_, observer_);

  if (!selector_list.IsValid()) {
    // Read the rest of the prelude if there was an error
    stream.EnsureLookAhead();
    while (!stream.UncheckedAtEnd() &&
           stream.UncheckedPeek().GetType() != kLeftBraceToken)
      stream.UncheckedConsumeComponentValue();
  }

  if (observer_)
    observer_->EndRuleHeader(stream.LookAheadOffset());

  if (stream.AtEnd())
    return nullptr;  // Parse error, EOF instead of qualified rule block

  DCHECK_EQ(stream.Peek().GetType(), kLeftBraceToken);
  CSSParserTokenStream::BlockGuard guard(stream);

  if (!selector_list.IsValid())
    return nullptr;  // Parse error, invalid selector list

  // TODO(csharrison): How should we lazily parse css that needs the observer?
  if (!observer_ && lazy_state_) {
    DCHECK(style_sheet_);
    return MakeGarbageCollected<StyleRule>(
        std::move(selector_list),
        MakeGarbageCollected<CSSLazyPropertyParserImpl>(stream.Offset() - 1,
                                                        lazy_state_));
  }
  ConsumeDeclarationList(stream, StyleRule::kStyle);

  return MakeGarbageCollected<StyleRule>(
      std::move(selector_list),
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

void CSSParserImpl::ConsumeDeclarationList(CSSParserTokenStream& stream,
                                           StyleRule::RuleType rule_type) {
  DCHECK(parsed_properties_.IsEmpty());

  bool use_observer = observer_ && (rule_type == StyleRule::kStyle ||
                                    rule_type == StyleRule::kKeyframe);
  if (use_observer) {
    observer_->StartRuleBody(stream.Offset());
  }

  while (true) {
    // Having a lookahead may skip comments, which are used by the observer.
    DCHECK(!stream.HasLookAhead() || stream.AtEnd());

    if (use_observer && !stream.HasLookAhead()) {
      while (true) {
        wtf_size_t start_offset = stream.Offset();
        if (!stream.ConsumeCommentOrNothing())
          break;
        observer_->ObserveComment(start_offset, stream.Offset());
      }
    }

    if (stream.AtEnd())
      break;

    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
      case kSemicolonToken:
        stream.UncheckedConsume();
        break;
      case kIdentToken: {
        // TODO(crbug.com/661854): Use streams instead of ranges
        const wtf_size_t decl_offset_start = stream.Offset();
        const CSSParserTokenRange decl =
            stream.ConsumeUntilPeekedTypeIs<kSemicolonToken>();
        // We want the offset of the kSemicolonToken, which is peeked but not
        // consumed.
        const RangeOffset decl_offset(decl_offset_start,
                                      stream.LookAheadOffset());

        ConsumeDeclaration(decl, decl_offset, rule_type);

        if (!stream.AtEnd())
          stream.UncheckedConsume();  // kSemicolonToken

        break;
      }
      default:
        while (!stream.UncheckedAtEnd() &&
               stream.UncheckedPeek().GetType() != kSemicolonToken) {
          stream.UncheckedConsumeComponentValue();
        }

        if (!stream.UncheckedAtEnd())
          stream.UncheckedConsume();  // kSemicolonToken

        break;
    }
  }

  if (use_observer)
    observer_->EndRuleBody(stream.LookAheadOffset());
}

void CSSParserImpl::ConsumeDeclaration(CSSParserTokenRange range,
                                       const RangeOffset& decl_offset,
                                       StyleRule::RuleType rule_type) {
  DCHECK_EQ(range.Peek().GetType(), kIdentToken);
  const CSSParserToken& lhs = range.ConsumeIncludingWhitespace();
  if (range.Consume().GetType() != kColonToken)
    return;  // Parse error

  bool important = false;
  const CSSParserToken* declaration_value_end = range.end();
  const CSSParserToken* last = range.end() - 1;
  while (last->GetType() == kWhitespaceToken)
    --last;
  if (last->GetType() == kIdentToken &&
      EqualIgnoringASCIICase(last->Value(), "important")) {
    --last;
    while (last->GetType() == kWhitespaceToken)
      --last;
    if (last->GetType() == kDelimiterToken && last->Delimiter() == '!') {
      important = true;
      declaration_value_end = last;
    }
  }

  size_t properties_count = parsed_properties_.size();

  CSSPropertyID unresolved_property = CSSPropertyID::kInvalid;
  AtRuleDescriptorID atrule_id = AtRuleDescriptorID::Invalid;
  if (rule_type == StyleRule::kFontFace || rule_type == StyleRule::kProperty) {
    if (important)  // Invalid
      return;
    atrule_id = lhs.ParseAsAtRuleDescriptorID();
    AtRuleDescriptorParser::ParseAtRule(atrule_id, range, *context_,
                                        parsed_properties_);
  } else {
    unresolved_property = lhs.ParseAsUnresolvedCSSPropertyID(context_->Mode());
  }

  // @rules other than FontFace still handled with legacy code.
  if (important && rule_type == StyleRule::kKeyframe)
    return;

  if (unresolved_property == CSSPropertyID::kVariable) {
    if (rule_type != StyleRule::kStyle && rule_type != StyleRule::kKeyframe)
      return;
    AtomicString variable_name = lhs.Value().ToAtomicString();
    bool is_animation_tainted = rule_type == StyleRule::kKeyframe;
    ConsumeVariableValue(
        range.MakeSubRange(&range.Peek(), declaration_value_end), variable_name,
        important, is_animation_tainted);
  } else if (unresolved_property != CSSPropertyID::kInvalid) {
    if (style_sheet_ && style_sheet_->SingleOwnerDocument())
      Deprecation::WarnOnDeprecatedProperties(
          style_sheet_->SingleOwnerDocument()->GetFrame(), unresolved_property);
    ConsumeDeclarationValue(
        range.MakeSubRange(&range.Peek(), declaration_value_end),
        unresolved_property, important, rule_type);
  }

  if (observer_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kKeyframe)) {
    observer_->ObserveProperty(decl_offset.start, decl_offset.end, important,
                               parsed_properties_.size() != properties_count);
  }
}

void CSSParserImpl::ConsumeVariableValue(CSSParserTokenRange range,
                                         const AtomicString& variable_name,
                                         bool important,
                                         bool is_animation_tainted) {
  if (CSSCustomPropertyDeclaration* value =
          CSSVariableParser::ParseDeclarationValue(
              variable_name, range, is_animation_tainted, *context_)) {
    parsed_properties_.push_back(
        CSSPropertyValue(GetCSSPropertyVariable(), *value, important));
    context_->Count(context_->Mode(), CSSPropertyID::kVariable);
  }
}

void CSSParserImpl::ConsumeDeclarationValue(CSSParserTokenRange range,
                                            CSSPropertyID unresolved_property,
                                            bool important,
                                            StyleRule::RuleType rule_type) {
  CSSPropertyParser::ParseValue(unresolved_property, important, range, context_,
                                parsed_properties_, rule_type);
}

std::unique_ptr<Vector<double>> CSSParserImpl::ConsumeKeyframeKeyList(
    CSSParserTokenRange range) {
  std::unique_ptr<Vector<double>> result = std::make_unique<Vector<double>>();
  while (true) {
    range.ConsumeWhitespace();
    const CSSParserToken& token = range.ConsumeIncludingWhitespace();
    if (token.GetType() == kPercentageToken && token.NumericValue() >= 0 &&
        token.NumericValue() <= 100)
      result->push_back(token.NumericValue() / 100);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "from"))
      result->push_back(0);
    else if (token.GetType() == kIdentToken &&
             EqualIgnoringASCIICase(token.Value(), "to"))
      result->push_back(1);
    else
      return nullptr;  // Parser error, invalid value in keyframe selector
    if (range.AtEnd())
      return result;
    if (range.Consume().GetType() != kCommaToken)
      return nullptr;  // Parser error
  }
}

}  // namespace blink
