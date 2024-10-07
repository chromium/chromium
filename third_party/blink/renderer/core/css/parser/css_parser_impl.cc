// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include <bitset>
#include <limits>
#include <memory>
#include <utility>

#include "base/cpu.h"
#include "third_party/blink/renderer/core/animation/timeline_offset.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_try_rule.h"
#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
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
#include "third_party/blink/renderer/core/css/parser/find_length_of_declaration_list-inl.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame_ukm_aggregator.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

using std::swap;

namespace blink {

namespace {

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
    stream.ConsumeWhitespace();
    // If the block doesn't start with a quote, then the tokenizer
    // would return a kUrlToken or kBadUrlToken instead of a
    // kFunctionToken. Note also that this Peek() placates the
    // DCHECK that we Peek() before Consume().
    DCHECK(stream.Peek().GetType() == kStringToken ||
           stream.Peek().GetType() == kBadStringToken)
        << "Got unexpected token " << stream.Peek();
    const CSSParserToken& uri = stream.ConsumeIncludingWhitespace();
    if (uri.GetType() != kBadStringToken && stream.UncheckedAtEnd()) {
      DCHECK_EQ(uri.GetType(), kStringToken);
      result = uri.Value().ToAtomicString();
    }
  }
  stream.ConsumeWhitespace();
  return result;
}

// Finds the longest prefix of |stream| that matches a <layer-name> and parses
// it. Returns an empty result with |stream| unmodified if parsing fails.
StyleRuleBase::LayerName ConsumeCascadeLayerName(CSSParserTokenStream& stream) {
  CSSParserTokenStream::State savepoint = stream.Save();
  StyleRuleBase::LayerName name;
  while (!stream.AtEnd() && stream.Peek().GetType() == kIdentToken) {
    const CSSParserToken& name_part = stream.Consume();
    name.emplace_back(name_part.Value().ToString());

    // Check if we have a next part.
    if (stream.Peek().GetType() != kDelimiterToken ||
        stream.Peek().Delimiter() != '.') {
      break;
    }
    CSSParserTokenStream::State inner_savepoint = stream.Save();
    stream.Consume();
    if (stream.Peek().GetType() != kIdentToken) {
      stream.Restore(inner_savepoint);
      break;
    }
  }

  if (!name.size()) {
    stream.Restore(savepoint);
  } else {
    stream.ConsumeWhitespace();
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
    case kCSSFontPaletteValuesRuleMode:
      return StyleRule::kFontPaletteValues;
    case kCSSPositionTryRuleMode:
      return StyleRule::kPositionTry;
    default:
      return StyleRule::kStyle;
  }
}

std::optional<StyleRuleFontFeature::FeatureType> ToStyleRuleFontFeatureType(
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
      NOTREACHED_IN_MIGRATION();
  }
  return std::nullopt;
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
  CSSParserTokenStream stream(string);
  parser.ConsumeDeclarationValue(stream, unresolved_property,
                                 /*is_in_declaration_list=*/false, rule_type);
  if (parser.parsed_properties_.empty()) {
    return MutableCSSPropertyValueSet::kParseError;
  }
  if (important) {
    for (CSSPropertyValue& property : parser.parsed_properties_) {
      property.SetImportant();
    }
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
  CSSParserTokenStream stream(value);
  if (!parser.ConsumeVariableValue(stream, property_name,
                                   /*allow_important_annotation=*/false,
                                   is_animation_tainted)) {
    return MutableCSSPropertyValueSet::kParseError;
  }
  if (important) {
    parser.parsed_properties_.back().SetImportant();
  }
  return declaration->AddParsedProperties(parser.parsed_properties_);
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
    CSSParserMode mode,
    const Document* document) {
  if (mode != kHTMLQuirksMode &&
      (parsed_properties.size() < 2 ||
       (parsed_properties.size() == 2 &&
        parsed_properties[0].Id() != parsed_properties[1].Id()))) {
    // Fast path for the situations where we can trivially detect that there can
    // be no collision between properties, and don't need to reorder, make
    // bitsets, or similar.
    ImmutableCSSPropertyValueSet* result = ImmutableCSSPropertyValueSet::Create(
        parsed_properties.data(), parsed_properties.size(), mode);
    parsed_properties.clear();
    return result;
  }

  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parsed_properties.size();
  HeapVector<CSSPropertyValue, 64> results(unused_entries);
  HashSet<AtomicString> seen_custom_properties;

  FilterProperties(true, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);
  FilterProperties(false, parsed_properties, results, unused_entries,
                   seen_properties, seen_custom_properties);

  bool count_cursor_hand = false;
  if (document && mode == kHTMLQuirksMode &&
      seen_properties.test(GetCSSPropertyIDIndex(CSSPropertyID::kCursor))) {
    // See if the properties contain “cursor: hand” without also containing
    // “cursor: pointer”. This is a reasonable approximation for whether
    // removing support for the former would actually matter. (Of course,
    // we don't check whether “cursor: hand” could lose in the cascade
    // due to properties coming from other declarations, but that would be
    // much more complicated)
    bool contains_cursor_hand = false;
    bool contains_cursor_pointer = false;
    for (const CSSPropertyValue& property : parsed_properties) {
      const CSSIdentifierValue* value =
          DynamicTo<CSSIdentifierValue>(property.Value());
      if (value) {
        if (value->WasQuirky()) {
          contains_cursor_hand = true;
        } else if (value->GetValueID() == CSSValueID::kPointer) {
          contains_cursor_pointer = true;
        }
      }
    }
    if (contains_cursor_hand && !contains_cursor_pointer) {
      document->CountUse(WebFeature::kQuirksModeCursorHand);
      count_cursor_hand = true;
    }
  }

  ImmutableCSSPropertyValueSet* result = ImmutableCSSPropertyValueSet::Create(
      results.data() + unused_entries, results.size() - unused_entries, mode,
      count_cursor_hand);
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
  CSSParserTokenStream stream(string);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*nested_declarations_start_index=*/kNotFound,
                                /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, mode, &document);
}

ImmutableCSSPropertyValueSet* CSSParserImpl::ParseInlineStyleDeclaration(
    const String& string,
    CSSParserMode parser_mode,
    SecureContextMode secure_context_mode,
    const Document* document) {
  auto* context =
      MakeGarbageCollected<CSSParserContext>(parser_mode, secure_context_mode);
  CSSParserImpl parser(context);
  CSSParserTokenStream stream(string);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*nested_declarations_start_index=*/kNotFound,
                                /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, parser_mode,
                                   document);
}

