// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/fetch/fetch_request_type_converters.h"

#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/referrer.h"
#include "ui/base/page_transition_types.h"

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
  // We leave |output->body| as base::nullopt when |input.request_body| is
  // nullptr.
  if (input.request_body)
    output->body = input.request_body;
  output->referrer = blink::mojom::Referrer::New(
      input.referrer, content::Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                          input.referrer_policy));
  output->mode = input.mode;
  output->is_main_resource_load =
      content::ServiceWorkerUtils::IsMainResourceType(
          static_cast<content::ResourceType>(input.resource_type));
  output->credentials_mode = input.credentials_mode;
  output->cache_mode =
      content::ServiceWorkerUtils::GetCacheModeFromLoadFlags(input.load_flags);
  output->redirect_mode = input.redirect_mode;
  output->request_context_type = static_cast<blink::mojom::RequestContextType>(
      input.fetch_request_context_type);
  output->is_reload = ui::PageTransitionCoreTypeIs(
      static_cast<ui::PageTransition>(input.transition_type),
      ui::PAGE_TRANSITION_RELOAD);
  output->integrity = input.fetch_integrity;
  output->priority = input.priority;
  output->fetch_window_id = input.fetch_window_id;
  output->keepalive = input.keepalive;
  output->is_history_navigation =
      input.transition_type & ui::PAGE_TRANSITION_FORWARD_BACK;
  return output;
}

}  // namespace mojo
