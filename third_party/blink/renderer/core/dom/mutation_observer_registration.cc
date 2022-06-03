/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/dom/mutation_observer_registration.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"

namespace blink {

MutationObserverRegistration::MutationObserverRegistration(
    MutationObserver& observer,
    Node* registration_node,
    MutationObserverOptions options,
    const HashSet<AtomicString>& attribute_filter)
    : observer_(&observer),
      registration_node_(registration_node),
      options_(options),
      attribute_filter_(attribute_filter) {
  observer_->ObservationStarted(this);
}

MutationObserverRegistration::~MutationObserverRegistration() = default;

void MutationObserverRegistration::Dispose() {
  ClearTransientRegistrations();
  observer_->ObservationEnded(this);
  observer_.Clear();
}

void MutationObserverRegistration::ResetObservation(
    MutationObserverOptions options,
    const HashSet<AtomicString>& attribute_filter) {
  ClearTransientRegistrations();
  options_ = options;
  attribute_filter_ = attribute_filter;
}

void MutationObserverRegistration::ObservedSubtreeNodeWillDetach(Node& node) {
  if (!IsSubtree())
    return;

  node.RegisterTransientMutationObserver(this);
  observer_->SetHasTransientRegistration();

  if (!transient_registration_nodes_) {
    transient_registration_nodes_ = MakeGarbageCollected<NodeHashSet>();

    DCHECK(registration_node_);
    DCHECK(!registration_node_keep_alive_);
    registration_node_keep_alive_ =
        registration_node_.Get();  // Balanced in clearTransientRegistrations.
  }
  transient_registration_nodes_->insert(&node);
}

void MutationObserverRegistration::ClearTransientRegistrations() {
  if (!transient_registration_nodes_) {
    DCHECK(!registration_node_keep_alive_);
    return;
  }

  for (auto& node : *transient_registration_nodes_)
    node->UnregisterTransientMutationObserver(this);

  transient_registration_nodes_.Clear();

  DCHECK(registration_node_keep_alive_);
  registration_node_keep_alive_ =
      nullptr;  // Balanced in observeSubtreeNodeWillDetach.
}

void MutationObserverRegistration::Unregister() {
  // |this| can outlives registration_node_.
  if (registration_node_)
    registration_node_->UnregisterMutationObserver(this);
  else
    Dispose();
}

bool MutationObserverRegistration::ShouldReceiveMutationFrom(
    Node& node,
    MutationType type,
    const QualifiedName* attribute_name) const {
  DCHECK((type == kMutationTypeAttributes && attribute_name) ||
         !attribute_name);
  if (!(options_ & type))
    return false;

  if (registration_node_ != &node && !IsSubtree())
    return false;

  if (type != kMutationTypeAttributes ||
      !(options_ & MutationObserver::kAttributeFilter))
    return true;

  if (!attribute_name->NamespaceURI().IsNull())
    return false;

  return attribute_filter_.Contains(attribute_name->LocalName());
}

void MutationObserverRegistration::AddRegistrationNodesToSet(
    HeapHashSet<Member<Node>>& nodes) const {
  DCHECK(registration_node_);
  nodes.insert(registration_node_.Get());
  if (!transient_registration_nodes_)
    return;
  for (NodeHashSet::const_iterator iter =
           transient_registration_nodes_->begin();
       iter != transient_registration_nodes_->end(); ++iter)
    nodes.insert(iter->Get());
}

void MutationObserverRegistration::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(registration_node_);
  visitor->Trace(registration_node_keep_alive_);
  visitor->Trace(transient_registration_nodes_);
}

}  // namespace blink
