// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include <bitset>
#include <limits>
#include <memory>
#include <utility>

#include "third_party/blink/renderer/core/animation/timeline_offset.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_custom_property_declaration.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser_impl.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

using std::swap;

namespace blink {

namespace {

// Max number of @try rules in a @position-fallback rule to prevent the amount
// of excess layout work that may be required.
const size_t kPositionFallbackRuleMaxLength = 5;

// This may still consume tokens if it fails
AtomicString ConsumeStringOrURI(CSSParserTokenStream& stream) {
  const CSSParserToken& token = stream.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken) {
    return stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  }

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url")) {
    return AtomicString();
  }

  AtomicString result;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    const CSSParserToken& uri = stream.ConsumeIncludingWhitespace();
    if (uri.GetType() != kBadStringToken && stream.UncheckedAtEnd()) {
      DCHECK_EQ(uri.GetType(), kStringToken);
      result = uri.Value().ToAtomicString();
    }
  }
  stream.ConsumeWhitespace();
  return result;
}

// Finds the longest prefix of |range| that matches a <layer-name> and parses
// it. Returns an empty result with |range| unmodified if parsing fails.
StyleRuleBase::LayerName ConsumeCascadeLayerName(CSSParserTokenRange& range) {
  CSSParserTokenRange original_range = range;
  StyleRuleBase::LayerName name;
  while (!range.AtEnd() && range.Peek().GetType() == kIdentToken) {
    const CSSParserToken& name_part = range.Consume();
    name.emplace_back(name_part.Value().ToString());

    const bool has_next_part = range.Peek().GetType() == kDelimiterToken &&
                               range.Peek().Delimiter() == '.' &&
                               range.Peek(1).GetType() == kIdentToken;
    if (!has_next_part) {
      break;
    }
    range.Consume();
  }

  if (!name.size()) {
    original_range = range;
  } else {
    range.ConsumeWhitespace();
  }

  return name;
}

StyleRule::RuleType RuleTypeForMutableDeclaration(
    MutableCSSPropertyValueSet* declaration) {
  switch (declaration->CssParserMode()) {
    case kCSSFontFaceRuleMode:
      return StyleRule::kFontFace;
    case kCSSKeyframeRuleMode:
      return StyleRule::kKeyframe;
    case kCSSPropertyRuleMode:
      return StyleRule::kProperty;
    default:
      return StyleRule::kStyle;
  }
}

absl::optional<StyleRuleFontFeature::FeatureType> ToStyleRuleFontFeatureType(
    CSSAtRuleID rule_id) {
  switch (rule_id) {
    case CSSAtRuleID::kCSSAtRuleStylistic:
      return StyleRuleFontFeature::FeatureType::kStylistic;
    case CSSAtRuleID::kCSSAtRuleStyleset:
      return StyleRuleFontFeature::FeatureType::kStyleset;
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      return StyleRuleFontFeature::FeatureType::kCharacterVariant;
    case CSSAtRuleID::kCSSAtRuleSwash:
      return StyleRuleFontFeature::FeatureType::kSwash;
    case CSSAtRuleID::kCSSAtRuleOrnaments:
      return StyleRuleFontFeature::FeatureType::kOrnaments;
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      return StyleRuleFontFeature::FeatureType::kAnnotation;
    default:
      NOTREACHED();
  }
  return absl::nullopt;
}

}  // namespace

CSSParserImpl::CSSParserImpl(const CSSParserContext* context,
                             StyleSheetContents* style_sheet)
    : context_(context),
      style_sheet_(style_sheet),
      observer_(nullptr),
      lazy_state_(nullptr) {}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseValue(
    MutableCSSPropertyValueSet* declaration,
    CSSPropertyID unresolved_property,
    StringView string,
    bool important,
    const CSSParserContext* context) {
  STACK_UNINITIALIZED CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = RuleTypeForMutableDeclaration(declaration);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  CSSTokenizedValue tokenized_value = ConsumeRestrictedPropertyValue(stream);
  parser.ConsumeDeclarationValue(tokenized_value, unresolved_property,
                                 important, rule_type);
  if (parser.parsed_properties_.empty()) {
    return MutableCSSPropertyValueSet::kParseError;
  }
  return declaration->AddParsedProperties(parser.parsed_properties_);
}

MutableCSSPropertyValueSet::SetResult CSSParserImpl::ParseVariableValue(
    MutableCSSPropertyValueSet* declaration,
    const AtomicString& property_name,
    StringView value,
    bool important,
    const CSSParserContext* context,
    bool is_animation_tainted) {
  STACK_UNINITIALIZED CSSParserImpl parser(context);
  CSSTokenizer tokenizer(value);
  CSSParserTokenStream stream(tokenizer);
  CSSTokenizedValue tokenized_value = ConsumeUnrestrictedPropertyValue(stream);
  parser.ConsumeVariableValue(tokenized_value, property_name, important,
                              is_animation_tainted);
  if (parser.parsed_properties_.empty()) {
    return MutableCSSPropertyValueSet::kParseError;
  } else {
    return declaration->AddParsedProperties(parser.parsed_properties_);
  }
}

static inline void FilterProperties(
    bool important,
    const HeapVector<CSSPropertyValue, 64>& input,
    HeapVector<CSSPropertyValue, 64>& output,
    wtf_size_t& unused_entries,
    std::bitset<kNumCSSProperties>& seen_properties,
    HashSet<AtomicString>& seen_custom_properties) {
  // Add properties in reverse order so that highest priority definitions are
  // reached first. Duplicate definitions can then be ignored when found.
  for (wtf_size_t i = input.size(); i--;) {
    const CSSPropertyValue& property = input[i];
    if (property.IsImportant() != important) {
      continue;
    }
    if (property.Id() == CSSPropertyID::kVariable) {
      const AtomicString& name = property.CustomPropertyName();
      if (seen_custom_properties.Contains(name)) {
        continue;
      }
      seen_custom_properties.insert(name);
    } else {
      const unsigned property_id_index = GetCSSPropertyIDIndex(property.Id());
      if (seen_properties.test(property_id_index)) {
        continue;
      }
      seen_properties.set(property_id_index);
    }
    output[--unused_entries] = property;
  }
}

static ImmutableCSSPropertyValueSet* CreateCSSPropertyValueSet(
    HeapVector<CSSPropertyValue, 64>& parsed_properties,
    CSSParserMode mode) {
  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parsed_properties.size();
  HeapVector<CSSPropertyValue, 64> results(unused_entries);
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
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*child_rules=*/nullptr);
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
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, parser_mode);
}

bool CSSParserImpl::ParseDeclarationList(
    MutableCSSPropertyValueSet* declaration,
    const String& string,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = RuleTypeForMutableDeclaration(declaration);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  // See function declaration comment for why parent_rule_for_nesting ==
  // nullptr.
  parser.ConsumeDeclarationList(stream, rule_type, CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*child_rules=*/nullptr);
  if (parser.parsed_properties_.empty()) {
    return false;
  }

  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parser.parsed_properties_.size();
  HeapVector<CSSPropertyValue, 64> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;
  FilterProperties(true, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parser.parsed_properties_, results, unused_entries,
                   seen_properties, seen_custom_properties);
  if (unused_entries) {
    results.EraseAt(0, unused_entries);
  }
  return declaration->AddParsedProperties(results);
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        CSSNestingType nesting_type,
                                        StyleRule* parent_rule_for_nesting,
                                        StyleSheetContents* style_sheet,
                                        AllowedRulesType allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  stream.ConsumeWhitespace();
  if (stream.UncheckedAtEnd()) {
    return nullptr;  // Parse error, empty rule
  }
  StyleRuleBase* rule;
  if (stream.UncheckedPeek().GetType() == kAtKeywordToken) {
    rule = parser.ConsumeAtRule(stream, allowed_rules, CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr);
  } else {
    rule = parser.ConsumeQualifiedRule(stream, allowed_rules, nesting_type,
                                       parent_rule_for_nesting);
  }
  if (!rule) {
    return nullptr;  // Parse error, failed to consume rule
  }
  stream.ConsumeWhitespace();
  if (!rule || !stream.UncheckedAtEnd()) {
    return nullptr;  // Parse error, trailing garbage
  }
  return rule;
}

