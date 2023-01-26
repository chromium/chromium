// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_SET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Document;
class ExecutionContext;
class KURL;
class SpeculationRule;
class StyleRule;

// A set of rules generated from a single <script type=speculationrules>, which
// provides rules to identify URLs and corresponding conditions for speculation,
// grouped by the action that is suggested.
//
// https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-set
class CORE_EXPORT SpeculationRuleSet final
    : public GarbageCollected<SpeculationRuleSet> {
 public:
  // Stores the original source text and base URL (if the base URL used isn't
  // the document's base URL) used for parsing a rule set.
  class CORE_EXPORT Source : public GarbageCollected<Source> {
   public:
    Source(const String& source_text, Document&);
    Source(const String& source_text, const KURL& base_url);

    const String& GetSourceText() const;
    KURL GetBaseURL() const;

    void Trace(Visitor*) const;

   private:
    String source_text_;
    // Only set when the SpeculationRuleSet was "out-of-document" (i.e. loaded
    // by a SpeculationRuleLoader).
    absl::optional<KURL> base_url_;
    // Only set when the SpeculationRuleSet was loaded from inline script.
    Member<Document> document_;
  };

  // If provided, |out_error| may be populated with an error/warning message.
  // A warning may be present even if parsing succeeds, to indicate a case that,
  // though valid, is likely to be an error.
  static SpeculationRuleSet* Parse(Source* source,
                                   ExecutionContext* context,
                                   String* out_error = nullptr);

  const HeapVector<Member<SpeculationRule>>& prefetch_rules() const {
    return prefetch_rules_;
  }
  const HeapVector<Member<SpeculationRule>>& prefetch_with_subresources_rules()
      const {
    return prefetch_with_subresources_rules_;
  }
  const HeapVector<Member<SpeculationRule>>& prerender_rules() const {
    return prerender_rules_;
  }

  bool has_document_rule() const { return has_document_rule_; }

  Source* source() const { return source_; }

  const HeapVector<Member<StyleRule>>& selectors() { return selectors_; }

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<SpeculationRule>> prefetch_rules_;
  HeapVector<Member<SpeculationRule>> prefetch_with_subresources_rules_;
  HeapVector<Member<SpeculationRule>> prerender_rules_;
  // The original source is reused to reparse speculation rule sets when the
  // document base URL changes.
  Member<Source> source_;
  HeapVector<Member<StyleRule>> selectors_;
  bool has_document_rule_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_SET_H_
