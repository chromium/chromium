// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "content/public/browser/global_routing_id.h"
#include "extensions/browser/api/declarative_net_request/regex_rules_matcher.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {
class CompositeMatcher;

// Struct to hold parameters for a network request.
struct RequestParams {
  // |info| must outlive this instance.
  explicit RequestParams(const WebRequestInfo& info);
  // |host| must not undergo a navigation or get deleted for the duration of
  // this instance.
  explicit RequestParams(content::RenderFrameHost* host);
  RequestParams();
  ~RequestParams();

  // This is a pointer to a GURL. Hence the GURL must outlive this struct.
  const GURL* url = nullptr;
  url::Origin first_party_origin;
  url_pattern_index::flat::ElementType element_type =
      url_pattern_index::flat::ElementType_OTHER;
  bool is_third_party = false;

  // ID of the parent RenderFrameHost.
  content::GlobalFrameRoutingId parent_routing_id;

  // A map from CompositeMatcher to the priority of its highest priority
  // matching allow or allowAllRequests rule if there is one, or base::nullopt
  // otherwise. Used as a cache to prevent additional calls to
  // GetBeforeRequestAction.
  mutable base::flat_map<const CompositeMatcher*, base::Optional<uint64_t>>
      allow_rule_max_priority;

  // Lower cased url, used for regex matching. Cached for performance.
  mutable base::Optional<std::string> lower_cased_url_spec;

  // Map from RegexRulesMatcher to a vector of potential matches for this
  // request. Cached for performance.
  mutable base::flat_map<const RegexRulesMatcher*, std::vector<RegexRuleInfo>>
      potential_regex_matches;

  DISALLOW_COPY_AND_ASSIGN(RequestParams);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_
