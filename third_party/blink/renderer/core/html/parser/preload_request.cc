// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/preload_request.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "services/network/public/mojom/attribution.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/attribution_src_loader.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/lcp_critical_path_predictor/lcp_critical_path_predictor.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/script/document_write_intervention.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

PreloadRequest::ExclusionInfo::ExclusionInfo(const KURL& document_url,
                                             HashSet<KURL> scopes,
                                             HashSet<KURL> resources)
    : document_url_(document_url),
      scopes_(std::move(scopes)),
      resources_(std::move(resources)) {}

PreloadRequest::ExclusionInfo::~ExclusionInfo() = default;

bool PreloadRequest::ExclusionInfo::ShouldExclude(
    const KURL& base_url,
    const String& resource_url) const {
  if (resources_.empty() && scopes_.empty())
    return false;
  KURL url = KURL(base_url.IsEmpty() ? document_url_ : base_url, resource_url);
  if (resources_.Contains(url))
    return true;
  for (const auto& scope : scopes_) {
    if (url.GetString().StartsWith(scope.GetString()))
      return true;
  }
  return false;
}

KURL PreloadRequest::CompleteURL(Document* document) {
  if (!base_url_.IsEmpty()) {
    return document->CompleteURLWithOverride(resource_url_, base_url_,
                                             Document::kIsPreload);
  }
  return document->CompleteURL(resource_url_, Document::kIsPreload);
}

// static
std::unique_ptr<PreloadRequest> PreloadRequest::CreateIfNeeded(
    const String& initiator_name,
    const String& resource_url,
    const KURL& base_url,
    ResourceType resource_type,
    const network::mojom::ReferrerPolicy referrer_policy,
    ResourceFetcher::IsImageSet is_image_set,
    const ExclusionInfo* exclusion_info,
    std::optional<float> resource_width,
    std::optional<float> resource_height,
    RequestType request_type) {
  // Never preload data URLs. We also disallow relative ref URLs which become
  // data URLs if the document's URL is a data URL. We don't want to create
  // extra resource requests with data URLs to avoid copy / initialization
  // overhead, which can be significant for large URLs.
  if (resource_url.empty() || resource_url.StartsWith("#") ||
      ProtocolIs(resource_url, "data")) {
    return nullptr;
  }

  if (exclusion_info && exclusion_info->ShouldExclude(base_url, resource_url))
    return nullptr;

  return base::WrapUnique(new PreloadRequest(
      initiator_name, resource_url, base_url, resource_type, resource_width,
      resource_height, request_type, referrer_policy, is_image_set));
}

Resource* PreloadRequest::Start(Document* document) {
  DCHECK(document->domWindow());
  base::UmaHistogramTimes("Blink.PreloadRequestWaitTime",
                          base::TimeTicks::Now() - creation_time_);

  FetchInitiatorInfo initiator_info;
  initiator_info.name = AtomicString(initiator_name_);
  initiator_info.position = initiator_position_;

  const KURL& url = CompleteURL(document);
  // Data URLs are filtered out in the preload scanner.
  DCHECK(!url.ProtocolIsData());

  ResourceRequest resource_request(url);
  resource_request.SetReferrerPolicy(referrer_policy_);

  resource_request.SetRequestContext(
      ResourceFetcher::DetermineRequestContext(resource_type_, is_image_set_));
  resource_request.SetRequestDestination(
      ResourceFetcher::DetermineRequestDestination(resource_type_));

  resource_request.SetFetchPriorityHint(fetch_priority_hint_);

  // Disable issue logging to avoid duplicates, since `CanRegister()` will be
  // called again later.
  if (is_attribution_reporting_eligible_img_or_script_ &&
      document->domWindow()->GetFrame()->GetAttributionSrcLoader()->CanRegister(
          url, /*element=*/nullptr,
          /*request_id=*/std::nullopt, /*log_issues=*/false)) {
    resource_request.SetAttributionReportingEligibility(
        network::mojom::AttributionReportingEligibility::kEventSourceOrTrigger);
  }

  bool shared_storage_writable_opted_in =
      shared_storage_writable_opted_in_ &&
      RuntimeEnabledFeatures::SharedStorageAPIM118Enabled(
          document->domWindow()) &&
      document->domWindow()->IsSecureContext() &&
      !document->domWindow()->GetSecurityOrigin()->IsOpaque();
  resource_request.SetSharedStorageWritableOptedIn(
      shared_storage_writable_opted_in);
  if (shared_storage_writable_opted_in) {
    CHECK_EQ(resource_type_, ResourceType::kImage);
    UseCounter::Count(document, WebFeature::kSharedStorageAPI_Image_Attribute);
  }

  ResourceLoaderOptions options(document->domWindow()->GetCurrentWorld());
  options.initiator_info = initiator_info;
  FetchParameters params(std::move(resource_request), options);

  auto* origin = document->domWindow()->GetSecurityOrigin();
  if (script_type_ == mojom::blink::ScriptType::kModule) {
    DCHECK_EQ(resource_type_, ResourceType::kScript);
    params.SetCrossOriginAccessControl(
        origin, ScriptLoader::ModuleScriptCredentialsMode(cross_origin_));
    params.SetModuleScript();
  } else if (cross_origin_ != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(origin, cross_origin_);
  }

  params.SetDefer(defer_);
  params.SetResourceWidth(resource_width_);
  params.SetResourceHeight(resource_height_);
  params.SetIntegrityMetadata(integrity_metadata_);
  params.SetContentSecurityPolicyNonce(nonce_);
  params.SetParserDisposition(kParserInserted);

  if (request_type_ == kRequestTypeLinkRelPreload)
    params.SetLinkPreload(true);

  if (script_type_ == mojom::blink::ScriptType::kModule) {
    DCHECK_EQ(resource_type_, ResourceType::kScript);
    params.SetDecoderOptions(TextResourceDecoderOptions::CreateUTF8Decode());
  } else if (resource_type_ == ResourceType::kScript ||
             resource_type_ == ResourceType::kCSSStyleSheet) {
    params.SetCharset(charset_.empty() ? document->Encoding()
                                       : WTF::TextEncoding(charset_));
  }
  FetchParameters::SpeculativePreloadType speculative_preload_type =
      FetchParameters::SpeculativePreloadType::kInDocument;
  if (from_insertion_scanner_) {
    speculative_preload_type =
        FetchParameters::SpeculativePreloadType::kInserted;
  }
  params.SetSpeculativePreloadType(speculative_preload_type);

  if (resource_type_ == ResourceType::kScript) {
    // We intentionally ignore the returned value, because we don't resend
    // the async request to the blocked script here.
    MaybeDisallowFetchForDocWrittenScript(params, *document);

    if (base::FeatureList::IsEnabled(features::kLCPScriptObserver)) {
      if (LCPCriticalPathPredictor* lcpp = document->GetFrame()->GetLCPP()) {
        if (lcpp->lcp_influencer_scripts().Contains(url)) {
          is_potentially_lcp_influencer_ = true;
        }
      }
    }
  }
  params.SetRenderBlockingBehavior(render_blocking_behavior_);

  params.SetIsPotentiallyLCPElement(is_potentially_lcp_element_);
  params.SetIsPotentiallyLCPInfluencer(is_potentially_lcp_influencer_);

  if (LCPCriticalPathPredictor* lcpp = document->GetFrame()->GetLCPP()) {
    lcpp->OnStartPreload(url, resource_type_);
  }

  return PreloadHelper::StartPreload(resource_type_, params, *document);
}

}  // namespace blink
