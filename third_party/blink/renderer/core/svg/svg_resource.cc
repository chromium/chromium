// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cycle_solver.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

SVGResource::SVGResource() = default;

SVGResource::~SVGResource() = default;

void SVGResource::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(clients_);
}

void SVGResource::AddClient(SVGResourceClient& client) {
  auto& entry = clients_.insert(&client, ClientEntry()).stored_value->value;
  entry.count++;
  entry.cached_cycle_check = kNeedCheck;
  if (LayoutSVGResourceContainer* container = ResourceContainerNoCycleCheck())
    container->ClearInvalidationMask();
}

void SVGResource::RemoveClient(SVGResourceClient& client) {
  auto it = clients_.find(&client);
  DCHECK(it != clients_.end());
  it->value.count--;
  if (it->value.count)
    return;
  clients_.erase(it);
  // The last instance of |client| was removed. Clear its entry in
  // resource's cache.
  if (LayoutSVGResourceContainer* container = ResourceContainerNoCycleCheck())
    container->RemoveClientFromCache(client);
}

void SVGResource::InvalidateCycleCache() {
  for (auto& client_entry : clients_.Values())
    client_entry.cached_cycle_check = kNeedCheck;
}

void SVGResource::NotifyElementChanged() {
  InvalidateCycleCache();

  HeapVector<Member<SVGResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceElementChanged();
}

LayoutSVGResourceContainer* SVGResource::ResourceContainerNoCycleCheck() const {
  if (!target_)
    return nullptr;
  return DynamicTo<LayoutSVGResourceContainer>(target_->GetLayoutObject());
}

LayoutSVGResourceContainer* SVGResource::ResourceContainer(
    SVGResourceClient& client) const {
  auto it = clients_.find(&client);
  if (it == clients_.end())
    return nullptr;
  auto* container = ResourceContainerNoCycleCheck();
  if (!container)
    return nullptr;
  ClientEntry& entry = it->value;
  if (entry.cached_cycle_check == kNeedCheck) {
    SVGResourcesCycleSolver solver;
    entry.cached_cycle_check = kPerformingCheck;
    bool has_cycle = container->FindCycle(solver);
    DCHECK_EQ(entry.cached_cycle_check, kPerformingCheck);
    entry.cached_cycle_check = has_cycle ? kHasCycle : kNoCycle;
  }
  if (entry.cached_cycle_check == kHasCycle)
    return nullptr;
  DCHECK_EQ(entry.cached_cycle_check, kNoCycle);
  return container;
}

bool SVGResource::FindCycle(SVGResourceClient& client,
                            SVGResourcesCycleSolver& solver) const {
  auto it = clients_.find(&client);
  if (it == clients_.end())
    return false;
  auto* container = ResourceContainerNoCycleCheck();
  if (!container)
    return false;
  ClientEntry& entry = it->value;
  switch (entry.cached_cycle_check) {
    case kNeedCheck: {
      entry.cached_cycle_check = kPerformingCheck;
      bool has_cycle = container->FindCycle(solver);
      DCHECK_EQ(entry.cached_cycle_check, kPerformingCheck);
      // Update our cached state based on the result of FindCycle(), but don't
      // signal a cycle since ResourceContainer() will consider the resource
      // invalid if one is present, thus we break the cycle at this resource.
      entry.cached_cycle_check = has_cycle ? kHasCycle : kNoCycle;
      return false;
    }
    case kPerformingCheck:
      // If we're on the current checking path, signal a cycle.
      return true;
    case kHasCycle:
    case kNoCycle:
      // We have a cached result, but don't signal a cycle since
      // ResourceContainer() will consider the resource invalid if one is
      // present.
      return false;
  }
}

LocalSVGResource::LocalSVGResource(TreeScope& tree_scope,
                                   const AtomicString& id)
    : tree_scope_(tree_scope) {
  target_ = SVGURIReference::ObserveTarget(
      id_observer_, tree_scope, id,
      WTF::BindRepeating(&LocalSVGResource::TargetChanged,
                         WrapWeakPersistent(this), id));
}

void LocalSVGResource::Unregister() {
  SVGURIReference::UnobserveTarget(id_observer_);
}

void LocalSVGResource::NotifyContentChanged(
    InvalidationModeMask invalidation_mask) {
  InvalidateCycleCache();

  HeapVector<Member<SVGResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceContentChanged(invalidation_mask);
}

void LocalSVGResource::NotifyFilterPrimitiveChanged(
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  HeapVector<Member<SVGResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->FilterPrimitiveChanged(primitive, attribute);
}

void LocalSVGResource::TargetChanged(const AtomicString& id) {
  Element* new_target = tree_scope_->getElementById(id);
  if (new_target == target_)
    return;
  // Clear out caches on the old resource, and then notify clients about the
  // change.
  LayoutSVGResourceContainer* old_resource = ResourceContainerNoCycleCheck();
  if (old_resource)
    old_resource->RemoveAllClientsFromCache();
  target_ = new_target;
  NotifyElementChanged();
}

void LocalSVGResource::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(id_observer_);
  SVGResource::Trace(visitor);
}

ExternalSVGResource::ExternalSVGResource(const KURL& url) : url_(url) {}

void ExternalSVGResource::Load(Document& document) {
  if (cache_entry_)
    return;
  cache_entry_ = SVGExternalDocumentCache::From(document)->Get(
      this, url_, fetch_initiator_type_names::kCSS);
  target_ = ResolveTarget();
}

void ExternalSVGResource::LoadWithoutCSP(Document& document) {
  if (cache_entry_)
    return;
  cache_entry_ = SVGExternalDocumentCache::From(document)->Get(
      this, url_, fetch_initiator_type_names::kCSS,
      network::mojom::blink::CSPDisposition::DO_NOT_CHECK);
  target_ = ResolveTarget();
}

void ExternalSVGResource::NotifyFinished(Resource*) {
  Element* new_target = ResolveTarget();
  if (new_target == target_)
    return;
  target_ = new_target;
  NotifyElementChanged();
}

String ExternalSVGResource::DebugName() const {
  return "ExternalSVGResource";
}

Element* ExternalSVGResource::ResolveTarget() {
  if (!cache_entry_)
    return nullptr;
  if (!url_.HasFragmentIdentifier())
    return nullptr;
  Document* external_document = cache_entry_->GetDocument();
  if (!external_document)
    return nullptr;
  AtomicString decoded_fragment(DecodeURLEscapeSequences(
      url_.FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
  return external_document->getElementById(decoded_fragment);
}

void ExternalSVGResource::Trace(Visitor* visitor) const {
  visitor->Trace(cache_entry_);
  SVGResource::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
