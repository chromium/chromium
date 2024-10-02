/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/core/inspector/inspector_style_sheet.h"

#include <algorithm>
#include <memory>

#include "third_party/blink/renderer/core/css/css_container_rule.h"
#include "third_party/blink/renderer/core/css/css_font_palette_values_rule.h"
#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframe_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_layer_block_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_property_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_scope_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_local_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_observer.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands/custom_property.h"
#include "third_party/blink/renderer/core/css/properties/shorthand.h"
#include "third_party/blink/renderer/core/css/property_registry.h"
#include "third_party/blink/renderer/core/css/resolver/style_cascade.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_css_parser_observer.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_resource_container.h"
#include "third_party/blink/renderer/core/inspector/inspector_style_resolver.h"
#include "third_party/blink/renderer/core/inspector/protocol/css.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_regexp.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

using blink::protocol::Array;

namespace blink {

namespace {

static const CSSParserContext* ParserContextForDocument(
    const Document* document) {
  // Fallback to an insecure context parser if no document is present.
  return document ? MakeGarbageCollected<CSSParserContext>(*document)
                  : StrictCSSParserContext(SecureContextMode::kInsecureContext);
}

String FindMagicComment(const String& content, const String& name) {
  DCHECK(name.Find("=") == kNotFound);

  wtf_size_t length = content.length();
  wtf_size_t name_length = name.length();
  const bool kMultiline = true;

  wtf_size_t pos = length;
  wtf_size_t equal_sign_pos = 0;
  wtf_size_t closing_comment_pos = 0;
  while (true) {
    pos = content.ReverseFind(name, pos);
    if (pos == kNotFound)
      return g_empty_string;

    // Check for a /\/[\/*][@#][ \t]/ regexp (length of 4) before found name.
    if (pos < 4)
      return g_empty_string;
    pos -= 4;
    if (content[pos] != '/')
      continue;
    if ((content[pos + 1] != '/' || kMultiline) &&
        (content[pos + 1] != '*' || !kMultiline))
      continue;
    if (content[pos + 2] != '#' && content[pos + 2] != '@')
      continue;
    if (content[pos + 3] != ' ' && content[pos + 3] != '\t')
      continue;
    equal_sign_pos = pos + 4 + name_length;
    if (equal_sign_pos < length && content[equal_sign_pos] != '=')
      continue;
    if (kMultiline) {
      closing_comment_pos = content.Find("*/", equal_sign_pos + 1);
      if (closing_comment_pos == kNotFound)
        return g_empty_string;
    }

    break;
  }

  DCHECK(equal_sign_pos);
  DCHECK(!kMultiline || closing_comment_pos);
  wtf_size_t url_pos = equal_sign_pos + 1;
  String match = kMultiline
                     ? content.Substring(url_pos, closing_comment_pos - url_pos)
                     : content.Substring(url_pos);

  wtf_size_t new_line = match.Find("\n");
  if (new_line != kNotFound)
    match = match.Substring(0, new_line);
  match = match.StripWhiteSpace();

  String disallowed_chars("\"' \t");
  for (uint32_t i = 0; i < match.length(); ++i) {
    if (disallowed_chars.find(match[i]) != kNotFound)
      return g_empty_string;
  }

  return match;
}

void GetClassNamesFromRule(CSSStyleRule* rule, HashSet<String>& unique_names) {
  for (const CSSSelector* sub_selector = rule->GetStyleRule()->FirstSelector();
       sub_selector; sub_selector = CSSSelectorList::Next(*sub_selector)) {
    const CSSSelector* simple_selector = sub_selector;
    while (simple_selector) {
      if (simple_selector->Match() == CSSSelector::kClass)
        unique_names.insert(simple_selector->Value());
      simple_selector = simple_selector->NextSimpleSelector();
    }
  }
}

bool VerifyRuleText(Document* document, const String& rule_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = rule_text + " div { " + bogus_property_name + ": none; }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);
  unsigned rule_count = source_data->size();

  // Exactly two rules should be parsed.
  if (rule_count != 2)
    return false;

  // Added rule must be style rule.
  if (!source_data->at(0)->HasProperties())
    return false;

  Vector<CSSPropertySourceData>& property_data =
      source_data->at(1)->property_data;
  unsigned property_count = property_data.size();

  // Exactly one property should be in rule.
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

bool VerifyStyleText(Document* document,
                     const String& text,
                     StyleRule::RuleType rule_type = StyleRule::kStyle) {
  if (rule_type == StyleRule::kProperty) {
    return VerifyRuleText(document, "@property --property {" + text + "}");
  }
  return VerifyRuleText(document, "div {" + text + "}");
}

bool VerifyNestedDeclarations(Document* document, const String& rule_text) {
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = ".a { .b {} " + rule_text + " }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kStyle) {
    return false;
  }
  const CSSRuleSourceData& property_data = *source_data->front();

  return property_data.child_rules.size() == 2;
}

bool VerifyPropertyNameText(Document* document, const String& name_text) {
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text =
      "@property " + name_text + " { syntax: \"*\"; inherits: false; }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kProperty)
    return false;

  const CSSRuleSourceData& property_data = *source_data->at(0);
  if (property_data.property_data.size() != 2)
    return false;

  return true;
}

bool VerifyKeyframeKeyText(Document* document, const String& key_text) {
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = "@keyframes boguzAnim { " + key_text +
                " { -webkit-boguz-propertee : none; } }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // Exactly one should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kKeyframes)
    return false;

  const CSSRuleSourceData& keyframe_data = *source_data->at(0);
  if (keyframe_data.child_rules.size() != 1 ||
      keyframe_data.child_rules.at(0)->type != StyleRule::kKeyframe)
    return false;

  // Exactly one property should be in keyframe rule.
  const unsigned property_count =
      keyframe_data.child_rules.at(0)->property_data.size();
  if (property_count != 1)
    return false;

  return true;
}

bool VerifySelectorText(Document* document, const String& selector_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = selector_text + " { " + bogus_property_name + ": none; }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // Exactly one rule should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kStyle)
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& property_data =
      source_data->at(0)->property_data;
  unsigned property_count = property_data.size();
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

bool VerifyMediaText(Document* document, const String& media_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = "@media " + media_text + " { div { " + bogus_property_name +
                ": none; } }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // Exactly one media rule should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kMedia)
    return false;

  // Media rule should have exactly one style rule child.
  CSSRuleSourceDataList& child_source_data = source_data->at(0)->child_rules;
  rule_count = child_source_data.size();
  if (rule_count != 1 || !child_source_data.at(0)->HasProperties())
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& property_data =
      child_source_data.at(0)->property_data;
  unsigned property_count = property_data.size();
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

bool VerifyContainerQueryText(Document* document,
                              const String& container_query_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = "@container " + container_query_text + " { div { " +
                bogus_property_name + ": none; } }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // TODO(crbug.com/1146422): for now these checks are identical to
  // those for media queries. We should enforce container-query-specific
  // checks once the spec is finalized.
  // Exactly one container rule should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kContainer)
    return false;

  // Container rule should have exactly one style rule child.
  CSSRuleSourceDataList& child_source_data = source_data->at(0)->child_rules;
  rule_count = child_source_data.size();
  if (rule_count != 1 || !child_source_data.at(0)->HasProperties())
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& property_data =
      child_source_data.at(0)->property_data;
  unsigned property_count = property_data.size();
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

bool VerifySupportsText(Document* document, const String& supports_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = "@supports " + supports_text + " { div { " +
                bogus_property_name + ": none; } }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // Exactly one supports rule should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kSupports)
    return false;

  // Supports rule should have exactly one style rule child.
  CSSRuleSourceDataList& child_source_data = source_data->at(0)->child_rules;
  rule_count = child_source_data.size();
  if (rule_count != 1 || !child_source_data.at(0)->HasProperties())
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& property_data =
      child_source_data.at(0)->property_data;
  unsigned property_count = property_data.size();
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

bool VerifyScopeText(Document* document, const String& scope_text) {
  DEFINE_STATIC_LOCAL(String, bogus_property_name, ("-webkit-boguz-propertee"));
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      ParserContextForDocument(document));
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  String text = "@scope " + scope_text + " { div { " + bogus_property_name +
                ": none; } }";
  InspectorCSSParserObserver observer(text, document, source_data);
  CSSParser::ParseSheetForInspector(ParserContextForDocument(document),
                                    style_sheet, text, observer);

  // Exactly one scope rule should be parsed.
  unsigned rule_count = source_data->size();
  if (rule_count != 1 || source_data->at(0)->type != StyleRule::kScope)
    return false;

  // Scope rule should have exactly one style rule child.
  CSSRuleSourceDataList& child_source_data = source_data->at(0)->child_rules;
  rule_count = child_source_data.size();
  if (rule_count != 1 || !child_source_data.at(0)->HasProperties())
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& property_data =
      child_source_data.at(0)->property_data;
  unsigned property_count = property_data.size();
  if (property_count != 1)
    return false;

  // Check for the property name.
  if (property_data.at(0).name != bogus_property_name)
    return false;

  return true;
}

void FlattenSourceData(const CSSRuleSourceDataList& data_list,
                       CSSRuleSourceDataList* result) {
  for (CSSRuleSourceData* data : data_list) {
    // The result->append()'ed types should be exactly the same as in
    // collectFlatRules().
    switch (data->type) {
      case StyleRule::kImport:
      case StyleRule::kFontFace:
      case StyleRule::kKeyframe:
      case StyleRule::kFontFeature:
      case StyleRule::kPositionTry:
      case StyleRule::kViewTransition:
      case StyleRule::kFontPaletteValues:
        result->push_back(data);
        break;
      case StyleRule::kStyle:
      case StyleRule::kMedia:
      case StyleRule::kScope:
      case StyleRule::kSupports:
      case StyleRule::kKeyframes:
      case StyleRule::kContainer:
      case StyleRule::kLayerBlock:
      case StyleRule::kFontFeatureValues:
      case StyleRule::kProperty:
        result->push_back(data);
        FlattenSourceData(data->child_rules, result);
        break;
      default:
        break;
    }
  }
}

CSSRuleList* AsCSSRuleList(CSSRule* rule) {
  if (!rule)
    return nullptr;

  if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
    return style_rule->cssRules();
  }

  if (auto* media_rule = DynamicTo<CSSMediaRule>(rule))
    return media_rule->cssRules();

  if (auto* scope_rule = DynamicTo<CSSScopeRule>(rule))
    return scope_rule->cssRules();

  if (auto* supports_rule = DynamicTo<CSSSupportsRule>(rule))
    return supports_rule->cssRules();

  if (auto* keyframes_rule = DynamicTo<CSSKeyframesRule>(rule))
    return keyframes_rule->cssRules();

  if (auto* container_rule = DynamicTo<CSSContainerRule>(rule))
    return container_rule->cssRules();

  if (auto* layer_rule = DynamicTo<CSSLayerBlockRule>(rule))
    return layer_rule->cssRules();

  if (auto* property_rule = DynamicTo<CSSPropertyRule>(rule))
    return property_rule->cssRules();

  if (auto* font_palette_values_rule =
          DynamicTo<CSSFontPaletteValuesRule>(rule))
    return font_palette_values_rule->cssRules();

  return nullptr;
}

template <typename RuleList>
void CollectFlatRules(RuleList rule_list, CSSRuleVector* result) {
  if (!rule_list)
    return;

  for (unsigned i = 0, size = rule_list->length(); i < size; ++i) {
    CSSRule* rule = rule_list->ItemInternal(i);

    // The result->append()'ed types should be exactly the same as in
    // flattenSourceData().
    switch (rule->GetType()) {
      case CSSRule::kImportRule:
      case CSSRule::kFontFaceRule:
      case CSSRule::kKeyframeRule:
      case CSSRule::kFontFeatureRule:
      case CSSRule::kPositionTryRule:
      case CSSRule::kViewTransitionRule:
      case CSSRule::kFontPaletteValuesRule:
        result->push_back(rule);
        break;
      case CSSRule::kStyleRule:
      case CSSRule::kMediaRule:
      case CSSRule::kScopeRule:
      case CSSRule::kSupportsRule:
      case CSSRule::kKeyframesRule:
      case CSSRule::kContainerRule:
      case CSSRule::kLayerBlockRule:
      case CSSRule::kFontFeatureValuesRule:
      case CSSRule::kPropertyRule:
        result->push_back(rule);
        CollectFlatRules(AsCSSRuleList(rule), result);
        break;
      case CSSRule::kNestedDeclarationsRule:
        result->push_back(
            To<CSSNestedDeclarationsRule>(*rule).InnerCSSStyleRule());
        break;
      default:
        break;
    }
  }
}

// Warning: it does not always produce valid CSS.
// Use the rule's cssText method if you need to expose CSS externally.
String CanonicalCSSText(CSSRule* rule) {
  auto* style_rule = DynamicTo<CSSStyleRule>(rule);
  if (!style_rule)
    return rule->cssText();

  Vector<std::pair<unsigned, String>> properties;
  CSSStyleDeclaration* style = style_rule->style();
  for (unsigned i = 0; i < style->length(); ++i)
    properties.emplace_back(i, style->item(i));

  std::sort(properties.begin(), properties.end(),
            [](const auto& a, const auto& b) -> bool {
              return WTF::CodeUnitCompareLessThan(a.second, b.second);
            });

  StringBuilder builder;
  builder.Append(style_rule->selectorText());
  builder.Append('{');
  for (const auto& [index, name] : properties) {
    builder.Append(' ');
    builder.Append(name);
    builder.Append(':');
    builder.Append(style->GetPropertyValueWithHint(name, index));
    String priority = style->GetPropertyPriorityWithHint(name, index);
    if (!priority.empty()) {
      builder.Append(' ');
      builder.Append(priority);
    }
    builder.Append(';');
  }
  builder.Append('}');

  return builder.ToString();
}

}  // namespace

enum MediaListSource {
  kMediaListSourceLinkedSheet,
  kMediaListSourceInlineSheet,
  kMediaListSourceMediaRule,
  kMediaListSourceImportRule
};

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheetBase::BuildSourceRangeObject(const SourceRange& range) {
  const LineEndings* line_endings = GetLineEndings();
  if (!line_endings)
    return nullptr;
  TextPosition start =
      TextPosition::FromOffsetAndLineEndings(range.start, *line_endings);
  TextPosition end =
      TextPosition::FromOffsetAndLineEndings(range.end, *line_endings);

  std::unique_ptr<protocol::CSS::SourceRange> result =
      protocol::CSS::SourceRange::create()
          .setStartLine(start.line_.ZeroBasedInt())
          .setStartColumn(start.column_.ZeroBasedInt())
          .setEndLine(end.line_.ZeroBasedInt())
          .setEndColumn(end.column_.ZeroBasedInt())
          .build();
  return result;
}

InspectorStyle::InspectorStyle(CSSStyleDeclaration* style,
                               CSSRuleSourceData* source_data,
                               InspectorStyleSheetBase* parent_style_sheet)
    : style_(style),
      source_data_(source_data),
      parent_style_sheet_(parent_style_sheet) {
  DCHECK(style_);
}

std::unique_ptr<protocol::CSS::CSSStyle> InspectorStyle::BuildObjectForStyle(
    Element* element,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  std::unique_ptr<protocol::CSS::CSSStyle> result =
      StyleWithProperties(element, pseudo_id, pseudo_argument);
  if (source_data_) {
    if (parent_style_sheet_ && !parent_style_sheet_->Id().empty())
      result->setStyleSheetId(parent_style_sheet_->Id());
    result->setRange(parent_style_sheet_->BuildSourceRangeObject(
        source_data_->rule_declarations_range));
    String sheet_text;
    bool success = parent_style_sheet_->GetText(&sheet_text);
    if (success) {
      const SourceRange& declarations_range =
          source_data_->rule_declarations_range;
      result->setCssText(sheet_text.Substring(
          declarations_range.start,
          declarations_range.end - declarations_range.start));
    }
  }

  return result;
}

bool InspectorStyle::StyleText(String* result) {
  if (!source_data_)
    return false;

  return TextForRange(source_data_->rule_declarations_range, result);
}

bool InspectorStyle::TextForRange(const SourceRange& range, String* result) {
  String style_sheet_text;
  bool success = parent_style_sheet_->GetText(&style_sheet_text);
  if (!success)
    return false;

  DCHECK(0 <= range.start);
  DCHECK_LE(range.start, range.end);
  DCHECK_LE(range.end, style_sheet_text.length());
  *result = style_sheet_text.Substring(range.start, range.end - range.start);
  return true;
}

void InspectorStyle::PopulateAllProperties(
    Vector<CSSPropertySourceData>& result) {
  if (source_data_ && source_data_->HasProperties()) {
    Vector<CSSPropertySourceData>& source_property_data =
        source_data_->property_data;
    for (const auto& data : source_property_data)
      result.push_back(data);
  }

  for (int i = 0, size = style_->length(); i < size; ++i) {
    String name = style_->item(i);
    if (!IsValidCSSPropertyID(
            CssPropertyID(style_->GetExecutionContext(), name)))
      continue;

    String value = style_->GetPropertyValueWithHint(name, i);
    bool important = !style_->GetPropertyPriorityWithHint(name, i).empty();
    if (important)
      value = value + " !important";
    result.push_back(CSSPropertySourceData(name, value, important, false, true,
                                           SourceRange()));
  }
}

bool InspectorStyle::CheckRegisteredPropertySyntaxWithVarSubstitution(
    Element* element,
    const CSSPropertySourceData& property,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) const {
  if (!element) {
    return false;
  }
  const Document* document = parent_style_sheet_->GetDocument();
  if (!document) {
    return false;
  }
  if (!property.name.StartsWith("--")) {
    return false;
  }
  const PropertyRegistry* registry = document->GetPropertyRegistry();
  if (!registry) {
    return false;
  }
  AtomicString atomic_name(property.name);
  const PropertyRegistration* registration =
      registry->Registration(atomic_name);
  if (!registration) {
    return false;
  }

  const ComputedStyle* style =
      element->EnsureComputedStyle(pseudo_id, pseudo_argument);
  if (!style) {
    return false;
  }

  PropertyRegistry* empty_registry = MakeGarbageCollected<PropertyRegistry>();
  CustomProperty p(atomic_name, empty_registry);

  const CSSParserContext* parser_context = ParserContextForDocument(document);
  const CSSValue* result = p.Parse(property.value, *parser_context, {});
  if (!result) {
    return false;
  }

  CSSPropertyName property_name(atomic_name);
  // Substitute var()s in the property value from element's computed style.
  const auto* computed_value =
      StyleResolver::ResolveValue(*element, *style, property_name, *result);
  if (!computed_value) {
    return false;
  }

  // Now check the substitution result against the registered syntax.
  if (!registration->Syntax().Parse(computed_value->CssText(), *parser_context,
                                    false)) {
    return false;
  }
  return true;
}

std::unique_ptr<protocol::CSS::CSSStyle> InspectorStyle::StyleWithProperties(
    Element* element,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  auto properties_object =
      std::make_unique<protocol::Array<protocol::CSS::CSSProperty>>();
  auto shorthand_entries =
      std::make_unique<protocol::Array<protocol::CSS::ShorthandEntry>>();
  HashSet<String> found_shorthands;

  Vector<CSSPropertySourceData> properties;
  PopulateAllProperties(properties);

  for (auto& style_property : properties) {
    const CSSPropertySourceData& property_entry = style_property;
    const String& name = property_entry.name;

    std::unique_ptr<protocol::CSS::CSSProperty> property =
        protocol::CSS::CSSProperty::create()
            .setName(name)
            .setValue(property_entry.value)
            .build();

    // Default "parsedOk" == true.
    if (!property_entry.parsed_ok) {
      property->setParsedOk(CheckRegisteredPropertySyntaxWithVarSubstitution(
          element, property_entry, pseudo_id, pseudo_argument));
    }

    String text;
    if (style_property.range.length() &&
        TextForRange(style_property.range, &text))
      property->setText(text);
    if (property_entry.important)
      property->setImportant(true);
    if (style_property.range.length()) {
      property->setRange(parent_style_sheet_
                             ? parent_style_sheet_->BuildSourceRangeObject(
                                   property_entry.range)
                             : nullptr);
      if (!property_entry.disabled) {
        property->setImplicit(false);
      }
      property->setDisabled(property_entry.disabled);
    } else if (!property_entry.disabled) {
      bool implicit = style_->IsPropertyImplicit(name);
      // Default "implicit" == false.
      if (implicit)
        property->setImplicit(true);

      String shorthand = style_->GetPropertyShorthand(name);
      if (!shorthand.empty()) {
        if (found_shorthands.insert(shorthand).is_new_entry) {
          std::unique_ptr<protocol::CSS::ShorthandEntry> entry =
              protocol::CSS::ShorthandEntry::create()
                  .setName(shorthand)
                  .setValue(ShorthandValue(shorthand))
                  .build();
          if (!style_->getPropertyPriority(name).empty())
            entry->setImportant(true);
          shorthand_entries->emplace_back(std::move(entry));
        }
      }
    }

    if (auto longhandProperties = LonghandProperties(property_entry))
      property->setLonghandProperties(std::move(longhandProperties));

    properties_object->emplace_back(std::move(property));
  }

  std::unique_ptr<protocol::CSS::CSSStyle> result =
      protocol::CSS::CSSStyle::create()
          .setCssProperties(std::move(properties_object))
          .setShorthandEntries(std::move(shorthand_entries))
          .build();
  return result;
}

String InspectorStyle::ShorthandValue(const String& shorthand_property) {
  StringBuilder builder;
  String value = style_->getPropertyValue(shorthand_property);
  if (value.empty()) {
    for (unsigned i = 0; i < style_->length(); ++i) {
      String individual_property = style_->item(i);
      if (style_->GetPropertyShorthand(individual_property) !=
          shorthand_property)
        continue;
      if (style_->IsPropertyImplicit(individual_property))
        continue;
      String individual_value = style_->getPropertyValue(individual_property);
      if (individual_value == "initial")
        continue;
      if (!builder.empty())
        builder.Append(' ');
      builder.Append(individual_value);
    }
  } else {
    builder.Append(value);
  }

  if (!style_->getPropertyPriority(shorthand_property).empty())
    builder.Append(" !important");

  return builder.ToString();
}

std::unique_ptr<protocol::Array<protocol::CSS::CSSProperty>>
InspectorStyle::LonghandProperties(
    const CSSPropertySourceData& property_entry) {
  String property_value = property_entry.value;
  if (property_entry.important) {
    property_value = property_value.Substring(
        0, property_value.length() - 10 /* length of "!important" */);
  }
  CSSParserTokenStream stream(property_value);
  stream.EnsureLookAhead();  // Several parsers expect this.
  CSSPropertyID property_id =
      CssPropertyID(style_->GetExecutionContext(), property_entry.name);
  if (property_id == CSSPropertyID::kInvalid ||
      property_id == CSSPropertyID::kVariable)
    return nullptr;
  const CSSProperty& property =
      CSSProperty::Get(ResolveCSSPropertyID(property_id));
  if (!property.IsProperty() || !property.IsShorthand())
    return nullptr;
  const auto local_context =
      CSSParserLocalContext().WithCurrentShorthand(property_id);
  HeapVector<CSSPropertyValue, 64> longhand_properties;
  if (To<Shorthand>(property).ParseShorthand(
          property_entry.important, stream,
          *ParserContextForDocument(parent_style_sheet_->GetDocument()),
          local_context, longhand_properties)) {
    auto result =
        std::make_unique<protocol::Array<protocol::CSS::CSSProperty>>();
    for (auto longhand_property : longhand_properties) {
      String value = longhand_property.Value()->CssText();
      std::unique_ptr<protocol::CSS::CSSProperty> longhand =
          protocol::CSS::CSSProperty::create()
              .setName(longhand_property.Name().ToAtomicString())
              .setValue(value)
              .build();
      if (property_entry.important) {
        longhand->setValue(value + " !important");
        longhand->setImportant(true);
      }
      result->emplace_back(std::move(longhand));
    }
    return result;
  }
  return nullptr;
}

void InspectorStyle::Trace(Visitor* visitor) const {
  visitor->Trace(style_);
  visitor->Trace(parent_style_sheet_);
  visitor->Trace(source_data_);
}

InspectorStyleSheetBase::InspectorStyleSheetBase(Listener* listener, String id)
    : id_(id),
      listener_(listener),
      line_endings_(std::make_unique<LineEndings>()) {}

void InspectorStyleSheetBase::OnStyleSheetTextChanged() {
  line_endings_ = std::make_unique<LineEndings>();
  if (GetListener())
    GetListener()->StyleSheetChanged(this);
}

std::unique_ptr<protocol::CSS::CSSStyle>
InspectorStyleSheetBase::BuildObjectForStyle(
    CSSStyleDeclaration* style,
    Element* element,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  return GetInspectorStyle(style)->BuildObjectForStyle(element, pseudo_id,
                                                       pseudo_argument);
}

const LineEndings* InspectorStyleSheetBase::GetLineEndings() {
  if (line_endings_->size() > 0)
    return line_endings_.get();
  String text;
  if (GetText(&text))
    line_endings_ = WTF::GetLineEndings(text);
  return line_endings_.get();
}

void InspectorStyleSheetBase::ResetLineEndings() {
  line_endings_ = std::make_unique<LineEndings>();
}

bool InspectorStyleSheetBase::LineNumberAndColumnToOffset(
    unsigned line_number,
    unsigned column_number,
    unsigned* offset) {
  const LineEndings* endings = GetLineEndings();
  if (line_number >= endings->size())
    return false;
  unsigned characters_in_line =
      line_number > 0
          ? endings->at(line_number) - endings->at(line_number - 1) - 1
          : endings->at(0);
  if (column_number > characters_in_line)
    return false;
  TextPosition position(OrdinalNumber::FromZeroBasedInt(line_number),
                        OrdinalNumber::FromZeroBasedInt(column_number));
  *offset = position.ToOffset(*endings).ZeroBasedInt();
  return true;
}

InspectorStyleSheet::InspectorStyleSheet(
    InspectorNetworkAgent* network_agent,
    CSSStyleSheet* page_style_sheet,
    const String& origin,
    const String& document_url,
    InspectorStyleSheetBase::Listener* listener,
    InspectorResourceContainer* resource_container)
    : InspectorStyleSheetBase(
          listener,
          IdentifiersFactory::IdForCSSStyleSheet(page_style_sheet)),
      resource_container_(resource_container),
      network_agent_(network_agent),
      page_style_sheet_(page_style_sheet),
      origin_(origin),
      document_url_(document_url) {
  UpdateText();
}

InspectorStyleSheet::~InspectorStyleSheet() = default;

void InspectorStyleSheet::Trace(Visitor* visitor) const {
  visitor->Trace(resource_container_);
  visitor->Trace(network_agent_);
  visitor->Trace(page_style_sheet_);
  visitor->Trace(cssom_flat_rules_);
  visitor->Trace(parsed_flat_rules_);
  visitor->Trace(source_data_);
  InspectorStyleSheetBase::Trace(visitor);
}

static String StyleSheetURL(CSSStyleSheet* page_style_sheet) {
  if (page_style_sheet && !page_style_sheet->Contents()->BaseURL().IsEmpty())
    return page_style_sheet->Contents()->BaseURL().GetString();
  return g_empty_string;
}

String InspectorStyleSheet::FinalURL() {
  String url = StyleSheetURL(page_style_sheet_.Get());
  return url.empty() ? document_url_ : url;
}

bool InspectorStyleSheet::SetText(const String& text,
                                  ExceptionState& exception_state) {
  page_style_sheet_->SetText(text, CSSImportRules::kAllow);
  InnerSetText(text, true);
  OnStyleSheetTextChanged();
  return true;
}

void InspectorStyleSheet::CSSOMStyleSheetTextReplaced(const String& text) {
  InnerSetText(text, false);
}

CSSStyleRule* InspectorStyleSheet::SetRuleSelector(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifySelectorText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Selector or media text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasProperties()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kStyleRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSStyleRule* style_rule = InspectorCSSAgent::AsCSSStyleRule(rule);
  style_rule->setSelectorText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return style_rule;
}

CSSPropertyRule* InspectorStyleSheet::SetPropertyName(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifyPropertyNameText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Property name text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasProperties()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kPropertyRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSPropertyRule* property_rule = To<CSSPropertyRule>(rule);
  if (!property_rule->SetNameText(
          page_style_sheet_->OwnerDocument()->GetExecutionContext(), text)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The property name '" + text + "' is invalid and cannot be parsed");
    return nullptr;
  }

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return property_rule;
}

CSSKeyframeRule* InspectorStyleSheet::SetKeyframeKey(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifyKeyframeKeyText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Keyframe key text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasProperties()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kKeyframeRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSKeyframeRule* keyframe_rule = To<CSSKeyframeRule>(rule);
  keyframe_rule->setKeyText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text,
      exception_state);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return keyframe_rule;
}

CSSRule* InspectorStyleSheet::SetStyleText(const SourceRange& range,
                                           const String& text,
                                           SourceRange* new_range,
                                           String* old_text,
                                           ExceptionState& exception_state) {
  CSSRuleSourceData* source_data = FindRuleByDeclarationsRange(range);
  if (!source_data || !source_data->HasProperties()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  if (!VerifyStyleText(page_style_sheet_->OwnerDocument(), text,
                       source_data->type)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Style text is not valid.");
    return nullptr;
  }

  if (source_data->type == StyleRule::RuleType::kStyle &&
      source_data->rule_header_range.length() == 0u &&
      !VerifyNestedDeclarations(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Style text would cause rule to disappear");
    // TODO(crbug.com/361116768): This should work, but we're not yet
    // equipped to handle rules that disappear.
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      (!IsA<CSSStyleRule>(rule) && !IsA<CSSKeyframeRule>(rule) &&
       !IsA<CSSPropertyRule>(rule) && !IsA<CSSFontPaletteValuesRule>(rule) &&
       !IsA<CSSPositionTryRule>(rule))) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSStyleDeclaration* style = nullptr;
  if (auto* style_rule = DynamicTo<CSSStyleRule>(rule)) {
    style = style_rule->style();
  } else if (auto* property_rule = DynamicTo<CSSPropertyRule>(rule)) {
    style = property_rule->Style();
  } else if (auto* font_palette_values_rule =
                 DynamicTo<CSSFontPaletteValuesRule>(rule)) {
    style = font_palette_values_rule->Style();
  } else if (auto* position_try_rule = DynamicTo<CSSPositionTryRule>(rule)) {
    style = position_try_rule->style();
  } else {
    style = To<CSSKeyframeRule>(rule)->style();
  }

  Document* owner_document = page_style_sheet_->OwnerDocument();
  ExecutionContext* execution_context =
      owner_document ? owner_document->GetExecutionContext() : nullptr;

  style->setCSSText(execution_context, text, exception_state);

  ReplaceText(source_data->rule_declarations_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return rule;
}

CSSMediaRule* InspectorStyleSheet::SetMediaRuleText(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifyMediaText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Selector or media text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasMedia()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kMediaRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSMediaRule* media_rule = InspectorCSSAgent::AsCSSMediaRule(rule);
  media_rule->media()->setMediaText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return media_rule;
}

CSSContainerRule* InspectorStyleSheet::SetContainerRuleText(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifyContainerQueryText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Selector or container query text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasContainer()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kContainerRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "CQ Source range didn't match existing style source range");
    return nullptr;
  }

  CSSContainerRule* container_rule =
      InspectorCSSAgent::AsCSSContainerRule(rule);
  container_rule->SetConditionText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return container_rule;
}

CSSSupportsRule* InspectorStyleSheet::SetSupportsRuleText(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifySupportsText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Selector or supports rule text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasSupports()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kSupportsRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Supports source range didn't match existing source range");
    return nullptr;
  }

  CSSSupportsRule* supports_rule = InspectorCSSAgent::AsCSSSupportsRule(rule);
  supports_rule->SetConditionText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return supports_rule;
}

CSSScopeRule* InspectorStyleSheet::SetScopeRuleText(
    const SourceRange& range,
    const String& text,
    SourceRange* new_range,
    String* old_text,
    ExceptionState& exception_state) {
  if (!VerifyScopeText(page_style_sheet_->OwnerDocument(), text)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Selector or scope rule text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* source_data = FindRuleByHeaderRange(range);
  if (!source_data || !source_data->HasScope()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = RuleForSourceData(source_data);
  if (!rule || !rule->parentStyleSheet() ||
      rule->GetType() != CSSRule::kScopeRule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "Scope source range didn't match existing source range");
    return nullptr;
  }

  CSSScopeRule* scope_rule = InspectorCSSAgent::AsCSSScopeRule(rule);
  scope_rule->SetPreludeText(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), text);

  ReplaceText(source_data->rule_header_range, text, new_range, old_text);
  OnStyleSheetTextChanged();

  return scope_rule;
}

CSSRuleSourceData* InspectorStyleSheet::RuleSourceDataAfterSourceRange(
    const SourceRange& source_range) {
  DCHECK(source_data_);
  unsigned index = 0;
  for (; index < source_data_->size(); ++index) {
    CSSRuleSourceData* sd = source_data_->at(index).Get();
    if (sd->rule_header_range.start >= source_range.end)
      break;
  }
  return index < source_data_->size() ? source_data_->at(index).Get() : nullptr;
}

CSSStyleRule* InspectorStyleSheet::InsertCSSOMRuleInStyleSheet(
    CSSRule* insert_before,
    const String& rule_text,
    ExceptionState& exception_state) {
  unsigned index = 0;
  for (; index < page_style_sheet_->length(); ++index) {
    CSSRule* rule = page_style_sheet_->ItemInternal(index);
    if (rule == insert_before)
      break;
  }

  page_style_sheet_->insertRule(rule_text, index, exception_state);
  CSSRule* rule = page_style_sheet_->ItemInternal(index);
  CSSStyleRule* style_rule = InspectorCSSAgent::AsCSSStyleRule(rule);
  if (!style_rule) {
    page_style_sheet_->deleteRule(index, ASSERT_NO_EXCEPTION);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The rule '" + rule_text + "' could not be added in style sheet.");
    return nullptr;
  }
  return style_rule;
}

CSSStyleRule* InspectorStyleSheet::InsertCSSOMRuleInMediaRule(
    CSSMediaRule* media_rule,
    CSSRule* insert_before,
    const String& rule_text,
    ExceptionState& exception_state) {
  unsigned index = 0;
  for (; index < media_rule->length(); ++index) {
    CSSRule* rule = media_rule->ItemInternal(index);
    if (rule == insert_before)
      break;
  }

  media_rule->insertRule(
      page_style_sheet_->OwnerDocument()->GetExecutionContext(), rule_text,
      index, exception_state);
  CSSRule* rule = media_rule->ItemInternal(index);
  CSSStyleRule* style_rule = InspectorCSSAgent::AsCSSStyleRule(rule);
  if (!style_rule) {
    media_rule->deleteRule(index, ASSERT_NO_EXCEPTION);
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The rule '" + rule_text + "' could not be added in media rule.");
    return nullptr;
  }
  return style_rule;
}

CSSStyleRule* InspectorStyleSheet::InsertCSSOMRuleBySourceRange(
    const SourceRange& source_range,
    const String& rule_text,
    ExceptionState& exception_state) {
  DCHECK(source_data_);

  CSSRuleSourceData* containing_rule_source_data = nullptr;
  for (wtf_size_t i = 0; i < source_data_->size(); ++i) {
    CSSRuleSourceData* rule_source_data = source_data_->at(i).Get();
    if (rule_source_data->rule_header_range.start < source_range.start &&
        source_range.start < rule_source_data->rule_body_range.start) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "Cannot insert rule inside rule selector.");
      return nullptr;
    }
    if (source_range.start < rule_source_data->rule_body_range.start ||
        rule_source_data->rule_body_range.end < source_range.start)
      continue;
    if (!containing_rule_source_data ||
        containing_rule_source_data->rule_body_range.length() >
            rule_source_data->rule_body_range.length())
      containing_rule_source_data = rule_source_data;
  }

