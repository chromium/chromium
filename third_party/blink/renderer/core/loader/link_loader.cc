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
 *
 */

#include "third_party/blink/renderer/core/loader/link_loader.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/importance_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/private/prerender_handle.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

class WebPrescientNetworking;

namespace {

// Decide the prerender type based on the link rel attribute. Returns
// base::nullopt if the attribute doesn't indicate the prerender type.
base::Optional<mojom::blink::PrerenderRelType> PrerenderRelTypeFromRelAttribute(
    const LinkRelAttribute& rel_attribute,
    Document& document) {
  base::Optional<mojom::blink::PrerenderRelType> rel_type;
  if (rel_attribute.IsLinkPrerender()) {
    UseCounter::Count(document, WebFeature::kLinkRelPrerender);
    rel_type = mojom::blink::PrerenderRelType::kPrerender;
  }
  if (rel_attribute.IsLinkNext()) {
    UseCounter::Count(document, WebFeature::kLinkRelNext);
    // Prioritize mojom::blink::PrerenderRelType::kPrerender.
    if (!rel_type)
      rel_type = mojom::blink::PrerenderRelType::kNext;
  }
  return rel_type;
}

}  // namespace

class LinkLoader::FinishObserver final : public ResourceFinishObserver {
  USING_PRE_FINALIZER(FinishObserver, ClearResource);

 public:
  FinishObserver(LinkLoader* loader, Resource* resource)
      : loader_(loader), resource_(resource) {
    resource_->AddFinishObserver(
        this, loader_->client_->GetLoadingTaskRunner().get());
  }

  // ResourceFinishObserver implementation
  void NotifyFinished() override {
    if (!resource_)
      return;
    loader_->NotifyFinished();
    ClearResource();
  }
  String DebugName() const override {
    return "LinkLoader::ResourceFinishObserver";
  }

  Resource* GetResource() { return resource_; }
  void ClearResource() {
    if (!resource_)
      return;
    resource_->RemoveFinishObserver(this);
    resource_ = nullptr;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(loader_);
    visitor->Trace(resource_);
    blink::ResourceFinishObserver::Trace(visitor);
  }

 private:
  Member<LinkLoader> loader_;
  Member<Resource> resource_;
};