ParseSheetResult CSSParserImpl::ParseStyleSheet(
    const String& string,
    const CSSParserContext* context,
    StyleSheetContents* style_sheet,
    CSSDeferPropertyParsing defer_property_parsing,
    bool allow_import_rules) {
  absl::optional<LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer> timer;
  if (context->GetDocument() && context->GetDocument()->View()) {
    if (auto* metrics_aggregator =
            context->GetDocument()->View()->GetUkmAggregator()) {
      timer.emplace(metrics_aggregator->GetScopedTimer(
          static_cast<size_t>(LocalFrameUkmAggregator::kParseStyleSheet)));
    }
  }
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
      stream, kTopLevelRuleList, CSSNestingType::kNone,
      /*parent_rule_for_nesting=*/nullptr,
      [&style_sheet, &result, &string, allow_import_rules, context](
          StyleRuleBase* rule, wtf_size_t offset) {
        if (rule->IsCharsetRule()) {
          return;
        }
        if (rule->IsImportRule()) {
          if (!allow_import_rules || context->IsForMarkupSanitization()) {
            result = ParseSheetResult::kHasUnallowedImportRule;
            return;
          }

          Document* document = style_sheet->AnyOwnerDocument();
          if (document) {
            TextPosition position = TextPosition::MinimumPosition();
            probe::GetTextPosition(document, offset, &string, &position);
            To<StyleRuleImport>(rule)->SetPositionHint(position);
          }
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

// static
CSSSelectorList* CSSParserImpl::ParsePageSelector(
    CSSParserTokenRange range,
    StyleSheetContents* style_sheet,
    const CSSParserContext& context) {
  // We only support a small subset of the css-page spec.
  range.ConsumeWhitespace();
  AtomicString type_selector;
  if (range.Peek().GetType() == kIdentToken) {
    type_selector = range.Consume().Value().ToAtomicString();
  }

  AtomicString pseudo;
  if (range.Peek().GetType() == kColonToken) {
    range.Consume();
    if (range.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    pseudo = range.Consume().Value().ToAtomicString();
  }

  range.ConsumeWhitespace();
  if (!range.AtEnd()) {
    return nullptr;  // Parse error; extra tokens in @page selector
  }

  HeapVector<CSSSelector> selectors;
  if (!type_selector.IsNull()) {
    selectors.push_back(
        CSSSelector(QualifiedName(g_null_atom, type_selector, g_star_atom)));
  }
  if (!pseudo.IsNull()) {
    CSSSelector selector;
    selector.SetMatch(CSSSelector::kPagePseudoClass);
    selector.UpdatePseudoPage(pseudo.LowerASCII(), context.GetDocument());
    if (selector.GetPseudoType() == CSSSelector::kPseudoUnknown) {
      return nullptr;
    }
    if (selectors.size() != 0) {
      selectors[0].SetLastInComplexSelector(false);
    }
    selectors.push_back(selector);
  }
  if (selectors.empty()) {
    selectors.push_back(CSSSelector());
  }
  selectors[0].SetForPage();
  selectors.back().SetLastInComplexSelector(true);
  return CSSSelectorList::AdoptSelectorVector(
      base::span<CSSSelector>(selectors));
}

std::unique_ptr<Vector<KeyframeOffset>> CSSParserImpl::ParseKeyframeKeyList(
    const CSSParserContext* context,
    const String& key_list) {
  CSSTokenizer tokenizer(key_list);
  // TODO(crbug.com/661854): Use streams instead of ranges
  return ConsumeKeyframeKeyList(context,
                                CSSParserTokenRange(tokenizer.TokenizeToEOF()));
}

String CSSParserImpl::ParseCustomPropertyName(const String& name_text) {
  CSSTokenizer tokenizer(name_text);
  auto tokens = tokenizer.TokenizeToEOF();
  CSSParserTokenRange range = tokens;
  const CSSParserToken& name_token = range.ConsumeIncludingWhitespace();
  if (!range.AtEnd()) {
    return {};
  }
  if (!CSSVariableParser::IsValidVariableName(name_token)) {
    return {};
  }
  return name_token.Value().ToString();
}

bool CSSParserImpl::ConsumeSupportsDeclaration(CSSParserTokenStream& stream) {
  DCHECK(parsed_properties_.empty());
  // Even though we might use an observer here, this is just to test if we
  // successfully parse the range, so we can temporarily remove the observer.
  CSSParserObserver* observer_copy = observer_;
  observer_ = nullptr;
  ConsumeDeclaration(stream, StyleRule::kStyle);
  observer_ = observer_copy;

  bool result = !parsed_properties_.empty();
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
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*child_rules=*/nullptr);
}

void CSSParserImpl::ParseStyleSheetForInspector(const String& string,
                                                const CSSParserContext* context,
                                                StyleSheetContents* style_sheet,
                                                CSSParserObserver& observer) {
  CSSParserImpl parser(context, style_sheet);
  parser.observer_ = &observer;
  CSSTokenizer tokenizer(string);
  CSSParserTokenStream stream(tokenizer);
  bool first_rule_valid =
      parser.ConsumeRuleList(stream, kTopLevelRuleList, CSSNestingType::kNone,
                             /*parent_rule_for_nesting=*/nullptr,
                             [&style_sheet](StyleRuleBase* rule, wtf_size_t) {
                               if (rule->IsCharsetRule()) {
                                 return;
                               }
                               style_sheet->ParserAppendRule(rule);
                             });
  style_sheet->SetHasSyntacticallyValidCSSHeader(first_rule_valid);
}

CSSPropertyValueSet* CSSParserImpl::ParseDeclarationListForLazyStyle(
    const String& string,
    wtf_size_t offset,
    const CSSParserContext* context) {
  // NOTE: Lazy parsing does not support nested rules (it happens
  // only after matching, which means that we cannot insert child rules
  // we encounter during parsing -- we never match against them),
  // so parent_rule_for_nesting is always nullptr here. The parser
  // explicitly makes sure we do not invoke lazy parsing for rules
  // with child rules in them.
  CSSTokenizer tokenizer(string, offset);
  CSSParserTokenStream stream(tokenizer);
  CSSParserTokenStream::BlockGuard guard(stream);
  CSSParserImpl parser(context);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, context->Mode());
}

static CSSParserImpl::AllowedRulesType ComputeNewAllowedRules(
    CSSParserImpl::AllowedRulesType allowed_rules,
    StyleRuleBase* rule) {
  if (!rule || allowed_rules == CSSParserImpl::kKeyframeRules ||
      allowed_rules == CSSParserImpl::kFontFeatureRules ||
      allowed_rules == CSSParserImpl::kTryRules ||
      allowed_rules == CSSParserImpl::kNoRules) {
    return allowed_rules;
  }
  DCHECK_LE(allowed_rules, CSSParserImpl::kRegularRules);
  if (rule->IsCharsetRule()) {
    return CSSParserImpl::kAllowLayerStatementRules;
  }
  if (rule->IsLayerStatementRule()) {
    if (allowed_rules <= CSSParserImpl::kAllowLayerStatementRules) {
      return CSSParserImpl::kAllowLayerStatementRules;
    }
    return CSSParserImpl::kRegularRules;
  }
  if (rule->IsImportRule()) {
    return CSSParserImpl::kAllowImportRules;
  }
  if (rule->IsNamespaceRule()) {
    return CSSParserImpl::kAllowNamespaceRules;
  }
  return CSSParserImpl::kRegularRules;
}

template <typename T>
bool CSSParserImpl::ConsumeRuleList(CSSParserTokenStream& stream,
                                    RuleListType rule_list_type,
                                    CSSNestingType nesting_type,
                                    StyleRule* parent_rule_for_nesting,
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
    case kPositionFallbackRuleList:
      allowed_rules = kTryRules;
      break;
    default:
      NOTREACHED();
  }

  bool seen_rule = false;
  bool first_rule_valid = false;
  while (!stream.AtEnd()) {
    wtf_size_t offset = stream.Offset();
    StyleRuleBase* rule = nullptr;
    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
        stream.UncheckedConsume();
        continue;
      case kAtKeywordToken:
        rule = ConsumeAtRule(stream, allowed_rules, nesting_type,
                             parent_rule_for_nesting);
        break;
      case kCDOToken:
      case kCDCToken:
        if (rule_list_type == kTopLevelRuleList) {
          stream.UncheckedConsume();
          continue;
        }
        [[fallthrough]];
      default:
        rule = ConsumeQualifiedRule(stream, allowed_rules, nesting_type,
                                    parent_rule_for_nesting);
        break;
    }
    if (!seen_rule) {
      seen_rule = true;
      first_rule_valid = rule;
    }
    if (rule) {
      allowed_rules = ComputeNewAllowedRules(allowed_rules, rule);
      callback(rule, offset);
    }
    DCHECK_GT(stream.Offset(), offset);
  }

  return first_rule_valid;
}

CSSParserTokenRange ConsumeAtRulePrelude(CSSParserTokenStream& stream) {
  return stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
}

bool ConsumeEndOfPreludeForAtRuleWithoutBlock(CSSParserTokenStream& stream) {
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!stream.UncheckedAtEnd()) {
      stream.UncheckedConsume();  // kSemicolonToken
    }
    return true;
  }

  // Consume the erroneous block.
  CSSParserTokenStream::BlockGuard guard(stream);
  return false;  // Parse error, we expected no block.
}

bool ConsumeEndOfPreludeForAtRuleWithBlock(CSSParserTokenStream& stream) {
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!stream.UncheckedAtEnd()) {
      stream.UncheckedConsume();  // kSemicolonToken
    }
    return false;  // Parse error, we expected a block.
  }

  return true;
}

void CSSParserImpl::ConsumeErroneousAtRule(CSSParserTokenStream& stream,
                                           CSSAtRuleID id) {
  if (observer_) {
    observer_->ObserveErroneousAtRule(stream.Offset(), id);
  }
  // Consume the prelude and block if present.
  ConsumeAtRulePrelude(stream);
  if (!stream.AtEnd()) {
    if (stream.UncheckedPeek().GetType() == kLeftBraceToken) {
      CSSParserTokenStream::BlockGuard guard(stream);
    } else {
      stream.UncheckedConsume();  // kSemicolonToken
    }
  }
}

StyleRuleBase* CSSParserImpl::ConsumeAtRule(
    CSSParserTokenStream& stream,
    AllowedRulesType allowed_rules,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  DCHECK_EQ(stream.Peek().GetType(), kAtKeywordToken);
  CSSParserToken name_token =
      stream.ConsumeIncludingWhitespace();  // Must live until CssAtRuleID().
  const StringView name = name_token.Value();
  const CSSAtRuleID id = CssAtRuleID(name);
  return ConsumeAtRuleContents(id, stream, allowed_rules, nesting_type,
                               parent_rule_for_nesting);
}