  CSSRuleSourceData* insert_before =
      RuleSourceDataAfterSourceRange(source_range);
  CSSRule* insert_before_rule = RuleForSourceData(insert_before);

  if (!containing_rule_source_data)
    return InsertCSSOMRuleInStyleSheet(insert_before_rule, rule_text,
                                       exception_state);

  CSSRule* rule = RuleForSourceData(containing_rule_source_data);
  if (!rule || rule->GetType() != CSSRule::kMediaRule) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Cannot insert rule in non-media rule.");
    return nullptr;
  }

  return InsertCSSOMRuleInMediaRule(To<CSSMediaRule>(rule), insert_before_rule,
                                    rule_text, exception_state);
}

CSSStyleRule* InspectorStyleSheet::AddRule(const String& rule_text,
                                           const SourceRange& location,
                                           SourceRange* added_range,
                                           ExceptionState& exception_state) {
  if (location.start != location.end) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Source range must be collapsed.");
    return nullptr;
  }

  if (!VerifyRuleText(page_style_sheet_->OwnerDocument(), rule_text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Rule text is not valid.");
    return nullptr;
  }

  if (!source_data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Style is read-only.");
    return nullptr;
  }

  CSSStyleRule* style_rule =
      InsertCSSOMRuleBySourceRange(location, rule_text, exception_state);
  if (exception_state.HadException())
    return nullptr;

  ReplaceText(location, rule_text, added_range, nullptr);
  OnStyleSheetTextChanged();
  return style_rule;
}

