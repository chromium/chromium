// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/parser/css_parser_impl.h"

#include <bitset>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/cpu.h"
#include "third_party/blink/renderer/core/animation/timeline_offset.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/css/css_custom_ident_value.h"
#include "third_party/blink/renderer/core/css/css_font_family_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_try_rule.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_syntax_string_parser.h"
#include "third_party/blink/renderer/core/css/css_unparsed_declaration_value.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/container_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_at_rule_id.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_parsing_state.h"
#include "third_party/blink/renderer/core/css/parser/css_lazy_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_stream.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_selector_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_supports_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/parser/css_variable_parser.h"
#include "third_party/blink/renderer/core/css/parser/find_length_of_declaration_list-inl.h"
#include "third_party/blink/renderer/core/css/parser/media_query_parser.h"
#include "third_party/blink/renderer/core/css/parser/navigation_parser.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_feature_values.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/core/css/style_rule_function_declarations.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_keyframe.h"
#include "third_party/blink/renderer/core/css/style_rule_namespace.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/css/style_rule_route.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transition.h"
#include "third_party/blink/renderer/core/css/style_scope.h"
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
    case kCSSFunctionDescriptorsMode:
      return StyleRule::kFunction;
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
      NOTREACHED();
  }
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

unsigned CSSParserImpl::ParseValue(HeapVector<CSSPropertyValue, 8>& result,
                                   CSSPropertyID unresolved_property,
                                   StringView string,
                                   const CSSParserContext* context) {
  STACK_UNINITIALIZED CSSParserImpl parser(context);
  CSSParserTokenStream stream(string);
  parser.ConsumeDeclarationValue(stream, unresolved_property,
                                 /*is_in_declaration_list=*/false,
                                 StyleRule::kStyle);
  result.AppendVector(parser.parsed_properties_);
  return parser.parsed_properties_.size();
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
    HeapVector<CSSPropertyValue, 64>& values,
    wtf_size_t& unused_entries,
    std::bitset<kNumCSSProperties>& seen_properties,
    HashSet<AtomicString>& seen_custom_properties) {
  // Move !important declarations last, using a simple insertion sort.
  // This is O(n²), but n is typically small, and std::stable_partition
  // wants to allocate memory to get to O(n), which is overkill here.
  // Moreover, this is O(n) if there are no !important properties
  // (the common case) or only !important properties.
  wtf_size_t last_nonimportant_idx = values.size() - 1;
  for (wtf_size_t i = values.size(); i--;) {
    if (values[i].IsImportant()) {
      if (i != last_nonimportant_idx) {
        // Move this element to the end, preserving the order
        // of the other elements.
        CSSPropertyValue tmp = std::move(values[i]);
        for (unsigned j = i; j < last_nonimportant_idx; ++j) {
          values[j] = std::move(values[j + 1]);
        }
        values[last_nonimportant_idx] = std::move(tmp);
      }
      --last_nonimportant_idx;
    }
  }

  // Add properties in reverse order so that highest priority definitions are
  // reached first. Duplicate definitions can then be ignored when found.
  for (wtf_size_t i = values.size(); i--;) {
    const CSSPropertyValue& property = values[i];
    if (property.PropertyID() == CSSPropertyID::kVariable) {
      const AtomicString& name = property.CustomPropertyName();
      if (seen_custom_properties.Contains(name)) {
        continue;
      }
      seen_custom_properties.insert(name);
    } else {
      const unsigned property_id_index =
          GetCSSPropertyIDIndex(property.PropertyID());
      if (seen_properties.test(property_id_index)) {
        continue;
      }
      seen_properties.set(property_id_index);
    }
    values[--unused_entries] = property;
  }
}

