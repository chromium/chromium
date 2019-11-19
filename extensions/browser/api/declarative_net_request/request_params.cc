// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/request_params.h"

#include "content/public/common/resource_type.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

namespace extensions {
namespace declarative_net_request {

namespace {
namespace flat_rule = url_pattern_index::flat;

// Maps content::ResourceType to flat_rule::ElementType.
flat_rule::ElementType GetElementType(content::ResourceType type) {
  switch (type) {
    case content::ResourceType::kPrefetch:
    case content::ResourceType::kSubResource:
      return flat_rule::ElementType_OTHER;
    case content::ResourceType::kMainFrame:
    case content::ResourceType::kNavigationPreloadMainFrame:
      return flat_rule::ElementType_MAIN_FRAME;
    case content::ResourceType::kCspReport:
      return flat_rule::ElementType_CSP_REPORT;
    case content::ResourceType::kScript:
    case content::ResourceType::kWorker:
    case content::ResourceType::kSharedWorker:
    case content::ResourceType::kServiceWorker:
      return flat_rule::ElementType_SCRIPT;
    case content::ResourceType::kImage:
    case content::ResourceType::kFavicon:
      return flat_rule::ElementType_IMAGE;
    case content::ResourceType::kStylesheet:
      return flat_rule::ElementType_STYLESHEET;
    case content::ResourceType::kObject:
    case content::ResourceType::kPluginResource:
      return flat_rule::ElementType_OBJECT;
    case content::ResourceType::kXhr:
      return flat_rule::ElementType_XMLHTTPREQUEST;
    case content::ResourceType::kSubFrame:
    case content::ResourceType::kNavigationPreloadSubFrame:
      return flat_rule::ElementType_SUBDOCUMENT;
    case content::ResourceType::kPing:
      return flat_rule::ElementType_PING;
    case content::ResourceType::kMedia:
      return flat_rule::ElementType_MEDIA;
    case content::ResourceType::kFontResource:
      return flat_rule::ElementType_FONT;
  }
  NOTREACHED();
  return flat_rule::ElementType_OTHER;
}

// Returns the flat_rule::ElementType for the given |request|.
flat_rule::ElementType GetElementType(const WebRequestInfo& request) {
  if (request.url.SchemeIsWSOrWSS())
    return flat_rule::ElementType_WEBSOCKET;

  return GetElementType(request.type);
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

}  // namespace

RequestParams::RequestParams(const WebRequestInfo& info)
    : url(&info.url),
      first_party_origin(info.initiator.value_or(url::Origin())),
      element_type(GetElementType(info)),
      request_info(&info) {
  is_third_party = IsThirdPartyRequest(*url, first_party_origin);
}

RequestParams::RequestParams() = default;
RequestParams::~RequestParams() = default;

}  // namespace declarative_net_request
}  // namespace extensions