bool InspectorStyleSheet::DeleteRule(const SourceRange& range,
                                     ExceptionState& exception_state) {
  if (!source_data_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "Style is read-only.");
    return false;
  }

  // Find index of CSSRule that entirely belongs to the range.
  CSSRuleSourceData* found_data = nullptr;

  for (wtf_size_t i = 0; i < source_data_->size(); ++i) {
    CSSRuleSourceData* rule_source_data = source_data_->at(i).Get();
    unsigned rule_start = rule_source_data->rule_header_range.start;
    unsigned rule_end = rule_source_data->rule_body_range.end + 1;
    bool start_belongs = rule_start >= range.start && rule_start < range.end;
    bool end_belongs = rule_end > range.start && rule_end <= range.end;

    if (start_belongs != end_belongs)
      break;
    if (!start_belongs)
      continue;
    if (!found_data || found_data->rule_body_range.length() >
                           rule_source_data->rule_body_range.length())
      found_data = rule_source_data;
  }
  CSSRule* rule = RuleForSourceData(found_data);
  if (!rule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotFoundError,
        "No style rule could be found in given range.");
    return false;
  }
  CSSStyleSheet* style_sheet = rule->parentStyleSheet();
  if (!style_sheet) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "No parent stylesheet could be found.");
    return false;
  }
  CSSRule* parent_rule = rule->parentRule();
  if (parent_rule) {
    if (parent_rule->GetType() != CSSRule::kMediaRule) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotFoundError,
          "Cannot remove rule from non-media rule.");
      return false;
    }
    CSSMediaRule* parent_media_rule = To<CSSMediaRule>(parent_rule);
    wtf_size_t index = 0;
    while (index < parent_media_rule->length() &&
           parent_media_rule->ItemInternal(index) != rule) {
      ++index;
    }
    DCHECK_LT(index, parent_media_rule->length());
    parent_media_rule->deleteRule(index, exception_state);
  } else {
    wtf_size_t index = 0;
    while (index < style_sheet->length() &&
           style_sheet->ItemInternal(index) != rule) {
      ++index;
    }
    DCHECK_LT(index, style_sheet->length());
    style_sheet->deleteRule(index, exception_state);
  }
  // |rule| MAY NOT be addressed after this line!

  if (exception_state.HadException())
    return false;

  ReplaceText(range, "", nullptr, nullptr);
  OnStyleSheetTextChanged();
  return true;
}