static ImmutableCSSPropertyValueSet* CreateCSSPropertyValueSet(
    HeapVector<CSSPropertyValue, 64>& parsed_properties,
    CSSParserMode mode,
    const Document* document) {
  if (mode != kHTMLQuirksMode && (parsed_properties.size() < 2 ||
                                  (parsed_properties.size() == 2 &&
                                   parsed_properties[0].PropertyID() !=
                                       parsed_properties[1].PropertyID()))) {
    // Fast path for the situations where we can trivially detect that there can
    // be no collision between properties, and don't need to reorder, make
    // bitsets, or similar.
    ImmutableCSSPropertyValueSet* result =
        ImmutableCSSPropertyValueSet::Create(parsed_properties, mode);
    parsed_properties.resize(0);  // clear() deallocates the backing.
    return result;
  }

  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parsed_properties.size();
  HashSet<AtomicString> seen_custom_properties;

  FilterProperties(parsed_properties, unused_entries, seen_properties,
                   seen_custom_properties);

  // TODO: When we remove this use counter, we can move seen_properties
  // into FilterProperties().
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
      base::span(parsed_properties).subspan(unused_entries), mode,
      count_cursor_hand);
  parsed_properties.resize(0);  // clear() deallocates the backing.
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
  parser.ConsumeBlockContents(stream, StyleRule::kStyle, CSSNestingType::kNone,
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
  parser.ConsumeBlockContents(stream, StyleRule::kStyle, CSSNestingType::kNone,
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
  parser.ConsumeBlockContents(stream, rule_type, CSSNestingType::kNone,
                              /*parent_rule_for_nesting=*/nullptr,
                              /*nested_declarations_start_index=*/kNotFound,
                              /*child_rules=*/nullptr);
  if (parser.parsed_properties_.empty()) {
    return false;
  }

  std::bitset<kNumCSSProperties> seen_properties;
  wtf_size_t unused_entries = parser.parsed_properties_.size();
  HashSet<AtomicString> seen_custom_properties;
  FilterProperties(parser.parsed_properties_, unused_entries, seen_properties,
                   seen_custom_properties);
  return declaration->AddParsedProperties(
      base::span(parser.parsed_properties_).subspan(unused_entries));
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
  // See comment above CSSParserImpl::ConsumeBlockContents (definition)
  // for more on nested_declarations_start_index.
  parser.ConsumeBlockContents(stream, StyleRule::RuleType::kStyle, nesting_type,
                              parent_rule_for_nesting,
                              /*nested_declarations_start_index=*/0u,
                              &child_rules);

  return child_rules.size() == 1u ? child_rules.back().Get() : nullptr;
}

StyleRuleBase* CSSParserImpl::ParseRule(const String& string,
                                        const CSSParserContext* context,
                                        CSSNestingType nesting_type,
                                        StyleRule* parent_rule_for_nesting,
                                        StyleSheetContents* style_sheet,
                                        AllowedRules allowed_rules) {
  CSSParserImpl parser(context, style_sheet);
  CSSParserTokenStream stream(string);
  stream.ConsumeWhitespace();
  if (stream.UncheckedAtEnd()) {
    return nullptr;  // Parse error, empty rule
  }
  StyleRuleBase* rule;
  if (stream.UncheckedPeek().GetType() == kAtKeywordToken) {
    rule = parser.ConsumeAtRule(stream, allowed_rules, nesting_type,
                                parent_rule_for_nesting);
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
      stream, kTopLevelRules, /*allow_cdo_cdc_tokens=*/true,
      CSSNestingType::kNone,
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
  parsed_properties_.resize(0);  // clear() deallocates the backing.
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
  parser.ConsumeBlockContents(stream, StyleRule::kStyle, CSSNestingType::kNone,
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
  bool first_rule_valid = parser.ConsumeRuleList(
      stream, kTopLevelRules, /*allow_cdo_cdc_tokens=*/true,
      CSSNestingType::kNone,
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
  parser.ConsumeBlockContents(stream, StyleRule::kStyle, CSSNestingType::kNone,
                              /*parent_rule_for_nesting=*/nullptr,
                              /*nested_declarations_start_index=*/kNotFound,
                              /*child_rules=*/nullptr);
  return CreateCSSPropertyValueSet(parser.parsed_properties_, context->Mode(),
                                   context->GetDocument());
}

static AllowedRules ComputeNewAllowedRules(
    AllowedRules old_allowed_rules,
    StyleRuleBase* rule,
    bool& seen_import_or_namespace_rule) {
  if (!rule) {
    return old_allowed_rules;
  }
  // Certain rules have ordering restrictions; we expect to see them
  // in this order:
  //
  // - @charset
  // - [ @layer (statement) ]
  // - @import
  // - @namespace
  //
  // The restrictions are applied by disallowing certain rule types once
  // a "later" rule has been seen, for example: once @import (or @namespace,
  // or any later regular rule) has been seen, it's too late to parse @charset.
  //
  // @layer statement rules are in brackets above because they are special:
  // they can be used before @import/namespace rules (without causing them
  // to become disallowed), but can *also* be used as a regular rule
  // (i.e. where @layer block rules are allowed).
  //
  // https://drafts.csswg.org/css-cascade-5/#layer-empty
  AllowedRules new_allowed_rules = old_allowed_rules;
  if (rule->IsCharsetRule()) {
    // @charset is only allowed once.
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleCharset);
  } else if (rule->IsLayerStatementRule() && !seen_import_or_namespace_rule) {
    // Any number of @layer statements may appear before @import rules.
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleCharset);
  } else if (rule->IsImportRule()) {
    // @layer statements are still allowed once @import rules have been seen,
    // but they are treated as regular rules ("else" branch).
    seen_import_or_namespace_rule = true;
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleCharset);
  } else if (rule->IsNamespaceRule()) {
    // @layer statements are still allowed once @namespace rules have been seen,
    // but they are treated as regular rules ("else" branch).
    seen_import_or_namespace_rule = true;
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleCharset);
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleImport);
  } else {
    // Any regular rule must come after @charset/@import/@namespace.
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleCharset);
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleImport);
    new_allowed_rules.Remove(CSSAtRuleID::kCSSAtRuleNamespace);
  }
  return new_allowed_rules;
}