StyleRuleBase* CSSParserImpl::ConsumeAtRuleContents(
    CSSAtRuleID id,
    CSSParserTokenStream& stream,
    AllowedRulesType allowed_rules,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  if (allowed_rules == kNestedGroupRules) {
    if (id != CSSAtRuleID::kCSSAtRuleMedia &&      // [css-conditional-3]
        id != CSSAtRuleID::kCSSAtRuleSupports &&   // [css-conditional-3]
        id != CSSAtRuleID::kCSSAtRuleContainer &&  // [css-contain-3]
        id != CSSAtRuleID::kCSSAtRuleLayer &&      // [css-cascade-5]
        id != CSSAtRuleID::kCSSAtRuleScope &&      // [css-cascade-6]
        id != CSSAtRuleID::kCSSAtRuleStartingStyle &&
        id != CSSAtRuleID::kCSSAtRuleViewTransitions) {
      ConsumeErroneousAtRule(stream, id);
      return nullptr;
    }
    allowed_rules = kRegularRules;
  }

  // @import rules have a URI component that is not technically part of the
  // prelude.
  AtomicString import_prelude_uri;
  if (allowed_rules <= kAllowImportRules &&
      id == CSSAtRuleID::kCSSAtRuleImport) {
    import_prelude_uri = ConsumeStringOrURI(stream);
  }

  if (id != CSSAtRuleID::kCSSAtRuleInvalid &&
      context_->IsUseCounterRecordingEnabled()) {
    CountAtRule(context_, id);
  }

  if (allowed_rules == kKeyframeRules || allowed_rules == kNoRules) {
    // Parse error, no at-rules supported inside @keyframes,
    // or blocks supported inside declaration lists.
    ConsumeErroneousAtRule(stream, id);
    return nullptr;
  }

  stream.EnsureLookAhead();
  if (allowed_rules == kAllowCharsetRules &&
      id == CSSAtRuleID::kCSSAtRuleCharset) {
    return ConsumeCharsetRule(stream);
  } else if (allowed_rules <= kAllowImportRules &&
             id == CSSAtRuleID::kCSSAtRuleImport) {
    return ConsumeImportRule(std::move(import_prelude_uri), stream);
  } else if (allowed_rules <= kAllowNamespaceRules &&
             id == CSSAtRuleID::kCSSAtRuleNamespace) {
    return ConsumeNamespaceRule(stream);
  } else if (allowed_rules == kTryRules) {
    if (id == CSSAtRuleID::kCSSAtRuleTry) {
      return ConsumeTryRule(stream);
    }
    ConsumeErroneousAtRule(stream, id);
    return nullptr;
  } else if (allowed_rules == kFontFeatureRules) {
    if (id == CSSAtRuleID::kCSSAtRuleStylistic ||
        id == CSSAtRuleID::kCSSAtRuleStyleset ||
        id == CSSAtRuleID::kCSSAtRuleCharacterVariant ||
        id == CSSAtRuleID::kCSSAtRuleSwash ||
        id == CSSAtRuleID::kCSSAtRuleOrnaments ||
        id == CSSAtRuleID::kCSSAtRuleAnnotation) {
      return ConsumeFontFeatureRule(id, stream);
    } else {
      return nullptr;
    }
  } else {
    DCHECK_LE(allowed_rules, kRegularRules);

    switch (id) {
      case CSSAtRuleID::kCSSAtRuleViewTransitions:
        return ConsumeViewTransitionsRule(stream);
      case CSSAtRuleID::kCSSAtRuleContainer:
        return ConsumeContainerRule(stream, nesting_type,
                                    parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRuleMedia:
        return ConsumeMediaRule(stream, nesting_type, parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRuleSupports:
        return ConsumeSupportsRule(stream, nesting_type,
                                   parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRuleStartingStyle:
        return ConsumeStartingStyleRule(stream, nesting_type,
                                        parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRuleFontFace:
        return ConsumeFontFaceRule(stream);
      case CSSAtRuleID::kCSSAtRuleFontPaletteValues:
        return ConsumeFontPaletteValuesRule(stream);
      case CSSAtRuleID::kCSSAtRuleFontFeatureValues:
        return ConsumeFontFeatureValuesRule(stream);
      case CSSAtRuleID::kCSSAtRuleWebkitKeyframes:
        return ConsumeKeyframesRule(true, stream);
      case CSSAtRuleID::kCSSAtRuleKeyframes:
        return ConsumeKeyframesRule(false, stream);
      case CSSAtRuleID::kCSSAtRuleLayer:
        return ConsumeLayerRule(stream, nesting_type, parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRulePage:
        return ConsumePageRule(stream);
      case CSSAtRuleID::kCSSAtRuleProperty:
        return ConsumePropertyRule(stream);
      case CSSAtRuleID::kCSSAtRuleScope:
        return ConsumeScopeRule(stream, nesting_type, parent_rule_for_nesting);
      case CSSAtRuleID::kCSSAtRuleCounterStyle:
        return ConsumeCounterStyleRule(stream);
      case CSSAtRuleID::kCSSAtRulePositionFallback:
        return ConsumePositionFallbackRule(stream);
      case CSSAtRuleID::kCSSAtRuleInvalid:
      case CSSAtRuleID::kCSSAtRuleCharset:
      case CSSAtRuleID::kCSSAtRuleImport:
      case CSSAtRuleID::kCSSAtRuleNamespace:
      case CSSAtRuleID::kCSSAtRuleTry:
      case CSSAtRuleID::kCSSAtRuleStylistic:
      case CSSAtRuleID::kCSSAtRuleStyleset:
      case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      case CSSAtRuleID::kCSSAtRuleSwash:
      case CSSAtRuleID::kCSSAtRuleOrnaments:
      case CSSAtRuleID::kCSSAtRuleAnnotation:
        ConsumeErroneousAtRule(stream, id);
        return nullptr;  // Parse error, unrecognised or not-allowed at-rule
    }
  }
}

StyleRuleBase* CSSParserImpl::ConsumeQualifiedRule(
    CSSParserTokenStream& stream,
    AllowedRulesType allowed_rules,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  if (allowed_rules <= kRegularRules) {
    return ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                            /* semicolon_aborts_nested_selector */ false);
  }

  if (allowed_rules == kKeyframeRules) {
    stream.EnsureLookAhead();
    const wtf_size_t prelude_offset_start = stream.LookAheadOffset();
    const CSSParserTokenRange prelude =
        stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
    const RangeOffset prelude_offset(prelude_offset_start,
                                     stream.LookAheadOffset());

    if (stream.AtEnd()) {
      return nullptr;  // Parse error, EOF instead of qualified rule block
    }

    CSSParserTokenStream::BlockGuard guard(stream);
    StyleRuleKeyframe* keyframe_style_rule =
        ConsumeKeyframeStyleRule(prelude, prelude_offset, stream);
    if (keyframe_style_rule) {
      context_->ReportLayoutAnimationsViolationIfNeeded(*keyframe_style_rule);
    }
    return keyframe_style_rule;
  }
  if (allowed_rules == kFontFeatureRules) {
    // We get here if something other than an at rule (e.g. @swash,
    // @ornaments... ) was found within @font-feature-values. As we don't
    // support font-display in @font-feature-values, we try to it by scanning
    // until the at-rule or until the block may end. Compare
    // https://drafts.csswg.org/css-fonts-4/#ex-invalid-ignored
    stream.EnsureLookAhead();
    stream.ConsumeUntilPeekedTypeIs<kAtKeywordToken>();
    return nullptr;
  }
  if (allowed_rules == kTryRules) {
    // We reach here only when there's a parse error. Treat everything before
    // the first block we reach as a bad prelude, then skip this block.
    stream.EnsureLookAhead();
    stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
    if (!stream.AtEnd()) {
      CSSParserTokenStream::BlockGuard guard(stream);
    }
    return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

// This may still consume tokens if it fails
static AtomicString ConsumeStringOrURI(CSSParserTokenRange& range) {
  const CSSParserToken& token = range.Peek();

  if (token.GetType() == kStringToken || token.GetType() == kUrlToken) {
    return range.ConsumeIncludingWhitespace().Value().ToAtomicString();
  }

  if (token.GetType() != kFunctionToken ||
      !EqualIgnoringASCIICase(token.Value(), "url")) {
    return AtomicString();
  }

  CSSParserTokenRange contents = range.ConsumeBlock();
  const CSSParserToken& uri = contents.ConsumeIncludingWhitespace();
  if (uri.GetType() == kBadStringToken || !contents.AtEnd()) {
    return AtomicString();
  }
  DCHECK_EQ(uri.GetType(), kStringToken);
  return uri.Value().ToAtomicString();
}

StyleRuleCharset* CSSParserImpl::ConsumeCharsetRule(
    CSSParserTokenStream& stream) {
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream)) {
    return nullptr;
  }

  const CSSParserToken& string = prelude.ConsumeIncludingWhitespace();
  if (string.GetType() != kStringToken || !prelude.AtEnd()) {
    return nullptr;  // Parse error, expected a single string
  }
  return MakeGarbageCollected<StyleRuleCharset>();
}

// We need the token offsets for MediaQueryParser, so re-parse the prelude.
static CSSParserTokenOffsets ReparseForOffsets(
    const StringView prelude,
    const CSSParserTokenRange range) {
  Vector<wtf_size_t, 32> raw_offsets =
      CSSTokenizer(prelude).TokenizeToEOFWithOffsets().second;
  return {range.RemainingSpan(), std::move(raw_offsets), prelude};
}

StyleRuleImport* CSSParserImpl::ConsumeImportRule(
    const AtomicString& uri,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream)) {
    return nullptr;
  }

  if (uri.IsNull()) {
    return nullptr;  // Parse error, expected string or URI
  }

  CSSParserTokenOffsets offsets = ReparseForOffsets(
      stream.StringRangeAt(prelude_offset_start,
                           prelude_offset_end - prelude_offset_start),
      prelude);

  StyleRuleBase::LayerName layer;
  if (prelude.Peek().GetType() == kIdentToken &&
      prelude.Peek().Id() == CSSValueID::kLayer) {
    prelude.ConsumeIncludingWhitespace();
    layer = StyleRuleBase::LayerName({g_empty_atom});
  } else if (prelude.Peek().GetType() == kFunctionToken &&
             prelude.Peek().FunctionId() == CSSValueID::kLayer) {
    CSSParserTokenRange original_prelude = prelude;
    CSSParserTokenRange name_range =
        css_parsing_utils::ConsumeFunction(prelude);
    StyleRuleBase::LayerName name = ConsumeCascadeLayerName(name_range);
    if (!name.size() || !name_range.AtEnd()) {
      // Invalid layer() function can still be parsed as <general-enclosed>
      prelude = original_prelude;
    } else {
      layer = std::move(name);
    }
  }
  if (layer.size()) {
    context_->Count(WebFeature::kCSSCascadeLayers);
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kImport, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  return MakeGarbageCollected<StyleRuleImport>(
      uri, std::move(layer),
      MediaQueryParser::ParseMediaQuerySet(prelude, offsets,
                                           context_->GetExecutionContext()),
      context_->IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse);
}

StyleRuleNamespace* CSSParserImpl::ConsumeNamespaceRule(
    CSSParserTokenStream& stream) {
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream)) {
    return nullptr;
  }

  AtomicString namespace_prefix;
  if (prelude.Peek().GetType() == kIdentToken) {
    namespace_prefix =
        prelude.ConsumeIncludingWhitespace().Value().ToAtomicString();
  }

  AtomicString uri(ConsumeStringOrURI(prelude));
  if (uri.IsNull() || !prelude.AtEnd()) {
    return nullptr;  // Parse error, expected string or URI
  }

  return MakeGarbageCollected<StyleRuleNamespace>(namespace_prefix, uri);
}

StyleRule* CSSParserImpl::CreateImplicitNestedRule(
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  constexpr bool kNotImplicit =
      false;  // The rule is implicit, but the &/:scope is not.

  HeapVector<CSSSelector, 2> selectors;

  switch (nesting_type) {
    case CSSNestingType::kNone:
      NOTREACHED();
      break;
    case CSSNestingType::kNesting:
      // kPseudoParent
      selectors.push_back(CSSSelector(parent_rule_for_nesting, kNotImplicit));
      break;
    case CSSNestingType::kScope: {
      // See CSSSelector::RelationType::kScopeActivation.
      CSSSelector selector;
      selector.SetTrue();
      selector.SetRelation(CSSSelector::kScopeActivation);
      selectors.push_back(selector);
      selectors.push_back(CSSSelector(AtomicString("scope"), kNotImplicit));
      break;
    }
  }

  CHECK(!selectors.empty());
  selectors.back().SetLastInComplexSelector(true);
  selectors.back().SetLastInSelectorList(true);

  return StyleRule::Create(
      base::span<CSSSelector>{selectors.data(), selectors.size()},
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleMedia* CSSParserImpl::ConsumeMediaRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kMedia, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  if (style_sheet_) {
    style_sheet_->SetHasMediaQueries();
  }

  String prelude_string =
      stream
          .StringRangeAt(prelude_offset_start,
                         prelude_offset_end - prelude_offset_start)
          .ToString();
  CSSParserTokenOffsets offsets = ReparseForOffsets(prelude_string, prelude);
  const MediaQuerySet* media =
      CachedMediaQuerySet(prelude_string, prelude, offsets);
  DCHECK(media);

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      nesting_type, parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  // NOTE: There will be a copy of rules here, to deal with the different inline
  // size.
  return MakeGarbageCollected<StyleRuleMedia>(media, std::move(rules));
}

StyleRuleSupports* CSSParserImpl::ConsumeSupportsRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSSupportsParser::Result supported =
      CSSSupportsParser::ConsumeSupportsCondition(stream, *this);
  // Check whether the entire prelude was consumed. If it wasn't, ensure we
  // consume any leftovers plus the block before returning a parse error.
  stream.ConsumeWhitespace();
  CSSParserTokenRange prelude_remainder = ConsumeAtRulePrelude(stream);
  if (!prelude_remainder.AtEnd()) {
    supported = CSSSupportsParser::Result::kParseFailure;
  }
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (supported == CSSSupportsParser::Result::kParseFailure) {
    return nullptr;  // Parse error, invalid @supports condition
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kSupports, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  const auto prelude_serialized =
      stream
          .StringRangeAt(prelude_offset_start,
                         prelude_offset_end - prelude_offset_start)
          .ToString()
          .SimplifyWhiteSpace();

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      nesting_type, parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  // NOTE: There will be a copy of rules here, to deal with the different inline
  // size.
  return MakeGarbageCollected<StyleRuleSupports>(
      prelude_serialized, supported == CSSSupportsParser::Result::kSupported,
      std::move(rules));
}

StyleRuleStartingStyle* CSSParserImpl::ConsumeStartingStyleRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (!prelude.AtEnd()) {
    return nullptr;  // Parse error; @starting-style prelude should be empty
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kStartingStyle, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      nesting_type, parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  // NOTE: There will be a copy of rules here, to deal with the different inline
  // size.
  return MakeGarbageCollected<StyleRuleStartingStyle>(std::move(rules));
}

StyleRuleFontFace* CSSParserImpl::ConsumeFontFaceRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (!prelude.AtEnd()) {
    return nullptr;  // Parse error; @font-face prelude should be empty
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFace, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  if (style_sheet_) {
    style_sheet_->SetHasFontFaceRule();
  }

  ConsumeDeclarationList(stream, StyleRule::kFontFace, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);
  return MakeGarbageCollected<StyleRuleFontFace>(
      CreateCSSPropertyValueSet(parsed_properties_, kCSSFontFaceRuleMode));
}

StyleRuleKeyframes* CSSParserImpl::ConsumeKeyframesRule(
    bool webkit_prefixed,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd()) {
    return nullptr;  // Parse error; expected single non-whitespace token in
                     // @keyframes header
  }

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
    observer_->StartRuleHeader(StyleRule::kKeyframes, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  auto* keyframe_rule = MakeGarbageCollected<StyleRuleKeyframes>();
  ConsumeRuleList(
      stream, kKeyframesRuleList, CSSNestingType::kNone,
      /*parent_rule_for_nesting=*/nullptr,
      [keyframe_rule](StyleRuleBase* keyframe, wtf_size_t) {
        keyframe_rule->ParserAppendKeyframe(To<StyleRuleKeyframe>(keyframe));
      });
  keyframe_rule->SetName(name);
  keyframe_rule->SetVendorPrefixed(webkit_prefixed);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return keyframe_rule;
}

StyleRuleFontFeature* CSSParserImpl::ConsumeFontFeatureRule(
    CSSAtRuleID rule_id,
    CSSParserTokenStream& stream) {
  absl::optional<StyleRuleFontFeature::FeatureType> feature_type =
      ToStyleRuleFontFeatureType(rule_id);
  if (!feature_type) {
    return nullptr;
  }

  wtf_size_t max_allowed_values = 1;
  if (feature_type == StyleRuleFontFeature::FeatureType::kCharacterVariant) {
    max_allowed_values = 2;
  }
  if (feature_type == StyleRuleFontFeature::FeatureType::kStyleset) {
    max_allowed_values = std::numeric_limits<wtf_size_t>::max();
  }

  stream.ConsumeWhitespace();

  if (stream.Peek().GetType() != kLeftBraceToken) {
    return nullptr;
  }

  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();

  auto* font_feature_rule =
      MakeGarbageCollected<StyleRuleFontFeature>(*feature_type);

  while (!stream.AtEnd()) {
    const CSSParserToken& alias_token = stream.Peek();

    if (alias_token.GetType() != kIdentToken) {
      return nullptr;
    }
    AtomicString alias =
        stream.ConsumeIncludingWhitespace().Value().ToAtomicString();

    const CSSParserToken& colon_token = stream.Peek();

    if (colon_token.GetType() != kColonToken) {
      return nullptr;
    }

    stream.UncheckedConsume();
    stream.ConsumeWhitespace();

    CSSValueList* numbers = CSSValueList::CreateSpaceSeparated();

    CSSParserTokenRange list =
        stream.ConsumeUntilPeekedTypeIs<kSemicolonToken>();
    list.ConsumeWhitespace();

    do {
      if (numbers->length() == max_allowed_values) {
        return nullptr;
      }
      CSSPrimitiveValue* parsed_number =
          css_parsing_utils::ConsumeIntegerOrNumberCalc(
              list, *context_,
              CSSPrimitiveValue::ValueRange::kNonNegativeInteger);
      if (!parsed_number) {
        return nullptr;
      }
      numbers->Append(*parsed_number);
    } while (!list.AtEnd());

    if (!numbers->length()) {
      return nullptr;
    }

    Vector<uint32_t> parsed_numbers;
    for (auto value : *numbers) {
      const CSSPrimitiveValue* number_value =
          DynamicTo<CSSPrimitiveValue>(*value);
      if (!number_value) {
        return nullptr;
      }
      parsed_numbers.push_back(number_value->GetIntValue());
    }

    const CSSParserToken& expected_semicolon = stream.Peek();
    if (expected_semicolon.GetType() == kSemicolonToken) {
      stream.UncheckedConsume();
    }
    stream.ConsumeWhitespace();

    font_feature_rule->UpdateAlias(alias, parsed_numbers);
  }

  return font_feature_rule;
}

StyleRuleFontFeatureValues* CSSParserImpl::ConsumeFontFeatureValuesRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFeatureValues,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  CSSValueList* family_list = css_parsing_utils::ConsumeFontFamily(prelude);

  if (!family_list || !family_list->length()) {
    return nullptr;
  }

  // The nesting logic for parsing @font-feature-values looks as follow:
  // 1) ConsumeRuleList, calls ConsumeAtRule, and in turn ConsumeAtRuleContents
  // 2) ConsumeAtRuleContents uses new ids for inner at-rules, for swash,
  // styleset etc.
  // 3) ConsumeFeatureRule (with type) consumes the inner mappings from aliases
  // to number lists.

  FontFeatureAliases stylistic;
  FontFeatureAliases styleset;
  FontFeatureAliases character_variant;
  FontFeatureAliases swash;
  FontFeatureAliases ornaments;
  FontFeatureAliases annotation;

  HeapVector<Member<StyleRuleFontFeature>> feature_rules;
  bool had_valid_rules = false;
  // ConsumeRuleList returns true only if the first rule is true, but we need to
  // be more generous with the internals of what's inside a font feature value
  // declaration, e.g. inside a @stylsitic, @styleset, etc.
  if (ConsumeRuleList(
          stream, kFontFeatureRuleList, CSSNestingType::kNone,
          /*parent_rule_for_nesting=*/nullptr,
          [&feature_rules, &had_valid_rules](StyleRuleBase* rule, wtf_size_t) {
            if (rule) {
              had_valid_rules = true;
            }
            feature_rules.push_back(To<StyleRuleFontFeature>(rule));
          }) ||
      had_valid_rules) {
    // https://drafts.csswg.org/css-fonts-4/#font-feature-values-syntax
    // "Specifying the same <font-feature-value-type> more than once is valid;
    // their contents are cascaded together."
    for (auto& feature_rule : feature_rules) {
      switch (feature_rule->GetFeatureType()) {
        case StyleRuleFontFeature::FeatureType::kStylistic:
          feature_rule->OverrideAliasesIn(stylistic);
          break;
        case StyleRuleFontFeature::FeatureType::kStyleset:
          feature_rule->OverrideAliasesIn(styleset);
          break;
        case StyleRuleFontFeature::FeatureType::kCharacterVariant:
          feature_rule->OverrideAliasesIn(character_variant);
          break;
        case StyleRuleFontFeature::FeatureType::kSwash:
          feature_rule->OverrideAliasesIn(swash);
          break;
        case StyleRuleFontFeature::FeatureType::kOrnaments:
          feature_rule->OverrideAliasesIn(ornaments);
          break;
        case StyleRuleFontFeature::FeatureType::kAnnotation:
          feature_rule->OverrideAliasesIn(annotation);
          break;
      }
    }
  }

  Vector<AtomicString> families;
  for (const auto family_entry : *family_list) {
    const CSSFontFamilyValue* family_value =
        DynamicTo<CSSFontFamilyValue>(*family_entry);
    if (!family_value) {
      return nullptr;
    }
    families.push_back(family_value->Value());
  }

  auto* feature_values_rule = MakeGarbageCollected<StyleRuleFontFeatureValues>(
      families, stylistic, styleset, character_variant, swash, ornaments,
      annotation);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return feature_values_rule;
}

StyleRulePage* CSSParserImpl::ConsumePageRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  CSSSelectorList* selector_list =
      ParsePageSelector(prelude, style_sheet_, *context_);
  if (!selector_list || !selector_list->IsValid()) {
    return nullptr;  // Parse error, invalid @page selector
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPage, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kStyle, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);

  return MakeGarbageCollected<StyleRulePage>(
      selector_list,
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleProperty* CSSParserImpl::ConsumePropertyRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd()) {
    return nullptr;
  }
  if (!CSSVariableParser::IsValidVariableName(name_token)) {
    if (observer_) {
      observer_->ObserveErroneousAtRule(prelude_offset_start,
                                        CSSAtRuleID::kCSSAtRuleProperty);
    }
    return nullptr;
  }
  String name = name_token.Value().ToString();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kProperty, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kProperty, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);
  StyleRuleProperty* rule = MakeGarbageCollected<StyleRuleProperty>(
      name,
      CreateCSSPropertyValueSet(parsed_properties_, kCSSPropertyRuleMode));

  absl::optional<CSSSyntaxDefinition> syntax =
      PropertyRegistration::ConvertSyntax(rule->GetSyntax());
  absl::optional<bool> inherits =
      PropertyRegistration::ConvertInherits(rule->Inherits());
  absl::optional<const CSSValue*> initial =
      syntax.has_value() ? PropertyRegistration::ConvertInitial(
                               rule->GetInitialValue(), *syntax, *context_)
                         : absl::nullopt;

  bool invalid_rule =
      !syntax.has_value() || !inherits.has_value() || !initial.has_value();

  if (observer_ && invalid_rule) {
    Vector<CSSPropertyID, 2> failed_properties;
    if (!syntax.has_value()) {
      failed_properties.push_back(CSSPropertyID::kSyntax);
    }
    if (!inherits.has_value()) {
      failed_properties.push_back(CSSPropertyID::kInherits);
    }
    if (!initial.has_value() && syntax.has_value()) {
      failed_properties.push_back(CSSPropertyID::kInitialValue);
    }
    DCHECK(!failed_properties.empty());
    observer_->ObserveErroneousAtRule(prelude_offset_start,
                                      CSSAtRuleID::kCSSAtRuleProperty,
                                      failed_properties);
  }
  if (invalid_rule) {
    return nullptr;
  }
  return rule;
}