std::unique_ptr<protocol::Array<String>>
InspectorStyleSheet::CollectClassNames() {
  HashSet<String> unique_names;
  auto result = std::make_unique<protocol::Array<String>>();

  for (wtf_size_t i = 0; i < parsed_flat_rules_.size(); ++i) {
    if (auto* style_rule =
            DynamicTo<CSSStyleRule>(parsed_flat_rules_.at(i).Get()))
      GetClassNamesFromRule(style_rule, unique_names);
  }
  for (const String& class_name : unique_names)
    result->emplace_back(class_name);
  return result;
}

void InspectorStyleSheet::ReplaceText(const SourceRange& range,
                                      const String& text,
                                      SourceRange* new_range,
                                      String* old_text) {
  String sheet_text = text_;
  if (old_text)
    *old_text = sheet_text.Substring(range.start, range.length());
  sheet_text.replace(range.start, range.length(), text);
  if (new_range)
    *new_range = SourceRange(range.start, range.start + text.length());
  InnerSetText(sheet_text, true);
}

void InspectorStyleSheet::ParseText(const String& text) {
  CSSRuleSourceDataList* rule_tree =
      MakeGarbageCollected<CSSRuleSourceDataList>();
  auto* style_sheet = MakeGarbageCollected<StyleSheetContents>(
      page_style_sheet_->Contents()->ParserContext());
  Document* owner_document = page_style_sheet_->OwnerDocument();
  InspectorCSSParserObserver observer(
      text, owner_document, rule_tree,
      InspectorCSSParserObserver::IssueReportingContext{
          page_style_sheet_->BaseURL(),
          page_style_sheet_->StartPositionInSource()});
  CSSParser::ParseSheetForInspector(
      page_style_sheet_->Contents()->ParserContext(), style_sheet, text,
      observer);
  CSSStyleSheet* source_data_sheet = nullptr;
  if (auto* import_rule =
          DynamicTo<CSSImportRule>(page_style_sheet_->ownerRule())) {
    source_data_sheet =
        MakeGarbageCollected<CSSStyleSheet>(style_sheet, import_rule);
  } else {
    if (page_style_sheet_->ownerNode()) {
      source_data_sheet = MakeGarbageCollected<CSSStyleSheet>(
          style_sheet, *page_style_sheet_->ownerNode());
    } else {
      source_data_sheet = MakeGarbageCollected<CSSStyleSheet>(style_sheet);
    }
  }

  parsed_flat_rules_.clear();
  CollectFlatRules(source_data_sheet, &parsed_flat_rules_);

  source_data_ = MakeGarbageCollected<CSSRuleSourceDataList>();
  FlattenSourceData(*rule_tree, source_data_.Get());

  // The number of rules parsed should be equal to the number of source data
  // entries:
  DCHECK_EQ(parsed_flat_rules_.size(), source_data_->size());

  if (owner_document) {
    const auto* property_registry = owner_document->GetPropertyRegistry();

    if (property_registry) {
      for (const auto& rule_source_data : *source_data_) {
        for (auto& property_source_data : rule_source_data->property_data) {
          if (!property_source_data.name.StartsWith("--") ||
              !property_source_data.parsed_ok) {
            continue;
          }

          // The defaulting keywords are always allowed
          if (css_parsing_utils::IsCSSWideKeyword(property_source_data.value)) {
            continue;
          }

          const auto* registration = property_registry->Registration(
              AtomicString(property_source_data.name));
          if (!registration) {
            continue;
          }
          if (!registration->Syntax().Parse(property_source_data.value,
                                            *style_sheet->ParserContext(),
                                            false)) {
            property_source_data.parsed_ok = false;
          }
        }
      }
    }
  }
}

