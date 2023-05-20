/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"

#include <memory>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_prescient_networking.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

HTMLResourcePreloader::HTMLResourcePreloader(Document& document)
    : document_(document) {}

void HTMLResourcePreloader::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

static void PreconnectHost(LocalFrame* local_frame, PreloadRequest* request) {
  DCHECK(request);
  DCHECK(request->IsPreconnect());
  KURL host(request->BaseURL(), request->ResourceURL());
  if (!host.IsValid() || !host.ProtocolIsInHTTPFamily())
    return;
  WebPrescientNetworking* web_prescient_networking =
      local_frame->PrescientNetworking();
  if (web_prescient_networking) {
    web_prescient_networking->Preconnect(
        host, request->CrossOrigin() != kCrossOriginAttributeAnonymous);
  }
}

void HTMLResourcePreloader::Preload(std::unique_ptr<PreloadRequest> preload) {
  if (preload->IsPreconnect()) {
    PreconnectHost(document_->GetFrame(), preload.get());
    return;
  }

  if (!AllowPreloadRequest(preload.get())) {
    return;
  }
  if (!document_->Loader())
    return;

  preload->Start(document_);
}

bool HTMLResourcePreloader::AllowPreloadRequest(PreloadRequest* preload) const {
  if (!base::FeatureList::IsEnabled(features::kLightweightNoStatePrefetch))
    return true;

  if (!document_->IsPrefetchOnly())
    return true;

  // Don't fetch any other resources when in the HTML only arm of the
  // experiment.
  if (GetFieldTrialParamByFeatureAsBool(features::kLightweightNoStatePrefetch,
                                        "html_only", false)) {
    return false;
  }

  switch (preload->FetchPriorityHint()) {
    case mojom::blink::FetchPriorityHint::kHigh:
      return true;
    case mojom::blink::FetchPriorityHint::kLow:
    case mojom::blink::FetchPriorityHint::kAuto:
      break;
  }

  // When running lightweight prefetch, always skip image resources. Other
  // resources are either classified into CSS (always fetched when not in the
  // HTML only arm), JS (skip_script param), or other.
  switch (preload->GetResourceType()) {
    case ResourceType::kRaw:
    case ResourceType::kSVGDocument:
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kLinkPrefetch:
    case ResourceType::kTextTrack:
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kManifest:
    case ResourceType::kMock:
      return !GetFieldTrialParamByFeatureAsBool(
          features::kLightweightNoStatePrefetch, "skip_other", true);
    case ResourceType::kSpeculationRules:
      return false;
    case ResourceType::kImage:
      return false;
    case ResourceType::kCSSStyleSheet:
      return true;
    case ResourceType::kFont:
      return false;
    case ResourceType::kScript:
      // We might skip all script.
      if (GetFieldTrialParamByFeatureAsBool(
              features::kLightweightNoStatePrefetch, "skip_script", false)) {
        return false;
      }

      // Otherwise, we might skip async/deferred script.
      return !GetFieldTrialParamByFeatureAsBool(
                 features::kLightweightNoStatePrefetch, "skip_async_script",
                 true) ||
             preload->DeferOption() == FetchParameters::DeferOption::kNoDefer;
    case ResourceType::kDictionary:
      return false;
  }
}

}  // namespace blink
