// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fetch/fetch_request_type_converters.h"

#include "net/base/load_flags.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/service_worker/service_worker_loader_helpers.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

// Converts an enum defined in net/base/load_flags.h to
// blink::mojom::FetchCacheMode.
blink::mojom::FetchCacheMode GetFetchCacheModeFromLoadFlags(int load_flags) {
  if (load_flags & net::LOAD_DISABLE_CACHE)
    return blink::mojom::FetchCacheMode::kNoStore;

  if (load_flags & net::LOAD_VALIDATE_CACHE)
    return blink::mojom::FetchCacheMode::kValidateCache;

  if (load_flags & net::LOAD_BYPASS_CACHE) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kUnspecifiedForceCacheMiss;
    return blink::mojom::FetchCacheMode::kBypassCache;
  }

  if (load_flags & net::LOAD_SKIP_CACHE_VALIDATION) {
    if (load_flags & net::LOAD_ONLY_FROM_CACHE)
      return blink::mojom::FetchCacheMode::kOnlyIfCached;
    return blink::mojom::FetchCacheMode::kForceCache;
  }

  if (load_flags & net::LOAD_ONLY_FROM_CACHE) {
    DCHECK(!(load_flags & net::LOAD_SKIP_CACHE_VALIDATION));
    DCHECK(!(load_flags & net::LOAD_BYPASS_CACHE));
    return blink::mojom::FetchCacheMode::kUnspecifiedOnlyIfCachedStrict;
  }
  return blink::mojom::FetchCacheMode::kDefault;
}

}  // namespace

blink::mojom::FetchCacheMode GetFetchCacheModeFromLoadFlagsForTest(
    int load_flags) {
  return GetFetchCacheModeFromLoadFlags(load_flags);
}

}  // namespace content

namespace mojo {

blink::mojom::FetchAPIRequestPtr TypeConverter<
    blink::mojom::FetchAPIRequestPtr,
    network::ResourceRequest>::Convert(const network::ResourceRequest& input) {
  auto output = blink::mojom::FetchAPIRequest::New();
  output->url = input.url;
  output->method = input.method;
  if (!input.headers.IsEmpty()) {
    for (net::HttpRequestHeaders::Iterator it(input.headers); it.GetNext();)
      output->headers.insert(std::make_pair(it.name(), it.value()));
  }
  // We put the request body data into |output->body| rather than
  // |output->blob|. The |blob| is used in cases without
  // network::ResourceRequest involved. See fetch_api_request.mojom.
  // We leave |output->body| as std::nullopt when |input.request_body| is
  // nullptr.
  if (input.request_body)
    output->body = input.request_body;
  output->request_initiator = input.request_initiator;
  output->navigation_redirect_chain = input.navigation_redirect_chain;
  output->referrer = blink::mojom::Referrer::New(
      input.referrer,
      blink::ReferrerUtils::NetToMojoReferrerPolicy(input.referrer_policy));
  output->mode = input.mode;
  output->is_main_resource_load =
      blink::ServiceWorkerLoaderHelpers::IsMainRequestDestination(
          input.destination);
  output->credentials_mode = input.credentials_mode;
  output->cache_mode =
      content::GetFetchCacheModeFromLoadFlags(input.load_flags);
  output->redirect_mode = input.redirect_mode;
  output->destination =
      static_cast<network::mojom::RequestDestination>(input.destination);
  output->is_reload = ui::PageTransitionCoreTypeIs(
      static_cast<ui::PageTransition>(input.transition_type),
      ui::PAGE_TRANSITION_RELOAD);
  output->integrity = input.fetch_integrity;
  output->priority = input.priority;
  output->fetch_window_id = input.fetch_window_id;
  output->keepalive = input.keepalive;
  output->is_history_navigation =
      input.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK;
  output->devtools_stack_id = input.devtools_stack_id;
  if (input.trust_token_params) {
    output->trust_token_params = input.trust_token_params->Clone();
  }
  output->target_address_space = static_cast<network::mojom::IPAddressSpace>(
      input.required_ip_address_space);
  output->attribution_reporting_eligibility =
      input.attribution_reporting_eligibility;
  output->attribution_reporting_support = input.attribution_reporting_support;
  return output;
}

}  // namespace mojo
