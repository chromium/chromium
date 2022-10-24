// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"

#include "base/containers/contains.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"
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

  bool Matches(const Element& el) const override {
    return base::ranges::all_of(clauses_, [&](DocumentRulePredicate* clause) {
      return clause->Matches(el);
    });
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

  bool Matches(const Element& el) const override {
    return base::ranges::any_of(clauses_, [&](DocumentRulePredicate* clause) {
      return clause->Matches(el);
    });
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

  bool Matches(const Element& el) const override {
    return !clause_->Matches(el);
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

// static
DocumentRulePredicate* DocumentRulePredicate::Parse(JSONObject* input,
                                                    const KURL& base_url) {
  // If input is not a map, then return null.
  if (!input)
    return nullptr;

  // If input does not contain exactly one of "and", "or", "not", "href_matches"
  // and "selector_matches", then return null.
  const char* const kKnownKeys[] = {"and", "or", "not", "href_matches",
                                    "selector_matches"};
  // Note: The spec currently makes an allowance for a predicate to have
  // multiple keys, but currently none of the defined predicates can be paired
  // with other keys, so we just assume that there can only be one key and it is
  // the predicate type.
  if (input->size() != 1)
    return nullptr;
  if (!base::Contains(kKnownKeys, input->at(0).first))
    return nullptr;

  // Otherwise, let predicateType be that key.
  const String& predicate_type = input->at(0).first;

  // If predicateType is "and" or "or"
  if (predicate_type == "and" || predicate_type == "or") {
    // Let rawClauses be the input[predicateType].
    blink::JSONArray* raw_clauses = input->GetArray(predicate_type);

    // If rawClauses is not a list, then return null.
    if (!raw_clauses)
      return nullptr;

    // Let clauses be an empty list.
    HeapVector<Member<DocumentRulePredicate>> clauses;
    clauses.ReserveInitialCapacity(raw_clauses->size());
    // For each rawClause of rawClauses:
    for (wtf_size_t i = 0; i < raw_clauses->size(); i++) {
      JSONObject* raw_clause = JSONObject::Cast(raw_clauses->at(i));
      // Let clause be the result of parsing a document rule predicate given
      // rawClause and baseURL.
      DocumentRulePredicate* clause = Parse(raw_clause, base_url);
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
    // Let rawClause be the input[predicateType].
    JSONObject* raw_clause = input->GetJSONObject(predicate_type);

    // Let clause be the result of parsing a document rule predicate given
    // rawClause and baseURL.
    DocumentRulePredicate* clause = Parse(raw_clause, base_url);

    // If clause is null, then return null.
    if (!clause)
      return nullptr;

    // Return a document rule negation whose clause is clause.
    return MakeGarbageCollected<Negation>(clause);
  }

  // If predicateType is "href_matches"
  if (predicate_type == "href_matches") {
    // TODO(crbug.com/1371522): Implement this.
    NOTIMPLEMENTED();
  }

  // If predicateType is "selector_matches"
  if (predicate_type == "selector_matches") {
    // TODO(crbug.com/1371522): Implement this.
    NOTIMPLEMENTED();
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
  NOTREACHED();
  return {};
}

void DocumentRulePredicate::Trace(Visitor*) const {}

}  // namespace blink
