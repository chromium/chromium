// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_params.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace {
namespace flat_rule = url_pattern_index::flat;

// Maps WebRequestResourceType to flat_rule::ElementType.
flat_rule::ElementType GetElementType(WebRequestResourceType web_request_type) {
  switch (web_request_type) {
    case WebRequestResourceType::OTHER:
      return flat_rule::ElementType_OTHER;
    case WebRequestResourceType::MAIN_FRAME:
      return flat_rule::ElementType_MAIN_FRAME;
    case WebRequestResourceType::CSP_REPORT:
      return flat_rule::ElementType_CSP_REPORT;
    case WebRequestResourceType::SCRIPT:
      return flat_rule::ElementType_SCRIPT;
    case WebRequestResourceType::IMAGE:
      return flat_rule::ElementType_IMAGE;
    case WebRequestResourceType::STYLESHEET:
      return flat_rule::ElementType_STYLESHEET;
    case WebRequestResourceType::OBJECT:
      return flat_rule::ElementType_OBJECT;
    case WebRequestResourceType::XHR:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case WebRequestResourceType::SUB_FRAME:
      return flat_rule::ElementType_SUBDOCUMENT;
    case WebRequestResourceType::PING:
      return flat_rule::ElementType_PING;
    case WebRequestResourceType::MEDIA:
      return flat_rule::ElementType_MEDIA;
    case WebRequestResourceType::FONT:
      return flat_rule::ElementType_FONT;
    case WebRequestResourceType::WEB_SOCKET:
      return flat_rule::ElementType_WEBSOCKET;
  }
  NOTREACHED();
  return flat_rule::ElementType_OTHER;
}

// Returns whether the request to |url| is third party to its |document_origin|.
// TODO(crbug.com/696822): Look into caching this.
bool IsThirdPartyRequest(const GURL& url, const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      url, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsThirdPartyRequest(const url::Origin& origin,
                         const url::Origin& document_origin) {
  if (document_origin.opaque())
    return true;

  return !net::registry_controlled_domains::SameDomainOrHost(
      origin, document_origin,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

content::GlobalFrameRoutingId GetFrameRoutingId(
    content::RenderFrameHost* host) {
  if (!host)
    return content::GlobalFrameRoutingId();

  return content::GlobalFrameRoutingId(host->GetProcess()->GetID(),
                                       host->GetRoutingID());
}

}  // namespace

RequestParams::RequestParams(const WebRequestInfo& info)
    : url(&info.url),
      first_party_origin(info.initiator.value_or(url::Origin())),
      element_type(GetElementType(info.web_request_type)),
      parent_routing_id(info.parent_routing_id) {
  is_third_party = IsThirdPartyRequest(*url, first_party_origin);
}

RequestParams::RequestParams(content::RenderFrameHost* host)
    : url(&host->GetLastCommittedURL()),
      parent_routing_id(GetFrameRoutingId(host->GetParent())) {
  if (host->GetParent()) {
    // Note the discrepancy with the WebRequestInfo constructor. For a
    // navigation request, we'd use the request initiator as the
    // |first_party_origin|. But here we use the origin of the parent frame.
    // This is the same as crbug.com/996998.
    first_party_origin = host->GetParent()->GetLastCommittedOrigin();
    element_type = url_pattern_index::flat::ElementType_SUBDOCUMENT;
  } else {
    first_party_origin = url::Origin();
    element_type = url_pattern_index::flat::ElementType_MAIN_FRAME;
  }
  is_third_party =
      IsThirdPartyRequest(host->GetLastCommittedOrigin(), first_party_origin);
}

RequestParams::RequestParams() = default;
RequestParams::~RequestParams() = default;

}  // namespace declarative_net_request
}  // namespace extensions
