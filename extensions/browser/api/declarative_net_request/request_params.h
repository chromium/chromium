// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "components/url_pattern_index/url_pattern_index.h"
#include "url/origin.h"

class GURL;

namespace extensions {
struct WebRequestInfo;

namespace declarative_net_request {
class RulesetMatcher;

// Struct to hold parameters for a network request.
struct RequestParams {
  // |info| must outlive this instance.
  explicit RequestParams(const WebRequestInfo& info);
  RequestParams();
  ~RequestParams();

  // This is a pointer to a GURL. Hence the GURL must outlive this struct.
  const GURL* url = nullptr;
  url::Origin first_party_origin;
  url_pattern_index::flat::ElementType element_type =
      url_pattern_index::flat::ElementType_OTHER;
  bool is_third_party = false;

  // A map from RulesetMatchers to whether it has a matching allow rule. Used as
  // a cache to prevent additional calls to GetAllowAction.
  mutable base::flat_map<const RulesetMatcher*, bool> allow_rule_cache;

  // Pointer to the corresponding WebRequestInfo object. Outlives this struct.
  // Can be null for some unit tests.
  const WebRequestInfo* request_info = nullptr;

  DISALLOW_COPY_AND_ASSIGN(RequestParams);
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_REQUEST_PARAMS_H_
