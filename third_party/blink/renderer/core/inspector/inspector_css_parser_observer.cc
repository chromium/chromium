// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/inspector/inspector_css_parser_observer.h"

#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"

namespace blink {

namespace {

const CSSParserContext* ParserContextForDocument(const Document* document) {
  // Fallback to an insecure context parser if no document is present.
  return document ? MakeGarbageCollected<CSSParserContext>(*document)
                  : StrictCSSParserContext(SecureContextMode::kInsecureContext);
}

}  // namespace

void InspectorCSSParserObserver::StartRuleHeader(StyleRule::RuleType type,
                                                 unsigned offset) {
  // Pop off data for a previous invalid rule.
  if (current_rule_data_) {
    current_rule_data_stack_.pop_back();
  }

  CSSRuleSourceData* data = MakeGarbageCollected<CSSRuleSourceData>(type);
  data->rule_header_range.start = offset;
  current_rule_data_ = data;
  current_rule_data_stack_.push_back(data);
}

template <typename CharacterType>
void InspectorCSSParserObserver::SetRuleHeaderEnd(
    const base::span<const CharacterType> data_start,
    unsigned list_end_offset) {
  while (list_end_offset > 1) {
    if (IsHTMLSpace<CharacterType>(data_start[list_end_offset - 1])) {
      --list_end_offset;
    } else {
      break;
    }
  }

  current_rule_data_stack_.back()->rule_header_range.end = list_end_offset;
  if (!current_rule_data_stack_.back()->selector_ranges.empty()) {
    current_rule_data_stack_.back()->selector_ranges.back().end =
        list_end_offset;
  }
}

void InspectorCSSParserObserver::EndRuleHeader(unsigned offset) {
  DCHECK(!current_rule_data_stack_.empty());

  if (parsed_text_.Is8Bit()) {
    SetRuleHeaderEnd<LChar>(parsed_text_.Span8(), offset);
  } else {
    SetRuleHeaderEnd<UChar>(parsed_text_.Span16(), offset);
  }
}

void InspectorCSSParserObserver::ObserveSelector(unsigned start_offset,
                                                 unsigned end_offset) {
  DCHECK(current_rule_data_stack_.size());
  current_rule_data_stack_.back()->selector_ranges.push_back(
      SourceRange(start_offset, end_offset));
}

void InspectorCSSParserObserver::StartRuleBody(unsigned offset) {
  current_rule_data_ = nullptr;
  DCHECK(!current_rule_data_stack_.empty());
  if (parsed_text_[offset] == '{') {
    ++offset;  // Skip the rule body opening brace.
  }
  current_rule_data_stack_.back()->rule_body_range.start = offset;

  // If this style rule appears on the same offset as a failed property,
  // we need to remove the corresponding CSSPropertySourceData.
  // See `replaceable_property_offset_` for more information.
  if (replaceable_property_offset_.has_value() &&
      current_rule_data_stack_.size() >= 2) {
    if (replaceable_property_offset_ ==
        current_rule_data_stack_.back()->rule_header_range.start) {
      // The outer rule holds a property at the same offset. Remove it.
      CSSRuleSourceData& outer_rule =
          *current_rule_data_stack_[current_rule_data_stack_.size() - 2];
      DCHECK(!outer_rule.property_data.empty());
      outer_rule.property_data.pop_back();
      replaceable_property_offset_ = std::nullopt;
    }
  }
}

void InspectorCSSParserObserver::EndRuleBody(unsigned offset) {
  // Pop off data for a previous invalid rule.
  if (current_rule_data_) {
    current_rule_data_ = nullptr;
    current_rule_data_stack_.pop_back();
  }
  DCHECK(!current_rule_data_stack_.empty());

  CSSRuleSourceData* current_rule = current_rule_data_stack_.back().Get();
  Vector<CSSPropertySourceData>& property_data = current_rule->property_data;

  // See comment about non-empty property_data for rules with
  // HasProperties()==false in ObserveProperty.
  if (!current_rule->HasProperties()) {
    // It's possible for nested grouping rules to still hold some
    // CSSPropertySourceData objects if only commented-out or invalid
    // declarations were observed. There will be no ObserveNestedDeclarations
    // call in that case.
    property_data.clear();
  }

  current_rule->rule_body_range.end = offset;
  current_rule->rule_declarations_range = current_rule->rule_body_range;

  if (!current_rule->child_rules.empty() && !property_data.empty()) {
    // Cut off the declarations range at the end of the last declaration
    // if there are child rules. Following bare declarations are captured
    // by CSSNestedDeclarations.
    unsigned end_of_last_declaration =
        property_data.empty() ? current_rule->rule_declarations_range.start
                              : property_data.back().range.end;
    current_rule->rule_declarations_range.end = end_of_last_declaration;
  }

  AddNewRuleToSourceTree(PopRuleData());
}

void InspectorCSSParserObserver::AddNewRuleToSourceTree(
    CSSRuleSourceData* rule) {
  // After a rule is parsed, if it doesn't have a header range
  // and if it is a style rule it means that this is a "nested group
  // rule"[1][2]. When there are property declarations in this rule there is an
  // implicit nested rule is created for this to hold these declarations[3].
  // However, when there aren't any property declarations in this rule
  // there won't be an implicit nested rule for it and it will only
  // contain parsed child rules[3].
  // So, for that case, we are not adding the source data for the non
  // existent implicit nested rule since it won't exist in the parsed
  // CSS rules from the parser itself.
  //
  // We're also not adding the source data for the non-existent
  // implicit nested rule when there aren't any non-disabled properties
  // inside the rule. A `disabled` property means that
  // it is a commented out property and parsing it happens
  // inside the inspector[4] and it is not a feature of the Blink CSS parser.
  // So, even if there is a disabled property in the rule; the rule is not added
  // as a CSSOM rule in the blink parser, because of this, we're not adding it
  // as a rule to the source data as well.
  //
  //   NOTE: After the introduction of CSSNestedDeclarations, the implicit
  //         wrapper rules are instead handled by ObserveNestedDeclarations.
  //
  // [1]: https://drafts.csswg.org/css-nesting-1/#nested-group-rules
  // [2]:
  // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/css/parser/css_parser_impl.cc;l=2122;drc=255b4e7036f1326f2219bd547d3d6dcf76064870
  // [3]:
  // https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/css/parser/css_parser_impl.cc;l=2131;drc=255b4e7036f1326f2219bd547d3d6dcf76064870
  // [4]:
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/inspector/inspector_style_sheet.cc;l=484?q=f:inspector_style_sheet
  if (!RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled() &&
      rule->rule_header_range.length() == 0 &&
      (rule->type == StyleRule::RuleType::kStyle)) {
    // Check if there is an active property inside the style rule.
    bool contains_active_property = false;
    for (auto property_data : rule->property_data) {
      if (!property_data.disabled) {
        contains_active_property = true;
        break;
      }
    }

    // If there isn't any active property declaration
    // there won't be an implicit nested rule created for this rule.
    // So, we skip adding it here too and only add its child rules.
    if (!contains_active_property) {
      // Add the source data for the child rules since they exist in the
      // rule data coming from the parser.
      for (auto& child_rule : rule->child_rules) {
        AddNewRuleToSourceTree(child_rule);
      }
      return;
    }
  }

  if (current_rule_data_stack_.empty()) {
    result_->push_back(rule);
  } else {
    current_rule_data_stack_.back()->child_rules.push_back(rule);
  }
}

void InspectorCSSParserObserver::RemoveLastRuleFromSourceTree() {
  if (current_rule_data_stack_.empty()) {
    result_->pop_back();
  } else {
    current_rule_data_stack_.back()->child_rules.pop_back();
  }
}

CSSRuleSourceData* InspectorCSSParserObserver::PopRuleData() {
  DCHECK(!current_rule_data_stack_.empty());
  current_rule_data_ = nullptr;
  CSSRuleSourceData* data = current_rule_data_stack_.back().Get();
  current_rule_data_stack_.pop_back();
  return data;
}

namespace {

wtf_size_t FindColonIndex(const String& property_string) {
  wtf_size_t index = 0;
  while (index != kNotFound && index < property_string.length()) {
    index = std::min(property_string.Find("/*", index),
                     property_string.Find(":", index));
    if (index == kNotFound || property_string[index] == ':') {
      return index;
    }
    if (index >= property_string.length() - 2) {
      return kNotFound;
    }
    // We're in a comment inside the property name, skip past it.
    index = property_string.Find("*/", index + 2);
    if (index != kNotFound) {
      index += 2;
    }
  }
  return kNotFound;
}

}  // namespace

void InspectorCSSParserObserver::ObserveProperty(unsigned start_offset,
                                                 unsigned end_offset,
                                                 bool is_important,
                                                 bool is_parsed) {
  // Pop off data for a previous invalid rule.
  if (current_rule_data_) {
    current_rule_data_ = nullptr;
    current_rule_data_stack_.pop_back();
  }

  if (current_rule_data_stack_.empty()) {
    return;
  }
  if (!current_rule_data_stack_.back()->HasProperties()) {
    if (!RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled()) {
      // We normally don't allow rules with HasProperties()==false to hold
      // properties directly.
      return;
    }
    // However, with CSSNestedDeclarations enabled, we *can* see ObserveProperty
    // calls for nested group rules, e.g. @media.
    //
    // Example:
    //
    //  div {
    //    @media (width > 100px) {
    //      width: 100px;
    //      height: 100px;
    //    }
    //  }
    //
    // Here, the declarations appear directly within @media, and they are
    // reported as such through the CSSParserObserver. We therefore allow
    // properties (CSSPropertySourceData objects) to exist temporarily
    // on rules with HasProperties()==false, with the expectation that
    // an ObserveNestedDeclarations call will come later and erase those
    // properties again.
  }

  DCHECK_LE(end_offset, parsed_text_.length());
  if (end_offset < parsed_text_.length() &&
      parsed_text_[end_offset] ==
          ';') {  // Include semicolon into the property text.
    ++end_offset;
  }

  DCHECK_LT(start_offset, end_offset);
  String property_string =
      parsed_text_.Substring(start_offset, end_offset - start_offset)
          .StripWhiteSpace();
  if (property_string.EndsWith(';')) {
    property_string = property_string.Left(property_string.length() - 1);
  }
  wtf_size_t colon_index = FindColonIndex(property_string);
  DCHECK_NE(colon_index, kNotFound);

  String name = property_string.Left(colon_index).StripWhiteSpace();
  String value =
      property_string.Substring(colon_index + 1, property_string.length())
          .StripWhiteSpace();
  current_rule_data_stack_.back()->property_data.push_back(
      CSSPropertySourceData(name, value, is_important, false, is_parsed,
                            SourceRange(start_offset, end_offset)));

  // Any property with is_parsed=false becomes a replaceable property.
  // A replaceable property can be replaced by a (valid) style rule
  // at the same offset.
  replaceable_property_offset_ = is_parsed
                                     ? std::optional<unsigned>()
                                     : std::optional<unsigned>(start_offset);
}

void InspectorCSSParserObserver::ObserveComment(unsigned start_offset,
                                                unsigned end_offset) {
  // Pop off data for a previous invalid rule.
  if (current_rule_data_) {
    current_rule_data_ = nullptr;
    current_rule_data_stack_.pop_back();
  }
  DCHECK_LE(end_offset, parsed_text_.length());

  if (current_rule_data_stack_.empty() ||
      !current_rule_data_stack_.back()->rule_header_range.end) {
    return;
  }
  if (!current_rule_data_stack_.back()->HasProperties() &&
      !RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled()) {
    // See comment for similar check in ObserveProperty.
    return;
  }

  // The lexer is not inside a property AND it is scanning a declaration-aware
  // rule body.
  String comment_text =
      parsed_text_.Substring(start_offset, end_offset - start_offset);

  DCHECK(comment_text.StartsWith("/*"));
  comment_text = comment_text.Substring(2);

  // Require well-formed comments.
  if (!comment_text.EndsWith("*/")) {
    return;
  }
  comment_text =
      comment_text.Substring(0, comment_text.length() - 2).StripWhiteSpace();
  if (comment_text.empty()) {
    return;
  }

  // FIXME: Use the actual rule type rather than STYLE_RULE?
  CSSRuleSourceDataList* source_data =
      MakeGarbageCollected<CSSRuleSourceDataList>();

  InspectorCSSParserObserver observer(comment_text, document_, source_data);
  CSSParser::ParseDeclarationListForInspector(
      ParserContextForDocument(document_), comment_text, observer);
  Vector<CSSPropertySourceData>& comment_property_data =
      source_data->front()->property_data;
  if (comment_property_data.size() != 1) {
    return;
  }
  CSSPropertySourceData& property_data = comment_property_data.at(0);
  bool parsed_ok = property_data.parsed_ok ||
                   property_data.name.StartsWith("-moz-") ||
                   property_data.name.StartsWith("-o-") ||
                   property_data.name.StartsWith("-webkit-") ||
                   property_data.name.StartsWith("-ms-");
  if (!parsed_ok || property_data.range.length() != comment_text.length()) {
    return;
  }

  current_rule_data_stack_.back()->property_data.push_back(
      CSSPropertySourceData(property_data.name, property_data.value, false,
                            true, true, SourceRange(start_offset, end_offset)));
}

static OrdinalNumber AddOrdinalNumbers(OrdinalNumber a, OrdinalNumber b) {
  if (a == OrdinalNumber::BeforeFirst() || b == OrdinalNumber::BeforeFirst()) {
    return a;
  }
  return OrdinalNumber::FromZeroBasedInt(a.ZeroBasedInt() + b.ZeroBasedInt());
}

TextPosition InspectorCSSParserObserver::GetTextPosition(
    unsigned start_offset) {
  if (!issue_reporting_context_) {
    return TextPosition::BelowRangePosition();
  }
  const LineEndings* line_endings = GetLineEndings();
  TextPosition start =
      TextPosition::FromOffsetAndLineEndings(start_offset, *line_endings);
  if (start.line_.ZeroBasedInt() == 0) {
    start.column_ = AddOrdinalNumbers(
        start.column_, issue_reporting_context_->OffsetInSource.column_);
  }
  start.line_ = AddOrdinalNumbers(
      start.line_, issue_reporting_context_->OffsetInSource.line_);
  return start;
}

void InspectorCSSParserObserver::ObserveErroneousAtRule(
    unsigned start_offset,
    CSSAtRuleID id,
    const Vector<CSSPropertyID, 2>& invalid_properties) {
  switch (id) {
    case CSSAtRuleID::kCSSAtRuleImport:
      if (issue_reporting_context_) {
        TextPosition start = GetTextPosition(start_offset);
        AuditsIssue::ReportStylesheetLoadingLateImportIssue(
            document_, issue_reporting_context_->DocumentURL, start.line_,
            start.column_);
      }
      break;
    case CSSAtRuleID::kCSSAtRuleProperty: {
      if (invalid_properties.empty()) {
        if (issue_reporting_context_) {
          // Invoked from the prelude handling, which means the name is invalid.
          TextPosition start = GetTextPosition(start_offset);
          AuditsIssue::ReportPropertyRuleIssue(
              document_, issue_reporting_context_->DocumentURL, start.line_,
              start.column_,
              protocol::Audits::PropertyRuleIssueReasonEnum::InvalidName, {});
        }
      } else {
        // The rule is being dropped because it lacks required descriptors, or
        // some descriptors have invalid values. The rule has already been
        // committed and must be removed.
        for (CSSPropertyID invalid_property : invalid_properties) {
          ReportPropertyRuleFailure(start_offset, invalid_property);
        }
        RemoveLastRuleFromSourceTree();
      }
      break;
    }
    default:
      break;
  }
}

void InspectorCSSParserObserver::ObserveNestedDeclarations(
    unsigned insert_rule_index) {
  CHECK(!current_rule_data_stack_.empty());
  CSSRuleSourceData* rule = current_rule_data_stack_.back().Get();
  Vector<CSSPropertySourceData>& property_data = rule->property_data;
  HeapVector<Member<CSSRuleSourceData>>& child_rules = rule->child_rules;

  CHECK_LE(insert_rule_index, child_rules.size());

  // We're going to insert a CSSRuleSourceData for the nested declarations
  // rule at `insert_rule_index`. The rule that ends up immediately before
  // that CSSRuleSourceData is the "preceding rule".
  CSSRuleSourceData* preceding_rule =
      (insert_rule_index > 0) ? child_rules[insert_rule_index - 1].Get()
                              : nullptr;

  // Traverse backwards until we see a declaration at the preceding rule,
  // or earlier.
  Vector<CSSPropertySourceData>::iterator iter = property_data.end();
  while (iter != property_data.begin()) {
    Vector<CSSPropertySourceData>::iterator prev = std::prev(iter);
    if (preceding_rule &&
        (prev->range.start <= preceding_rule->rule_body_range.end)) {
      break;
    }
    iter = prev;
  }

  // Copy the CSSPropertySourceData objects between preceding and following
  // rules into a new CSSRuleSourceData object for the nested declarations.
  Vector<CSSPropertySourceData> nested_property_data;
  std::ranges::copy(iter, property_data.end(),
                    std::back_inserter(nested_property_data));
  // Remove the objects we just copied from the original vector. They should
  // only exist in one place.
  property_data.resize(property_data.size() - nested_property_data.size());

  CHECK(!nested_property_data.empty());

  // Note that the nested declarations rule has no prelude (i.e. no selector
  // list), and no curly brackets surrounding its body. Therefore, the header
  // range is empty, and exists at the same offset as the body-start.
  auto* nested_declarations_rule =
      MakeGarbageCollected<CSSRuleSourceData>(StyleRule::kStyle);
  nested_declarations_rule->rule_header_range.start =
      nested_property_data.front().range.start;
  nested_declarations_rule->rule_header_range.end =
      nested_property_data.front().range.start;
  nested_declarations_rule->rule_body_range.start =
      nested_property_data.front().range.start;
  nested_declarations_rule->rule_body_range.end =
      nested_property_data.back().range.end;
  nested_declarations_rule->rule_declarations_range =
      nested_declarations_rule->rule_body_range;
  nested_declarations_rule->property_data = std::move(nested_property_data);
  child_rules.insert(insert_rule_index, nested_declarations_rule);
}

static CSSPropertySourceData* GetPropertySourceData(
    CSSRuleSourceData& source_data,
    StringView propertyName) {
  auto property = std::find_if(
      source_data.property_data.rbegin(), source_data.property_data.rend(),
      [propertyName](auto&& prop) { return prop.name == propertyName; });
  if (property == source_data.property_data.rend()) {
    return nullptr;
  }
  return &*property;
}

static std::pair<const char*, const char*> GetPropertyNameAndIssueReason(
    CSSPropertyID invalid_property) {
  switch (invalid_property) {
    case CSSPropertyID::kInitialValue:
      return std::make_pair(
          "initial-value",
          protocol::Audits::PropertyRuleIssueReasonEnum::InvalidInitialValue);
    case CSSPropertyID::kSyntax:
      return std::make_pair(
          "syntax",
          protocol::Audits::PropertyRuleIssueReasonEnum::InvalidSyntax);
    case CSSPropertyID::kInherits:
      return std::make_pair(
          "inherits",
          protocol::Audits::PropertyRuleIssueReasonEnum::InvalidInherits);
    default:
      return std::make_pair(nullptr, nullptr);
  }
}

void InspectorCSSParserObserver::ReportPropertyRuleFailure(
    unsigned start_offset,
    CSSPropertyID invalid_property) {
  if (!issue_reporting_context_) {
    return;
  }
  auto [property_name, issue_reason] =
      GetPropertyNameAndIssueReason(invalid_property);
  if (!property_name) {
    return;
  }

  // We expect AddNewRuleToSourceTree to have been called
  DCHECK((current_rule_data_stack_.empty() && !result_->empty()) ||
         (!current_rule_data_stack_.empty() &&
          !current_rule_data_stack_.back()->child_rules.empty()));
  auto source_data = current_rule_data_stack_.empty()
                         ? result_->back()
                         : current_rule_data_stack_.back()->child_rules.back();

  CSSPropertySourceData* property_data =
      GetPropertySourceData(*source_data, property_name);
  TextPosition start = GetTextPosition(
      property_data ? property_data->range.start : start_offset);
  String value = property_data ? property_data->value : String();
  AuditsIssue::ReportPropertyRuleIssue(
      document_, issue_reporting_context_->DocumentURL, start.line_,
      start.column_, issue_reason, value);
}

const LineEndings* InspectorCSSParserObserver::GetLineEndings() {
  if (line_endings_->size() > 0) {
    return line_endings_.get();
  }
  line_endings_ = WTF::GetLineEndings(parsed_text_);
  return line_endings_.get();
}

}  // namespace blink