// The stylesheet text might be out of sync with `page_style_sheet_` rules.
// This method checks if a rule is present in the source text using
// `SourceDataForRule` and produces a new text with all rules merged into the
// original text. For example, if the source text is
//
//   /* comment */ .rule1 {} .rule3 {}
//
// and the page_style_sheet_ contains
//
//   .rule0 {} .rule1 {} .rule2 {} .rule3 {} .rule4 {}
//
// The result should be
//
//   .rule0 {} /* comment */ .rule1 {} .rule2 {} .rule3 {} .rule4 {}
//
// Note that page_style_sheet_ does not maintain comments and original
// formatting.
String InspectorStyleSheet::MergeCSSOMRulesWithText(const String& text) {
  String merged_text = text;
  unsigned original_insert_pos = 0;
  unsigned inserted_count = 0;
  for (unsigned i = 0; i < page_style_sheet_->length(); i++) {
    CSSRuleSourceData* source_data =
        SourceDataForRule(page_style_sheet_->ItemInternal(i));
    if (source_data) {
      original_insert_pos = source_data->rule_body_range.end + 1;
      continue;
    }
    String rule_text = page_style_sheet_->ItemInternal(i)->cssText();
    merged_text.replace(original_insert_pos + inserted_count, 0, rule_text);
    inserted_count += rule_text.length();
  }
  rule_to_source_data_.clear();
  source_data_to_rule_.clear();
  cssom_flat_rules_.clear();
  return merged_text;
}

void InspectorStyleSheet::InnerSetText(const String& text,
                                       bool mark_as_locally_modified) {
  marked_for_sync_ = false;
  ParseText(text);

  text_ = text;

  if (mark_as_locally_modified) {
    Element* element = OwnerStyleElement();
    if (element) {
      resource_container_->StoreStyleElementContent(element->GetDomNodeId(),
                                                    text);
    } else if (origin_ == protocol::CSS::StyleSheetOriginEnum::Inspector) {
      resource_container_->StoreStyleElementContent(
          page_style_sheet_->OwnerDocument()->GetDomNodeId(), text);
    } else {
      resource_container_->StoreStyleSheetContent(FinalURL(), text);
    }
  }
}

namespace {

TextPosition TextPositionFromOffsetAndLineEndingsRelativeToStartPosition(
    unsigned offset,
    const Vector<unsigned>& line_endings,
    const TextPosition& start_position) {
  TextPosition position =
      TextPosition::FromOffsetAndLineEndings(offset, line_endings);
  unsigned column = position.column_.ZeroBasedInt();
  // A non-zero `start_position.column_` means that the text started in the
  // middle of a line, so the start column position must be added if `offset`
  // translates to a `position` in the first line of the text.
  if (position.line_.ZeroBasedInt() == 0) {
    column += start_position.column_.ZeroBasedInt();
  }
  unsigned line_index =
      start_position.line_.ZeroBasedInt() + position.line_.ZeroBasedInt();
  return TextPosition(OrdinalNumber::FromZeroBasedInt(line_index),
                      OrdinalNumber::FromZeroBasedInt(column));
}

}  // namespace

