// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include "base/containers/contains.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/http/http_response_headers.h"
#include "net/reporting/reporting_header_parser.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/content_language_parser.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"
#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/link_header_parser.h"
#include "services/network/public/cpp/no_vary_search_header_parser.h"
#include "services/network/public/cpp/origin_agent_cluster_parser.h"
#include "services/network/public/cpp/supports_loading_mode/supports_loading_mode_parser.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/cpp/variants_header_parser.h"
#include "services/network/public/cpp/x_frame_options_parser.h"
#include "services/network/public/mojom/supports_loading_mode.mojom.h"

namespace network {

mojom::ParsedHeadersPtr PopulateParsedHeaders(
    const net::HttpResponseHeaders* headers,
    const GURL& url) {
  auto parsed_headers = mojom::ParsedHeaders::New();
  if (!headers)
    return parsed_headers;

  AddContentSecurityPolicyFromHeaders(*headers, url,
                                      &parsed_headers->content_security_policy);

  parsed_headers->allow_csp_from = ParseAllowCSPFromHeader(*headers);

  parsed_headers->cross_origin_embedder_policy =
      ParseCrossOriginEmbedderPolicy(*headers);
  parsed_headers->cross_origin_opener_policy =
      ParseCrossOriginOpenerPolicy(*headers);

  std::string origin_agent_cluster;
  headers->GetNormalizedHeader("Origin-Agent-Cluster", &origin_agent_cluster);
  parsed_headers->origin_agent_cluster =
      ParseOriginAgentCluster(origin_agent_cluster);

  std::string accept_ch;
  if (headers->GetNormalizedHeader("Accept-CH", &accept_ch))
    parsed_headers->accept_ch = ParseClientHintsHeader(accept_ch);

  std::string critical_ch;
  if (headers->GetNormalizedHeader("Critical-CH", &critical_ch))
    parsed_headers->critical_ch = ParseClientHintsHeader(critical_ch);

  parsed_headers->xfo = ParseXFrameOptions(*headers);

  parsed_headers->link_headers = ParseLinkHeaders(*headers, url);

  std::string timing_allow_origin_value;
  if (headers->GetNormalizedHeader("Timing-Allow-Origin",
                                   &timing_allow_origin_value)) {
    parsed_headers->timing_allow_origin =
        ParseTimingAllowOrigin(timing_allow_origin_value);
  }

  network::mojom::SupportsLoadingModePtr result =
      network::ParseSupportsLoadingMode(*headers);
  if (!result.is_null() &&
      base::Contains(result->supported_modes,
                     network::mojom::LoadingMode::kCredentialedPrerender)) {
    parsed_headers->supports_loading_mode.push_back(
        network::mojom::LoadingMode::kCredentialedPrerender);
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (base::FeatureList::IsEnabled(net::features::kDocumentReporting)) {
    std::string reporting_endpoints;
    if (headers->GetNormalizedHeader("Reporting-Endpoints",
                                     &reporting_endpoints)) {
      parsed_headers->reporting_endpoints =
          net::ParseReportingEndpoints(reporting_endpoints);
    }
  }
#endif

  if (base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage) ||
      base::FeatureList::IsEnabled(
          network::features::kReduceAcceptLanguageOriginTrial)) {
    std::string variants;
    if (headers->GetNormalizedHeader("Variants", &variants)) {
      parsed_headers->variants_headers = ParseVariantsHeaders(variants);
    }
    std::string content_language;
    if (headers->GetNormalizedHeader("Content-Language", &content_language)) {
      parsed_headers->content_language =
          ParseContentLanguages(content_language);
    }
  }

  // We're not checking that PrefetchNoVarySearch is enabled on the
  // renderer side through the Origin Trial, as the network service
  // doesn't know anything about blink.
  // The code here only parses the No-Vary-Search header if it is present.
  if (base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch))
    parsed_headers->no_vary_search_with_parse_error =
        ParseNoVarySearch(*headers);

  return parsed_headers;
}

}  // namespace network
