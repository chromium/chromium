// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_anchor_element.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Represents a document rule conjunction:
// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-conjunction.
class Conjunction : public DocumentRulePredicate {
 public:
  explicit Conjunction(HeapVector<Member<DocumentRulePredicate>> clauses)
      : clauses_(std::move(clauses)) {}
  ~Conjunction() override = default;

  bool Matches(const HTMLAnchorElementBase& el) const override {
    return base::ranges::all_of(clauses_, [&](DocumentRulePredicate* clause) {
      return clause->Matches(el);
    });
  }

  HeapVector<Member<StyleRule>> GetStyleRules() const override {
    HeapVector<Member<StyleRule>> rules;
    for (DocumentRulePredicate* clause : clauses_) {
      rules.AppendVector(clause->GetStyleRules());
    }
    return rules;
  }

  String ToString() const override {
    StringBuilder builder;
    builder.Append("And(");
    for (wtf_size_t i = 0; i < clauses_.size(); i++) {
      builder.Append(clauses_[i]->ToString());
      if (i != clauses_.size() - 1)
        builder.Append(", ");
    }
    builder.Append(")");
    return builder.ReleaseString();
  }

  Type GetTypeForTesting() const override { return Type::kAnd; }

  HeapVector<Member<DocumentRulePredicate>> GetSubPredicatesForTesting()
      const override {
    return clauses_;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(clauses_);
    DocumentRulePredicate::Trace(visitor);
  }

 private:
  HeapVector<Member<DocumentRulePredicate>> clauses_;
};

// Represents a document rule disjunction:
// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-disjunction.
class Disjunction : public DocumentRulePredicate {
 public:
  explicit Disjunction(HeapVector<Member<DocumentRulePredicate>> clauses)
      : clauses_(std::move(clauses)) {}
  ~Disjunction() override = default;

  bool Matches(const HTMLAnchorElementBase& el) const override {
    return base::ranges::any_of(clauses_, [&](DocumentRulePredicate* clause) {
      return clause->Matches(el);
    });
  }

  HeapVector<Member<StyleRule>> GetStyleRules() const override {
    HeapVector<Member<StyleRule>> rules;
    for (DocumentRulePredicate* clause : clauses_) {
      rules.AppendVector(clause->GetStyleRules());
    }
    return rules;
  }

  String ToString() const override {
    StringBuilder builder;
    builder.Append("Or(");
    for (wtf_size_t i = 0; i < clauses_.size(); i++) {
      builder.Append(clauses_[i]->ToString());
      if (i != clauses_.size() - 1)
        builder.Append(", ");
    }
    builder.Append(")");
    return builder.ReleaseString();
  }

  Type GetTypeForTesting() const override { return Type::kOr; }

  HeapVector<Member<DocumentRulePredicate>> GetSubPredicatesForTesting()
      const override {
    return clauses_;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(clauses_);
    DocumentRulePredicate::Trace(visitor);
  }

 private:
  HeapVector<Member<DocumentRulePredicate>> clauses_;
};

// Represents a document rule negation:
// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-negation.
class Negation : public DocumentRulePredicate {
 public:
  explicit Negation(DocumentRulePredicate* clause) : clause_(clause) {}
  ~Negation() override = default;

  bool Matches(const HTMLAnchorElementBase& el) const override {
    return !clause_->Matches(el);
  }

  HeapVector<Member<StyleRule>> GetStyleRules() const override {
    return clause_->GetStyleRules();
  }

  String ToString() const override {
    StringBuilder builder;
    builder.Append("Not(");
    builder.Append(clause_->ToString());
    builder.Append(")");
    return builder.ReleaseString();
  }

  Type GetTypeForTesting() const override { return Type::kNot; }

  HeapVector<Member<DocumentRulePredicate>> GetSubPredicatesForTesting()
      const override {
    HeapVector<Member<DocumentRulePredicate>> result;
    result.push_back(clause_);
    return result;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(clause_);
    DocumentRulePredicate::Trace(visitor);
  }

 private:
  Member<DocumentRulePredicate> clause_;
};

}  // namespace