template <typename T>
bool CSSParserImpl::ConsumeRuleList(CSSParserTokenStream& stream,
                                    AllowedRules allowed_rules,
                                    bool allow_cdo_cdc_tokens,
                                    CSSNestingType nesting_type,
                                    StyleRule* parent_rule_for_nesting,
                                    const T callback) {
  bool seen_rule = false;
  bool seen_import_or_namespace_rule = false;
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
        if (allow_cdo_cdc_tokens) {
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
      allowed_rules = ComputeNewAllowedRules(allowed_rules, rule,
                                             seen_import_or_namespace_rule);
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
    AllowedRules allowed_rules,
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
    AllowedRules allowed_rules,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  if (!allowed_rules.Has(id)) {
    ConsumeErroneousAtRule(stream, id);
    return nullptr;
  }

  if (id != CSSAtRuleID::kCSSAtRuleInvalid &&
      context_->IsUseCounterRecordingEnabled()) {
    CountAtRule(context_, id);
  }

  stream.EnsureLookAhead();
  switch (id) {
    case CSSAtRuleID::kCSSAtRuleViewTransition:
      return ConsumeViewTransitionRule(stream);
    case CSSAtRuleID::kCSSAtRuleContainer:
      return ConsumeContainerRule(stream, nesting_type,
                                  parent_rule_for_nesting);
    case CSSAtRuleID::kCSSAtRuleMedia:
      return ConsumeMediaRule(stream, nesting_type, parent_rule_for_nesting);
    case CSSAtRuleID::kCSSAtRuleSupports:
      return ConsumeSupportsRule(stream, nesting_type, parent_rule_for_nesting);
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
    case CSSAtRuleID::kCSSAtRuleRoute:
      return ConsumeRouteRule(stream);
    case CSSAtRuleID::kCSSAtRuleNavigation:
      return ConsumeNavigationRule(stream, nesting_type,
                                   parent_rule_for_nesting);
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
    case CSSAtRuleID::kCSSAtRuleContents:
      return ConsumeContentsRule(stream);
    case CSSAtRuleID::kCSSAtRulePositionTry:
      return ConsumePositionTryRule(stream);
    case CSSAtRuleID::kCSSAtRuleCharset:
      return ConsumeCharsetRule(stream);
    case CSSAtRuleID::kCSSAtRuleImport: {
      // @import rules have a URI component that is not technically part of the
      // prelude.
      AtomicString uri = ConsumeStringOrURI(stream);
      stream.EnsureLookAhead();
      return ConsumeImportRule(std::move(uri), stream);
    }
    case CSSAtRuleID::kCSSAtRuleNamespace:
      return ConsumeNamespaceRule(stream);
    case CSSAtRuleID::kCSSAtRuleStylistic:
    case CSSAtRuleID::kCSSAtRuleStyleset:
    case CSSAtRuleID::kCSSAtRuleCharacterVariant:
    case CSSAtRuleID::kCSSAtRuleSwash:
    case CSSAtRuleID::kCSSAtRuleOrnaments:
    case CSSAtRuleID::kCSSAtRuleAnnotation:
      return ConsumeFontFeatureRule(id, stream);
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
      return ConsumePageMarginRule(id, stream);
    case CSSAtRuleID::kCSSAtRuleCustomMedia:
      return ConsumeCustomMediaRule(stream);
    case CSSAtRuleID::kCSSAtRuleInvalid:
    case CSSAtRuleID::kCount:
      ConsumeErroneousAtRule(stream, id);
      return nullptr;  // Parse error, unrecognised or not-allowed at-rule
  }
}

StyleRuleBase* CSSParserImpl::ConsumeQualifiedRule(
    CSSParserTokenStream& stream,
    AllowedRules allowed_rules,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // TODO(andruud): This function assumes 'nested=false', even though
  // a CSSNestingType and parent rule is provided. This means error recovery
  // always works as if non-nested, which is fragile.

  if (allowed_rules.Has(QualifiedRuleType::kStyle)) {
    bool invalid_rule_error_ignored = false;  // Only relevant when nested.
    return ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                            /* nested */ false, invalid_rule_error_ignored);
  }

  if (allowed_rules.Has(QualifiedRuleType::kKeyframe)) {
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

  // We still consume a qualified rule per css-syntax even when no rule
  // is allowed. This "error recovery" allows ConsumeRuleList to use
  // this function as the default branch.
  //
  // https://drafts.csswg.org/css-syntax/#consume-qualified-rule

  // Discard prelude and block.
  stream.SkipUntilPeekedTypeIs<kLeftBraceToken>();
  if (stream.Peek().GetType() == kLeftBraceToken) {
    CSSParserTokenStream::BlockGuard guard(stream);
  }

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

  ConsumeBlockContents(stream, StyleRule::kPageMargin, CSSNestingType::kNone,
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

  const StyleScope* style_scope = nullptr;
  if (RuntimeEnabledFeatures::CSSScopeImportEnabled() &&
      stream.Peek().FunctionId() == CSSValueID::kScope) {
    {
      CSSParserTokenStream::RestoringBlockGuard guard(stream);
      stream.ConsumeWhitespace();
      style_scope =
          StyleScope::Parse(stream, context_, CSSNestingType::kNone,
                            /*parent_rule_for_nesting=*/nullptr, style_sheet_);
      if (!guard.Release()) {
        style_scope = nullptr;
      }
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
      uri, std::move(layer), style_scope,
      supported == CSSSupportsParser::Result::kSupported,
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

namespace {

// Returns a :where(:scope) selector.
//
// Nested declaration rules within @scope behave as :where(:scope) rules.
//
// https://github.com/w3c/csswg-drafts/issues/10431
HeapVector<CSSSelector> WhereScopeSelector() {
  HeapVector<CSSSelector> selectors;

  CSSSelector inner[1] = {
      CSSSelector(AtomicString("scope"), /* implicit */ false)};
  inner[0].SetLastInComplexSelector(true);
  inner[0].SetLastInSelectorList(true);
  CSSSelectorList* inner_list =
      CSSSelectorList::AdoptSelectorVector(base::span<CSSSelector>(inner));

  CSSSelector where;
  where.SetWhere(inner_list);
  where.SetScopeContaining(true);
  selectors.push_back(where);

  selectors.back().SetLastInComplexSelector(true);
  selectors.back().SetLastInSelectorList(true);

  return selectors;
}

// https://drafts.csswg.org/css-nesting-1/#nested-declarations-rule
StyleRuleNestedDeclarations* CreateNestedDeclarationsRule(
    CSSNestingType nesting_type,
    const CSSParserContext& context,
    HeapVector<CSSSelector> selectors,
    HeapVector<CSSPropertyValue, 64>& declarations) {
  return MakeGarbageCollected<StyleRuleNestedDeclarations>(
      nesting_type,
      StyleRule::Create(selectors,
                        CreateCSSPropertyValueSet(declarations, context.Mode(),
                                                  context.GetDocument())));
}

}  // namespace

StyleRuleBase* CSSParserImpl::CreateDeclarationsRule(
    CSSNestingType nesting_type,
    const CSSSelector* selector_list,
    wtf_size_t start_index) {
  DCHECK(selector_list || (nesting_type != CSSNestingType::kNesting));

  // Create a nested declarations rule containing all declarations from
  // start_index to the end.
  HeapVector<CSSPropertyValue, 64> declarations(
      base::span(parsed_properties_).subspan(start_index));

  // Create the selector for StyleRuleNestedDeclarations's inner StyleRule.

  switch (nesting_type) {
    case CSSNestingType::kNone:
      break;
    case CSSNestingType::kNesting:
      // For regular nesting, the nested declarations rule should match
      // exactly what the parent rule matches, with top-level specificity
      // behavior. This means the selector list is copied rather than just
      // being referenced with '&'.
      return blink::CreateNestedDeclarationsRule(
          nesting_type, *context_,
          /*selectors=*/CSSSelectorList::Copy(selector_list), declarations);
    case CSSNestingType::kScope:
      // For direct nesting within @scope
      // (e.g. .foo { @scope (...) { color:green } }),
      // the nested declarations rule should match like a :where(:scope) rule.
      //
      // https://github.com/w3c/csswg-drafts/issues/10431
      return blink::CreateNestedDeclarationsRule(
          nesting_type, *context_,
          /*selectors=*/WhereScopeSelector(), declarations);
    case CSSNestingType::kFunction:
      // For descriptors within @function, e.g.:
      //
      //  @function --x() {
      //    --local: 1px;
      //    result: var(--local);
      //  }
      //
      return MakeGarbageCollected<StyleRuleFunctionDeclarations>(
          *CreateCSSPropertyValueSet(declarations, kCSSFunctionDescriptorsMode,
                                     context_->GetDocument()));
  }

  NOTREACHED();
}

void CSSParserImpl::EmitDeclarationsRuleIfNeeded(
    StyleRule::RuleType rule_type,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    wtf_size_t start_index,
    HeapVector<Member<StyleRuleBase>, 4>& child_rules) {
  if (rule_type == StyleRule::kPage) {
    // @page does not keep interleaved declarations "in place" by means of
    // CSSNestedDeclarations; they are effectively shifted to the top instead.
    return;
  }
  if (start_index == kNotFound) {
    return;
  }
  // The spec only allows creating non-empty rules, however, the inspector needs
  // empty rules to appear as well. This has no effect on the styles seen by
  // the page (the styles parsed with an `observer_` are for local use in the
  // inspector only).
  const bool emit_empty_rule = observer_;
  if (start_index >= parsed_properties_.size() && !emit_empty_rule) {
    return;
  }

  StyleRuleBase* nested_declarations_rule = CreateDeclarationsRule(
      nesting_type,
      parent_rule_for_nesting ? parent_rule_for_nesting->FirstSelector()
                              : nullptr,
      start_index);
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
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

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
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

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
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

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
    observer_->StartRuleBody(stream.Offset());
  }

  if (style_sheet_) {
    style_sheet_->SetHasFontFaceRule();
  }

  ConsumeBlockContents(stream, StyleRule::kFontFace, CSSNestingType::kNone,
                       /*parent_rule_for_nesting=*/nullptr,
                       /*nested_declarations_start_index=*/kNotFound,
                       /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

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
  if (name_token.GetType() == kIdentToken &&
      css_parsing_utils::IsValidIdentAnimationName(
          name_token.Value().ToAtomicString())) {
    name = name_token.Value().ToString();
  } else if (name_token.GetType() == kStringToken) {
    context_->Count(WebFeature::kOBSOLETE_QuotedKeyframesRule);
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
      stream, kKeyframeRules, /*allow_cdo_cdc_tokens=*/false,
      CSSNestingType::kNone,
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

StyleRuleFontFeature* CSSParserImpl::ConsumeFontFeatureRuleBlock(
    StyleRuleFontFeature::FeatureType feature_type,
    CSSParserTokenStream& stream) {
  wtf_size_t max_allowed_values = 1;
  if (feature_type == StyleRuleFontFeature::FeatureType::kCharacterVariant) {
    max_allowed_values = 2;
  }
  if (feature_type == StyleRuleFontFeature::FeatureType::kStyleset) {
    max_allowed_values = std::numeric_limits<wtf_size_t>::max();
  }
  auto* font_feature_rule =
      MakeGarbageCollected<StyleRuleFontFeature>(feature_type);

  while (!stream.AtEnd()) {
    const CSSParserToken& alias_token = stream.Peek();

    wtf_size_t decl_offset_start = stream.Offset();
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
      std::optional<double> number = number_value->GetValueIfKnown();
      if (!number.has_value()) {
        return nullptr;
      }
      parsed_numbers.push_back(ClampTo<int>(number.value()));
    }

    if (observer_) {
      observer_->ObserveProperty(decl_offset_start, stream.LookAheadOffset(),
                                 /*is_important=*/false, /*is_parsed=*/true);
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

StyleRuleFontFeature* CSSParserImpl::ConsumeFontFeatureRule(
    CSSAtRuleID rule_id,
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();

  std::optional<StyleRuleFontFeature::FeatureType> feature_type =
      ToStyleRuleFontFeatureType(rule_id);
  if (!feature_type) {
    return nullptr;
  }

  stream.ConsumeWhitespace();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();

  if (stream.Peek().GetType() != kLeftBraceToken) {
    return nullptr;
  }

  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFontFeature, prelude_offset_start);
    observer_->ObserveFontFeatureType(*feature_type);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  stream.ConsumeWhitespace();
  auto* font_feature_rule = ConsumeFontFeatureRuleBlock(*feature_type, stream);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
    if (!font_feature_rule) {
      observer_->ObserveErroneousAtRule(prelude_offset_start, rule_id, {});
    }
  }

  return font_feature_rule;
}

StyleRuleFontFeatureValues* CSSParserImpl::ConsumeFontFeatureValuesRule(
    CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  CSSValueList* family_list =
      css_parsing_utils::ConsumeFontFamily(stream, *context_);
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
          stream, kFontFeatureRules, /*allow_cdo_cdc_tokens=*/false,
          CSSNestingType::kNone,
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
  for (const auto& family_entry : *family_list) {
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
  ConsumeBlockContents(stream, StyleRule::kPage, CSSNestingType::kNone,
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
  const CSSParserToken& name_token = stream.Peek();
  if (!CSSVariableParser::IsValidVariableName(name_token)) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleProperty);
    return nullptr;
  }
  String name = name_token.Value().ToString();
  stream.ConsumeIncludingWhitespace();
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

  ConsumeBlockContents(stream, StyleRule::kProperty, CSSNestingType::kNone,
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

StyleRuleRoute* CSSParserImpl::ConsumeRouteRule(CSSParserTokenStream& stream) {
  // Parse the prelude.
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();
  const CSSParserToken& name_token = stream.Peek();
  // <dashed-ident>
  String name;
  if (name_token.GetType() == kIdentToken) {
    name = name_token.Value().ToString();
    if (!name.StartsWith("--")) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleRoute);
      return nullptr;
    }
  } else {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleRoute);
    return nullptr;
  }
  stream.ConsumeIncludingWhitespace();
  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleRoute)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kRoute, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  ConsumeBlockContents(stream, StyleRule::kRoute, CSSNestingType::kNone,
                       /*parent_rule_for_nesting=*/nullptr,
                       /*nested_declarations_start_index=*/kNotFound,
                       /*child_rules=*/nullptr);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  // TODO(crbug.com/436805487): Honor [ <pattern-descriptors> |
  // <init-descriptors> ] (it should either be a URLPattern, OR init
  // descriptors, not a combination).
  return MakeGarbageCollected<StyleRuleRoute>(
      name, CreateCSSPropertyValueSet(parsed_properties_, context_->Mode(),
                                      context_->GetDocument()));
}

StyleRuleNavigation* CSSParserImpl::ConsumeNavigationRule(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting) {
  // Parse the prelude.
  wtf_size_t header_start_offset = stream.LookAheadOffset();
  NavigationQuery* query =
      NavigationParser::ParseQuery(stream, *context_->GetDocument());
  if (!query) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleNavigation);
    return nullptr;
  }
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(
          stream, CSSAtRuleID::kCSSAtRuleNavigation)) {
    return nullptr;
  }
  wtf_size_t header_end_offset = stream.LookAheadOffset();

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard body_guard(stream);
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kNavigation, header_start_offset);
    observer_->EndRuleHeader(header_end_offset);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return MakeGarbageCollected<StyleRuleNavigation>(query, std::move(rules));
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

  ConsumeBlockContents(stream, StyleRule::kCounterStyle, CSSNestingType::kNone,
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

  ConsumeBlockContents(stream, StyleRule::kFontPaletteValues,
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
  auto* style_scope = StyleScope::Parse(stream, context_, nesting_type,
                                        parent_rule_for_nesting, style_sheet_);
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

  HeapVector<Member<StyleRuleBase>, 4> rules;
  ConsumeBlockContents(
      stream, StyleRule::kScope, CSSNestingType::kScope,
      /*parent_rule_for_nesting=*/style_scope->RuleForNesting(),
      /*nested_declarations_start_index=*/0, &rules);

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return MakeGarbageCollected<StyleRuleScope>(*style_scope, std::move(rules));
}

StyleRuleViewTransition* CSSParserImpl::ConsumeViewTransitionRule(
    CSSParserTokenStream& stream) {
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
  ConsumeBlockContents(stream, StyleRule::kViewTransition,
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

  const ConditionalExpNode* query = query_parser.ParseCondition(stream);
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
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

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
  ConsumeRuleListOrNestedDeclarationList(stream, nesting_type,
                                         parent_rule_for_nesting, &rules);

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

  ConsumeBlockContents(stream, StyleRule::kPositionTry, CSSNestingType::kNone,
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

// Consume a type for CSS Functions; e.g. <length>, <color>, etc..
//
// https://drafts.csswg.org/css-mixins-1/#typedef-css-type
static std::optional<CSSSyntaxDefinition> ConsumeFunctionType(
    CSSParserTokenStream& stream) {
  // The <syntax> must generally be wrapped in type().
  if (stream.Peek().FunctionId() == CSSValueID::kType) {
    CSSParserTokenStream::RestoringBlockGuard guard(stream);
    stream.ConsumeWhitespace();
    std::optional<CSSSyntaxDefinition> type =
        CSSSyntaxDefinition::Consume(stream);
    if (type.has_value() && guard.Release()) {
      stream.ConsumeWhitespace();
      return type;
    }
  }
  // However, a lone <syntax-component> may appear unwrapped.
  return CSSSyntaxDefinition::ConsumeComponent(stream);
}

StyleRuleFunction* CSSParserImpl::ConsumeFunctionRule(
    CSSParserTokenStream& stream) {
  wtf_size_t prelude_offset_start = stream.LookAheadOffset();

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
  std::optional<HeapVector<StyleRuleFunction::Parameter>> parameters;
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

  std::optional<CSSSyntaxDefinition> return_type;
  if (stream.Peek().Id() == CSSValueID::kReturns) {
    stream.ConsumeIncludingWhitespace();  // kReturns
    return_type = ConsumeFunctionType(stream);
    if (!return_type.has_value()) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleFunction);
      return nullptr;
    }
  } else {
    return_type = CSSSyntaxDefinition::CreateUniversal();
  }

  wtf_size_t prelude_offset_end = stream.LookAheadOffset();
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleFunction)) {
    return nullptr;
  }

  // Parse the actual block.
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kFunction, prelude_offset_start);
    observer_->EndRuleHeader(prelude_offset_end);
    observer_->StartRuleBody(stream.Offset());
  }

  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  ConsumeBlockContents(stream, StyleRule::kFunction, CSSNestingType::kFunction,
                       /*parent_rule_for_nesting=*/nullptr,
                       /*nested_declarations_start_index=*/0, &child_rules,
                       /*has_visited_pseudo=*/false);

  if (observer_) {
    observer_->EndRuleBody(stream.LookAheadOffset());
  }

  return MakeGarbageCollected<StyleRuleFunction>(
      name, std::move(*parameters),
      HeapVector<Member<StyleRuleBase>>(child_rules), std::move(*return_type));
}

