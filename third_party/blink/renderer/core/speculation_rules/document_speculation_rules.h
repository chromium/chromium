// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_

#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

// This corresponds to the document's list of speculation rule sets.
//
// Updates are pushed asynchronously.
class CORE_EXPORT DocumentSpeculationRules
    : public GarbageCollected<DocumentSpeculationRules>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];

  static DocumentSpeculationRules& From(Document&);
  static DocumentSpeculationRules* FromIfExists(Document&);

  explicit DocumentSpeculationRules(Document&);

  const HeapVector<Member<SpeculationRuleSet>>& rule_sets() const {
    return rule_sets_;
  }

  // Appends a newly added rule set.
  void AddRuleSet(SpeculationRuleSet*);

  void Trace(Visitor*) const override;

 private:
  // Retrieves a valid proxy to the speculation host in the browser.
  // May be null if the execution context does not exist.
  mojom::blink::SpeculationHost* GetHost();

  // Pushes the current speculation candidates to the browser, immediately.
  void UpdateSpeculationCandidates();

  HeapVector<Member<SpeculationRuleSet>> rule_sets_;
  HeapMojoRemote<mojom::blink::SpeculationHost> host_;
  bool has_pending_update_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPECULATION_RULES_DOCUMENT_SPECULATION_RULES_H_
