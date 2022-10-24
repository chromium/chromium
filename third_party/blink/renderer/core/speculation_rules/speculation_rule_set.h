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

class ExecutionContext;
class KURL;
class SpeculationRule;

// A set of rules generated from a single <script type=speculationrules>, which
// provides rules to identify URLs and corresponding conditions for speculation,
// grouped by the action that is suggested.
//
// https://wicg.github.io/nav-speculation/speculation-rules.html#speculation-rule-set
class CORE_EXPORT SpeculationRuleSet final
    : public GarbageCollected<SpeculationRuleSet> {
 public:
  // If provided, |out_error| may be populated with an error/warning message.
  // A warning may be present even if parsing succeeds, to indicate a case that,
  // though valid, is likely to be an error.
  static SpeculationRuleSet* Parse(const String& source_text,
                                   const KURL& base_url,
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

  void Trace(Visitor*) const;

 private:
  HeapVector<Member<SpeculationRule>> prefetch_rules_;
  HeapVector<Member<SpeculationRule>> prefetch_with_subresources_rules_;
  HeapVector<Member<SpeculationRule>> prerender_rules_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_SPECULATION_RULE_SET_H_