bool CSSParserImpl::ParseDeclarationList(
    MutableCSSPropertyValueSet* declaration,
    const String& string,
    const CSSParserContext* context) {
  CSSParserImpl parser(context);
  StyleRule::RuleType rule_type = RuleTypeForMutableDeclaration(declaration);
  CSSParserTokenStream stream(string);
  // See function declaration comment for why parent_rule_for_nesting ==
  // nullptr.
  parser.ConsumeDeclarationList(stream, rule_type, CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*nested_declarations_start_index=*/kNotFound,
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

StyleRuleBase* CSSParserImpl::ParseNestedDeclarationsRule(
    const CSSParserContext* context,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    StringView text) {
  CSSParserImpl parser(context);
  CSSParserTokenStream stream(text);

  HeapVector<Member<StyleRuleBase>, 4> child_rules;

  // Using nested_declarations_start_index=0u causes the leading block
  // of declarations (the only block) to be wrapped in a CSSNestedDeclarations
  // rule.
  //
  // See comment above CSSParserImpl::ConsumeDeclarationList (definition)
  // for more on nested_declarations_start_index.
  parser.ConsumeDeclarationList(stream, StyleRule::RuleType::kStyle,
                                nesting_type, parent_rule_for_nesting,
                                /*nested_declarations_start_index=*/0u,
                                &child_rules);

  return child_rules.size() == 1u ? child_rules.back().Get() : nullptr;
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        CSSNestingType nesting_type,
                                        StyleRule* parent_rule_for_nesting,
                                        StyleSheetContents* style_sheet,
                                        AllowedRulesType allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSParserTokenStream stream(string);
  stream.ConsumeWhitespace();
  if (stream.UncheckedAtEnd()) {
    return nullptr;  // Parse error, empty rule
  }
  StyleRuleBase* rule;
  if (stream.UncheckedPeek().GetType() == kAtKeywordToken) {
    rule = parser.ConsumeAtRule(stream, allowed_rules, CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr);
  } else if (allowed_rules == kPageMarginRules) {
    // Style rules are not allowed inside @page.
    rule = nullptr;
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
  std::optional<LocalFrameUkmAggregator::ScopedUkmHierarchicalTimer> timer;
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
  CSSParserTokenStream stream(string);
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
                   "tokenCount", stream.TokenCount(), "length",
                   string.length());
  return result;
}

// static
CSSSelectorList* CSSParserImpl::ParsePageSelector(
    CSSParserTokenStream& stream,
    StyleSheetContents* style_sheet,
    const CSSParserContext& context) {
  // We only support a small subset of the css-page spec.
  stream.ConsumeWhitespace();
  AtomicString type_selector;
  if (stream.Peek().GetType() == kIdentToken) {
    type_selector = stream.Consume().Value().ToAtomicString();
  }

  AtomicString pseudo;
  if (stream.Peek().GetType() == kColonToken) {
    stream.Consume();
    if (stream.Peek().GetType() != kIdentToken) {
      return nullptr;
    }
    pseudo = stream.Consume().Value().ToAtomicString();
  }

  stream.ConsumeWhitespace();

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
  CSSParserTokenStream stream(key_list);
  std::unique_ptr<Vector<KeyframeOffset>> result =
      ConsumeKeyframeKeyList(context, stream);
  if (stream.AtEnd()) {
    return result;
  } else {
    return nullptr;
  }
}

String CSSParserImpl::ParseCustomPropertyName(StringView name_text) {
  CSSParserTokenStream stream(name_text);
  const CSSParserToken name_token = stream.Peek();
  if (!CSSVariableParser::IsValidVariableName(name_token)) {
    return {};
  }
  stream.ConsumeIncludingWhitespace();
  if (!stream.AtEnd()) {
    return {};
  }
  return name_token.Value().ToString();
}

bool CSSParserImpl::ConsumeSupportsDeclaration(CSSParserTokenStream& stream) {
  DCHECK(parsed_properties_.empty());
  // Even though we might use an observer here, this is just to test if we
  // successfully parse the stream, so we can temporarily remove the observer.
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
  observer.StartRuleHeader(StyleRule::kStyle, 0);
  observer.EndRuleHeader(1);
  CSSParserTokenStream stream(declaration);
  observer.StartRuleBody(stream.Offset());
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*nested_declarations_start_index=*/kNotFound,
                                /*child_rules=*/nullptr);
  observer.EndRuleBody(stream.LookAheadOffset());
}

void CSSParserImpl::ParseStyleSheetForInspector(const String& string,
                                                const CSSParserContext* context,
                                                StyleSheetContents* style_sheet,
                                                CSSParserObserver& observer) {
  CSSParserImpl parser(context, style_sheet);
  parser.observer_ = &observer;
  CSSParserTokenStream stream(string);
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
  CSSParserTokenStream stream(string, offset);
  CSSParserTokenStream::BlockGuard guard(stream);
  CSSParserImpl parser(context);
  parser.ConsumeDeclarationList(stream, StyleRule::kStyle,
                                CSSNestingType::kNone,
                                /*parent_rule_for_nesting=*/nullptr,
                                /*nested_declarations_start_index=*/kNotFound,
                                /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, context->Mode(),
                                   context->GetDocument());
}

static CSSParserImpl::AllowedRulesType ComputeNewAllowedRules(
    CSSParserImpl::AllowedRulesType allowed_rules,
    StyleRuleBase* rule) {
  if (!rule || allowed_rules == CSSParserImpl::kKeyframeRules ||
      allowed_rules == CSSParserImpl::kFontFeatureRules ||
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
    default:
      NOTREACHED_IN_MIGRATION();
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

// Same as ConsumeEndOfPreludeForAtRuleWithBlock() below, but for at-rules
// that don't have a block and are terminated only by semicolon.
bool CSSParserImpl::ConsumeEndOfPreludeForAtRuleWithoutBlock(
    CSSParserTokenStream& stream,
    CSSAtRuleID id) {
  stream.ConsumeWhitespace();
  if (stream.AtEnd()) {
    return true;
  }
  if (stream.UncheckedPeek().GetType() == kSemicolonToken) {
    stream.UncheckedConsume();  // kSemicolonToken
    return true;
  }

  if (observer_) {
    observer_->ObserveErroneousAtRule(stream.Offset(), id);
  }

  // Consume the erroneous block.
  ConsumeErroneousAtRule(stream, id);
  return false;  // Parse error, we expected no block.
}

// Call this after parsing the prelude of an at-rule that takes a block
// (i.e. @foo-rule <prelude> /* call here */ { ... }). It will check
// that there is no junk after the prelude, and that there is indeed
// a block starting. If either of these are false, then it will consume
// until the end of the declaration (any junk after the prelude,
// and the block if one exists), notify the observer, and return false.
bool CSSParserImpl::ConsumeEndOfPreludeForAtRuleWithBlock(
    CSSParserTokenStream& stream,
    CSSAtRuleID id) {
  stream.ConsumeWhitespace();

  if (stream.AtEnd()) {
    // Parse error, we expected a block.
    if (observer_) {
      observer_->ObserveErroneousAtRule(stream.Offset(), id);
    }
    return false;
  }
  if (stream.UncheckedPeek().GetType() == kLeftBraceToken) {
    return true;
  }

  // We have a parse error, so we need to return an error, but before that,
  // we need to consume until the end of the declaration.
  ConsumeErroneousAtRule(stream, id);
  return false;
}

void CSSParserImpl::ConsumeErroneousAtRule(CSSParserTokenStream& stream,
                                           CSSAtRuleID id) {
  if (observer_) {
    observer_->ObserveErroneousAtRule(stream.Offset(), id);
  }
  // Consume the prelude and block if present.
  stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
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
        id != CSSAtRuleID::kCSSAtRuleViewTransition &&
        id != CSSAtRuleID::kCSSAtRuleApplyMixin &&
        (id < CSSAtRuleID::kCSSAtRuleTopLeftCorner ||
         id > CSSAtRuleID::kCSSAtRuleRightBottom)) {
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
  } else if (allowed_rules == kPageMarginRules) {
    if (id < CSSAtRuleID::kCSSAtRuleTopLeftCorner ||
        id > CSSAtRuleID::kCSSAtRuleRightBottom ||
        !RuntimeEnabledFeatures::PageMarginBoxesEnabled()) {
      ConsumeErroneousAtRule(stream, id);
      return nullptr;
    }

    return ConsumePageMarginRule(id, stream);
  } else {
    DCHECK_LE(allowed_rules, kRegularRules);

    switch (id) {
      case CSSAtRuleID::kCSSAtRuleViewTransition:
        return ConsumeViewTransitionRule(stream);
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
      case CSSAtRuleID::kCSSAtRuleFunction:
        return ConsumeFunctionRule(stream);
      case CSSAtRuleID::kCSSAtRuleMixin:
        return ConsumeMixinRule(stream);
      case CSSAtRuleID::kCSSAtRuleApplyMixin:
        return ConsumeApplyMixinRule(stream);
      case CSSAtRuleID::kCSSAtRulePositionTry:
        return ConsumePositionTryRule(stream);
      case CSSAtRuleID::kCSSAtRuleInvalid:
      case CSSAtRuleID::kCSSAtRuleCharset:
      case CSSAtRuleID::kCSSAtRuleImport:
      case CSSAtRuleID::kCSSAtRuleNamespace:
      case CSSAtRuleID::kCSSAtRuleStylistic:
      case CSSAtRuleID::kCSSAtRuleStyleset:
      case CSSAtRuleID::kCSSAtRuleCharacterVariant:
      case CSSAtRuleID::kCSSAtRuleSwash:
      case CSSAtRuleID::kCSSAtRuleOrnaments:
      case CSSAtRuleID::kCSSAtRuleAnnotation:
      case CSSAtRuleID::kCSSAtRuleTopLeftCorner:
      case CSSAtRuleID::kCSSAtRuleTopLeft:
      case CSSAtRuleID::kCSSAtRuleTopCenter:
      case CSSAtRuleID::kCSSAtRuleTopRight:
      case CSSAtRuleID::kCSSAtRuleTopRightCorner:
      case CSSAtRuleID::kCSSAtRuleBottomLeftCorner:
      case CSSAtRuleID::kCSSAtRuleBottomLeft:
      case CSSAtRuleID::kCSSAtRuleBottomCenter:
      case CSSAtRuleID::kCSSAtRuleBottomRight:
      case CSSAtRuleID::kCSSAtRuleBottomRightCorner:
      case CSSAtRuleID::kCSSAtRuleLeftTop:
      case CSSAtRuleID::kCSSAtRuleLeftMiddle:
      case CSSAtRuleID::kCSSAtRuleLeftBottom:
      case CSSAtRuleID::kCSSAtRuleRightTop:
      case CSSAtRuleID::kCSSAtRuleRightMiddle:
      case CSSAtRuleID::kCSSAtRuleRightBottom:
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
    bool invalid_rule_error_ignored = false;  // Only relevant when nested.
    return ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                            /* nested */ false, invalid_rule_error_ignored);
  }

  if (allowed_rules == kKeyframeRules) {
    stream.EnsureLookAhead();
    const wtf_size_t prelude_offset_start = stream.LookAheadOffset();
    std::unique_ptr<Vector<KeyframeOffset>> key_list =
        ConsumeKeyframeKeyList(context_, stream);
    stream.ConsumeWhitespace();
    const RangeOffset prelude_offset(prelude_offset_start,
                                     stream.LookAheadOffset());

    if (stream.Peek().GetType() != kLeftBraceToken) {
      key_list = nullptr;  // Parse error, junk after prelude
      stream.SkipUntilPeekedTypeIs<kLeftBraceToken>();
    }
    if (stream.AtEnd()) {
      return nullptr;  // Parse error, EOF instead of qualified rule block
    }

    CSSParserTokenStream::BlockGuard guard(stream);
    return ConsumeKeyframeStyleRule(std::move(key_list), prelude_offset,
                                    stream);
  }
  if (allowed_rules == kFontFeatureRules) {
    // We get here if something other than an at rule (e.g. @swash,
    // @ornaments... ) was found within @font-feature-values. As we don't
    // support font-display in @font-feature-values, we try to it by scanning
    // until the at-rule or until the block may end. Compare
    // https://drafts.csswg.org/css-fonts-4/#ex-invalid-ignored
    stream.EnsureLookAhead();
    stream.SkipUntilPeekedTypeIs<kAtKeywordToken>();
    return nullptr;
  }

  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

StyleRulePageMargin* CSSParserImpl::ConsumePageMarginRule(
    CSSAtRuleID rule_id,
    CSSParserTokenStream& stream) {
  wtf_size_t header_start = stream.LookAheadOffset();
  // NOTE: @page-margin prelude should be empty.
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream, rule_id)) {
    return nullptr;
  }
  wtf_size_t header_end = stream.LookAheadOffset();

  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPageMargin, header_start);
    observer_->EndRuleHeader(header_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeDeclarationList(stream, StyleRule::kPageMargin, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRulePageMargin>(
      rule_id, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                         context_->GetDocument()));
}

StyleRuleCharset* CSSParserImpl::ConsumeCharsetRule(
    CSSParserTokenStream& stream) {
  const CSSParserToken& string = stream.Peek();
  if (string.GetType() != kStringToken || !stream.AtEnd()) {
    // Parse error, expected a single string.
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleCharset);
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
          stream, CSSAtRuleID::kCSSAtRuleCharset)) {
    return nullptr;
  }

  return MakeGarbageCollected<StyleRuleCharset>();
}