StyleRuleCounterStyle* CSSParserImpl::ConsumeCounterStyleRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  AtomicString name = css_parsing_utils::ConsumeCounterStyleNameInPrelude(
      prelude, *GetContext());
  if (!name) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kCounterStyle, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kCounterStyle,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);
  return MakeGarbageCollected<StyleRuleCounterStyle>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleFontPaletteValues* CSSParserImpl::ConsumeFontPaletteValuesRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd()) {
    return nullptr;
  }

  if (!css_parsing_utils::IsDashedIdent(name_token)) {
    return nullptr;
  }
  AtomicString name = name_token.Value().ToAtomicString();
  if (!name) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontPaletteValues,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kFontPaletteValues,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);
  return MakeGarbageCollected<StyleRuleFontPaletteValues>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleBase* CSSParserImpl::ConsumeScopeRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  DCHECK(RuntimeEnabledFeatures::CSSScopeEnabled());

  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kScope, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  auto* style_scope = StyleScope::Parse(prelude, context_, nesting_type,
                                        parent_rule_for_nesting, style_sheet_);
  if (!style_scope) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      CSSNestingType::kScope, style_scope->RuleForNesting(), &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return MakeGarbageCollected<StyleRuleScope>(*style_scope, std::move(rules));
}

StyleRuleViewTransitions* CSSParserImpl::ConsumeViewTransitionsRule(
    CSSParserTokenStream& stream) {
  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  [[maybe_unused]] CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }

  if (!prelude.AtEnd()) {
    return nullptr;  // Parse error; @view-transitions prelude should be empty
  }

  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kViewTransitions,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kViewTransitions,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);

  return MakeGarbageCollected<StyleRuleViewTransitions>(
      *CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleContainer* CSSParserImpl::ConsumeContainerRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kContainer, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ContainerQueryParser query_parser(*context_);

  CSSParserTokenOffsets offsets = ReparseForOffsets(
      stream.StringRangeAt(prelude_offset_start,
                           prelude_offset_end - prelude_offset_start),
      prelude);

  // <container-name>
  AtomicString name;
  if (prelude.Peek().GetType() == kIdentToken) {
    auto* ident = DynamicTo<CSSCustomIdentValue>(
        css_parsing_utils::ConsumeSingleContainerName(prelude, *context_));
    if (ident) {
      name = ident->Value();
    }
  }

  const MediaQueryExpNode* query =
      query_parser.ParseCondition(prelude, offsets);
  if (!query) {
    return nullptr;
  }
  ContainerQuery* container_query = MakeGarbageCollected<ContainerQuery>(
      ContainerSelector(std::move(name), *query), query);

  if (observer_) {
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      nesting_type, parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  // NOTE: There will be a copy of rules here, to deal with the different inline
  // size.
  return MakeGarbageCollected<StyleRuleContainer>(*container_query,
                                                  std::move(rules));
}

StyleRuleBase* CSSParserImpl::ConsumeLayerRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();

  // @layer statement rule without style declarations.
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(stream)) {
      return nullptr;
    }
    if (nesting_type == CSSNestingType::kNesting) {
      // @layer statement rules are not group rules, and can therefore
      // not be nested.
      //
      // https://drafts.csswg.org/css-nesting-1/#nested-group-rules
      return nullptr;
    }

    Vector<StyleRuleBase::LayerName> names;
    while (!prelude.AtEnd()) {
      if (names.size()) {
        if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(prelude)) {
          return nullptr;
        }
      }
      StyleRuleBase::LayerName name = ConsumeCascadeLayerName(prelude);
      if (!name.size()) {
        return nullptr;
      }
      names.push_back(std::move(name));
    }
    if (!names.size()) {
      return nullptr;
    }

    if (observer_) {
      observer_->StartRuleHeader(StyleRule::kLayerStatement,
                                 prelude_offset_start);
      observer_->EndRuleHeader(prelude_offset_end);
      observer_->StartRuleBody(prelude_offset_end);
      observer_->EndRuleBody(prelude_offset_end);
    }

    return MakeGarbageCollected<StyleRuleLayerStatement>(std::move(names));
  }

  // @layer block rule with style declarations.
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  StyleRuleBase::LayerName name;
  prelude.ConsumeWhitespace();
  if (prelude.AtEnd()) {
    name.push_back(g_empty_atom);
  } else {
    name = ConsumeCascadeLayerName(prelude);
    if (!name.size() || !prelude.AtEnd()) {
      return nullptr;
    }
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kLayerBlock, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(
      stream,
      /* is_nested_group_rule */ nesting_type == CSSNestingType::kNesting,
      nesting_type, parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return MakeGarbageCollected<StyleRuleLayerBlock>(std::move(name),
                                                   std::move(rules));
}

StyleRulePositionFallback* CSSParserImpl::ConsumePositionFallbackRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  const CSSParserToken& name_token = prelude.ConsumeIncludingWhitespace();
  if (!prelude.AtEnd()) {
    return nullptr;
  }

  // <dashed-ident>, and -internal-* for UA sheets only.
  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
    if (!name.StartsWith("--") &&
        !(context_->Mode() == kUASheetMode && name.StartsWith("-internal-"))) {
      return nullptr;
    }
  } else {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPositionFallback,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  const bool has_max_length = context_->Mode() != kUASheetMode;
  HeapVector<Member<StyleRuleBase>> try_rules;
  ConsumeRuleList(
      stream, kPositionFallbackRuleList, CSSNestingType::kNone,
      /*parent_rule_for_nesting=*/nullptr,
      [&try_rules, has_max_length](StyleRuleBase* try_rule, wtf_size_t) {
        if (!has_max_length ||
            try_rules.size() < kPositionFallbackRuleMaxLength) {
          try_rules.push_back(try_rule);
        }
      });

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return MakeGarbageCollected<StyleRulePositionFallback>(AtomicString(name),
                                                         std::move(try_rules));
}