std::unique_ptr<protocol::CSS::CSSStyleSheetHeader>
InspectorStyleSheet::BuildObjectForStyleSheetInfo() {
  CSSStyleSheet* style_sheet = PageStyleSheet();
  if (!style_sheet)
    return nullptr;

  Document* document = style_sheet->OwnerDocument();
  LocalFrame* frame = document ? document->GetFrame() : nullptr;
  const LineEndings* line_endings = GetLineEndings();
  TextPosition start = style_sheet->StartPositionInSource();
  TextPosition end = start;
  unsigned text_length = 0;
  if (line_endings->size() > 0) {
    text_length = line_endings->back();
    end = TextPositionFromOffsetAndLineEndingsRelativeToStartPosition(
        text_length, *line_endings, start);
  }

  // DevTools needs to be able to distinguish between constructed
  // stylesheets created with `new` and constructed stylesheets
  // imported as a CSS module. Only the latter have a separate
  // source file to display.
  // For constructed stylesheets created with `new`, Url()
  // returns the URL of the document in which the sheet was created,
  // which may confuse the client. Only set the URL if we have a
  // proper URL of the source of the stylesheet.
  const String& source_url =
      (style_sheet->IsConstructed() && !style_sheet->IsForCSSModuleScript())
          ? SourceURL()
          : Url();

  std::unique_ptr<protocol::CSS::CSSStyleSheetHeader> result =
      protocol::CSS::CSSStyleSheetHeader::create()
          .setStyleSheetId(Id())
          .setOrigin(origin_)
          .setDisabled(style_sheet->disabled())
          .setSourceURL(source_url)
          .setTitle(style_sheet->title())
          .setFrameId(frame ? IdentifiersFactory::FrameId(frame) : "")
          .setIsInline(style_sheet->IsInline() && !StartsAtZero())
          .setIsConstructed(style_sheet->IsConstructed())
          .setIsMutable(style_sheet->Contents()->IsMutable())
          .setStartLine(start.line_.ZeroBasedInt())
          .setStartColumn(start.column_.ZeroBasedInt())
          .setLength(text_length)
          .setEndLine(end.line_.ZeroBasedInt())
          .setEndColumn(end.column_.ZeroBasedInt())
          .build();

  if (HasSourceURL())
    result->setHasSourceURL(true);

  if (request_failed_to_load_.has_value()) {
    result->setLoadingFailed(*request_failed_to_load_);
  }

  if (style_sheet->ownerNode()) {
    result->setOwnerNode(
        IdentifiersFactory::IntIdForNode(style_sheet->ownerNode()));
  }

  String source_map_url_value = SourceMapURL();
  if (!source_map_url_value.empty())
    result->setSourceMapURL(source_map_url_value);
  return result;
}

std::unique_ptr<protocol::Array<protocol::CSS::Value>>
InspectorStyleSheet::SelectorsFromSource(CSSRuleSourceData* source_data,
                                         const String& sheet_text,
                                         CSSStyleRule* rule) {
  ScriptRegexp* comment = nullptr;
  if (page_style_sheet_->OwnerDocument()) {
    comment = MakeGarbageCollected<ScriptRegexp>(
        page_style_sheet_->OwnerDocument()->GetAgent().isolate(),
        "/\\*[^]*?\\*/", kTextCaseSensitive, MultilineMode::kMultilineEnabled);
  }
  auto result = std::make_unique<protocol::Array<protocol::CSS::Value>>();
  const Vector<SourceRange>& ranges = source_data->selector_ranges;
  const CSSSelector* obj_selector = rule->GetStyleRule()->FirstSelector();

  for (wtf_size_t i = 0, size = ranges.size(); i < size && obj_selector;
       ++i, obj_selector = CSSSelectorList::Next(*obj_selector)) {
    const SourceRange& range = ranges.at(i);
    String selector = sheet_text.Substring(range.start, range.length());

    if (comment) {
      // We don't want to see any comments in the selector components, only the
      // meaningful parts.
      int match_length;
      int offset = 0;
      while ((offset = comment->Match(selector, offset, &match_length)) >= 0) {
        selector.replace(offset, match_length, "");
      }
    }

    std::unique_ptr<protocol::CSS::Value> simple_selector =
        protocol::CSS::Value::create()
            .setText(selector.StripWhiteSpace())
            .build();
    simple_selector->setRange(BuildSourceRangeObject(range));

    std::array<uint8_t, 3> specificity_tuple = obj_selector->SpecificityTuple();
    simple_selector->setSpecificity(protocol::CSS::Specificity::create()
                                        .setA(specificity_tuple[0])
                                        .setB(specificity_tuple[1])
                                        .setC(specificity_tuple[2])
                                        .build());

    result->emplace_back(std::move(simple_selector));
  }
  return result;
}

std::unique_ptr<protocol::CSS::SelectorList>
InspectorStyleSheet::BuildObjectForSelectorList(CSSStyleRule* rule) {
  CSSRuleSourceData* source_data = SourceDataForRule(rule);
  std::unique_ptr<protocol::Array<protocol::CSS::Value>> selectors;

  // This intentionally does not rely on the source data to avoid catching the
  // trailing comments (before the declaration starting '{').
  String selector_text = rule->selectorText();

  if (source_data) {
    selectors = SelectorsFromSource(source_data, text_, rule);
  } else {
    selectors = std::make_unique<protocol::Array<protocol::CSS::Value>>();
    for (const CSSSelector* selector = rule->GetStyleRule()->FirstSelector();
         selector; selector = CSSSelectorList::Next(*selector)) {
      std::array<uint8_t, 3> specificity_tuple = selector->SpecificityTuple();

      std::unique_ptr<protocol::CSS::Specificity> reworked_specificity =
          protocol::CSS::Specificity::create()
              .setA(specificity_tuple[0])
              .setB(specificity_tuple[1])
              .setC(specificity_tuple[2])
              .build();

      std::unique_ptr<protocol::CSS::Value> simple_selector =
          protocol::CSS::Value::create()
              .setText(selector->SelectorText())
              .setSpecificity(std::move(reworked_specificity))
              .build();

      selectors->emplace_back(std::move(simple_selector));
    }
  }
  return protocol::CSS::SelectorList::create()
      .setSelectors(std::move(selectors))
      .setText(selector_text)
      .build();
}

static bool CanBind(const String& origin) {
  return origin != protocol::CSS::StyleSheetOriginEnum::UserAgent &&
         origin != protocol::CSS::StyleSheetOriginEnum::Injected;
}

std::unique_ptr<protocol::CSS::CSSRule>
InspectorStyleSheet::BuildObjectForRuleWithoutAncestorData(
    CSSStyleRule* rule,
    Element* element,
    PseudoId pseudo_id,
    const AtomicString& pseudo_argument) {
  std::unique_ptr<protocol::CSS::CSSRule> result =
      protocol::CSS::CSSRule::create()
          .setSelectorList(BuildObjectForSelectorList(rule))
          .setOrigin(origin_)
          .setStyle(BuildObjectForStyle(rule->style(), element, pseudo_id,
                                        pseudo_argument))
          .build();

  if (CanBind(origin_)) {
    if (!Id().empty())
      result->setStyleSheetId(Id());
  }

  return result;
}

std::unique_ptr<protocol::CSS::RuleUsage>
InspectorStyleSheet::BuildObjectForRuleUsage(CSSRule* rule, bool was_used) {
  CSSRuleSourceData* source_data = SourceDataForRule(rule);

  if (!source_data)
    return nullptr;

  SourceRange whole_rule_range(source_data->rule_header_range.start,
                               source_data->rule_body_range.end + 1);
  auto type = rule->GetType();
  if (type == CSSRule::kMediaRule || type == CSSRule::kSupportsRule ||
      type == CSSRule::kScopeRule || type == CSSRule::kContainerRule) {
    whole_rule_range.end = source_data->rule_header_range.end + 1;
  }

  std::unique_ptr<protocol::CSS::RuleUsage> result =
      protocol::CSS::RuleUsage::create()
          .setStyleSheetId(Id())
          .setStartOffset(whole_rule_range.start)
          .setEndOffset(whole_rule_range.end)
          .setUsed(was_used)
          .build();

  return result;
}

std::unique_ptr<protocol::CSS::CSSPositionTryRule>
InspectorStyleSheet::BuildObjectForPositionTryRule(
    CSSPositionTryRule* position_try_rule,
    bool active) {
  std::unique_ptr<protocol::CSS::Value> name =
      protocol::CSS::Value::create().setText(position_try_rule->name()).build();
  if (CSSRuleSourceData* source_data = SourceDataForRule(position_try_rule)) {
    name->setRange(BuildSourceRangeObject(source_data->rule_header_range));
  }
  std::unique_ptr<protocol::CSS::CSSPositionTryRule> result =
      protocol::CSS::CSSPositionTryRule::create()
          .setName(std::move(name))
          .setOrigin(origin_)
          .setStyle(BuildObjectForStyle(position_try_rule->style(), nullptr))
          .setActive(active)
          .build();
  if (CanBind(origin_) && !Id().empty()) {
    result->setStyleSheetId(Id());
  }
  return result;
}