StyleRuleImport* CSSParserImpl::ConsumeImportRule(
    const AtomicString& uri,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();

  if (uri.IsNull()) {
    // Parse error, expected string or URI.
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleImport);
    return nullptr;
  }

  StyleRuleBase::LayerName layer;
  if (stream.Peek().GetType() == kIdentToken &&
      stream.Peek().Id() == CSSValueID::kLayer) {
    stream.ConsumeIncludingWhitespace();
    layer = StyleRuleBase::LayerName({g_empty_atom});
  } else if (stream.Peek().GetType() == kFunctionToken &&
             stream.Peek().FunctionId() == CSSValueID::kLayer) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    StyleRuleBase::LayerName name = ConsumeCascadeLayerName(stream);
    if (name.size() && stream.AtEnd()) {
      layer = std::move(name);
      guard.Release();
    } else {
      // Invalid layer() function can still be parsed as <general-enclosed>
    }
  }
  if (layer.size()) {
    context_->Count(WebFeature::kCSSCascadeLayers);
  }

  stream.ConsumeWhitespace();

  // https://drafts.csswg.org/css-cascade-5/#at-import
  //
  // <import-conditions> =
  //     [ supports([ <supports-condition> | <declaration> ]) ]?
  //     <media-query-list>?
  StringView supports_string = g_null_atom;
  CSSSupportsParser::Result supported = CSSSupportsParser::Result::kSupported;
  if (RuntimeEnabledFeatures::CSSSupportsForImportRulesEnabled() &&
      stream.Peek().GetType() == kFunctionToken &&
      stream.Peek().FunctionId() == CSSValueID::kSupports) {
    {
      CSSParserTokenStream::BlockGuard guard(stream);
      stream.ConsumeWhitespace();
      wtf_size_t supports_offset_start = stream.Offset();

      // First, try parsing as <declaration>.
      CSSParserTokenStream::State savepoint = stream.Save();
      if (stream.Peek().GetType() == kIdentToken &&
          CSSParserImpl::ConsumeSupportsDeclaration(stream)) {
        supported = CSSSupportsParser::Result::kSupported;
      } else {
        // Rewind and try parsing as <supports-condition>.
        stream.Restore(savepoint);
        supported = CSSSupportsParser::ConsumeSupportsCondition(stream, *this);
      }
      wtf_size_t supports_offset_end = stream.Offset();
      supports_string = stream.StringRangeAt(
          supports_offset_start, supports_offset_end - supports_offset_start);
    }
    if (supported == CSSSupportsParser::Result::kParseFailure) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleImport);
      return nullptr;
    }
  }
  stream.ConsumeWhitespace();

  // Parse the rest of the prelude as a media query.
  // TODO(sesse): When the media query parser becomes streaming,
  // we can just parse media queries here instead.
  wtf_size_t media_query_offset_start = stream.Offset();
  stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  StringView media_query_string = stream.StringRangeAt(
      media_query_offset_start, prelude_offset_end - media_query_offset_start);

  MediaQuerySet* media_query_set = MediaQueryParser::ParseMediaQuerySet(
      media_query_string.ToString(), context_->GetExecutionContext());

  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
          stream, CSSAtRuleID::kCSSAtRuleImport)) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kImport, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  return MakeGarbageCollected<StyleRuleImport>(
      uri, std::move(layer), supported == CSSSupportsParser::Result::kSupported,
      supports_string.ToString(), media_query_set,
      context_->IsOriginClean() ? OriginClean::kTrue : OriginClean::kFalse);
}

StyleRuleNamespace* CSSParserImpl::ConsumeNamespaceRule(
    CSSParserTokenStream& stream) {
  AtomicString namespace_prefix;
  if (stream.Peek().GetType() == kIdentToken) {
    namespace_prefix =
        stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  }

  AtomicString uri(ConsumeStringOrURI(stream));
  if (uri.IsNull()) {
    // Parse error, expected string or URI.
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleNamespace);
    return nullptr;
  }
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
          stream, CSSAtRuleID::kCSSAtRuleNamespace)) {
    return nullptr;
  }

  return MakeGarbageCollected<StyleRuleNamespace>(namespace_prefix, uri);
}

StyleRule* CSSParserImpl::CreateImplicitNestedRule(
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  DCHECK(!RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled());

  constexpr bool kNotImplicit =
      false;  // The rule is implicit, but the &/:scope is not.

  HeapVector<CSSSelector, 2> selectors;

  switch (nesting_type) {
    case CSSNestingType::kNone:
      NOTREACHED_IN_MIGRATION();
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
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                context_->GetDocument()));
}

namespace {

// Returns a :where(:scope) selector.
//
// Nested declaration rules within @scope behave as :where(:scope) rules.
//
// https://github.com/w3c/csswg-drafts/issues/10431
HeapVector<CSSSelector> WhereScopeSelector() {
  HeapVector<CSSSelector> selectors;

  // An internal :true pseuo-class with a kScopeActivation relation type
  // must precede any compound which contains :scope.
  // See CSSSelector::RelationType::kScopeActivation.
  CSSSelector true_selector;
  true_selector.SetTrue();
  true_selector.SetRelation(CSSSelector::kScopeActivation);
  selectors.push_back(true_selector);

  CSSSelector inner[1] = {
      CSSSelector(AtomicString("scope"), /* implicit */ false)};
  inner[0].SetLastInComplexSelector(true);
  inner[0].SetLastInSelectorList(true);
  CSSSelectorList* inner_list =
      CSSSelectorList::AdoptSelectorVector(base::span<CSSSelector>(inner));

  CSSSelector where;
  where.SetWhere(inner_list);
  selectors.push_back(where);

  selectors.back().SetLastInComplexSelector(true);
  selectors.back().SetLastInSelectorList(true);

  return selectors;
}

}  // namespace