StyleRuleTry* CSSParserImpl::ConsumeTryRule(CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSParserTokenRange prelude = ConsumeAtRulePrelude(stream);
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  prelude.ConsumeWhitespace();
  if (!prelude.AtEnd()) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kTry, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
  }

  ConsumeDeclarationList(stream, StyleRule::kTry, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);
  return MakeGarbageCollected<StyleRuleTry>(
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
}

StyleRuleKeyframe* CSSParserImpl::ConsumeKeyframeStyleRule(
    const CSSParserTokenRange prelude,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  std::unique_ptr<Vector<KeyframeOffset>> key_list =
      ConsumeKeyframeKeyList(context_, prelude);
  if (!key_list) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframe, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
  }

  ConsumeDeclarationList(block, StyleRule::kKeyframe, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*child_rules=*/nullptr);

  return MakeGarbageCollected<StyleRuleKeyframe>(
      std::move(key_list),
      CreateCSSPropertyValueSet(parsed_properties_, kCSSKeyframeRuleMode));
}

// A (hopefully) fast check for whether the given declaration block could
// contain nested CSS rules. All of these have to involve { in some shape
// or form, so we simply check for the existence of that. (It means we will
// have false positives for e.g. { within comments or strings, but this
// only means we will turn off lazy parsing for that rule, nothing worse.)
// This will work even for UTF-16, although with some more false positives
// with certain Unicode characters such as U+017E (LATIN SMALL LETTER Z
// WITH CARON). This is, again, not a big problem for us.
static bool MayContainNestedRules(const String& text,
                                  wtf_size_t offset,
                                  wtf_size_t length) {
  if (length < 2u) {
    // {} is the shortest possible block (but if there's
    // a lone { and then EOF, we will be called with length 1).
    return false;
  }

  size_t char_size = text.Is8Bit() ? sizeof(LChar) : sizeof(UChar);

  // Strip away the outer {} pair (the { would always give us a false positive).
  DCHECK_EQ(text[offset], '{');
  if (text[offset + length - 1] != '}') {
    // EOF within the block, so just be on the safe side
    // and use the normal (non-lazy) code path.
    return true;
  }
  ++offset;
  length -= 2;

  return memchr(
             reinterpret_cast<const char*>(text.Bytes()) + offset * char_size,
             '{', length * char_size) != nullptr;
}