StyleRuleMixin* CSSParserImpl::ConsumeMixinRule(CSSParserTokenStream& stream) {
  wtf_size_t header_start = stream.LookAheadOffset();

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

  // Parse the prelude; just a function token (the name) and some arguments.
  if (stream.Peek().GetType() != kFunctionToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;  // Parse error.
  }
  AtomicString name = stream.Peek().Value().ToAtomicString();
  if (!name.StartsWith("--")) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;
  }

  // Parse the argument list (which may be empty).
  std::optional<HeapVector<StyleRuleFunction::Parameter>> parameters;
  {
    CSSParserTokenStream::BlockGuard guard(stream);
    stream.ConsumeWhitespace();
    parameters = ConsumeFunctionParameters(stream);
  }
  if (!parameters.has_value()) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleMixin);
    return nullptr;
  }
  stream.ConsumeWhitespace();

  // After the argument list, there should be nothing (there's no return value,
  // unlike with functions).
  if (!ConsumeEndOfPreludeForAtRuleWithBlock(stream,
                                             CSSAtRuleID::kCSSAtRuleMixin)) {
    return nullptr;
  }
  wtf_size_t header_end = stream.LookAheadOffset();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kApplyMixin, header_start);
    observer_->EndRuleHeader(header_end);
  }

  // Parse the actual block.
  StyleRule* fake_parent_rule;
  {
    base::AutoReset<bool> reset_in_nested_style_rule(&in_mixin_, true);
    fake_parent_rule = ConsumeDeclarationListForMixins(stream);
  }

  // ConsumeDeclarationListForMixins() must have a fake parent rule in case
  // there are any rules containing parent selectors (including raw
  // declarations, as they are wrapped in an implicit nested block); however,
  // StyleRuleMixin is a StyleRuleGroup and expects to own its rules itself.
  // This means that even though fake_parent_rule is the parent pointed to by
  // the selectors (and will be kept alive by them), it doesn't actually
  // contain the child rules and isn't used for anything anymore. Once
  // a mixin is actually used (in @apply), we clone all the rules and call
  // Clone(), which changes all the parent references to @apply's parent.
  fake_parent_rule->EnsureChildRules();
  return MakeGarbageCollected<StyleRuleMixin>(
      name, std::move(*parameters),
      HeapVector{std::move(*fake_parent_rule->ChildRules())});
}