StyleRuleNestedDeclarations* CSSParserImpl::CreateNestedDeclarationsRule(
    CSSNestingType nesting_type,
    const CSSSelector* selector_list,
    wtf_size_t start_index,
    wtf_size_t end_index) {
  DCHECK(RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled());
  DCHECK(selector_list);
  DCHECK_LT(start_index, end_index);

  // Create a nested declarations rule containing all declarations
  // in [start_index, end_index).
  HeapVector<CSSPropertyValue, 64> declarations;
  declarations.AppendRange(parsed_properties_.begin() + start_index,
                           parsed_properties_.begin() + end_index);

  // Create the selector for StyleRuleNestedDeclarations's inner StyleRule.

  HeapVector<CSSSelector> selectors;

  switch (nesting_type) {
    case CSSNestingType::kNone:
      NOTREACHED_IN_MIGRATION();
      break;
    case CSSNestingType::kNesting:
      // For regular nesting, the nested declarations rule should match
      // exactly what the parent rule matches, with top-level specificity
      // behavior. This means the selector list is copied rather than just
      // being referenced with '&'.
      selectors = CSSSelectorList::Copy(selector_list);
      break;
    case CSSNestingType::kScope:
      // For direct nesting within @scope
      // (e.g. .foo { @scope (...) { color:green } }),
      // the nested declarations rule should match like a :where(:scope) rule.
      //
      // https://github.com/w3c/csswg-drafts/issues/10431
      selectors = WhereScopeSelector();
      break;
  }

  return MakeGarbageCollected<StyleRuleNestedDeclarations>(StyleRule::Create(
      base::span<CSSSelector>{selectors.begin(), selectors.size()},
      CreateCSSPropertyValueSet(declarations, context_->Mode(),
                                context_->GetDocument())));
}

void CSSParserImpl::EmitNestedDeclarationsRuleIfNeeded(
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    wtf_size_t start_index,
    HeapVector<Member<StyleRuleBase>, 4>& child_rules) {
  if (!RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled()) {
    return;
  }
  if (!parent_rule_for_nesting) {
    // This can happen for @page, which behaves similarly to CSS Nesting
    // (and cares about child rules), but doesn't have a parent style rule.
    return;
  }
  wtf_size_t end_index = parsed_properties_.size();
  if (start_index >= end_index) {
    // No need to emit a rule with nothing in it.
    return;
  }

  StyleRuleNestedDeclarations* nested_declarations_rule =
      CreateNestedDeclarationsRule(nesting_type,
                                   parent_rule_for_nesting->FirstSelector(),
                                   start_index, end_index);
  DCHECK(nested_declarations_rule);
  child_rules.push_back(nested_declarations_rule);

  if (observer_) {
    observer_->ObserveNestedDeclarations(
        /* insert_rule_index */ child_rules.size() - 1);
  }

  // The declarations held by the nested declarations rule
  // should not *also* appear in the main style declarations of the parent rule.
  parsed_properties_.resize(start_index);
}

StyleRuleMedia* CSSParserImpl::ConsumeMediaRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // Consume the prelude.

  // First just get the string for the prelude to see if we've got a cached
  // version of this. (This is mainly to save memory in certain page with
  // lots of duplicate media queries.)
  CSSParserTokenStream::State savepoint = stream.Save();
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();

  String prelude_string =
      stream
          .StringRangeAt(prelude_offset_start,
                         prelude_offset_end - prelude_offset_start)
          .ToString();
  const MediaQuerySet* media;
  Member<const MediaQuerySet>& cached_media =
      media_query_cache_.insert(prelude_string, nullptr).stored_value->value;
  if (cached_media) {
    media = cached_media.Get();
  } else {
    // Not in the cache, so we'll have to rewind and actually parse it.
    // Note that the media query set grammar doesn't really have an idea
    // of when the stream should end; if it sees something it doesn't
    // understand (which includes a left brace), it will just forward to
    // the next comma, skipping over the entire stylesheet until the end.
    // The grammar is generally written in the understanding that the prelude
    // is extracted as a string and only then parsed, whereas we do fully
    // streaming prelude parsing. Thus, we need to set some boundaries
    // here ourselves to make sure we end when the prelude does; the alternative
    // would be to teach the media query set parser to stop there itself.
    stream.Restore(savepoint);
    CSSParserTokenStream::Boundary boundary(stream, kLeftBraceToken);
    CSSParserTokenStream::Boundary boundary2(stream, kSemicolonToken);
    media = MediaQueryParser::ParseMediaQuerySet(
        stream, context_->GetExecutionContext());
  }
  DCHECK(media);

  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleMedia)) {
    return nullptr;
  }

  cached_media = media;

  // Consume the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kMedia, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  if (style_sheet_) {
    style_sheet_->SetHasMediaQueries();
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
  return MakeGarbageCollected<StyleRuleMedia>(media, std::move(rules));
}

StyleRuleSupports* CSSParserImpl::ConsumeSupportsRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSSupportsParser::Result supported =
      CSSSupportsParser::ConsumeSupportsCondition(stream, *this);
  if (supported == CSSSupportsParser::Result::kParseFailure) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleSupports);
    return nullptr;
  }
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleSupports)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

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
  // NOTE: @starting-style prelude should be empty.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleStartingStyle)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

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
  // Consume the prelude.
  // NOTE: @font-face prelude should be empty.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleFontFace)) {
    return nullptr;
  }

  // Consume the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFace, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    // TODO(sesse): Is this really right?
    observer_->StartRuleBody(prelude_offset_end);
    observer_->EndRuleBody(prelude_offset_end);
  }

  if (style_sheet_) {
    style_sheet_->SetHasFontFaceRule();
  }

  base::AutoReset<CSSParserObserver*> disable_observer(&observer_, nullptr);
  ConsumeDeclarationList(stream, StyleRule::kFontFace, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  return MakeGarbageCollected<StyleRuleFontFace>(CreateCSSPropertyValueSet(
      parsed_properties_, kCSSFontFaceRuleMode, context_->GetDocument()));
}

StyleRuleKeyframes* CSSParserImpl::ConsumeKeyframesRule(
    bool webkit_prefixed,
    CSSParserTokenStream& stream) {
  // Parse the prelude, expecting a single non-whitespace token.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserToken& name_token = stream.Peek();
  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
  } else if (name_token.GetType() == kStringToken && webkit_prefixed) {
    context_->Count(WebFeature::kQuotedKeyframesRule);
    name = name_token.Value().ToString();
  } else {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleKeyframes);
    return nullptr;  // Parse error; expected ident token in @keyframes header
  }
  stream.ConsumeIncludingWhitespace();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleKeyframes)) {
    return nullptr;
  }

  // Parse the body.
  CSSParserTokenStream::BlockGuard guard(stream);

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
  std::optional<StyleRuleFontFeature::FeatureType> feature_type =
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

    stream.ConsumeWhitespace();

    do {
      if (numbers->length() == max_allowed_values) {
        return nullptr;
      }
      CSSPrimitiveValue* parsed_number =
          css_parsing_utils::ConsumeIntegerOrNumberCalc(
              stream, *context_,
              CSSPrimitiveValue::ValueRange::kNonNegativeInteger);
      if (!parsed_number) {
        return nullptr;
      }
      numbers->Append(*parsed_number);
    } while (stream.Peek().GetType() != kSemicolonToken && !stream.AtEnd());

    if (!stream.AtEnd()) {
      stream.ConsumeIncludingWhitespace();  // kSemicolonToken
    }

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

    font_feature_rule->UpdateAlias(alias, std::move(parsed_numbers));
  }

  return font_feature_rule;
}

StyleRuleFontFeatureValues* CSSParserImpl::ConsumeFontFeatureValuesRule(
    CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSValueList* family_list = css_parsing_utils::ConsumeFontFamily(stream);
  if (!family_list || !family_list->length()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFontFeatureValues);
    return nullptr;
  }
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleFontFeatureValues)) {
    return nullptr;
  }
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFeatureValues,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  // Parse the actual block.

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
      std::move(families), stylistic, styleset, character_variant, swash,
      ornaments, annotation);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return feature_values_rule;
}

// Parse an @page rule, with contents.
StyleRulePage* CSSParserImpl::ConsumePageRule(CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSSelectorList* selector_list =
      ParsePageSelector(stream, style_sheet_, *context_);
  if (!selector_list || !selector_list->IsValid()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRulePage);
    return nullptr;  // Parse error, invalid @page selector
  }
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRulePage)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPage, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  ConsumeDeclarationList(stream, StyleRule::kPage, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         &child_rules);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRulePage>(
      selector_list,
      CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                context_->GetDocument()),
      child_rules);
}

