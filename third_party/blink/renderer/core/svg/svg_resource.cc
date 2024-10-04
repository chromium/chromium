// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/svg/svg_resource.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/svg/graphics/svg_image.h"
#include "third_party/blink/renderer/core/svg/svg_resource_client.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"
#include "third_party/blink/renderer/core/svg/svg_uri_reference.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"

namespace blink {

SVGResource::SVGResource() = default;

SVGResource::~SVGResource() = default;

void SVGResource::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(clients_);
  visitor->Trace(observer_wrappers_);
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
  CHECK_NE(it, clients_.end());
  it->value.count--;
  if (it->value.count)
    return;
  clients_.erase(it);
  // The last instance of |client| was removed. Clear its entry in
  // resource's cache.
  if (LayoutSVGResourceContainer* container = ResourceContainerNoCycleCheck())
    container->RemoveClientFromCache(client);
}

class SVGResource::ImageResourceObserverWrapper
    : public GarbageCollected<SVGResource::ImageResourceObserverWrapper>,
      public SVGResourceClient {
 public:
  explicit ImageResourceObserverWrapper(ImageResourceObserver& observer)
      : observer_(observer) {}

  void IncRef() { count_++; }
  bool DecRef() {
    --count_;
    return count_ == 0;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(observer_);
    SVGResourceClient::Trace(visitor);
  }

 private:
  void ResourceContentChanged(SVGResource* resource) override {
    observer_->ImageChanged(static_cast<WrappedImagePtr>(resource),
                            ImageResourceObserver::CanDeferInvalidation::kNo);
  }

  Member<ImageResourceObserver> observer_;
  int count_ = 0;
};

void SVGResource::AddObserver(ImageResourceObserver& observer) {
  auto& wrapper =
      observer_wrappers_.insert(&observer, nullptr).stored_value->value;
  if (!wrapper) {
    wrapper = MakeGarbageCollected<ImageResourceObserverWrapper>(observer);
    AddClient(*wrapper);
  }
  wrapper->IncRef();
}

void SVGResource::RemoveObserver(ImageResourceObserver& observer) {
  auto it = observer_wrappers_.find(&observer);
  CHECK_NE(it, observer_wrappers_.end());
  if (it->value->DecRef()) {
    RemoveClient(*it->value);
    observer_wrappers_.erase(it);
  }
}

SVGResourceClient* SVGResource::GetObserverResourceClient(
    ImageResourceObserver& observer) {
  auto it = observer_wrappers_.find(&observer);
  return it != observer_wrappers_.end() ? it->value : nullptr;
}

void SVGResource::InvalidateCycleCache() {
  for (auto& client_entry : clients_.Values())
    client_entry.cached_cycle_check = kNeedCheck;
}

void SVGResource::NotifyContentChanged() {
  InvalidateCycleCache();

  HeapVector<Member<SVGResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->ResourceContentChanged(this);
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
    entry.cached_cycle_check = kPerformingCheck;
    bool has_cycle = container->FindCycle();
    DCHECK_EQ(entry.cached_cycle_check, kPerformingCheck);
    entry.cached_cycle_check = has_cycle ? kHasCycle : kNoCycle;
  }
  if (entry.cached_cycle_check == kHasCycle)
    return nullptr;
  DCHECK_EQ(entry.cached_cycle_check, kNoCycle);
  return container;
}

bool SVGResource::FindCycle(SVGResourceClient& client) const {
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
      bool has_cycle = container->FindCycle();
      DCHECK_EQ(entry.cached_cycle_check, kPerformingCheck);
      // Update our cached state based on the result of FindCycle(), but don't
      // signal a cycle since ResourceContainer() will consider the resource
      // invalid if one is present, thus we break the cycle at this resource.
      entry.cached_cycle_check = has_cycle ? kHasCycle : kNoCycle;
      return false;
    }
    case kNoCycle: {
      entry.cached_cycle_check = kPerformingCheck;
      bool has_cycle = container->FindCycle();
      DCHECK_EQ(entry.cached_cycle_check, kPerformingCheck);
      entry.cached_cycle_check = kNoCycle;
      return has_cycle;
    }
    case kPerformingCheck:
      // If we're on the current checking path, signal a cycle.
      return true;
    case kHasCycle:
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

void LocalSVGResource::NotifyFilterPrimitiveChanged(
    SVGFilterPrimitiveStandardAttributes& primitive,
    const QualifiedName& attribute) {
  HeapVector<Member<SVGResourceClient>> clients;
  CopyKeysToVector(clients_, clients);

  for (SVGResourceClient* client : clients)
    client->FilterPrimitiveChanged(this, primitive, attribute);
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
  NotifyContentChanged();
}

void LocalSVGResource::Trace(Visitor* visitor) const {
  visitor->Trace(tree_scope_);
  visitor->Trace(id_observer_);
  SVGResource::Trace(visitor);
}

ExternalSVGResourceDocumentContent::ExternalSVGResourceDocumentContent(
    const KURL& url)
    : url_(url) {}