StyleRule* CSSParserImpl::ConsumeStyleRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    bool semicolon_aborts_nested_selector) {
  if (!in_nested_style_rule_) {
    DCHECK_EQ(0u, arena_.size());
  }
  auto func_clear_arena = [&](HeapVector<CSSSelector>* arena) {
    if (!in_nested_style_rule_) {
      arena->resize(0);  // See class comment on CSSSelectorParser.
    }
  };
  std::unique_ptr<HeapVector<CSSSelector>, decltype(func_clear_arena)>
      scope_guard(&arena_, std::move(func_clear_arena));

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kStyle, stream.LookAheadOffset());
  }

  // Style rules that look like custom property declarations
  // are not allowed by css-syntax.
  //
  // https://drafts.csswg.org/css-syntax/#consume-qualified-rule
  bool custom_property_ambiguity = false;
  if (RuntimeEnabledFeatures::CSSNestingIdentEnabled() &&
      CSSVariableParser::IsValidVariableName(stream.Peek())) {
    CSSParserTokenStream::State state = stream.Save();
    stream.ConsumeIncludingWhitespace();  // <ident>
    custom_property_ambiguity = stream.Peek().GetType() == kColonToken;
    stream.Restore(state);
  }

  // Parse the prelude of the style rule
  base::span<CSSSelector> selector_vector = CSSSelectorParser::ConsumeSelector(
      stream, context_, nesting_type, parent_rule_for_nesting,
      semicolon_aborts_nested_selector, style_sheet_, observer_, arena_);

  if (selector_vector.empty()) {
    // Read the rest of the prelude if there was an error
    stream.EnsureLookAhead();
    while (!stream.UncheckedAtEnd() &&
           stream.UncheckedPeek().GetType() != kLeftBraceToken &&
           !AbortsNestedSelectorParsing(stream.UncheckedPeek().GetType(),
                                        semicolon_aborts_nested_selector,
                                        nesting_type)) {
      stream.UncheckedConsumeComponentValue();
    }
  }

  if (observer_) {
    observer_->EndRuleHeader(stream.LookAheadOffset());
  }

  if (stream.AtEnd() || AbortsNestedSelectorParsing(
                            stream.UncheckedPeek().GetType(),
                            semicolon_aborts_nested_selector, nesting_type)) {
    // Parse error, EOF instead of qualified rule block
    // (or we went into error recovery above).
    // NOTE: If we aborted due to a semicolon, don't consume it here;
    // the caller will do that for us.
    return nullptr;
  }

  DCHECK_EQ(stream.Peek().GetType(), kLeftBraceToken);
  CSSParserTokenStream::BlockGuard guard(stream);

  if (selector_vector.empty()) {
    // Parse error, invalid selector list.
    return nullptr;
  }
  if (custom_property_ambiguity) {
    return nullptr;
  }

  // TODO(csharrison): How should we lazily parse css that needs the observer?
  if (!observer_ && lazy_state_) {
    DCHECK(style_sheet_);

    wtf_size_t block_start_offset = stream.Offset() - 1;  // - 1 for the {.
    guard.SkipToEndOfBlock();
    wtf_size_t block_length = stream.Offset() - block_start_offset;

    // Lazy parsing cannot deal with nested rules. We make a very quick check
    // to see if there could possibly be any in there; if so, we need to go
    // back to normal (non-lazy) parsing. If that happens, we've wasted some
    // work; specifically, the SkipToEndOfBlock(), and potentially that we
    // cannot use the CachedCSSTokenizer if that would otherwise be in use.
    if (RuntimeEnabledFeatures::CSSNestingEnabled() &&
        MayContainNestedRules(lazy_state_->SheetText(), block_start_offset,
                              block_length)) {
      CSSTokenizer tokenizer(lazy_state_->SheetText(), block_start_offset);
      CSSParserTokenStream block_stream(tokenizer);
      CSSParserTokenStream::BlockGuard sub_guard(
          block_stream);  // Consume the {, and open the block stack.
      return ConsumeStyleRuleContents(selector_vector, block_stream);
    }

    return StyleRule::Create(selector_vector,
                             MakeGarbageCollected<CSSLazyPropertyParserImpl>(
                                 block_start_offset, lazy_state_));
  }
  return ConsumeStyleRuleContents(selector_vector, stream);
}

StyleRule* CSSParserImpl::ConsumeStyleRuleContents(
    base::span<CSSSelector> selector_vector,
    CSSParserTokenStream& stream) {
  StyleRule* style_rule = StyleRule::Create(selector_vector);
  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  ConsumeDeclarationList(stream, StyleRule::kStyle, CSSNestingType::kNesting,
                         /*parent_rule_for_nesting=*/style_rule, &child_rules);
  for (StyleRuleBase* child_rule : child_rules) {
    style_rule->AddChildRule(child_rule);
  }
  style_rule->SetProperties(
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode()));
  return style_rule;
}