StyleRule* CSSParserImpl::ConsumeDeclarationListForMixins(
    CSSParserTokenStream& stream) {
  CSSParserTokenStream::BlockGuard guard(stream);

  if (observer_) {
    observer_->StartRuleBody(stream.Offset());
  }

  // When we encounter a declaration list, the selector of our fake parent rule
  // will be _copied_, so it needs to be something sane; the implicit @nest rule
  // gives us the behavior that we want.
  CSSSelector dummy(/*parent_rule=*/nullptr, /*is_implicit=*/true);
  dummy.SetLastInSelectorList(true);
  dummy.SetLastInComplexSelector(true);

  // We do not use the properties for anything, but we need a valid pointer
  // or we will have a crash when we try to clone the rule during apply.
  ImmutableCSSPropertyValueSet* empty_properties =
      ImmutableCSSPropertyValueSet::Create({},
                                           CSSParserMode::kHTMLStandardMode);

  StyleRule* fake_parent_rule =
      StyleRule::Create(base::span_from_ref(dummy), empty_properties);
  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  ConsumeRuleListOrNestedDeclarationList(stream, CSSNestingType::kNesting,
                                         fake_parent_rule, &child_rules);
  for (StyleRuleBase* child_rule : child_rules) {
    fake_parent_rule->AddChildRule(child_rule);
  }

  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }

  return fake_parent_rule;
}

