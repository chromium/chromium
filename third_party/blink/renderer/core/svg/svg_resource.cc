// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/loader/resource/document_resource.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"

namespace blink {

SVGResource::SVGResource() = default;

SVGResource::~SVGResource() = default;

void SVGResource::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(clients_);
}

void SVGResource::AddClient(SVGResourceClient& client) {
  clients_.insert(&client);
  if (LayoutSVGResourceContainer* container = ResourceContainer())
    container->ClearInvalidationMask();
}

void SVGResource::RemoveClient(SVGResourceClient& client) {
  if (!clients_.erase(&client))
    return;
  // The last instance of |client| was removed. Clear its entry in
  // resource's cache.
  if (LayoutSVGResourceContainer* container = ResourceContainer())
    container->RemoveClientFromCache(client);
}

void SVGResource::NotifyElementChanged() {
  HeapVector<Member<SVGResourceClient>> clients;
  CopyToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceElementChanged();
}

LayoutSVGResourceContainer* SVGResource::ResourceContainer() const {
  if (!target_)
    return nullptr;
  LayoutObject* layout_object = target_->GetLayoutObject();
  if (!layout_object || !layout_object->IsSVGResourceContainer())
    return nullptr;
  return ToLayoutSVGResourceContainer(layout_object);
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
  HeapVector<Member<SVGResourceClient>> clients;
  CopyToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceContentChanged(invalidation_mask);
}

void LocalSVGResource::NotifyResourceAttached(
    LayoutSVGResourceContainer& attached_resource) {
  // Checking the element here because
  if (attached_resource.GetElement() != Target())
    return;
  NotifyElementChanged();
}

void LocalSVGResource::NotifyResourceDestroyed(
    LayoutSVGResourceContainer& destroyed_resource) {
  if (destroyed_resource.GetElement() != Target())
    return;
  destroyed_resource.RemoveAllClientsFromCache();

  HeapVector<Member<SVGResourceClient>> clients;
  CopyToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceDestroyed(&destroyed_resource);
}

void LocalSVGResource::TargetChanged(const AtomicString& id) {
  Element* new_target = tree_scope_->getElementById(id);
  if (new_target == target_)
    return;
  // Clear out caches on the old resource, and then notify clients about the
  // change.
  if (LayoutSVGResourceContainer* old_resource = ResourceContainer())
    old_resource->RemoveAllClientsFromCache();
  target_ = new_target;
  NotifyElementChanged();
}

void LocalSVGResource::Trace(Visitor* visitor) {
  visitor->Trace(tree_scope_);
  visitor->Trace(id_observer_);
  SVGResource::Trace(visitor);
}

ExternalSVGResource::ExternalSVGResource(const KURL& url) : url_(url) {}

void ExternalSVGResource::Load(const Document& document) {
  if (resource_document_)
    return;
  ResourceLoaderOptions options;
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(url_), options);
  params.MutableResourceRequest().SetMode(
      network::mojom::RequestMode::kSameOrigin);
  resource_document_ =
      DocumentResource::FetchSVGDocument(params, document.Fetcher(), this);
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
  if (!resource_document_)
    return nullptr;
  if (!url_.HasFragmentIdentifier())
    return nullptr;
  Document* external_document = resource_document_->GetDocument();
  if (!external_document)
    return nullptr;
  AtomicString decoded_fragment(DecodeURLEscapeSequences(
      url_.FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
  return external_document->getElementById(decoded_fragment);
}

void ExternalSVGResource::Trace(Visitor* visitor) {
  visitor->Trace(resource_document_);
  SVGResource::Trace(visitor);
  ResourceClient::Trace(visitor);
}

}  // namespace blink