StyleRuleProperty* CSSParserImpl::ConsumePropertyRule(
    CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserToken& name_token = stream.ConsumeIncludingWhitespace();
  if (!CSSVariableParser::IsValidVariableName(name_token)) {
    if (observer_) {
      observer_->ObserveErroneousAtRule(prelude_offset_start,
                                        CSSAtRuleID::kCSSAtRuleProperty);
    }
    return nullptr;
  }
  String name = name_token.Value().ToString();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleProperty)) {
    return nullptr;
  }

  // Parse the body.
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kProperty, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeDeclarationList(stream, StyleRule::kProperty, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  StyleRuleProperty* rule = MakeGarbageCollected<StyleRuleProperty>(
      name, CreateCSSPropertyValueSet(parsed_properties_, kCSSPropertyRuleMode,
                                      context_->GetDocument()));

  std::optional<CSSSyntaxDefinition> syntax =
      PropertyRegistration::ConvertSyntax(rule->GetSyntax());
  std::optional<bool> inherits =
      PropertyRegistration::ConvertInherits(rule->Inherits());
  std::optional<const CSSValue*> initial =
      syntax.has_value() ? PropertyRegistration::ConvertInitial(
                               rule->GetInitialValue(), *syntax, *context_)
                         : std::nullopt;

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
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  AtomicString name = css_parsing_utils::ConsumeCounterStyleNameInPrelude(
      stream, *GetContext());
  if (!name) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleCounterStyle);
    return nullptr;
  }
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleCounterStyle)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kCounterStyle, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeDeclarationList(stream, StyleRule::kCounterStyle,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRuleCounterStyle>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                      context_->GetDocument()));
}

StyleRuleFontPaletteValues* CSSParserImpl::ConsumeFontPaletteValuesRule(
    CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserToken& name_token = stream.Peek();
  if (!css_parsing_utils::IsDashedIdent(name_token)) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFontPaletteValues);
    return nullptr;
  }
  AtomicString name = name_token.Value().ToAtomicString();
  if (!name) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFontPaletteValues);
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleFontPaletteValues)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontPaletteValues,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeDeclarationList(stream, StyleRule::kFontPaletteValues,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRuleFontPaletteValues>(
      name, CreateCSSPropertyValueSet(parsed_properties_,
                                      kCSSFontPaletteValuesRuleMode,
                                      context_->GetDocument()));
}

StyleRuleBase* CSSParserImpl::ConsumeScopeRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  auto* style_scope =
      StyleScope::Parse(stream, context_, nesting_type, parent_rule_for_nesting,
                        is_within_scope_, style_sheet_);
  if (!style_scope) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleScope);
    return nullptr;
  }

  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleScope)) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kScope, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  base::AutoReset<bool> auto_is_within_scope(&is_within_scope_, true);

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

StyleRuleViewTransition* CSSParserImpl::ConsumeViewTransitionRule(
    CSSParserTokenStream& stream) {
  CHECK(RuntimeEnabledFeatures::ViewTransitionOnNavigationEnabled());
  // NOTE: @view-transition prelude should be empty.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleViewTransition)) {
    return nullptr;
  }

  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kViewTransition,
                               prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }
  ConsumeDeclarationList(stream, StyleRule::kViewTransition,
                         CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRuleViewTransition>(
      *CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                 context_->GetDocument()));
}

StyleRuleContainer* CSSParserImpl::ConsumeContainerRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // Consume the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  ContainerQueryParser query_parser(*context_);

  // <container-name>
  AtomicString name;
  if (stream.Peek().GetType() == kIdentToken) {
    auto* ident = DynamicTo<CSSCustomIdentValue>(
        css_parsing_utils::ConsumeSingleContainerName(stream, *context_));
    if (ident) {
      name = ident->Value();
    }
  }

  const MediaQueryExpNode* query = query_parser.ParseCondition(stream);
  if (!query) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleContainer);
    return nullptr;
  }
  ContainerQuery* container_query = MakeGarbageCollected<ContainerQuery>(
      ContainerSelector(std::move(name), *query), query);

  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleContainer)) {
    return nullptr;
  }

  // Consume the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kContainer, prelude_offset_start);
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
  return MakeGarbageCollected<StyleRuleContainer>(*container_query,
                                                  std::move(rules));
}

StyleRuleBase* CSSParserImpl::ConsumeLayerRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // Consume the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();

  Vector<StyleRuleBase::LayerName> names;
  while (!stream.AtEnd() && stream.Peek().GetType() != kLeftBraceToken &&
         stream.Peek().GetType() != kSemicolonToken) {
    if (names.size()) {
      if (!css_parsing_utils::ConsumeCommaIncludingWhitespace(stream)) {
        ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleLayer);
        return nullptr;
      }
    }
    StyleRuleBase::LayerName name = ConsumeCascadeLayerName(stream);
    if (!name.size()) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleLayer);
      return nullptr;
    }
    names.push_back(std::move(name));
  }

  // @layer statement rule without style declarations.
  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    if (!names.size()) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleLayer);
      return nullptr;
    }

    if (nesting_type == CSSNestingType::kNesting) {
      // @layer statement rules are not group rules, and can therefore
      // not be nested.
      //
      // https://drafts.csswg.org/css-nesting-1/#nested-group-rules
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleLayer);
      return nullptr;
    }

    wtf_size_t prelude_offset_end = stream.LookAheadOffset();
    if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
            stream, CSSAtRuleID::kCSSAtRuleLayer)) {
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
  StyleRuleBase::LayerName name;
  if (names.empty()) {
    name.push_back(g_empty_atom);
  } else if (names.size() > 1) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleLayer);
    return nullptr;
  } else {
    name = std::move(names[0]);
  }

  wtf_size_t prelude_offset_end = stream.LookAheadOffset();

  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleLayer)) {
    return nullptr;
  }

  // Consume the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

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

StyleRulePositionTry* CSSParserImpl::ConsumePositionTryRule(
    CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserToken& name_token = stream.Peek();
  // <dashed-ident>, and -internal-* for UA sheets only.
  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
    if (!name.StartsWith("--") &&
        !(context_->Mode() == kUASheetMode && name.StartsWith("-internal-"))) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRulePositionTry);
      return nullptr;
    }
  } else {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRulePositionTry);
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRulePositionTry)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kPositionTry, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeDeclarationList(stream, StyleRule::kPositionTry, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRulePositionTry>(
      AtomicString(name),
      CreateCSSPropertyValueSet(parsed_properties_, kCSSPositionTryRuleMode,
                                context_->GetDocument()));
}

// Parse a type for CSS Functions; e.g. length, color, etc..
// These are being converted to the syntax used by registered custom properties.
// The parameter is assumed to be a single ident token.
static std::optional<StyleRuleFunction::Type> ParseFunctionType(
    StringView type_name) {
  std::optional<CSSSyntaxDefinition> syntax_def;
  if (type_name == "any") {
    syntax_def = CSSSyntaxStringParser("*").Parse();
  } else {
    syntax_def =
        CSSSyntaxStringParser("<" + type_name.ToString() + ">").Parse();
  }
  if (!syntax_def) {
    return {};
  }

  CHECK_EQ(syntax_def->Components().size(), 1u);
  bool should_add_implicit_calc = false;
  if (!syntax_def->IsUniversal()) {
    // These are all the supported values in CSSSyntaxDefinition that are
    // acceptable as inputs to calc(); see
    // https://drafts.csswg.org/css-values/#math.
    switch (syntax_def->Components()[0].GetType()) {
      case CSSSyntaxType::kLength:
        // kFrequency is missing.
      case CSSSyntaxType::kAngle:
      case CSSSyntaxType::kTime:
        // kFlex is missing.
      case CSSSyntaxType::kResolution:
      case CSSSyntaxType::kPercentage:
      case CSSSyntaxType::kNumber:
      case CSSSyntaxType::kInteger:
      case CSSSyntaxType::kLengthPercentage:
        should_add_implicit_calc = true;
        break;
      case CSSSyntaxType::kTokenStream:
      case CSSSyntaxType::kIdent:
      case CSSSyntaxType::kColor:
      case CSSSyntaxType::kImage:
      case CSSSyntaxType::kUrl:
      case CSSSyntaxType::kTransformFunction:
      case CSSSyntaxType::kTransformList:
      case CSSSyntaxType::kCustomIdent:
        break;
      case CSSSyntaxType::kString:
        DCHECK(RuntimeEnabledFeatures::CSSAtPropertyStringSyntaxEnabled());
        break;
    }
  }

  return StyleRuleFunction::Type{std::move(*syntax_def),
                                 should_add_implicit_calc};
}