StyleRuleApplyMixin* CSSParserImpl::ConsumeApplyMixinRule(
    CSSParserTokenStream& stream) {
  wtf_size_t header_start = stream.LookAheadOffset();
  if (stream.Peek().GetType() != kIdentToken &&
      stream.Peek().GetType() != kFunctionToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;  // Parse error.
  }
  AtomicString name = stream.Peek().Value().ToAtomicString();
  if (!name.StartsWith("--")) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;
  }

  // Parse arguments, if any.
  HeapVector<Member<CSSVariableData>> arguments;
  if (stream.Peek().GetType() == kIdentToken) {
    // @apply --name ...
    stream.ConsumeIncludingWhitespace();
  } else {
    // @apply --name( ...
    if (!CSSVariableParser::ConsumeMixinArguments(stream, *context_,
                                                  arguments)) {
      ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
      return nullptr;
    }
  }

  stream.EnsureLookAhead();
  wtf_size_t header_end = stream.LookAheadOffset();
  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kApplyMixin, header_start);
    observer_->EndRuleHeader(header_end);
  }

  if (stream.AtEnd() || stream.UncheckedPeek().GetType() == kSemicolonToken) {
    // No declarations block, just a semicolon (possibly implicit.
    if (!stream.AtEnd()) {
      stream.UncheckedConsume();  // kSemicolonToken
    }
    if (observer_) {
      // Devtools expects to see a rule body for every rule; it is
      // the trigger for actually inserting the rule. So we need to
      // include a fake empty body here, or the indexing will be
      // messed up when the NestedDeclarations rules arrive.
      observer_->StartRuleBody(stream.Offset());
      observer_->EndRuleBody(stream.Offset());
    }
    return MakeGarbageCollected<StyleRuleApplyMixin>(name, std::move(arguments),
                                                     nullptr);
  }

  if (stream.UncheckedPeek().GetType() != kLeftBraceToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleApplyMixin);
    return nullptr;  // Parse error.
  }

  // Parse the @contents block.
  StyleRule* fake_parent_rule_for_contents =
      ConsumeDeclarationListForMixins(stream);
  return MakeGarbageCollected<StyleRuleApplyMixin>(
      name, std::move(arguments), fake_parent_rule_for_contents);
}

StyleRuleContentsStatement* CSSParserImpl::ConsumeContentsRule(
    CSSParserTokenStream& stream) {
  wtf_size_t header_start = stream.LookAheadOffset();
  stream.ConsumeWhitespace();
  if (stream.AtEnd()) {
    // Implicit semicolon at end of block.
    return MakeGarbageCollected<StyleRuleContentsStatement>(nullptr);
  }
  if (stream.UncheckedPeek().GetType() == kSemicolonToken) {
    // No block, just a semicolon.
    stream.UncheckedConsume();  // kSemicolonToken
    return MakeGarbageCollected<StyleRuleContentsStatement>(nullptr);
  }

  if (stream.UncheckedPeek().GetType() != kLeftBraceToken) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleContents);
    return nullptr;  // Parse error.
  }
  wtf_size_t header_end = stream.LookAheadOffset();

  if (observer_) {
    observer_->StartRuleHeader(StyleRule::kContents, header_start);
    observer_->EndRuleHeader(header_end);
    observer_->StartRuleBody(stream.Offset());
  }

  // Parse the actual block.
  StyleRule* fake_parent_rule = ConsumeDeclarationListForMixins(stream);
  if (observer_) {
    observer_->EndRuleBody(stream.Offset());
  }
  return MakeGarbageCollected<StyleRuleContentsStatement>(fake_parent_rule);
}