std::unique_ptr<protocol::CSS::CSSFontPaletteValuesRule>
InspectorStyleSheet::BuildObjectForFontPaletteValuesRule(
    CSSFontPaletteValuesRule* values_rule) {
  std::unique_ptr<protocol::CSS::Value> name_text =
      protocol::CSS::Value::create().setText(values_rule->name()).build();
  CSSRuleSourceData* source_data = SourceDataForRule(values_rule);
  if (source_data)
    name_text->setRange(BuildSourceRangeObject(source_data->rule_header_range));
  std::unique_ptr<protocol::CSS::CSSFontPaletteValuesRule> result =
      protocol::CSS::CSSFontPaletteValuesRule::create()
          .setFontPaletteName(std::move(name_text))
          .setOrigin(origin_)
          .setStyle(BuildObjectForStyle(values_rule->Style(), nullptr))
          .build();
  if (CanBind(origin_) && !Id().empty())
    result->setStyleSheetId(Id());
  return result;
}

std::unique_ptr<protocol::CSS::CSSPropertyRule>
InspectorStyleSheet::BuildObjectForPropertyRule(
    CSSPropertyRule* property_rule) {
  std::unique_ptr<protocol::CSS::Value> name_text =
      protocol::CSS::Value::create().setText(property_rule->name()).build();
  CSSRuleSourceData* source_data = SourceDataForRule(property_rule);
  if (source_data)
    name_text->setRange(BuildSourceRangeObject(source_data->rule_header_range));
  std::unique_ptr<protocol::CSS::CSSPropertyRule> result =
      protocol::CSS::CSSPropertyRule::create()
          .setPropertyName(std::move(name_text))
          .setOrigin(origin_)
          .setStyle(BuildObjectForStyle(property_rule->Style(), nullptr))
          .build();
  if (CanBind(origin_) && !Id().empty())
    result->setStyleSheetId(Id());
  return result;
}

std::unique_ptr<protocol::CSS::CSSKeyframeRule>
InspectorStyleSheet::BuildObjectForKeyframeRule(CSSKeyframeRule* keyframe_rule,
                                                Element* element) {
  std::unique_ptr<protocol::CSS::Value> key_text =
      protocol::CSS::Value::create().setText(keyframe_rule->keyText()).build();
  CSSRuleSourceData* source_data = SourceDataForRule(keyframe_rule);
  if (source_data)
    key_text->setRange(BuildSourceRangeObject(source_data->rule_header_range));
  std::unique_ptr<protocol::CSS::CSSKeyframeRule> result =
      protocol::CSS::CSSKeyframeRule::create()
          // TODO(samli): keyText() normalises 'from' and 'to' keyword values.
          .setKeyText(std::move(key_text))
          .setOrigin(origin_)
          .setStyle(BuildObjectForStyle(keyframe_rule->style(), element))
          .build();
  if (CanBind(origin_) && !Id().empty())
    result->setStyleSheetId(Id());
  return result;
}

bool InspectorStyleSheet::GetText(String* result) {
  if (source_data_) {
    *result = text_;
    return true;
  }
  return false;
}

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheet::RuleHeaderSourceRange(CSSRule* rule) {
  if (!source_data_)
    return nullptr;
  CSSRuleSourceData* source_data = SourceDataForRule(rule);
  if (!source_data)
    return nullptr;
  return BuildSourceRangeObject(source_data->rule_header_range);
}

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheet::MediaQueryExpValueSourceRange(
    CSSRule* rule,
    wtf_size_t media_query_index,
    wtf_size_t media_query_exp_index) {
  if (!source_data_)
    return nullptr;
  CSSRuleSourceData* source_data = SourceDataForRule(rule);
  if (!source_data || !source_data->HasMedia() ||
      media_query_index >= source_data->media_query_exp_value_ranges.size())
    return nullptr;
  const Vector<SourceRange>& media_query_exp_data =
      source_data->media_query_exp_value_ranges[media_query_index];
  if (media_query_exp_index >= media_query_exp_data.size())
    return nullptr;
  return BuildSourceRangeObject(media_query_exp_data[media_query_exp_index]);
}

InspectorStyle* InspectorStyleSheet::GetInspectorStyle(
    CSSStyleDeclaration* style) {
  return style ? MakeGarbageCollected<InspectorStyle>(
                     style, SourceDataForRule(style->parentRule()), this)
               : nullptr;
}

String InspectorStyleSheet::SourceURL() {
  if (!source_url_.IsNull())
    return source_url_;
  if (origin_ != protocol::CSS::StyleSheetOriginEnum::Regular) {
    source_url_ = "";
    return source_url_;
  }

  String style_sheet_text;
  bool success = GetText(&style_sheet_text);
  if (success) {
    String comment_value = FindMagicComment(style_sheet_text, "sourceURL");
    if (!comment_value.empty()) {
      source_url_ = comment_value;
      return comment_value;
    }
  }
  source_url_ = "";
  return source_url_;
}

String InspectorStyleSheet::Url() {
  // "sourceURL" is present only for regular rules, otherwise "origin" should be
  // used in the frontend.
  if (origin_ != protocol::CSS::StyleSheetOriginEnum::Regular)
    return String();

  CSSStyleSheet* style_sheet = PageStyleSheet();
  if (!style_sheet)
    return String();

  if (HasSourceURL())
    return SourceURL();

  if (style_sheet->IsInline() && StartsAtZero())
    return String();

  return FinalURL();
}

bool InspectorStyleSheet::HasSourceURL() {
  return !SourceURL().empty();
}

bool InspectorStyleSheet::StartsAtZero() {
  CSSStyleSheet* style_sheet = PageStyleSheet();
  if (!style_sheet)
    return true;

  return style_sheet->StartPositionInSource() ==
         TextPosition::MinimumPosition();
}

String InspectorStyleSheet::SourceMapURL() {
  if (origin_ != protocol::CSS::StyleSheetOriginEnum::Regular)
    return String();

  String style_sheet_text;
  bool success = GetText(&style_sheet_text);
  if (success) {
    String comment_value =
        FindMagicComment(style_sheet_text, "sourceMappingURL");
    if (!comment_value.empty())
      return comment_value;
  }
  return page_style_sheet_->Contents()->SourceMapURL();
}

const Document* InspectorStyleSheet::GetDocument() {
  return CSSStyleSheet::SingleOwnerDocument(
      InspectorStyleSheet::PageStyleSheet());
}

CSSRuleSourceData* InspectorStyleSheet::FindRuleByHeaderRange(
    const SourceRange& source_range) {
  if (!source_data_)
    return nullptr;

  for (wtf_size_t i = 0; i < source_data_->size(); ++i) {
    CSSRuleSourceData* rule_source_data = source_data_->at(i).Get();
    if (rule_source_data->rule_header_range.start == source_range.start &&
        rule_source_data->rule_header_range.end == source_range.end) {
      return rule_source_data;
    }
  }
  return nullptr;
}

CSSRuleSourceData* InspectorStyleSheet::FindRuleByDeclarationsRange(
    const SourceRange& source_range) {
  if (!source_data_)
    return nullptr;

  for (wtf_size_t i = 0; i < source_data_->size(); ++i) {
    CSSRuleSourceData* rule_source_data = source_data_->at(i).Get();
    if (rule_source_data->rule_declarations_range.start == source_range.start &&
        rule_source_data->rule_declarations_range.end == source_range.end) {
      return rule_source_data;
    }
  }
  return nullptr;
}

CSSRule* InspectorStyleSheet::RuleForSourceData(
    CSSRuleSourceData* source_data) {
  if (!source_data_ || !source_data)
    return nullptr;

  RemapSourceDataToCSSOMIfNecessary();

  wtf_size_t index = source_data_->Find(source_data);
  if (index == kNotFound)
    return nullptr;
  InspectorIndexMap::iterator it = source_data_to_rule_.find(index);
  if (it == source_data_to_rule_.end())
    return nullptr;

  DCHECK_LT(it->value, cssom_flat_rules_.size());

  // Check that CSSOM did not mutate this rule.
  CSSRule* result = cssom_flat_rules_.at(it->value);
  if (CanonicalCSSText(parsed_flat_rules_.at(index)) !=
      CanonicalCSSText(result))
    return nullptr;
  return result;
}

CSSRuleSourceData* InspectorStyleSheet::SourceDataForRule(CSSRule* rule) {
  if (!source_data_ || !rule)
    return nullptr;

  RemapSourceDataToCSSOMIfNecessary();

  wtf_size_t index = cssom_flat_rules_.Find(rule);
  if (index == kNotFound)
    return nullptr;
  InspectorIndexMap::iterator it = rule_to_source_data_.find(index);
  if (it == rule_to_source_data_.end())
    return nullptr;

  DCHECK_LT(it->value, source_data_->size());

  // Check that CSSOM did not mutate this rule.
  CSSRule* parsed_rule = parsed_flat_rules_.at(it->value);
  if (CanonicalCSSText(rule) != CanonicalCSSText(parsed_rule))
    return nullptr;
  return source_data_->at(it->value).Get();
}

