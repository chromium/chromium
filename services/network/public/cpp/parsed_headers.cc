// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"
#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/origin_isolation_parser.h"

namespace network {

mojom::ParsedHeadersPtr PopulateParsedHeaders(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const GURL& url) {
  auto parsed_headers = mojom::ParsedHeaders::New();
  if (!headers)
    return parsed_headers;

  AddContentSecurityPolicyFromHeaders(*headers, url,
                                      &parsed_headers->content_security_policy);

  parsed_headers->allow_csp_from = ParseAllowCSPFromHeader(*headers);

  parsed_headers->cross_origin_embedder_policy =
      ParseCrossOriginEmbedderPolicy(*headers);
  parsed_headers->cross_origin_opener_policy = ParseCrossOriginOpenerPolicy(
      *headers, parsed_headers->cross_origin_embedder_policy);

  // TODO(https://crbug.com/1157917): we implement the change at
  // https://github.com/whatwg/html/pull/6214 but all the class and function
  // names are not yet updated.
  std::string origin_isolation;
  if (headers->GetNormalizedHeader("Origin-Agent-Cluster", &origin_isolation))
    parsed_headers->origin_isolation = ParseOriginIsolation(origin_isolation);

  std::string accept_ch;
  if (headers->GetNormalizedHeader("Accept-CH", &accept_ch))
    parsed_headers->accept_ch = ParseClientHintsHeader(accept_ch);

  std::string accept_ch_lifetime;
  if (headers->GetNormalizedHeader("Accept-CH-Lifetime", &accept_ch_lifetime)) {
    parsed_headers->accept_ch_lifetime =
        ParseAcceptCHLifetime(accept_ch_lifetime);
  }

  std::string critical_ch;
  if (headers->GetNormalizedHeader("Critical-CH", &critical_ch))
    parsed_headers->critical_ch = ParseClientHintsHeader(critical_ch);

  return parsed_headers;
}

}  // namespace network