void ExternalSVGResourceDocumentContent::Load(
    Document& document,
    CrossOriginAttributeValue cross_origin) {
  if (document_content_)
    return;
  // Loading SVG resources should not trigger script, see
  // https://crbug.com/1196853 This could be allowed if DOMContentLoaded and
  // other checkpoints were asynchronous per https://crbug.com/961428
  ScriptForbiddenScope forbid_script;
  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(url_), options);
  if (cross_origin == kCrossOriginAttributeNotSet) {
    params.MutableResourceRequest().SetMode(
        network::mojom::blink::RequestMode::kSameOrigin);
  } else {
    params.SetCrossOriginAccessControl(execution_context->GetSecurityOrigin(),
                                       cross_origin);
  }
  document_content_ = SVGResourceDocumentContent::Fetch(params, document);
  if (!document_content_) {
    return;
  }
  document_content_->AddObserver(this);
  target_ = ResolveTarget();
}

void ExternalSVGResourceDocumentContent::LoadWithoutCSP(Document& document) {
  if (document_content_)
    return;
  // Loading SVG resources should not trigger script, see
  // https://crbug.com/1196853 This could be allowed if DOMContentLoaded and
  // other checkpoints were asynchronous per https://crbug.com/961428
  ScriptForbiddenScope forbid_script;
  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = fetch_initiator_type_names::kCSS;
  FetchParameters params(ResourceRequest(url_), options);
  params.SetContentSecurityCheck(
      network::mojom::blink::CSPDisposition::DO_NOT_CHECK);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  document_content_ = SVGResourceDocumentContent::Fetch(params, document);
  if (!document_content_) {
    return;
  }
  document_content_->AddObserver(this);
  target_ = ResolveTarget();
}

void ExternalSVGResourceDocumentContent::ResourceNotifyFinished(
    SVGResourceDocumentContent* document_content) {
  DCHECK_EQ(document_content_, document_content);
  Element* new_target = ResolveTarget();
  // If no target was found when resolving in Load(), we want to notify clients
  // regardless of if a target was found or not, to be able to update rendering
  // based on loading state.
  if (target_ && new_target == target_) {
    return;
  }
  target_ = new_target;
  NotifyContentChanged();
}

void ExternalSVGResourceDocumentContent::ResourceContentChanged(
    SVGResourceDocumentContent* document_content) {
  DCHECK_EQ(document_content_, document_content);
  if (!target_) {
    return;
  }
  NotifyContentChanged();
}

bool ExternalSVGResourceDocumentContent::IsLoading() const {
  return !document_content_ || document_content_->IsLoading();
}

Element* ExternalSVGResourceDocumentContent::ResolveTarget() {
  if (!document_content_)
    return nullptr;
  if (!url_.HasFragmentIdentifier())
    return nullptr;
  Document* external_document = document_content_->GetDocument();
  if (!external_document)
    return nullptr;
  AtomicString decoded_fragment(DecodeURLEscapeSequences(
      url_.FragmentIdentifier(), DecodeURLMode::kUTF8OrIsomorphic));
  return external_document->getElementById(decoded_fragment);
}

void ExternalSVGResourceDocumentContent::Trace(Visitor* visitor) const {
  visitor->Trace(document_content_);
  SVGResource::Trace(visitor);
}

ExternalSVGResourceImageContent::ExternalSVGResourceImageContent(
    ImageResourceContent* image_content,
    const AtomicString& fragment)
    : image_content_(image_content), fragment_(fragment) {
  image_content_->AddObserver(this);
}

void ExternalSVGResourceImageContent::Prefinalize() {
  image_content_->DidRemoveObserver();
  image_content_ = nullptr;
}

bool ExternalSVGResourceImageContent::IsLoading() const {
  return image_content_->IsLoading();
}

Element* ExternalSVGResourceImageContent::ResolveTarget() {
  if (!image_content_->IsLoaded() || image_content_->ErrorOccurred()) {
    return nullptr;
  }
  if (!fragment_) {
    return nullptr;
  }
  auto* svg_image = DynamicTo<SVGImage>(image_content_->GetImage());
  if (!svg_image) {
    return nullptr;
  }
  AtomicString decoded_fragment(
      DecodeURLEscapeSequences(fragment_, DecodeURLMode::kUTF8OrIsomorphic));
  return svg_image->GetResourceElement(decoded_fragment);
}

void ExternalSVGResourceImageContent::ImageNotifyFinished(
    ImageResourceContent*) {
  Element* new_target = ResolveTarget();
  // If no target was found when resolving in Load(), we want to notify clients
  // regardless of if a target was found or not, to be able to update rendering
  // based on loading state.
  if (target_ && new_target == target_) {
    return;
  }
  target_ = new_target;
  NotifyContentChanged();
}

String ExternalSVGResourceImageContent::DebugName() const {
  return "ExternalSVGResourceImageContent";
}

void ExternalSVGResourceImageContent::Trace(Visitor* visitor) const {
  visitor->Trace(image_content_);
  SVGResource::Trace(visitor);
  ImageResourceObserver::Trace(visitor);
}

}  // namespace blink