void InspectorStyleSheet::RemapSourceDataToCSSOMIfNecessary() {
  CSSRuleVector cssom_rules;
  CollectFlatRules(page_style_sheet_.Get(), &cssom_rules);

  if (cssom_rules.size() != cssom_flat_rules_.size()) {
    MapSourceDataToCSSOM();
    return;
  }

  for (wtf_size_t i = 0; i < cssom_flat_rules_.size(); ++i) {
    if (cssom_flat_rules_.at(i) != cssom_rules.at(i)) {
      MapSourceDataToCSSOM();
      return;
    }
  }
}

void InspectorStyleSheet::MapSourceDataToCSSOM() {
  rule_to_source_data_.clear();
  source_data_to_rule_.clear();

  cssom_flat_rules_.clear();
  CSSRuleVector& cssom_rules = cssom_flat_rules_;
  CollectFlatRules(page_style_sheet_.Get(), &cssom_rules);

  if (!source_data_)
    return;

  CSSRuleVector& parsed_rules = parsed_flat_rules_;

  if (page_style_sheet_->IsConstructed()) {
    // If we are dealing with constructed stylesheets, the order
    // of the parsed_rules matches the order of cssom_rules
    // because the source CSS is generated based on CSSOM rules
    // in the same order.
    // Therefore, we can skip the expensive diff algorithm below
    // that causes performance issues if there are subtle differences
    // in rules due to specific issues with the CSS parser.
    // See crbug.com/1131113, crbug.com/604023, crbug.com/1132778.
    DCHECK(parsed_rules.size() == cssom_rules.size());
    auto min_size = std::min(parsed_rules.size(), cssom_rules.size());
    for (wtf_size_t i = 0; i < min_size; ++i) {
      rule_to_source_data_.Set(i, i);
      source_data_to_rule_.Set(i, i);
    }
    return;
  }

  Vector<String> cssom_rules_text = Vector<String>();
  Vector<String> parsed_rules_text = Vector<String>();
  for (wtf_size_t i = 0; i < cssom_rules.size(); ++i)
    cssom_rules_text.push_back(CanonicalCSSText(cssom_rules.at(i)));
  for (wtf_size_t j = 0; j < parsed_rules.size(); ++j)
    parsed_rules_text.push_back(CanonicalCSSText(parsed_rules.at(j)));

  InspectorDiff::FindLCSMapping(cssom_rules_text, parsed_rules_text,
                                &rule_to_source_data_, &source_data_to_rule_);
}

const CSSRuleVector& InspectorStyleSheet::FlatRules() {
  RemapSourceDataToCSSOMIfNecessary();
  return cssom_flat_rules_;
}

bool InspectorStyleSheet::ResourceStyleSheetText(String* result,
                                                 bool* loadingFailed) {
  if (origin_ == protocol::CSS::StyleSheetOriginEnum::Injected ||
      origin_ == protocol::CSS::StyleSheetOriginEnum::UserAgent)
    return false;

  if (!page_style_sheet_->OwnerDocument())
    return false;

  // Original URL defined in CSS.
  String href = page_style_sheet_->href();

  // Not a resource style sheet.
  if (!href)
    return false;

  // FinalURL() is a URL after redirects, whereas, href is not.
  // FinalURL() is used to call resource_container_->StoreStyleSheetContent
  // so it has to be used for lookups.
  if (resource_container_->LoadStyleSheetContent(KURL(FinalURL()), result))
    return true;

  bool base64_encoded;
  bool success = network_agent_->FetchResourceContent(
      page_style_sheet_->OwnerDocument(), KURL(href), result, &base64_encoded,
      loadingFailed);
  return success && !base64_encoded;
}

Element* InspectorStyleSheet::OwnerStyleElement() {
  Node* owner_node = page_style_sheet_->ownerNode();
  auto* owner_element = DynamicTo<Element>(owner_node);
  if (!owner_element)
    return nullptr;

  if (!IsA<HTMLStyleElement>(owner_element) &&
      !IsA<SVGStyleElement>(owner_element))
    return nullptr;
  return owner_element;
}

String InspectorStyleSheet::CollectStyleSheetRules() {
  StringBuilder builder;
  for (unsigned i = 0; i < page_style_sheet_->length(); i++) {
    builder.Append(page_style_sheet_->ItemInternal(i)->cssText());
    builder.Append('\n');
  }
  return builder.ToString();
}

bool InspectorStyleSheet::CSSOMStyleSheetText(String* result) {
  if (origin_ != protocol::CSS::StyleSheetOriginEnum::Regular) {
    return false;
  }
  *result = CollectStyleSheetRules();
  return true;
}

void InspectorStyleSheet::Reset() {
  ResetLineEndings();
  if (source_data_)
    source_data_->clear();
  cssom_flat_rules_.clear();
  parsed_flat_rules_.clear();
  rule_to_source_data_.clear();
  source_data_to_rule_.clear();
}

void InspectorStyleSheet::SyncTextIfNeeded() {
  if (!marked_for_sync_)
    return;
  Reset();
  UpdateText();
  marked_for_sync_ = false;
}

void InspectorStyleSheet::UpdateText() {
  String text;
  request_failed_to_load_.reset();
  bool success = InspectorStyleSheetText(&text);
  if (!success)
    success = InlineStyleSheetText(&text);
  if (!success) {
    bool loadingFailed = false;
    success = ResourceStyleSheetText(&text, &loadingFailed);
    request_failed_to_load_ = loadingFailed;
  }
  if (!success)
    success = CSSOMStyleSheetText(&text);
  if (success)
    InnerSetText(text, false);
}

bool InspectorStyleSheet::IsMutable() const {
  return page_style_sheet_->Contents()->IsMutable();
}

bool InspectorStyleSheet::InlineStyleSheetText(String* out) {
  Element* owner_element = OwnerStyleElement();
  bool result = false;
  if (!owner_element)
    return result;

  result = resource_container_->LoadStyleElementContent(
      owner_element->GetDomNodeId(), out);

  if (!result) {
    *out = owner_element->textContent();
    result = true;
  }

  if (result && IsMutable()) {
    ParseText(*out);
    *out = MergeCSSOMRulesWithText(*out);
  }

  return result;
}

bool InspectorStyleSheet::InspectorStyleSheetText(String* result) {
  if (origin_ != protocol::CSS::StyleSheetOriginEnum::Inspector)
    return false;
  if (!page_style_sheet_->OwnerDocument())
    return false;
  if (resource_container_->LoadStyleElementContent(
          page_style_sheet_->OwnerDocument()->GetDomNodeId(), result)) {
    return true;
  }
  *result = "";
  return true;
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(
    Element* element,
    Listener* listener)
    : InspectorStyleSheetBase(listener, IdentifiersFactory::CreateIdentifier()),
      element_(element) {
  DCHECK(element_);
}

void InspectorStyleSheetForInlineStyle::DidModifyElementAttribute() {
  inspector_style_.Clear();
  OnStyleSheetTextChanged();
}

bool InspectorStyleSheetForInlineStyle::SetText(
    const String& text,
    ExceptionState& exception_state) {
  if (!VerifyStyleText(&element_->GetDocument(), text)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "Style text is not valid.");
    return false;
  }

  {
    InspectorCSSAgent::InlineStyleOverrideScope override_scope(
        element_->GetExecutionContext());
    element_->setAttribute(html_names::kStyleAttr, AtomicString(text),
                           exception_state);
  }
  if (!exception_state.HadException())
    OnStyleSheetTextChanged();
  return !exception_state.HadException();
}

bool InspectorStyleSheetForInlineStyle::GetText(String* result) {
  *result = ElementStyleText();
  return true;
}

InspectorStyle* InspectorStyleSheetForInlineStyle::GetInspectorStyle(
    CSSStyleDeclaration* style) {
  if (!inspector_style_) {
    inspector_style_ = MakeGarbageCollected<InspectorStyle>(
        element_->style(), RuleSourceData(), this);
  }

  return inspector_style_.Get();
}

CSSRuleSourceData* InspectorStyleSheetForInlineStyle::RuleSourceData() {
  const String& text = ElementStyleText();
  CSSRuleSourceData* rule_source_data = nullptr;
  if (text.empty()) {
    rule_source_data =
        MakeGarbageCollected<CSSRuleSourceData>(StyleRule::kStyle);
    rule_source_data->rule_body_range.start = 0;
    rule_source_data->rule_body_range.end = 0;
  } else {
    CSSRuleSourceDataList* rule_source_data_result =
        MakeGarbageCollected<CSSRuleSourceDataList>();
    InspectorCSSParserObserver observer(text, &element_->GetDocument(),
                                        rule_source_data_result);
    CSSParser::ParseDeclarationListForInspector(
        ParserContextForDocument(&element_->GetDocument()), text, observer);
    rule_source_data = rule_source_data_result->front();
  }
  return rule_source_data;
}

CSSStyleDeclaration* InspectorStyleSheetForInlineStyle::InlineStyle() {
  return element_->style();
}

const String& InspectorStyleSheetForInlineStyle::ElementStyleText() {
  return element_->getAttribute(html_names::kStyleAttr).GetString();
}

void InspectorStyleSheetForInlineStyle::Trace(Visitor* visitor) const {
  visitor->Trace(element_);
  visitor->Trace(inspector_style_);
  InspectorStyleSheetBase::Trace(visitor);
}

const Document* InspectorStyleSheetForInlineStyle::GetDocument() {
  return &InspectorStyleSheetForInlineStyle::element_->GetDocument();
}

}  // namespace blink