// Parse the parameters of a CSS function: Zero or more comma-separated
// instances of [ <name> <type>? [ : <default-value> ]? ].
// Returns the empty value on parse error.
std::optional<HeapVector<StyleRuleFunction::Parameter>>
CSSParserImpl::ConsumeFunctionParameters(CSSParserTokenStream& stream) {
  HeapVector<StyleRuleFunction::Parameter> parameters;
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

    std::optional<CSSSyntaxDefinition> type = ConsumeFunctionType(stream);

    CSSVariableData* default_value = nullptr;
    if (stream.Peek().GetType() == kColonToken) {
      stream.ConsumeIncludingWhitespace();

      // Note that this is a comma-containing production [1], and therefore
      // the value may not contain commas until we support the {} wrapper
      // defined by the spec.
      // [1] https://drafts.csswg.org/css-values-5/#component-function-commas
      bool important_ignored;
      default_value = CSSVariableParser::ConsumeUnparsedDeclaration(
          stream, /*allow_important_annotation=*/false,
          /*is_animation_tainted=*/false,
          /*must_contain_variable_reference=*/false,
          /*restricted_value=*/false,
          /*comma_ends_declaration=*/true, important_ignored, *context_);
    }

    // If a type and a default are both provided, the default must
    // parse successfully according to that type.
    //
    // https://drafts.csswg.org/css-mixins-1/#function-rule
    if (type.has_value() && default_value) {
      if (!default_value->NeedsVariableResolution() &&
          !type->Parse(default_value->OriginalText(), *context_,
                       /*is_animation_tainted=*/false,
                       /*is_attr_tainted=*/false)) {
        return std::nullopt;
      }
    }

    parameters.push_back(StyleRuleFunction::Parameter{
        parameter_name, type.value_or(CSSSyntaxDefinition::CreateUniversal()),
        default_value});
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

  ConsumeBlockContents(block, StyleRule::kKeyframe, CSSNestingType::kNone,
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

namespace {

// https://drafts.csswg.org/css-extensions-1/#typedef-extension-name
bool IsValidExtensionName(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken) {
    return false;
  }
  StringView value = token.Value();
  return value.length() >= 2 && value[0] == '-' && value[1] == '-';
}

std::optional<bool> GetBooleanValue(const CSSParserToken& token) {
  if (token.GetType() != kIdentToken) {
    return std::nullopt;
  }
  if (token.Value() == "true") {
    return true;
  }
  if (token.Value() == "false") {
    return false;
  }
  return std::nullopt;
}

}  // namespace

StyleRuleCustomMedia* CSSParserImpl::ConsumeCustomMediaRule(
    CSSParserTokenStream& stream) {
  const CSSParserToken& name_token = stream.Peek();
  if (!IsValidExtensionName(name_token)) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleCustomMedia);
    return nullptr;
  }
  String name = name_token.Value().ToString();
  stream.ConsumeIncludingWhitespace();

  std::optional<bool> bool_val = GetBooleanValue(stream.Peek());
  if (bool_val.has_value()) {
    stream.ConsumeIncludingWhitespace();
    if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
            stream, CSSAtRuleID::kCSSAtRuleCustomMedia)) {
      return nullptr;
    }
    return MakeGarbageCollected<StyleRuleCustomMedia>(
        StyleRuleCustomMedia(AtomicString(name), *bool_val));
  }

  MediaQuerySet* media_query_set = MediaQueryParser::ParseCustomMediaDefinition(
      stream, context_->GetExecutionContext());
  if (!media_query_set) {
    ConsumeErroneousAtRule(stream, CSSAtRuleID::kCSSAtRuleCustomMedia);
    return nullptr;
  }
  if (!ConsumeEndOfPreludeForAtRuleWithoutBlock(
          stream, CSSAtRuleID::kCSSAtRuleCustomMedia)) {
    return nullptr;
  }
  return MakeGarbageCollected<StyleRuleCustomMedia>(
      StyleRuleCustomMedia(AtomicString(name), media_query_set));
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
  bool custom_property_ambiguity =
      CSSVariableParser::StartsCustomPropertyDeclaration(stream);

  bool has_visited_pseudo = false;
  // Parse the prelude of the style rule
  base::span<CSSSelector> selector_vector = CSSSelectorParser::ConsumeSelector(
      stream, context_, nesting_type, parent_rule_for_nesting,
      /* semicolon_aborts_nested_selector*/ nested, style_sheet_, observer_,
      arena_, &has_visited_pseudo);

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
      // (see ConsumeBlockContents).
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

  // TODO(csharrison): How should we lazily parse css that needs the observer?
  if (!observer_ && lazy_state_) {
    DCHECK(style_sheet_);

    StringView text(stream.RemainingText(), 1);
#ifdef ARCH_CPU_X86_FAMILY
    wtf_size_t len;
    if (base::CPU::GetInstanceNoAllocation().has_avx2() &&
        base::CPU::GetInstanceNoAllocation().has_pclmul()) {
      len = static_cast<wtf_size_t>(FindLengthOfDeclarationListAVX2(text));
    } else {
      len = static_cast<wtf_size_t>(FindLengthOfDeclarationList(text));
    }
#else
    wtf_size_t len = static_cast<wtf_size_t>(FindLengthOfDeclarationList(text));
#endif
    if (len != 0) {
      wtf_size_t block_start_offset = stream.Offset();
      stream.SkipToEndOfBlock(len + 2);  // +2 for { and }.
      return StyleRule::Create(selector_vector,
                               MakeGarbageCollected<CSSLazyPropertyParser>(
                                   block_start_offset, lazy_state_));
    }
  }
  CSSParserTokenStream::BlockGuard guard(stream);
  return ConsumeStyleRuleContents(selector_vector, stream, has_visited_pseudo);
}

