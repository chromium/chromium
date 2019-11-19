/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/custom/v0_custom_element_upgrade_candidate_map.h"

#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

V0CustomElementUpgradeCandidateMap::~V0CustomElementUpgradeCandidateMap() =
    default;

void V0CustomElementUpgradeCandidateMap::Add(
    const V0CustomElementDescriptor& descriptor,
    Element* element) {
  Observe(element);

  UpgradeCandidateMap::AddResult result =
      upgrade_candidates_.insert(element, descriptor);
  DCHECK(result.is_new_entry);

  UnresolvedDefinitionMap::iterator it =
      unresolved_definitions_.find(descriptor);
  ElementSet* elements;
  if (it == unresolved_definitions_.end())
    elements = unresolved_definitions_
                   .insert(descriptor, MakeGarbageCollected<ElementSet>())
                   .stored_value->value.Get();
  else
    elements = it->value.Get();
  elements->insert(element);
}

void V0CustomElementUpgradeCandidateMap::ElementWasDestroyed(Element* element) {
  V0CustomElementObserver::ElementWasDestroyed(element);
  UpgradeCandidateMap::iterator candidate = upgrade_candidates_.find(element);
  SECURITY_DCHECK(candidate != upgrade_candidates_.end());

  UnresolvedDefinitionMap::iterator elements =
      unresolved_definitions_.find(candidate->value);
  SECURITY_DCHECK(elements != unresolved_definitions_.end());
  elements->value->erase(element);
  upgrade_candidates_.erase(candidate);
}

V0CustomElementUpgradeCandidateMap::ElementSet*
V0CustomElementUpgradeCandidateMap::TakeUpgradeCandidatesFor(
    const V0CustomElementDescriptor& descriptor) {
  ElementSet* candidates = unresolved_definitions_.Take(descriptor);

  if (!candidates)
    return nullptr;

  for (const auto& candidate : *candidates) {
    Unobserve(candidate);
    upgrade_candidates_.erase(candidate);
  }
  return candidates;
}

void V0CustomElementUpgradeCandidateMap::Trace(Visitor* visitor) {
  visitor->Trace(upgrade_candidates_);
  visitor->Trace(unresolved_definitions_);
  V0CustomElementObserver::Trace(visitor);
}

}  // namespace blink