// This function is used for two different but very similarly specified actions
// in [css-syntax-3], namely “parse a list of declarations” (used for style
// attributes, @page rules and a few other things) and “consume a style block's
// contents” (used for the interior of rules, such as in a normal stylesheet).
// The only real difference between the two is that the latter cannot contain
// nested rules. In particular, both have the effective behavior that when
// seeing something that is not an ident and is not a valid selector, we should
// skip to the next semicolon. (For “consume a style block's contents”, this is
// explicit, and for “parse a list of declarations”, it happens due to
// synchronization behavior. Of course, for the latter case, a _valid_ selector
// would get the same skipping behavior.)
//
// So as the spec stands, we can unify these cases; we use
// parent_rule_for_nesting as a marker for which case we are in (see [1]).
// If it's nullptr, we're parsing a declaration list and not a style block,
// so non-idents should not begin consuming qualified rules. See also
// AbortsNestedSelectorParsing(), which uses parent_rule_for_nesting to check
// whether semicolons should abort parsing (the prelude of) qualified rules;
// if semicolons always aborted such parsing, we wouldn't need this distinction.
void CSSParserImpl::ConsumeDeclarationList(
    CSSParserTokenStream& stream,
    StyleRule::RuleType rule_type,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    HeapVector<Member<StyleRuleBase>, 4>* child_rules) {
  DCHECK(parsed_properties_.empty());

  bool is_observer_rule_type =
      rule_type == StyleRule::kStyle || rule_type == StyleRule::kProperty ||
      rule_type == StyleRule::kContainer ||
      rule_type == StyleRule::kCounterStyle ||
      rule_type == StyleRule::kFontPaletteValues ||
      rule_type == StyleRule::kKeyframe || rule_type == StyleRule::kScope ||
      rule_type == StyleRule::kViewTransitions || rule_type == StyleRule::kTry;
  bool use_observer = observer_ && is_observer_rule_type;
  if (use_observer) {
    observer_->StartRuleBody(stream.Offset());
  }

  while (true) {
    // Having a lookahead may skip comments, which are used by the observer.
    DCHECK(!stream.HasLookAhead() || stream.AtEnd());

    if (use_observer && !stream.HasLookAhead()) {
      while (true) {
        wtf_size_t start_offset = stream.Offset();
        if (!stream.ConsumeCommentOrNothing()) {
          break;
        }
        observer_->ObserveComment(start_offset, stream.Offset());
      }
    }

    if (stream.AtEnd()) {
      break;
    }

    switch (stream.UncheckedPeek().GetType()) {
      case kWhitespaceToken:
      case kSemicolonToken:
        stream.UncheckedConsume();
        break;
      case kAtKeywordToken:
        if (RuntimeEnabledFeatures::CSSNestingEnabled()) {
          CSSParserToken name_token = stream.ConsumeIncludingWhitespace();
          const StringView name = name_token.Value();
          const CSSAtRuleID id = CssAtRuleID(name);
          StyleRuleBase* child = ConsumeNestedRule(id, stream, nesting_type,
                                                   parent_rule_for_nesting);
          if (child && child_rules) {
            child_rules->push_back(child);
          }
          break;
        }
        // Consume the remainder of the declaration (if any) for error
        // recovery.
        // TODO(sesse): This is largely untested; we need WPT tests
        // for error recovery, once the syntax has settled.
        ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleInvalid);
        break;
      case kIdentToken: {
        CSSParserTokenStream::State state = stream.Save();
        bool consumed_declaration = false;
        {
          CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
          consumed_declaration = ConsumeDeclaration(stream, rule_type);
        }
        if (consumed_declaration) {
          if (!stream.AtEnd()) {
            DCHECK_EQ(stream.UncheckedPeek().GetType(), kSemicolonToken);
            stream.UncheckedConsume();  // kSemicolonToken
          }
          if (child_rules && !child_rules->empty()) {
            // https://github.com/w3c/csswg-drafts/issues/8738
            context_->Count(WebFeature::kCSSDeclarationAfterNestedRule);
          }
          break;
        } else if (!RuntimeEnabledFeatures::CSSNestingIdentEnabled() ||
                   stream.UncheckedPeek().GetType() == kSemicolonToken) {
          // Recover from an error when CSSNestingIdent is not enabled.
          // When CSSNestingIdent is enabled, we would instead normally Restore
          // the stream and retry as a nested style rule, but as an optimization
          // we avoid this restart if we ended on a kSemicolonToken, as this situation
          // can't produce a valid rule.
          stream.ConsumeUntilPeekedTypeIs<kSemicolonToken>();
          if (!stream.AtEnd()) {
            stream.UncheckedConsume();  // kSemicolonToken
          }
          break;
        }
        // Retry as nested rule.
        stream.Restore(state);
        [[fallthrough]];
      }
      default:
        if (RuntimeEnabledFeatures::CSSNestingEnabled() &&
            parent_rule_for_nesting != nullptr) {  // [1] (see function comment)
          StyleRuleBase* child = ConsumeNestedRule(
              absl::nullopt, stream, nesting_type, parent_rule_for_nesting);
          if (child) {
            if (child_rules) {
              child_rules->push_back(child);
            }
            break;
          }
          // Fall through to error recovery.
          stream.EnsureLookAhead();
        }

        [[fallthrough]];
        // Function tokens should start parsing a declaration
        // (which then immediately goes into error recovery mode).
      case CSSParserTokenType::kFunctionToken:
        while (!stream.UncheckedAtEnd() &&
               stream.UncheckedPeek().GetType() != kSemicolonToken) {
          stream.UncheckedConsumeComponentValue();
        }

        if (!stream.UncheckedAtEnd()) {
          stream.UncheckedConsume();  // kSemicolonToken
        }

        break;
    }
  }

  if (use_observer) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }
}

// Consumes a list of style rules and stores the result in `child_rules`,
// or (if `is_nested_group_rule` is true) consumes the interior of a nested
// group rule [1]. Nested group rules allow a list of declarations to appear
// directly in place of where a list of rules would normally go.
//
// [1] https://drafts.csswg.org/css-nesting-1/#nested-group-rules
void CSSParserImpl::ConsumeRuleListOrNestedDeclarationList(
    CSSParserTokenStream& stream,
    bool is_nested_group_rule,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    HeapVector<Member<StyleRuleBase>, 4>* child_rules) {
  DCHECK(child_rules);

  if (RuntimeEnabledFeatures::CSSNestingEnabled() && is_nested_group_rule) {
    // This is a nested group rule, which allows *declarations* to appear
    // directly within the body of the rule, e.g.:
    //
    // .foo {
    //    @media (width > 800px) {
    //      color: green;
    //    }
    //  }
    //
    // Note that nested group rules may also contain *rules* within its body.
    // This is handled by `ConsumeDeclarationList`, see comment near that
    // function.
    if (observer_) {
      // Observe an empty rule header to ensure the observer has a new rule data
      // on the stack for the following ConsumeDeclarationList.
      observer_->StartRuleHeader(StyleRule::kStyle, stream.Offset());
      observer_->EndRuleHeader(stream.Offset());
    }
    ConsumeDeclarationList(stream, StyleRule::kStyle, nesting_type,
                           parent_rule_for_nesting, child_rules);
    if (!parsed_properties_.empty()) {
      child_rules->push_front(
          CreateImplicitNestedRule(nesting_type, parent_rule_for_nesting));
    }
  } else {
    ConsumeRuleList(stream, kRegularRuleList, nesting_type,
                    parent_rule_for_nesting,
                    [child_rules](StyleRuleBase* rule, wtf_size_t) {
                      child_rules->push_back(rule);
                    });
  }
}

StyleRuleBase* CSSParserImpl::ConsumeNestedRule(
    absl::optional<CSSAtRuleID> id,
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  DCHECK(RuntimeEnabledFeatures::CSSNestingEnabled());

  // A nested style rule. Recurse into the parser; we need to move the parsed
  // properties out of the way while we're parsing the child rule, though.
  // TODO(sesse): The spec says that any properties after a nested rule
  // should be ignored. We don't support this yet.
  // See https://github.com/w3c/csswg-drafts/issues/7501.
  HeapVector<CSSPropertyValue, 64> outer_parsed_properties;
  swap(parsed_properties_, outer_parsed_properties);
  StyleRuleBase* child;
  if (!id.has_value()) {
    base::AutoReset<bool> reset_in_nested_style_rule(&in_nested_style_rule_,
                                                     true);
    child = ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                             /* semicolon_aborts_nested_selector */ true);
  } else {
    child = ConsumeAtRuleContents(*id, stream, kNestedGroupRules, nesting_type,
                                  parent_rule_for_nesting);
  }
  parsed_properties_ = std::move(outer_parsed_properties);
  context_->Count(WebFeature::kCSSNesting);
  return child;
}