StyleRule* CSSParserImpl::ConsumeStyleRuleContents(
    base::span<CSSSelector> selector_vector,
    CSSParserTokenStream& stream,
    bool has_visited_pseudo) {
  StyleRule* style_rule = StyleRule::Create(selector_vector);
  HeapVector<Member<StyleRuleBase>, 4> child_rules;
  if (observer_) {
    observer_->StartRuleBody(stream.Offset());
  }
  ConsumeBlockContents(stream, StyleRule::kStyle, CSSNestingType::kNesting,
                       /*parent_rule_for_nesting=*/style_rule,
                       /*nested_declarations_start_index=*/kNotFound,
                       &child_rules, has_visited_pseudo);
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

// https://drafts.csswg.org/css-syntax/#consume-block-contents
//
// Consumes declarations and/or child rules from the block of a style rule
// or an at-rule (e.g. @media).
//
// The `nested_declarations_start_index` parameter controls how this function
// emits "nested declaration" rules for the leading block of declarations.
// For regular style rules (which can hold declarations directly), this should
// be kNotFound, which will prevent a wrapper rule for the leading block.
// (Subsequent declarations "interleaved" with child rules will still be
// wrapped). For nested group rules, or generally rules that cannot hold
// declarations directly (e.g. @media), the parameter value should be 0u,
// causing the leading declarations to get wrapped as well.
void CSSParserImpl::ConsumeBlockContents(
    CSSParserTokenStream& stream,
    StyleRule::RuleType rule_type,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    wtf_size_t nested_declarations_start_index,
    HeapVector<Member<StyleRuleBase>, 4>* child_rules,
    bool has_visited_pseudo) {
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
          EmitDeclarationsRuleIfNeeded(
              rule_type, nesting_type, parent_rule_for_nesting,
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
          consumed_declaration =
              ConsumeDeclaration(stream, rule_type, has_visited_pseudo);
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
        if (nesting_type != CSSNestingType::kNone &&
            nesting_type != CSSNestingType::kFunction) {
          bool invalid_rule_error = false;
          StyleRuleBase* child =
              ConsumeNestedRule(std::nullopt, rule_type, stream, nesting_type,
                                parent_rule_for_nesting, invalid_rule_error);
          if (child) {
            if (child_rules) {
              EmitDeclarationsRuleIfNeeded(
                  rule_type, nesting_type, parent_rule_for_nesting,
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

  // We need a final call to EmitDeclarationsRuleIfNeeded in case there
  // are trailing bare declarations. If no child rule has been observed,
  // nested_declarations_start_index is still kNotFound (UINT_MAX),
  // which causes EmitDeclarationsRuleIfNeeded to have no effect.
  if (child_rules) {
    EmitDeclarationsRuleIfNeeded(rule_type, nesting_type,
                                 parent_rule_for_nesting,
                                 nested_declarations_start_index, *child_rules);
  }
}

// Consumes a list of style rules and stores the result in `child_rules`,
// or (for nested group rules) consumes the interior of a nested group rule [1].
// Nested group rules allow a list of declarations to appear
// directly in place of where a list of rules would normally go.
//
// [1] https://drafts.csswg.org/css-nesting-1/#nested-group-rules
void CSSParserImpl::ConsumeRuleListOrNestedDeclarationList(
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    HeapVector<Member<StyleRuleBase>, 4>* child_rules) {
  DCHECK(child_rules);

  bool is_nested_group_rule = nesting_type == CSSNestingType::kNesting ||
                              nesting_type == CSSNestingType::kFunction;
  if (is_nested_group_rule) {
    // This is a nested group rule, which (in addition to rules) allows
    // *declarations* to appear directly within the body of the rule, e.g.:
    //
    // .foo {
    //    @media (width > 800px) {
    //      color: green;
    //    }
    //  }
    //
    // Using nested_declarations_start_index=0u here means that the leading
    // declarations will be wrapped in a CSSNestedDeclarations rule.
    // Unlike regular style rules, the leading declarations must be wrapped
    // in something that can hold them, because group rules (e.g. @media)
    // can not hold properties directly.
    //
    // RuleType determines which declarations are valid within the rule.
    // Within @function rules, only local variables and the 'result' descriptor
    // are allowed. All other cases accept regular properties without special
    // restrictions.
    StyleRule::RuleType rule_type = nesting_type == CSSNestingType::kFunction
                                        ? StyleRule::kFunction
                                        : StyleRule::kStyle;
    ConsumeBlockContents(stream, rule_type, nesting_type,
                         parent_rule_for_nesting,
                         /* nested_declarations_start_index */ 0u, child_rules);
  } else {
    ConsumeRuleList(stream, kRegularRules,
                    /*allow_cdo_cdc_tokens=*/false, nesting_type,
                    parent_rule_for_nesting,
                    [child_rules](StyleRuleBase* rule, wtf_size_t) {
                      child_rules->push_back(rule);
                    });
  }
}

namespace {

AllowedRules AllowedNestedRules(StyleRule::RuleType parent_rule_type,
                                bool in_nested_style_rule,
                                bool in_mixin) {
  switch (parent_rule_type) {
    case StyleRule::kScope:
      if (!in_nested_style_rule) {
        return CSSParserImpl::kRegularRules;
      }
      [[fallthrough]];
    case StyleRule::kStyle: {
      if (in_mixin) {
        AllowedRules allowed = CSSParserImpl::kNestedGroupRules |
                               AllowedRules{CSSAtRuleID::kCSSAtRuleContents};
        allowed.Remove(CSSAtRuleID::kCSSAtRuleLayer);
        return allowed;
      } else {
        return CSSParserImpl::kNestedGroupRules;
      }
    }
    case StyleRule::kPage:
      return CSSParserImpl::kPageMarginRules;
    case StyleRule::kFunction:
      return CSSParserImpl::kConditionalRules;
    default:
      break;
  }
  return AllowedRules();
}

}  // namespace

StyleRuleBase* CSSParserImpl::ConsumeNestedRule(
    std::optional<CSSAtRuleID> id,
    StyleRule::RuleType parent_rule_type,
    CSSParserTokenStream& stream,
    CSSNestingType nesting_type,
    StyleRule* parent_rule_for_nesting,
    bool& invalid_rule_error) {
  // A nested style rule. Recurse into the parser; we need to move the parsed
  // properties out of the way while we're parsing the child rule, though.
  HeapVector<CSSPropertyValue, 64> outer_parsed_properties;
  swap(parsed_properties_, outer_parsed_properties);
  StyleRuleBase* child;
  base::AutoReset<bool> reset_in_nested_style_rule(
      &in_nested_style_rule_,
      in_nested_style_rule_ || parent_rule_type == StyleRule::kStyle);
  if (!id.has_value()) {
    child = ConsumeStyleRule(stream, nesting_type, parent_rule_for_nesting,
                             /* nested */ true, invalid_rule_error);
  } else {
    child = ConsumeAtRuleContents(
        *id, stream,
        AllowedNestedRules(parent_rule_type, in_nested_style_rule_, in_mixin_),
        nesting_type, parent_rule_for_nesting);
  }
  parsed_properties_ = std::move(outer_parsed_properties);
  if (child && parent_rule_type != StyleRule::kPage &&
      parent_rule_type != StyleRule::kScope &&
      parent_rule_type != StyleRule::kFunction) {
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
// which may cause a restart at the call site (see ConsumeBlockContents,
// kIdentToken branch). If we are anyway going to restart, any work we do
// to leave the stream in a more consistent state is just wasted.
bool CSSParserImpl::ConsumeDeclaration(CSSParserTokenStream& stream,
                                       StyleRule::RuleType rule_type,
                                       bool has_visited_pseudo) {
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
                            rule_type == StyleRule::kRoute ||
                            rule_type == StyleRule::kCounterStyle ||
                            rule_type == StyleRule::kViewTransition ||
                            rule_type == StyleRule::kFunction;

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
      const AtomicString& variable_name =
          (atrule_id == AtRuleDescriptorID::Variable
               ? lhs.Value().ToAtomicString()
               : g_null_atom);
      AtRuleDescriptorParser::ParseDescriptorValue(
          rule_type, atrule_id, variable_name, stream, *context_,
          parsed_properties_);
    } else {
      const CSSPropertyID unresolved_property = static_cast<CSSPropertyID>(id);
      if (unresolved_property == CSSPropertyID::kVariable) {
        if (rule_type != StyleRule::kStyle && rule_type != StyleRule::kScope &&
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
                important, *context_);
          }
        } else {
          if (context_->IsUseCounterRecordingEnabled() && has_visited_pseudo &&
              unresolved_property == CSSPropertyID::kColumnRuleColor) {
            context_->Count(WebFeature::kVisitedColumnRuleColor);
          }
          ConsumeDeclarationValue(stream, unresolved_property,
                                  /*is_in_declaration_list=*/true, rule_type);
        }
      }
    }
  }
  if (observer_ &&
      (rule_type == StyleRule::kStyle || rule_type == StyleRule::kScope ||
       rule_type == StyleRule::kKeyframe || rule_type == StyleRule::kProperty ||
       rule_type == StyleRule::kPositionTry ||
       rule_type == StyleRule::kFontFace ||
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
          important, *context_);
    }

    // There could be remnants of a broken !important declaration,
    // that neither ConsumeUnparsedDeclaration() nor MaybeConsumeImportant()
    // would consume, but which Devtools wants us to include.
    stream.SkipUntilPeekedTypeIs<kLeftBraceToken, kSemicolonToken>();

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
            important, *context_);
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
        double percent =
            To<CSSNumericLiteralValue>(stream_name_percent->Item(1))
                .ClampedDoubleValue();
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
