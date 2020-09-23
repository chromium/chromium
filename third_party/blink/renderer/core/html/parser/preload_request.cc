// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/preload_request.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/script/document_write_intervention.h"
#include "third_party/blink/renderer/core/script/script_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/cross_origin_attribute_value.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_info.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/network/network_state_notifier.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"

namespace blink {

KURL PreloadRequest::CompleteURL(Document* document) {
  if (!base_url_.IsEmpty())
    return document->CompleteURLWithOverride(resource_url_, base_url_);
  return document->CompleteURL(resource_url_);
}

// static
std::unique_ptr<PreloadRequest> PreloadRequest::CreateIfNeeded(
    const String& initiator_name,
    const TextPosition& initiator_position,
    const String& resource_url,
    const KURL& base_url,
    ResourceType resource_type,
    const network::mojom::ReferrerPolicy referrer_policy,
    ReferrerSource referrer_source,
    ResourceFetcher::IsImageSet is_image_set,
    const FetchParameters::ResourceWidth& resource_width,
    const ClientHintsPreferences& client_hints_preferences,
    RequestType request_type) {
  // Never preload data URLs. We also disallow relative ref URLs which become
  // data URLs if the document's URL is a data URL. We don't want to create
  // extra resource requests with data URLs to avoid copy / initialization
  // overhead, which can be significant for large URLs.
  if (resource_url.IsEmpty() || resource_url.StartsWith("#") ||
      ProtocolIs(resource_url, "data")) {
    return nullptr;
  }
  return base::WrapUnique(new PreloadRequest(
      initiator_name, initiator_position, resource_url, base_url, resource_type,
      resource_width, client_hints_preferences, request_type, referrer_policy,
      referrer_source, is_image_set));
}

Resource* PreloadRequest::Start(Document* document) {
  DCHECK(document->domWindow());

  FetchInitiatorInfo initiator_info;
  initiator_info.name = AtomicString(initiator_name_);
  initiator_info.position = initiator_position_;

  const KURL& url = CompleteURL(document);
  // Data URLs are filtered out in the preload scanner.
  DCHECK(!url.ProtocolIsData());

  ResourceRequest resource_request(url);
  resource_request.SetReferrerPolicy(referrer_policy_);
  if (referrer_source_ == kBaseUrlIsReferrer) {
    resource_request.SetReferrerString(base_url_.StrippedForUseAsReferrer());
  }

  resource_request.SetRequestContext(
      ResourceFetcher::DetermineRequestContext(resource_type_, is_image_set_));
  resource_request.SetRequestDestination(
      ResourceFetcher::DetermineRequestDestination(resource_type_));

  resource_request.SetFetchImportanceMode(importance_);

  if (resource_type_ == ResourceType::kImage && url.ProtocolIsInHTTPFamily() &&
      base::FeatureList::IsEnabled(blink::features::kSubresourceRedirect) &&
      blink::GetNetworkStateNotifier().SaveDataEnabled()) {
    resource_request.SetPreviewsState(resource_request.GetPreviewsState() |
                                      PreviewsTypes::kSubresourceRedirectOn);
  }

  ResourceLoaderOptions options(document->domWindow()->GetCurrentWorld());
  options.initiator_info = initiator_info;
  FetchParameters params(std::move(resource_request), options);

  auto* origin = document->domWindow()->GetSecurityOrigin();
  if (resource_type_ == ResourceType::kImportResource) {
    params.SetCrossOriginAccessControl(origin, kCrossOriginAttributeAnonymous);
  }

  if (script_type_ == mojom::blink::ScriptType::kModule) {
    DCHECK_EQ(resource_type_, ResourceType::kScript);
    params.SetCrossOriginAccessControl(
        origin, ScriptLoader::ModuleScriptCredentialsMode(cross_origin_));
  } else if (cross_origin_ != kCrossOriginAttributeNotSet) {
    params.SetCrossOriginAccessControl(origin, cross_origin_);
  }

  params.SetDefer(defer_);
  params.SetResourceWidth(resource_width_);
  params.GetClientHintsPreferences().UpdateFrom(client_hints_preferences_);
  params.SetIntegrityMetadata(integrity_metadata_);
  params.SetContentSecurityPolicyNonce(nonce_);
  params.SetParserDisposition(kParserInserted);

  if (request_type_ == kRequestTypeLinkRelPreload)
    params.SetLinkPreload(true);

  if (script_type_ == mojom::blink::ScriptType::kModule) {
    DCHECK_EQ(resource_type_, ResourceType::kScript);
    params.SetDecoderOptions(TextResourceDecoderOptions::CreateUTF8Decode());
  } else if (resource_type_ == ResourceType::kScript ||
             resource_type_ == ResourceType::kCSSStyleSheet ||
             resource_type_ == ResourceType::kImportResource) {
    params.SetCharset(charset_.IsEmpty() ? document->Encoding()
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
    MaybeDisallowFetchForDocWrittenScript(params, *document);
    // We intentionally ignore the returned value, because we don't resend
    // the async request to the blocked script here.
  }

  return PreloadHelper::StartPreload(resource_type_, params, *document);
}

}  // namespace blink