// Represents a document rule URL pattern predicate:
// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-url-pattern-predicate
class URLPatternPredicate : public DocumentRulePredicate {
 public:
  explicit URLPatternPredicate(HeapVector<Member<URLPattern>> patterns,
                               ExecutionContext* execution_context)
      : patterns_(std::move(patterns)), execution_context_(execution_context) {}
  ~URLPatternPredicate() override = default;

  bool Matches(const HTMLAnchorElementBase& el) const override {
    // Let href be the result of running el’s href getter steps.
    const KURL href = el.HrefURL();
    // For each pattern of predicate’s patterns:
    for (const auto& pattern : patterns_) {
      // Match given pattern and href. If the result is not null, return true.
      if (pattern->test(ToScriptStateForMainWorld(execution_context_),
                        MakeGarbageCollected<V8URLPatternInput>(href),
                        ASSERT_NO_EXCEPTION)) {
        return true;
      }
    }
    return false;
  }

  HeapVector<Member<StyleRule>> GetStyleRules() const override { return {}; }

  String ToString() const override {
    StringBuilder builder;
    builder.Append("Href([");
    for (wtf_size_t i = 0; i < patterns_.size(); i++) {
      builder.Append(patterns_[i]->ToString());
      if (i != patterns_.size() - 1)
        builder.Append(", ");
    }
    builder.Append("])");
    return builder.ReleaseString();
  }

  Type GetTypeForTesting() const override { return Type::kURLPatterns; }

  HeapVector<Member<URLPattern>> GetURLPatternsForTesting() const override {
    return patterns_;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(patterns_);
    visitor->Trace(execution_context_);
    DocumentRulePredicate::Trace(visitor);
  }

 private:
  HeapVector<Member<URLPattern>> patterns_;
  Member<ExecutionContext> execution_context_;
};

// Represents a document rule CSS selector predicate:
// https://wicg.github.io/nav-speculation/speculation-rules.html#document-rule-css-selector-predicate
class CSSSelectorPredicate : public DocumentRulePredicate {
 public:
  explicit CSSSelectorPredicate(HeapVector<Member<StyleRule>> style_rules)
      : style_rules_(std::move(style_rules)) {}

  bool Matches(const HTMLAnchorElementBase& link) const override {
    DCHECK(!link.GetDocument().NeedsLayoutTreeUpdate());
    const ComputedStyle* computed_style = link.GetComputedStyle();
    DCHECK(computed_style);
    DCHECK(!DisplayLockUtilities::LockedAncestorPreventingStyle(link));
    const Persistent<HeapHashSet<WeakMember<StyleRule>>>& matched_selectors =
        computed_style->DocumentRulesSelectors();
    if (!matched_selectors) {
      return false;
    }

    for (StyleRule* style_rule : style_rules_) {
      if (matched_selectors->Contains(style_rule)) {
        return true;
      }
    }
    return false;
  }

  HeapVector<Member<StyleRule>> GetStyleRules() const override {
    return style_rules_;
  }

  String ToString() const override {
    StringBuilder builder;
    builder.Append("Selector([");
    for (wtf_size_t i = 0; i < style_rules_.size(); i++) {
      builder.Append(style_rules_[i]->SelectorsText());
      if (i != style_rules_.size() - 1) {
        builder.Append(", ");
      }
    }
    builder.Append("])");
    return builder.ReleaseString();
  }

  Type GetTypeForTesting() const override { return Type::kCSSSelectors; }

  HeapVector<Member<StyleRule>> GetStyleRulesForTesting() const override {
    return style_rules_;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(style_rules_);
    DocumentRulePredicate::Trace(visitor);
  }

 private:
  HeapVector<Member<StyleRule>> style_rules_;
};