StyleRuleFunction* CSSParserImpl::ConsumeFunctionRule(
    CSSParserTokenStream& stream) {
  if (!RuntimeEnabledFeatures::CSSFunctionsEnabled()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;
  }
  // Parse the prelude; first a function token (the name), then parameters,
  // then return type.
  if (stream.Peek().GetType() != kFunctionToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;  // Parse error.
  }
  AtomicString name =
      stream.Peek()
          .Value()
          .ToAtomicString();  // Includes the opening parenthesis.
  std::optional<Vector<StyleRuleFunction::Parameter>> parameters;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    parameters = ConsumeFunctionParameters(stream);
  }
  if (!parameters.has_value()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;
  }
  stream.ConsumeWhitespace();

  // Parse the return type.
  if (stream.Peek().GetType() != kColonToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();

  if (stream.Peek().GetType() != kIdentToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;
  }
  StringView return_type_name = stream.Peek().Value();
  std::optional<StyleRuleFunction::Type> return_type =
      ParseFunctionType(return_type_name);
  if (!return_type) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
    return nullptr;  // Invalid type name.
  }
  stream.ConsumeIncludingWhitespace();

  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleFunction)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  stream.ConsumeWhitespace();

  // TODO: Parse local variables.

  // Parse @return.
  if (stream.Peek().GetType() != kAtKeywordToken) {
    return nullptr;
  }
  const CSSParserToken return_token = stream.ConsumeIncludingWhitespace();
  if (return_token.Value() != "return") {
    return nullptr;
  }

  // Parse the actual returned value.
  CSSVariableData* return_value = nullptr;
  {
    CSSParserTokenStream::Boundary boundary(stream, kSemicolonToken);
    bool important_ignored;
    return_value = CSSVariableParser::ConsumeUnparsedDeclaration(
        stream, /*allow_important_annotation=*/false,
        /*is_animation_tainted=*/false,
        /*must_contain_variable_reference=*/false, /*restricted_value=*/false,
        /*comma_ends_declaration=*/false, important_ignored,
        context_->GetExecutionContext());
  }

  while (!stream.AtEnd()) {
    const CSSParserToken token = stream.ConsumeIncludingWhitespace();
    StringBuilder sb;
    token.Serialize(sb);
  }

  return MakeGarbageCollected<StyleRuleFunction>(
      name, std::move(*parameters), return_value, std::move(*return_type));
}

StyleRuleMixin* CSSParserImpl::ConsumeMixinRule(CSSParserTokenStream& stream) {
  if (!RuntimeEnabledFeatures::CSSMixinsEnabled()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;
  }

  // @mixin must be top-level, and as such, we need to clear the arena
  // after we're done parsing it (like ConsumeStyleRule() does).
  if (in_nested_style_rule_) {
    return nullptr;
  }
  auto func_clear_arena = [&](HeapVector<CSSSelector>* arena) {
    arena->resize(0);  // See class comment on CSSSelectorParser.
  };
  std::unique_ptr<HeapVector<CSSSelector>, decltype(func_clear_arena)>
      scope_guard(&arena_, std::move(func_clear_arena));

  // Parse the prelude; just a function token (the name).
  if (stream.Peek().GetType() != kIdentToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;  // Parse error.
  }
  AtomicString name =
      stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!name.StartsWith("--")) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;
  }

  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleMixin)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

  // The destructor expects there to be at least one selector in the StyleRule.
  CSSSelector dummy;
  StyleRule* fake_parent_rule = StyleRule::Create(std::vector{dummy});
  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  ConsumeRuleListOrNestedDeclarationList(stream,
                                         /*is_nested_group_rule=*/true,
                                         CSSNestingType::kNesting,
                                         fake_parent_rule, &child_rules);
  for (StyleRuleBase* child_rule : child_rules) {
    fake_parent_rule->AddChildRule(child_rule);
  }
  return MakeGarbageCollected<StyleRuleMixin>(name, fake_parent_rule);
}

StyleRuleApplyMixin* CSSParserImpl::ConsumeApplyMixinRule(
    CSSParserTokenStream& stream) {
  if (!RuntimeEnabledFeatures::CSSMixinsEnabled()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;
  }
  if (stream.Peek().GetType() != kIdentToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;  // Parse error.
  }
  AtomicString name =
      stream.ConsumeIncludingWhitespace().Value().ToAtomicString();
  if (!name.StartsWith("--")) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;
  }
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
          stream, CSSAtRuleID::kCSSAtRuleApplyMixin)) {
    return nullptr;
  }
  return MakeGarbageCollected<StyleRuleApplyMixin>(name);
}

// Parse the parameters of a CSS function: Zero or more comma-separated
// instances of [<name> <colon> <type>]. Returns the empty value
// on parse error.
std::optional<Vector<StyleRuleFunction::Parameter>>
CSSParserImpl::ConsumeFunctionParameters(CSSParserTokenStream& stream) {
  Vector<StyleRuleFunction::Parameter> parameters;
  bool first_parameter = true;
  for (;;) {
    stream.ConsumeWhitespace();

    if (first_parameter && stream.Peek().GetType() == kRightParenthesisToken) {
      // No arguments.
      break;
    }
    if (stream.Peek().GetType() != kIdentToken) {
      return {};  // Parse error.
    }
    String parameter_name = stream.Peek().Value().ToString();
    if (!CSSVariableParser::IsValidVariableName(parameter_name)) {
      return {};
    }
    stream.ConsumeIncludingWhitespace();

    if (stream.Peek().GetType() != kColonToken) {
      return {};
    }
    stream.ConsumeIncludingWhitespace();

    if (stream.Peek().GetType() != kIdentToken) {
      return {};
    }
    StringView type_name = stream.Peek().Value();
    std::optional<StyleRuleFunction::Type> type = ParseFunctionType(type_name);
    if (!type) {
      return {};  // Invalid type name.
    }
    stream.ConsumeIncludingWhitespace();
    parameters.push_back(
        StyleRuleFunction::Parameter{parameter_name, std::move(*type)});
    if (stream.Peek().GetType() == kRightParenthesisToken) {
      // No more arguments.
      break;
    }
    if (stream.Peek().GetType() != kCommaToken) {
      return {};  // Expected more parameters, or end of argument list.
    }
    stream.ConsumeIncludingWhitespace();
    first_parameter = false;
  }
  return parameters;
}

