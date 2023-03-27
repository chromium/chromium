// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/header_util.h"

#include "base/strings/strcat.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/common/content_client.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

const char kBrowsingTopicsRequestHeaderKey[] = "Sec-Browsing-Topics";

std::string DeriveTopicsHeaderValue(
    const std::vector<blink::mojom::EpochTopicPtr>& topics) {
  net::structured_headers::List header_list;
  for (auto& topic : topics) {
    header_list.push_back(net::structured_headers::ParameterizedMember(
        net::structured_headers::Item(static_cast<int64_t>(topic->topic)),
        {{"version", net::structured_headers::Item(topic->version)},
         {"config_version",
          net::structured_headers::Item(topic->config_version)},
         {"model_version", net::structured_headers::Item(topic->model_version)},
         {"taxonomy_version",
          net::structured_headers::Item(topic->taxonomy_version)}}));
  }

  return net::structured_headers::SerializeList(std::move(header_list))
      .value_or("");
}

void HandleTopicsEligibleResponse(
    const net::HttpResponseHeaders& headers,
    const url::Origin& caller_origin,
    RenderFrameHost& request_initiator_frame,
    browsing_topics::ApiCallerSource caller_source) {
  DCHECK(caller_source == browsing_topics::ApiCallerSource::kFetch ||
         caller_source == browsing_topics::ApiCallerSource::kIframeAttribute);

  std::string header_value;
  headers.GetNormalizedHeader("Observe-Browsing-Topics", &header_value);

  absl::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(header_value);
  if (!item || !item->is_boolean() || !item->GetBoolean())
    return;

  // Check the page's IsPrimary() status again in case it has changed since the
  // request time.
  if (!request_initiator_frame.GetPage().IsPrimary())
    return;

  // TODO(crbug.com/1244137): IsPrimary() doesn't actually detect portals yet.
  // Remove this when it does.
  if (!static_cast<const RenderFrameHostImpl*>(
           request_initiator_frame.GetMainFrame())
           ->IsOutermostMainFrame()) {
    return;
  }

  // Store the observation.
  std::vector<blink::mojom::EpochTopicPtr> topics;
  GetContentClient()->browser()->HandleTopicsWebApi(
      caller_origin, request_initiator_frame.GetMainFrame(), caller_source,
      /*get_topics=*/false,
      /*observe=*/true, topics);
}

}  // namespace content