namespace {
// If `out_error` is provided and hasn't already had a message set, sets it to
// `message`.
void SetParseErrorMessage(String* out_error, String message) {
  if (out_error && out_error->IsNull()) {
    *out_error = message;
  }
}

URLPattern* ParseRawPattern(v8::Isolate* isolate,
                            JSONValue* raw_pattern,
                            const KURL& base_url,
                            ExceptionState& exception_state,
                            String* out_error) {
  // If rawPattern is a string, then:
  if (String raw_string; raw_pattern->AsString(&raw_string)) {
    // Set pattern to the result of constructing a URLPattern using the
    // URLPattern(input, baseURL) constructor steps given rawPattern and
    // serializedBaseURL.
    V8URLPatternInput* url_pattern_input =
        MakeGarbageCollected<V8URLPatternInput>(raw_string);
    return URLPattern::Create(isolate, url_pattern_input, base_url,
                              exception_state);
  }
  // Otherwise, if rawPattern is a map
  if (JSONObject* pattern_object = JSONObject::Cast(raw_pattern)) {
    // Let init be «[ "baseURL" → serializedBaseURL ]», representing a
    // dictionary of type URLPatternInit.
    URLPatternInit* init = URLPatternInit::Create();
    init->setBaseURL(base_url);

    // For each key -> value of rawPattern:
    for (wtf_size_t i = 0; i < pattern_object->size(); i++) {
      JSONObject::Entry entry = pattern_object->at(i);
      String key = entry.first;
      String value;
      // If value is not a string
      if (!entry.second->AsString(&value)) {
        SetParseErrorMessage(
            out_error, "Values for a URL pattern object must be strings.");
        return nullptr;
      }

      // Set init[key] to value.
      if (key == "protocol") {
        init->setProtocol(value);
      } else if (key == "username") {
        init->setUsername(value);
      } else if (key == "password") {
        init->setPassword(value);
      } else if (key == "hostname") {
        init->setHostname(value);
      } else if (key == "port") {
        init->setPort(value);
      } else if (key == "pathname") {
        init->setPathname(value);
      } else if (key == "search") {
        init->setSearch(value);
      } else if (key == "hash") {
        init->setHash(value);
      } else if (key == "baseURL") {
        init->setBaseURL(value);
      } else {
        SetParseErrorMessage(
            out_error,
            String::Format("Invalid key \"%s\" for a URL pattern object found.",
                           key.Latin1().c_str()));
        return nullptr;
      }
    }

    // Set pattern to the result of constructing a URLPattern using the
    // URLPattern(input, baseURL) constructor steps given init.
    V8URLPatternInput* url_pattern_input =
        MakeGarbageCollected<V8URLPatternInput>(init);
    return URLPattern::Create(isolate, url_pattern_input, exception_state);
  }
  SetParseErrorMessage(out_error,
                       "Value for \"href_matches\" should either be a "
                       "string, an object, or a list of strings and objects.");
  return nullptr;
}

String GetPredicateType(JSONObject* input, String* out_error) {
  String predicate_type;
  constexpr const char* kValidTypes[] = {"and", "or", "not", "href_matches",
                                         "selector_matches"};
  for (String type : kValidTypes) {
    if (input->Get(type)) {
      // If we'd already found one, then this is ambiguous.
      if (!predicate_type.IsNull()) {
        SetParseErrorMessage(
            out_error,
            String::Format("Document rule predicate type is ambiguous, "
                           "two types found: \"%s\" and \"%s\".",
                           predicate_type.Latin1().c_str(),
                           type.Latin1().c_str()));
        return String();
      }

      // Otherwise, this is the predicate type.
      predicate_type = std::move(type);
    }
  }
  if (predicate_type.IsNull()) {
    SetParseErrorMessage(out_error,
                         "Could not infer type of document rule predicate, no "
                         "valid type specified.");
  }
  return predicate_type;
}
}  // namespace