LinkLoader::LinkLoader(LinkLoaderClient* client,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : client_(client) {
  DCHECK(client_);
}

LinkLoader::~LinkLoader() = default;

void LinkLoader::NotifyFinished() {
  DCHECK(finish_observer_);
  Resource* resource = finish_observer_->GetResource();
  if (resource->ErrorOccurred() ||
      (resource->IsLinkPreload() &&
       resource->IntegrityDisposition() ==
           ResourceIntegrityDisposition::kFailed)) {
    client_->LinkLoadingErrored();
  } else {
    client_->LinkLoaded();
  }
}

// https://html.spec.whatwg.org/C/#link-type-modulepreload
void LinkLoader::NotifyModuleLoadFinished(ModuleScript* module) {
  // Step 11. "If result is null, fire an event named error at the link element,
  // and return." [spec text]
  // Step 12. "Fire an event named load at the link element." [spec text]
  if (!module)
    client_->LinkLoadingErrored();
  else
    client_->LinkLoaded();
}

void LinkLoader::DidStartPrerender() {
  client_->DidStartLinkPrerender();
}

void LinkLoader::DidStopPrerender() {
  client_->DidStopLinkPrerender();
}

void LinkLoader::DidSendLoadForPrerender() {
  client_->DidSendLoadForLinkPrerender();
}

void LinkLoader::DidSendDOMContentLoadedForPrerender() {
  client_->DidSendDOMContentLoadedForLinkPrerender();
}

Resource* LinkLoader::GetResourceForTesting() {
  return finish_observer_ ? finish_observer_->GetResource() : nullptr;
}

bool LinkLoader::LoadLink(const LinkLoadParameters& params,
                          Document& document) {
  // If any loading process is in progress, abort it.
  Abort();

  if (!client_->ShouldLoadLink())
    return false;

  PreloadHelper::DnsPrefetchIfNeeded(params, &document, document.GetFrame(),
                                     PreloadHelper::kLinkCalledFromMarkup);

  PreloadHelper::PreconnectIfNeeded(params, &document, document.GetFrame(),
                                    PreloadHelper::kLinkCalledFromMarkup);

  Resource* resource = PreloadHelper::PreloadIfNeeded(
      params, document, NullURL(), PreloadHelper::kLinkCalledFromMarkup,
      nullptr /* viewport_description */,
      client_->IsLinkCreatedByParser() ? kParserInserted : kNotParserInserted);
  if (!resource) {
    resource = PreloadHelper::PrefetchIfNeeded(params, document);
  }
  if (resource)
    finish_observer_ = MakeGarbageCollected<FinishObserver>(this, resource);

  PreloadHelper::ModulePreloadIfNeeded(
      params, document, nullptr /* viewport_description */, this);

  base::Optional<mojom::blink::PrerenderRelType> prerender_rel_type =
      PrerenderRelTypeFromRelAttribute(params.rel, document);
  if (prerender_rel_type) {
    if (!prerender_) {
      prerender_ = PrerenderHandle::Create(document, this, params.href,
                                           *prerender_rel_type);
    } else if (prerender_->Url() != params.href) {
      prerender_->Cancel();
      prerender_ = PrerenderHandle::Create(document, this, params.href,
                                           *prerender_rel_type);
    }
    // TODO(gavinp): Handle changes to rel types of existing prerenders.
  } else if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  return true;
}

void LinkLoader::LoadStylesheet(const LinkLoadParameters& params,
                                const AtomicString& local_name,
                                const WTF::TextEncoding& charset,
                                FetchParameters::DeferOption defer_option,
                                Document& document,
                                ResourceClient* link_client) {
  ExecutionContext* context = document.GetExecutionContext();
  ResourceRequest resource_request(context->CompleteURL(params.href));
  resource_request.SetReferrerPolicy(params.referrer_policy);

  mojom::FetchImportanceMode importance_mode =
      GetFetchImportanceAttributeValue(params.importance);
  DCHECK(importance_mode == mojom::FetchImportanceMode::kImportanceAuto ||
         RuntimeEnabledFeatures::PriorityHintsEnabled(context));
  resource_request.SetFetchImportanceMode(importance_mode);

  ResourceLoaderOptions options(context->GetCurrentWorld());
  options.initiator_info.name = local_name;
  FetchParameters link_fetch_params(std::move(resource_request), options);
  link_fetch_params.SetCharset(charset);

  link_fetch_params.SetDefer(defer_option);

  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);

  CrossOriginAttributeValue cross_origin = params.cross_origin;
  if (cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(context->GetSecurityOrigin(),
                                                  cross_origin);
  }

  String integrity_attr = params.integrity;
  if (!integrity_attr.IsEmpty()) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attr, SubresourceIntegrityHelper::GetFeatures(context),
        metadata_set);
    link_fetch_params.SetIntegrityMetadata(metadata_set);
    link_fetch_params.MutableResourceRequest().SetFetchIntegrity(
        integrity_attr);
  }

  CSSStyleSheetResource::Fetch(link_fetch_params, context->Fetcher(),
                               link_client);
}

void LinkLoader::Abort() {
  if (prerender_) {
    prerender_->Cancel();
    prerender_.Clear();
  }
  if (finish_observer_) {
    finish_observer_->ClearResource();
    finish_observer_ = nullptr;
  }
}

void LinkLoader::Trace(Visitor* visitor) const {
  visitor->Trace(finish_observer_);
  visitor->Trace(client_);
  visitor->Trace(prerender_);
  SingleModuleClient::Trace(visitor);
  PrerenderClient::Trace(visitor);
}

}  // namespace blink