StyleRuleKeyframe* CSSParserImpl::ConsumeKeyframeStyleRule(
    std::unique_ptr<Vector<KeyframeOffset>> key_list,
    const RangeOffset& prelude_offset,
    CSSParserTokenStream& block) {
  if (!key_list) {
    return nullptr;
  }

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kKeyframe, prelude_offset.start);
    observer_->EndRuleHeader(prelude_offset.end);
    observer_->StartRuleBody(block.Offset());
  }

  ConsumeDeclarationList(block, StyleRule::kKeyframe, CSSNestingType::kNone,
                         /*parent_rule_for_nesting=*/nullptr,
                         /*nested_declarations_start_index=*/kNotFound,
                         /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(block.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRuleKeyframe>(
      std::move(key_list),
      CreateCSSPropertyValueSet(parsed_properties_, kCSSKeyframeRuleMode,
                                context_->GetDocument()));
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

  // Strip away the outer {} pair (the { would always give us a false positive).
  DCHECK_EQ(text[offset], '{');
  if (text[offset + length - 1] != '}') {
    // EOF within the block, so just be on the safe side
    // and use the normal (non-lazy) code path.
    return true;
  }
  ++offset;
  length -= 2;

  size_t char_size = text.Is8Bit() ? sizeof(LChar) : sizeof(UChar);
  auto text_bytes = base::as_chars(
      text.RawByteSpan().subspan(offset * char_size, length * char_size));
  return memchr(text_bytes.data(), '{', text_bytes.size()) != nullptr;
}

StyleRule* CSSParserImpl::ConsumeStyleRule(CSSParserTokenStream& stream,
                                           CSSNestingType nesting_type,
                                           StyleRule* parent_rule_for_nesting,
                                           bool nested,
                                           bool& invalid_rule_error) {
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
  if (CSSVariableParser::IsValidVariableName(stream.Peek())) {
    CSSParserTokenStream::State state = stream.Save();
    stream.ConsumeIncludingWhitespace();  // <ident>
    custom_property_ambiguity = stream.Peek().GetType() == kColonToken;
    stream.Restore(state);
  }

  // Parse the prelude of the style rule
  base::span<CSSSelector> selector_vector = CSSSelectorParser::ConsumeSelector(
      stream, context_, nesting_type, parent_rule_for_nesting, is_within_scope_,
      /* semicolon_aborts_nested_selector*/ nested, style_sheet_, observer_,
      arena_);

  if (selector_vector.empty()) {
    // Read the rest of the prelude if there was an error
    stream.EnsureLookAhead();
    if (nested) {
      stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();
    } else {
      stream.SkipUntilPeekedTypeIs<kLeftBraceToken>();
    }
  }

  if (observer_) {
    observer_->EndRuleHeader(stream.LookAheadOffset());
  }

  if (stream.Peek().GetType() != kLeftBraceToken) {
    // Parse error, EOF instead of qualified rule block
    // (or we went into error recovery above).
    // NOTE: If we aborted due to a semicolon, don't consume it here;
    // the caller will do that for us.
    return nullptr;
  }

  if (custom_property_ambiguity) {
    if (nested) {
      // https://drafts.csswg.org/css-syntax/#consume-the-remnants-of-a-bad-declaration
      // Note that the caller consumes the bad declaration remnants
      // (see ConsumeDeclarationList).
      return nullptr;
    }
    // "If nested is false, consume a block from input, and return nothing."
    // https://drafts.csswg.org/css-syntax/#consume-qualified-rule
    CSSParserTokenStream::BlockGuard guard(stream);
    return nullptr;
  }
  // Check if rule is "valid in current context".
  // https://drafts.csswg.org/css-syntax/#consume-qualified-rule
  //
  // This means checking if the selector parsed successfully.
  if (selector_vector.empty()) {
    CSSParserTokenStream::BlockGuard guard(stream);
    invalid_rule_error = true;
    return nullptr;
  }

  if (RuntimeEnabledFeatures::CSSLazyParsingFastPathEnabled()) {
    // TODO(csharrison): How should we lazily parse css that needs the observer?
    if (!observer_ && lazy_state_) {
      DCHECK(style_sheet_);

      StringView text(stream.RemainingText(), 1);
#ifdef ARCH_CPU_X86_FAMILY
      wtf_size_t len;
      if (base::CPU::GetInstanceNoAllocation().has_avx2()) {
        len = static_cast<wtf_size_t>(FindLengthOfDeclarationListAVX2(text));
      } else {
        len = static_cast<wtf_size_t>(FindLengthOfDeclarationList(text));
      }
#else
      wtf_size_t len =
          static_cast<wtf_size_t>(FindLengthOfDeclarationList(text));
#endif
      if (len != 0) {
        wtf_size_t block_start_offset = stream.Offset();
        stream.SkipToEndOfBlock(len + 2);  // +2 for { and }.
        return StyleRule::Create(
            selector_vector, MakeGarbageCollected<CSSLazyPropertyParserImpl>(
                                 block_start_offset, lazy_state_));
      }
    }
    CSSParserTokenStream::BlockGuard guard(stream);
    return ConsumeStyleRuleContents(selector_vector, stream);
  } else {
    CSSParserTokenStream::BlockGuard guard(stream);

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
      if (MayContainNestedRules(lazy_state_->SheetText(), block_start_offset,
                                block_length)) {
        CSSParserTokenStream block_stream(lazy_state_->SheetText(),
                                          block_start_offset);
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
}

StyleRule* CSSParserImpl::ConsumeStyleRuleContents(
    base::span<CSSSelector> selector_vector,
    CSSParserTokenStream& stream) {
  StyleRule* style_rule = StyleRule::Create(selector_vector);
  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  if (observer_) {
    observer_->StartRuleBody(stream.Offset());
  }
  ConsumeDeclarationList(stream, StyleRule::kStyle, CSSNestingType::kNesting,
                         /*parent_rule_for_nesting=*/style_rule,
                         /*nested_declarations_start_index=*/kNotFound,
                         &child_rules);
  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }
  for (StyleRuleBase* child_rule : child_rules) {
    style_rule->AddChildRule(child_rule);
  }
  style_rule->SetProperties(CreateCSSPropertyValueSet(
      parsed_properties_, context_->Mode(), context_->GetDocument()));
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
//
// The `nested_declarations_start_index` parameter controls how this function
// emits "nested declaration" rules for the leading block of declarations.
// For regular style rules (which can hold declarations directly), this should
// be kNotFound, which will prevent a wrapper rule for the leading block.
// (Subsequent declarations "interleaved" with child rules will still be
// wrapped). For nested group rules, or generally rules that cannot hold
// declarations directly (e.g. @media), the parameter value should be 0u,
// causing the leading declarations to get wrapped as well.
void CSSParserImpl::ConsumeDeclarationList(
    CSSParserTokenStream& stream,
    StyleRule::RuleType rule_type,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    wtf_size_t nested_declarations_start_index,
    HeapVector<Member<StyleRuleBase>, 4>* child_rules) {
  DCHECK(parsed_properties_.empty());

  while (true) {
    // Having a lookahead may skip comments, which are used by the observer.
    DCHECK(!stream.HasLookAhead() || stream.AtEnd());

    if (observer_ && !stream.HasLookAhead()) {
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
      case kAtKeywordToken: {
        CSSParserToken name_token = stream.ConsumeIncludingWhitespace();
        const StringView name = name_token.Value();
        const CSSAtRuleID id = CssAtRuleID(name);
        bool invalid_rule_error_ignored = false;
        StyleRuleBase* child = ConsumeNestedRule(
            id, rule_type, stream, nesting_type, parent_rule_for_nesting,
            invalid_rule_error_ignored);
        // "Consume an at-rule" can't return invalid-rule-error.
        // https://drafts.csswg.org/css-syntax/#consume-at-rule
        DCHECK(!invalid_rule_error_ignored);
        if (child && child_rules) {
          EmitNestedDeclarationsRuleIfNeeded(
              nesting_type, parent_rule_for_nesting,
              nested_declarations_start_index, *child_rules);
          nested_declarations_start_index = parsed_properties_.size();
          child_rules->push_back(child);
        }
        break;
      }
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
          break;
        } else if (stream.Peek().GetType() == kSemicolonToken) {
          // As an optimization, we avoid the restart below (retrying as a
          // nested style rule) if we ended on a kSemicolonToken, as this
          // situation can't produce a valid rule.
          stream.UncheckedConsume();  // kSemicolonToken
          break;
        }
        // Retry as nested rule.
        stream.Restore(state);
        [[fallthrough]];
      }
      default:
        if (parent_rule_for_nesting != nullptr) {  // [1] (see function comment)
          bool invalid_rule_error = false;
          StyleRuleBase* child =
              ConsumeNestedRule(std::nullopt, rule_type, stream, nesting_type,
                                parent_rule_for_nesting, invalid_rule_error);
          if (child) {
            if (child_rules) {
              EmitNestedDeclarationsRuleIfNeeded(
                  nesting_type, parent_rule_for_nesting,
                  nested_declarations_start_index, *child_rules);
              nested_declarations_start_index = parsed_properties_.size();
              child_rules->push_back(child);
            }
            break;
          } else if (invalid_rule_error) {
            // https://drafts.csswg.org/css-syntax/#invalid-rule-error
            //
            // This means the rule was valid per the "core" grammar of
            // css-syntax, but the prelude (i.e. selector list) didn't parse.
            // We should not fall through to error recovery in this case,
            // because we should continue parsing immediately after
            // the {}-block.
            break;
          }
          // Fall through to error recovery.
          stream.EnsureLookAhead();
        }

        [[fallthrough]];
        // Function tokens should start parsing a declaration
        // (which then immediately goes into error recovery mode).
      case CSSParserTokenType::kFunctionToken:
        stream.SkipUntilPeekedTypeIs<kSemicolonToken>();
        if (!stream.UncheckedAtEnd()) {
          stream.UncheckedConsume();  // kSemicolonToken
        }

        break;
    }
  }

  // We need a final call to EmitNestedDeclarationsRuleIfNeeded in case there
  // are trailing bare declarations. If no child rule has been observed,
  // nested_declarations_start_index is still kNotFound (UINT_MAX),
  // which causes EmitNestedDeclarationsRuleIfNeeded to have no effect.
  if (child_rules) {
    EmitNestedDeclarationsRuleIfNeeded(nesting_type, parent_rule_for_nesting,
                                       nested_declarations_start_index,
                                       *child_rules);
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

  if (is_nested_group_rule) {
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
    if (RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled()) {
      // Using nested_declarations_start_index=0u here means that the leading
      // declarations will be wrapped in a CSSNestedDeclarations rule.
      // Unlike regular style rules, the leading declarations must be wrapped
      // in something that can hold them, because group rules (e.g. @media)
      // can not hold properties directly.
      ConsumeDeclarationList(
          stream, StyleRule::kStyle, nesting_type, parent_rule_for_nesting,
          /* nested_declarations_start_index */ 0u, child_rules);
    } else {
      if (observer_) {
        // Observe an empty rule header to ensure the observer has a new rule
        // data on the stack for the following ConsumeDeclarationList.
        observer_->StartRuleHeader(StyleRule::kStyle, stream.Offset());
        observer_->EndRuleHeader(stream.Offset());
        observer_->StartRuleBody(stream.Offset());
      }
      ConsumeDeclarationList(
          stream, StyleRule::kStyle, nesting_type, parent_rule_for_nesting,
          /* nested_declarations_start_index */ kNotFound, child_rules);
      if (observer_) {
        observer_->EndRuleBody(stream.LookAheadOffset());
      }
      if (!parsed_properties_.empty()) {
        child_rules->push_front(
            CreateImplicitNestedRule(nesting_type, parent_rule_for_nesting));
      }
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
    std::optional<CSSAtRuleID> id,
    StyleRule::RuleType parent_rule_type,
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    bool& invalid_rule_error) {
  // A nested style rule. Recurse into the parser; we need to move the parsed
  // properties out of the way while we're parsing the child rule, though.
  // TODO(sesse): The spec says that any properties after a nested rule
  // should be ignored. We don't support this yet.
  // See https://github.com/w3c/csswg-drafts/issues/7501.
  HeapVector<CSSPropertyValue, 64> outer_parsed_properties;
  swap(parsed_properties_, outer_parsed_properties);
  StyleRuleBase* child;
  base::AutoReset<bool> reset_in_nested_style_rule(&in_nested_style_rule_,
                                                   true);
  if (!id.has_value()) {
    child = ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                             /* nested */ true, invalid_rule_error);
  } else {
    child = ConsumeAtRuleContents(*id, stream,
                                  parent_rule_type == StyleRule::kPage
                                      ? kPageMarginRules
                                      : kNestedGroupRules,
                                  nesting_type, parent_rule_for_nesting);
  }
  parsed_properties_ = std::move(outer_parsed_properties);
  if (child && parent_rule_type != StyleRule::kPage) {
    context_->Count(WebFeature::kCSSNesting);
  }
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
                            rule_type == StyleRule::kViewTransition;

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
      const AtRuleDescriptorID atrule_id = static_cast<AtRuleDescriptorID>(id);
      AtRuleDescriptorParser::ParseAtRule(rule_type, atrule_id, stream,
                                          *context_, parsed_properties_);
    } else {
      const CSSPropertyID unresolved_property = static_cast<CSSPropertyID>(id);
      if (unresolved_property == CSSPropertyID::kVariable) {
        if (rule_type != StyleRule::kStyle &&
            rule_type != StyleRule::kKeyframe) {
          return false;
        }
        AtomicString variable_name = lhs.Value().ToAtomicString();
        bool allow_important_annotation = (rule_type != StyleRule::kKeyframe);
        bool is_animation_tainted = rule_type == StyleRule::kKeyframe;
        if (!ConsumeVariableValue(stream, variable_name,
                                  allow_important_annotation,
                                  is_animation_tainted)) {
          return false;
        }
      } else if (unresolved_property != CSSPropertyID::kInvalid) {
        if (observer_) {
          CSSParserTokenStream::State savepoint = stream.Save();
          ConsumeDeclarationValue(stream, unresolved_property,
                                  /*is_in_declaration_list=*/true, rule_type);

          // The observer would like to know (below) whether this declaration
          // was !important or not. If our parse succeeded, we can just pick it
          // out from the list of properties. If not, we'll need to look at the
          // tokens ourselves.
          if (parsed_properties_.size() != properties_count) {
            important = parsed_properties_.back().IsImportant();
          } else {
            stream.Restore(savepoint);
            // NOTE: This call is solely to update “important”.
            CSSVariableParser::ConsumeUnparsedDeclaration(
                stream, /*allow_important_annotation=*/true,
                /*is_animation_tainted=*/false,
                /*must_contain_variable_reference=*/false,
                /*restricted_value=*/true, /*comma_ends_declaration=*/false,
                important, context_->GetExecutionContext());
          }
        } else {
          ConsumeDeclarationValue(stream, unresolved_property,
                                  /*is_in_declaration_list=*/true, rule_type);
        }
      }
    }
  }
  if (observer_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kKeyframe ||
       rule_type == StyleRule::kProperty ||
       rule_type == StyleRule::kPositionTry ||
       rule_type == StyleRule::kFontPaletteValues)) {
    if (!id) {
      // If we skipped the relevant Consume*() calls above due to an invalid
      // property/descriptor, the inspector still needs to know the offset
      // where the would-be declaration ends.
      CSSVariableParser::ConsumeUnparsedDeclaration(
          stream, /*allow_important_annotation=*/true,
          /*is_animation_tainted=*/false,
          /*must_contain_variable_reference=*/false,
          /*restricted_value=*/true, /*comma_ends_declaration=*/false,
          important, context_->GetExecutionContext());
    }
    // The end offset is the offset of the terminating token, which is peeked
    // but not yet consumed.
    observer_->ObserveProperty(decl_offset_start, stream.LookAheadOffset(),
                               important,
                               parsed_properties_.size() != properties_count);
  }

  return parsed_properties_.size() != properties_count;
}