// static
DocumentRulePredicate* DocumentRulePredicate::Parse(
    JSONObject* input,
    const KURL& ruleset_base_url,
    ExecutionContext* execution_context,
    ExceptionState& exception_state,
    String* out_error) {
  // If input is not a map, then return null.
  if (!input) {
    SetParseErrorMessage(out_error,
                         "Document rule predicate must be an object.");
    return nullptr;
  }

  // If we can't get a valid predicate type, return null.
  String predicate_type = GetPredicateType(input, out_error);
  if (predicate_type.IsNull())
    return nullptr;

  // If predicateType is "and" or "or"
  if (predicate_type == "and" || predicate_type == "or") {
    // "and" and "or" cannot be paired with any other keys.
    if (input->size() != 1) {
      SetParseErrorMessage(
          out_error,
          String::Format(
              "Document rule predicate with \"%s\" key cannot have other keys.",
              predicate_type.Latin1().c_str()));
      return nullptr;
    }
    // Let rawClauses be the input[predicateType].
    blink::JSONArray* raw_clauses = input->GetArray(predicate_type);

    // If rawClauses is not a list, then return null.
    if (!raw_clauses) {
      SetParseErrorMessage(
          out_error, String::Format("\"%s\" key should have a list value.",
                                    predicate_type.Latin1().c_str()));
      return nullptr;
    }

    // Let clauses be an empty list.
    HeapVector<Member<DocumentRulePredicate>> clauses;
    clauses.ReserveInitialCapacity(raw_clauses->size());
    // For each rawClause of rawClauses:
    for (wtf_size_t i = 0; i < raw_clauses->size(); i++) {
      JSONObject* raw_clause = JSONObject::Cast(raw_clauses->at(i));
      // Let clause be the result of parsing a document rule predicate given
      // rawClause and baseURL.
      DocumentRulePredicate* clause =
          Parse(raw_clause, ruleset_base_url, execution_context,
                exception_state, out_error);
      // If clause is null, then return null.
      if (!clause)
        return nullptr;
      // Append clause to clauses.
      clauses.push_back(clause);
    }

    // If predicateType is "and", then return a document rule conjunction whose
    // clauses is clauses.
    if (predicate_type == "and")
      return MakeGarbageCollected<Conjunction>(std::move(clauses));
    // If predicateType is "or", then return a document rule disjunction whose
    // clauses is clauses.
    if (predicate_type == "or")
      return MakeGarbageCollected<Disjunction>(std::move(clauses));
  }

  // If predicateType is "not"
  if (predicate_type == "not") {
    // "not" cannot be paired with any other keys.
    if (input->size() != 1) {
      SetParseErrorMessage(
          out_error,
          "Document rule predicate with \"not\" key cannot have other keys.");
      return nullptr;
    }
    // Let rawClause be the input[predicateType].
    JSONObject* raw_clause = input->GetJSONObject(predicate_type);

    // Let clause be the result of parsing a document rule predicate given
    // rawClause and baseURL.
    DocumentRulePredicate* clause =
        Parse(raw_clause, ruleset_base_url, execution_context, exception_state,
              out_error);

    // If clause is null, then return null.
    if (!clause)
      return nullptr;

    // Return a document rule negation whose clause is clause.
    return MakeGarbageCollected<Negation>(clause);
  }

  // If predicateType is "href_matches"
  if (predicate_type == "href_matches") {
    // Explainer:
    // https://github.com/WICG/nav-speculation/blob/main/triggers.md#using-the-documents-base-url-for-external-speculation-rule-sets

    // For now, use the ruleset's base URL to construct the predicates.
    KURL base_url = ruleset_base_url;

    for (wtf_size_t i = 0; i < input->size(); ++i) {
      const String key = input->at(i).first;
      if (key == "href_matches") {
        // This is always expected.
      } else if (key == "relative_to") {
        const char* const kKnownRelativeToValues[] = {"ruleset", "document"};
        // If relativeTo is neither the string "ruleset" nor the string
        // "document", then return null.
        String relative_to;
        if (!input->GetString("relative_to", &relative_to) ||
            !base::Contains(kKnownRelativeToValues, relative_to)) {
          SetParseErrorMessage(
              out_error,
              String::Format(
                  "Unrecognized \"relative_to\" value: %s.",
                  input->Get("relative_to")->ToJSONString().Latin1().c_str()));
          return nullptr;
        }
        // If relativeTo is "document", set baseURL to the document's
        // document base URL.
        if (relative_to == "document") {
          base_url = execution_context->BaseURL();
        }
      } else {
        // Otherwise, this is an unrecognized key. The predicate is invalid.
        SetParseErrorMessage(out_error,
                             String::Format("Unrecognized key found: \"%s\".",
                                            key.Latin1().c_str()));
        return nullptr;
      }
    }

    // Let rawPatterns be input["href_matches"].
    Vector<JSONValue*> raw_patterns;
    JSONArray* href_matches = input->GetArray("href_matches");
    if (href_matches) {
      for (wtf_size_t i = 0; i < href_matches->size(); i++) {
        raw_patterns.push_back(href_matches->at(i));
      }
    } else {
      // If rawPatterns is not a list, then set rawPatterns to « rawPatterns ».
      raw_patterns.push_back(input->Get("href_matches"));
    }
    // Let patterns be an empty list.
    HeapVector<Member<URLPattern>> patterns;
    // For each rawPattern of rawPatterns:
    for (JSONValue* raw_pattern : raw_patterns) {
      URLPattern* pattern =
          ParseRawPattern(execution_context->GetIsolate(), raw_pattern,
                          base_url, IGNORE_EXCEPTION, out_error);
      // If those steps throw, `pattern` will be null. Ignore the exception and
      // return null.
      if (!pattern) {
        SetParseErrorMessage(
            out_error,
            String::Format(
                "URL Pattern for \"href_matches\" could not be parsed: %s.",
                raw_pattern->ToJSONString().Latin1().c_str()));
        return nullptr;
      }
      // Append pattern to patterns.
      patterns.push_back(pattern);
    }
    // Return a document rule URL pattern predicate whose patterns is patterns.
    return MakeGarbageCollected<URLPatternPredicate>(std::move(patterns),
                                                     execution_context);
  }

  // If predicateType is "selector_matches"
  if (predicate_type == "selector_matches" && input->size() == 1) {
    // Let rawSelectors be input["selector_matches"].
    Vector<JSONValue*> raw_selectors;
    JSONArray* selector_matches = input->GetArray("selector_matches");
    if (selector_matches) {
      for (wtf_size_t i = 0; i < selector_matches->size(); i++) {
        raw_selectors.push_back(selector_matches->at(i));
      }
    } else {
      // If rawSelectors is not a list, then set rawSelectors to « rawSelectors
      // ».
      raw_selectors.push_back(input->Get("selector_matches"));
    }
    // Let selectors be an empty list.
    HeapVector<Member<StyleRule>> selectors;
    HeapVector<CSSSelector> arena;
    CSSPropertyValueSet* empty_properties =
        ImmutableCSSPropertyValueSet::Create(nullptr, 0, kUASheetMode);
    CSSParserContext* css_parser_context =
        MakeGarbageCollected<CSSParserContext>(*execution_context);
    for (auto* raw_selector : raw_selectors) {
      String raw_selector_string;
      // If rawSelector is not a string, then return null.
      if (!raw_selector->AsString(&raw_selector_string)) {
        SetParseErrorMessage(out_error,
                             "Value for \"selector_matches\" must be a string "
                             "or a list of strings.");
        return nullptr;
      }

      // Parse a selector from rawSelector. If the result is failure, then
      // return null. Otherwise, let selector be the result.
      base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
          css_parser_context, CSSNestingType::kNone,
          /*parent_rule_for_nesting=*/nullptr, /*is_within_scope=*/false,
          nullptr, raw_selector_string, arena);
      if (selector_vector.empty()) {
        SetParseErrorMessage(
            out_error, String::Format("\"%s\" is not a valid selector.",
                                      raw_selector_string.Latin1().c_str()));
        return nullptr;
      }
      StyleRule* selector =
          StyleRule::Create(selector_vector, empty_properties);
      // Append selector to selectors.
      selectors.push_back(std::move(selector));
    }
    UseCounter::Count(execution_context,
                      WebFeature::kSpeculationRulesSelectorMatches);
    return MakeGarbageCollected<CSSSelectorPredicate>(std::move(selectors));
  }

  return nullptr;
}

// static
DocumentRulePredicate* DocumentRulePredicate::MakeDefaultPredicate() {
  return MakeGarbageCollected<Conjunction>(
      HeapVector<Member<DocumentRulePredicate>>());
}

HeapVector<Member<DocumentRulePredicate>>
DocumentRulePredicate::GetSubPredicatesForTesting() const {
  NOTREACHED_IN_MIGRATION();
  return {};
}

HeapVector<Member<URLPattern>> DocumentRulePredicate::GetURLPatternsForTesting()
    const {
  NOTREACHED_IN_MIGRATION();
  return {};
}

HeapVector<Member<StyleRule>> DocumentRulePredicate::GetStyleRulesForTesting()
    const {
  NOTREACHED_IN_MIGRATION();
  return {};
}

void DocumentRulePredicate::Trace(Visitor*) const {}

}  // namespace blink
