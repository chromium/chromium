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

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/loader/fetch_priority_attribute.h"
#include "third_party/blink/renderer/core/loader/link_load_parameters.h"
#include "third_party/blink/renderer/core/loader/link_loader_client.h"
#include "third_party/blink/renderer/core/loader/pending_link_preload.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/loader/prerender_handle.h"
#include "third_party/blink/renderer/core/loader/resource/css_style_sheet_resource.h"
#include "third_party/blink/renderer/core/loader/subresource_integrity_helper.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_finish_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

namespace blink {

class WebPrescientNetworking;

namespace {

// Decide the prerender type based on the link rel attribute. Returns
// std::nullopt if the attribute doesn't indicate the prerender type.
std::optional<mojom::blink::PrerenderTriggerType>
PrerenderTriggerTypeFromRelAttribute(const LinkRelAttribute& rel_attribute,
                                     Document& document) {
  std::optional<mojom::blink::PrerenderTriggerType> trigger_type;
  if (rel_attribute.IsLinkPrerender()) {
    UseCounter::Count(document, WebFeature::kLinkRelPrerender);
    trigger_type = mojom::blink::PrerenderTriggerType::kLinkRelPrerender;
  }
  if (rel_attribute.IsLinkNext()) {
    UseCounter::Count(document, WebFeature::kLinkRelNext);
    // Prioritize mojom::blink::PrerenderTriggerType::kLinkRelPrerender.
    if (!trigger_type)
      trigger_type = mojom::blink::PrerenderTriggerType::kLinkRelNext;
  }
  return trigger_type;
}

}  // namespace

LinkLoader::LinkLoader(LinkLoaderClient* client) : client_(client) {
  DCHECK(client_);
}

void LinkLoader::NotifyFinished(Resource* resource) {
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
  // Step 14. "If result is null, fire an event named error at the link element,
  // and return." [spec text]
  // Step 15. "Fire an event named load at the link element." [spec text]
  if (!module)
    client_->LinkLoadingErrored();
  else
    client_->LinkLoaded();
}

Resource* LinkLoader::GetResourceForTesting() {
  return pending_preload_ ? pending_preload_->GetResourceForTesting() : nullptr;
}

bool LinkLoader::LoadLink(const LinkLoadParameters& params,
                          Document& document) {
  if (!client_->ShouldLoadLink()) {
    Abort();
    return false;
  }

  if (!pending_preload_ ||
      (params.reason != LinkLoadParameters::Reason::kMediaChange ||
       !pending_preload_->MatchesMedia())) {
    Abort();
    pending_preload_ = MakeGarbageCollected<PendingLinkPreload>(document, this);
  }

  // If any loading process is in progress, abort it.

  PreloadHelper::DnsPrefetchIfNeeded(params, &document, document.GetFrame(),
                                     PreloadHelper::kLinkCalledFromMarkup);

  PreloadHelper::PreconnectIfNeeded(params, &document, document.GetFrame(),
                                    PreloadHelper::kLinkCalledFromMarkup);

  PreloadHelper::PreloadIfNeeded(
      params, document, NullURL(), PreloadHelper::kLinkCalledFromMarkup,
      nullptr /* viewport_description */,
      client_->IsLinkCreatedByParser() ? kParserInserted : kNotParserInserted,
      pending_preload_);
  if (!pending_preload_->HasResource())
    PreloadHelper::PrefetchIfNeeded(params, document, pending_preload_);
  PreloadHelper::ModulePreloadIfNeeded(
      params, document, nullptr /* viewport_description */, pending_preload_);
  PreloadHelper::FetchCompressionDictionaryIfNeeded(params, document,
                                                    pending_preload_);

  std::optional<mojom::blink::PrerenderTriggerType> trigger_type =
      PrerenderTriggerTypeFromRelAttribute(params.rel, document);
  if (trigger_type) {
    // The previous prerender should already be aborted by Abort().
    DCHECK(!prerender_);
    prerender_ = PrerenderHandle::Create(document, params.href, *trigger_type);
  }
  return true;
}

void LinkLoader::LoadStylesheet(
    const LinkLoadParameters& params,
    const AtomicString& local_name,
    const WTF::TextEncoding& charset,
    FetchParameters::DeferOption defer_option,
    Document& document,
    ResourceClient* link_client,
    RenderBlockingBehavior render_blocking_behavior) {
  ExecutionContext* context = document.GetExecutionContext();
  ResourceRequest resource_request(context->CompleteURL(params.href));
  resource_request.SetReferrerPolicy(params.referrer_policy);

  mojom::blink::FetchPriorityHint fetch_priority_hint =
      GetFetchPriorityAttributeValue(params.fetch_priority_hint);
  resource_request.SetFetchPriorityHint(fetch_priority_hint);

  ResourceLoaderOptions options(context->GetCurrentWorld());
  options.initiator_info.name = local_name;

  FetchParameters link_fetch_params(std::move(resource_request), options);
  link_fetch_params.SetCharset(charset);
  link_fetch_params.SetDefer(defer_option);
  link_fetch_params.SetRenderBlockingBehavior(render_blocking_behavior);
  link_fetch_params.SetContentSecurityPolicyNonce(params.nonce);

  CrossOriginAttributeValue cross_origin = params.cross_origin;
  if (cross_origin != kCrossOriginAttributeNotSet) {
    link_fetch_params.SetCrossOriginAccessControl(context->GetSecurityOrigin(),
                                                  cross_origin);
  }

  String integrity_attr = params.integrity;
  if (!integrity_attr.empty()) {
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
  if (pending_preload_) {
    pending_preload_->Dispose();
    pending_preload_.Clear();
  }
}

void LinkLoader::Trace(Visitor* visitor) const {
  visitor->Trace(client_);
  visitor->Trace(pending_preload_);
  visitor->Trace(prerender_);
}

}  // namespace blink