bool CSSParserImpl::ConsumeVariableValue(CSSParserTokenStream& stream,
                                         const AtomicString& variable_name,
                                         bool allow_important_annotation,
                                         bool is_animation_tainted) {
  stream.EnsureLookAhead();

  // First, see if this is (only) a CSS-wide keyword.
  bool important;
  const CSSValue* value = CSSPropertyParser::ConsumeCSSWideKeyword(
      stream, allow_important_annotation, important);
  if (!value) {
    // It was not, so try to parse it as an unparsed declaration value
    // (which is pretty free-form).
    CSSVariableData* variable_data =
        CSSVariableParser::ConsumeUnparsedDeclaration(
            stream, allow_important_annotation, is_animation_tainted,
            /*must_contain_variable_reference=*/false,
            /*restricted_value=*/false, /*comma_ends_declaration=*/false,
            important, context_->GetExecutionContext());
    if (!variable_data) {
      return false;
    }

    value = MakeGarbageCollected<CSSUnparsedDeclarationValue>(variable_data,
                                                              context_);
  }
  parsed_properties_.push_back(
      CSSPropertyValue(CSSPropertyName(variable_name), *value, important));
  context_->Count(context_->Mode(), CSSPropertyID::kVariable);
  return true;
}

// NOTE: Leading whitespace must be stripped from the stream, since
// ParseValue() has the same requirement.
void CSSParserImpl::ConsumeDeclarationValue(CSSParserTokenStream& stream,
                                            CSSPropertyID unresolved_property,
                                            bool is_in_declaration_list,
                                            StyleRule::RuleType rule_type) {
  const bool allow_important_annotation = is_in_declaration_list &&
                                          rule_type != StyleRule::kKeyframe &&
                                          rule_type != StyleRule::kPositionTry;
  CSSPropertyParser::ParseValue(unresolved_property, allow_important_annotation,
                                stream, context_, parsed_properties_,
                                rule_type);
}

std::unique_ptr<Vector<KeyframeOffset>> CSSParserImpl::ConsumeKeyframeKeyList(
    const CSSParserContext* context,
    CSSParserTokenStream& stream) {
  std::unique_ptr<Vector<KeyframeOffset>> result =
      std::make_unique<Vector<KeyframeOffset>>();
  while (true) {
    stream.ConsumeWhitespace();
    const CSSParserToken& token = stream.Peek();
    if (token.GetType() == kPercentageToken && token.NumericValue() >= 0 &&
        token.NumericValue() <= 100) {
      result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone,
                                       token.NumericValue() / 100));
      stream.ConsumeIncludingWhitespace();
    } else if (token.GetType() == kIdentToken) {
      if (EqualIgnoringASCIICase(token.Value(), "from")) {
        result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone, 0));
        stream.ConsumeIncludingWhitespace();
      } else if (EqualIgnoringASCIICase(token.Value(), "to")) {
        result->push_back(KeyframeOffset(TimelineOffset::NamedRange::kNone, 1));
        stream.ConsumeIncludingWhitespace();
      } else {
        auto* stream_name_percent = To<CSSValueList>(
            css_parsing_utils::ConsumeTimelineRangeNameAndPercent(stream,
                                                                  *context));
        if (!stream_name_percent) {
          return nullptr;
        }

        auto stream_name = To<CSSIdentifierValue>(stream_name_percent->Item(0))
                               .ConvertTo<TimelineOffset::NamedRange>();
        auto percent =
            To<CSSPrimitiveValue>(stream_name_percent->Item(1)).GetFloatValue();

        if (!RuntimeEnabledFeatures::ScrollTimelineEnabled() &&
            stream_name != TimelineOffset::NamedRange::kNone) {
          return nullptr;
        }

        result->push_back(KeyframeOffset(stream_name, percent / 100.0));
      }
    } else {
      return nullptr;
    }

    if (stream.Peek().GetType() != kCommaToken) {
      return result;
    }
    stream.Consume();
  }
}

CSSParserMode CSSParserImpl::GetMode() const {
  return context_->Mode();
}

}  // namespace blink