// This function can leave the stream in one of the following states:
//
//  1) If the ident token is not immediately followed by kColonToken,
//     then the stream is left at the token where kColonToken was expected.
//  2) If the ident token is not a recognized property/descriptor,
//     then the stream is left at the token immediately after kColonToken.
//  3) Otherwise the stream is is left AtEnd(), regardless of whether or
//     not the value was valid.
//
// Leaving the stream in an awkward states is normally not desirable for
// Consume functions, but declarations are sometimes parsed speculatively,
// which may cause a restart at the call site (see ConsumeDeclarationList,
// kIdentToken branch). If we are anyway going to restart, any work we do
// to leave the stream in a more consistent state is just wasted.
bool CSSParserImpl::ConsumeDeclaration(CSSParserTokenStream& stream,
                                       StyleRule::RuleType rule_type) {
  const wtf_size_t decl_offset_start = stream.Offset();

  DCHECK_EQ(stream.Peek().GetType(), kIdentToken);
  const CSSParserToken& lhs = stream.ConsumeIncludingWhitespace();
  if (stream.Peek().GetType() != kColonToken) {
    return false;  // Parse error.
  }

  stream.UncheckedConsume();  // kColonToken
  stream.EnsureLookAhead();

  size_t properties_count = parsed_properties_.size();

  bool parsing_descriptor = rule_type == StyleRule::kFontFace ||
                            rule_type == StyleRule::kFontPaletteValues ||
                            rule_type == StyleRule::kProperty ||
                            rule_type == StyleRule::kCounterStyle ||
                            rule_type == StyleRule::kViewTransitions;

  uint64_t id = parsing_descriptor
                    ? static_cast<uint64_t>(lhs.ParseAsAtRuleDescriptorID())
                    : static_cast<uint64_t>(lhs.ParseAsUnresolvedCSSPropertyID(
                          context_->GetExecutionContext(), context_->Mode()));

  bool important = false;

  static_assert(static_cast<uint64_t>(AtRuleDescriptorID::Invalid) == 0u);
  static_assert(static_cast<uint64_t>(CSSPropertyID::kInvalid) == 0u);

  stream.ConsumeWhitespace();

  if (id) {
    if (parsing_descriptor) {
      CSSTokenizedValue tokenized_value =
          ConsumeUnrestrictedPropertyValue(stream);
      important = RemoveImportantAnnotationIfPresent(tokenized_value);
      if (important) {
        return false;  // Invalid for descriptors.
      }
      const AtRuleDescriptorID atrule_id = static_cast<AtRuleDescriptorID>(id);
      AtRuleDescriptorParser::ParseAtRule(rule_type, atrule_id, tokenized_value,
                                          *context_, parsed_properties_);
    } else {
      const CSSPropertyID unresolved_property = static_cast<CSSPropertyID>(id);
      if (unresolved_property == CSSPropertyID::kVariable) {
        if (rule_type != StyleRule::kStyle &&
            rule_type != StyleRule::kKeyframe) {
          return false;
        }
        CSSTokenizedValue tokenized_value =
            ConsumeUnrestrictedPropertyValue(stream);
        important = RemoveImportantAnnotationIfPresent(tokenized_value);
        if (important && (rule_type == StyleRule::kKeyframe)) {
          return false;
        }
        AtomicString variable_name = lhs.Value().ToAtomicString();
        bool is_animation_tainted = rule_type == StyleRule::kKeyframe;
        ConsumeVariableValue(tokenized_value, variable_name, important,
                             is_animation_tainted);
      } else if (unresolved_property != CSSPropertyID::kInvalid) {
        CSSTokenizedValue tokenized_value =
            ConsumeRestrictedPropertyValue(stream);
        important = RemoveImportantAnnotationIfPresent(tokenized_value);
        if (important && (rule_type == StyleRule::kKeyframe ||
                          rule_type == StyleRule::kTry)) {
          return false;
        }
        if (stream.AtEnd()) {
          ConsumeDeclarationValue(tokenized_value, unresolved_property,
                                  important, rule_type);
        }
      }
    }
  }

  if (observer_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kKeyframe ||
       rule_type == StyleRule::kProperty || rule_type == StyleRule::kTry)) {
    if (!id) {
      // If we skipped the main call to ConsumeValue due to an invalid
      // property/descriptor, the inspector still needs to know the offset
      // where the would-be declaration ends.
      CSSTokenizedValue tokenized_value =
          ConsumeRestrictedPropertyValue(stream);
      important = RemoveImportantAnnotationIfPresent(tokenized_value);
    }
    // The end offset is the offset of the terminating token, which is peeked
    // but not yet consumed.
    observer_->ObserveProperty(decl_offset_start, stream.LookAheadOffset(),
                               important,
                               parsed_properties_.size() != properties_count);
  }

  return parsed_properties_.size() != properties_count;
}

void CSSParserImpl::ConsumeVariableValue(
    const CSSTokenizedValue& tokenized_value,
    const AtomicString& variable_name,
    bool important,
    bool is_animation_tainted) {
  if (CSSValue* value = CSSVariableParser::ParseDeclarationIncludingCSSWide(
          tokenized_value, is_animation_tainted, *context_)) {
    parsed_properties_.push_back(
        CSSPropertyValue(CSSPropertyName(variable_name), *value, important));
    context_->Count(context_->Mode(), CSSPropertyID::kVariable);
  }
}

// NOTE: Leading whitespace must be stripped from tokenized_value, since
// ParseValue() has the same requirement.
void CSSParserImpl::ConsumeDeclarationValue(
    const CSSTokenizedValue& tokenized_value,
    CSSPropertyID unresolved_property,
    bool important,
    StyleRule::RuleType rule_type) {
  CSSPropertyParser::ParseValue(unresolved_property, important, tokenized_value,
                                context_, parsed_properties_, rule_type);
}

template <typename ConsumeFunction>
CSSTokenizedValue CSSParserImpl::ConsumeValue(
    CSSParserTokenStream& stream,
    ConsumeFunction consume_function) {
  // Consume leading whitespace and comments. This is needed
  // by ConsumeDeclarationValue() / CSSPropertyParser::ParseValue(),
  // and also CSSVariableParser::ParseDeclarationIncludingCSSWide().
  stream.ConsumeWhitespace();
  wtf_size_t value_start_offset = stream.LookAheadOffset();
  CSSParserTokenRange range = consume_function(stream);
  wtf_size_t value_end_offset = stream.LookAheadOffset();

  return {range, stream.StringRangeAt(value_start_offset,
                                      value_end_offset - value_start_offset)};
}

CSSTokenizedValue CSSParserImpl::ConsumeRestrictedPropertyValue(
    CSSParserTokenStream& stream) {
  if (!RuntimeEnabledFeatures::CSSNestingIdentEnabled()) {
    return ConsumeUnrestrictedPropertyValue(stream);
  }

  if (stream.Peek().GetType() == kLeftBraceToken) {
    // '{}' must be the whole value, hence we simply consume a component
    // value from the stream, and consider this the whole value.
    return ConsumeValue(stream, [](CSSParserTokenStream& stream) {
      return stream.ConsumeComponentValueIncludingWhitespace();
    });
  }
  // Otherwise, we consume until we're AtEnd() (which in the normal case
  // means we hit a kSemicolonToken), or until we see kLeftBraceToken.
  // The latter is a kind of error state, which is dealt with via additional
  // AtEnd() checks at the call site.
  return ConsumeValue(stream, [](CSSParserTokenStream& stream) {
    return stream.ConsumeUntilPeekedTypeIs<kLeftBraceToken>();
  });
}

CSSTokenizedValue CSSParserImpl::ConsumeUnrestrictedPropertyValue(
    CSSParserTokenStream& stream) {
  return ConsumeValue(stream, [](CSSParserTokenStream& stream) {
    return stream.ConsumeUntilPeekedTypeIs<>();
  });
}

bool CSSParserImpl::RemoveImportantAnnotationIfPresent(
    CSSTokenizedValue& tokenized_value) {
  if (tokenized_value.range.size() == 0) {
    return false;
  }
  const CSSParserToken* first = tokenized_value.range.begin();
  const CSSParserToken* last = tokenized_value.range.end() - 1;
  while (last >= first && last->GetType() == kWhitespaceToken) {
    --last;
  }
  if (last >= first && last->GetType() == kIdentToken &&
      EqualIgnoringASCIICase(last->Value(), "important")) {
    --last;
    while (last >= first && last->GetType() == kWhitespaceToken) {
      --last;
    }
    if (last >= first && last->GetType() == kDelimiterToken &&
        last->Delimiter() == '!') {
      tokenized_value.range = tokenized_value.range.MakeSubRange(first, last);

      // Truncate the text to remove the delimiter and everything after it.
      if (!tokenized_value.text.empty()) {
        DCHECK_NE(tokenized_value.text.ToString().find('!'), kNotFound);
        unsigned truncated_length = tokenized_value.text.length() - 1;
        while (tokenized_value.text[truncated_length] != '!') {
          --truncated_length;
        }
        tokenized_value.text =
            StringView(tokenized_value.text, 0, truncated_length);
      }
      return true;
    }
  }

  return false;
}

std::unique_ptr<Vector<KeyframeOffset>> CSSParserImpl::ConsumeKeyframeKeyList(
    const CSSParserContext* context,
    CSSParserTokenRange range) {
  std::unique_ptr<Vector<KeyframeOffset>> result =
      std::make_unique<Vector<KeyframeOffset>>();
  while (true) {
    range.ConsumeWhitespace();
    const CSSParserToken& token = range.Peek();
    if (token.GetType() == kPercentageToken && token.NumericValue() >= 0 &&
        token.NumericValue() <= 100) {
      result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone,
                                       token.NumericValue() / 100));
      range.ConsumeIncludingWhitespace();
    } else if (token.GetType() == kIdentToken) {
      if (EqualIgnoringASCIICase(token.Value(), "from")) {
        result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone, 0));
        range.ConsumeIncludingWhitespace();
      } else if (EqualIgnoringASCIICase(token.Value(), "to")) {
        result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone, 1));
        range.ConsumeIncludingWhitespace();
      } else {
        auto* range_name_percent = To<CSSValueList>(
            css_parsing_utils::ConsumeTimelineRangeNameAndPercent(range,
                                                                  *context));
        if (!range_name_percent) {
          return nullptr;
        }

        auto range_name = To<CSSIdentifierValue>(range_name_percent->Item(0))
                              .ConvertTo<TimelineOffset::NamedRange>();
        auto percent =
            To<CSSPrimitiveValue>(range_name_percent->Item(1)).GetFloatValue();

        if (!RuntimeEnabledFeatures::ScrollTimelineEnabled() &&
            range_name != TimelineOffset::NamedRange::kNone) {
          return nullptr;
        }

        result->push_back(KeyframeOffset(range_name, percent / 100.0));
      }
    } else {
      return nullptr;
    }

    if (range.AtEnd()) {
      return result;
    }
    if (range.Consume().GetType() != kCommaToken) {
      return nullptr;  // Parser error
    }
  }
}

const MediaQuerySet* CSSParserImpl::CachedMediaQuerySet(
    String prelude_string,
    CSSParserTokenRange prelude,
    const CSSParserTokenOffsets& offsets) {
  Member<const MediaQuerySet>& media =
      media_query_cache_.insert(prelude_string, nullptr).stored_value->value;
  if (!media) {
    media = MediaQueryParser::ParseMediaQuerySet(
        prelude, offsets, context_->GetExecutionContext());
  }
  DCHECK(media);
  return media.Get();
}

}  // namespace blink
