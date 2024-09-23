// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/parsed_headers.h"

#include <set>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "net/base/features.h"
#include "net/http/http_cookie_indices.h"
#include "net/http/http_response_headers.h"
#include "net/reporting/reporting_header_parser.h"
#include "net/url_request/clear_site_data.h"
#include "services/network/public/cpp/avail_language_header_parser.h"
#include "services/network/public/cpp/browsing_topics_parser.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/content_language_parser.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/cpp/cross_origin_embedder_policy_parser.h"
#include "services/network/public/cpp/cross_origin_opener_policy_parser.h"
#include "services/network/public/cpp/document_isolation_policy_parser.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/fence_event_reporting_parser.h"
#include "services/network/public/cpp/link_header_parser.h"
#include "services/network/public/cpp/no_vary_search_header_parser.h"
#include "services/network/public/cpp/origin_agent_cluster_parser.h"
#include "services/network/public/cpp/supports_loading_mode/supports_loading_mode_parser.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
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

  parsed_headers->document_isolation_policy =
      ParseDocumentIsolationPolicy(*headers);

  std::string origin_agent_cluster;
  headers->GetNormalizedHeader("Origin-Agent-Cluster", &origin_agent_cluster);
  parsed_headers->origin_agent_cluster =
      ParseOriginAgentCluster(origin_agent_cluster);

  // If the Clear-Site-Data header would clear client hints, we must not respect
  // any Accept-CH or Critical-CH headers.
  parsed_headers->client_hints_ignored_due_to_clear_site_data_header = false;
  std::string clear_site_data_header;
  headers->GetNormalizedHeader(net::kClearSiteDataHeader,
                               &clear_site_data_header);
  std::vector<std::string> clear_site_data_types =
      net::ClearSiteDataHeaderContents(clear_site_data_header);
  std::set<std::string> clear_site_data_set(clear_site_data_types.begin(),
                                            clear_site_data_types.end());
  if (clear_site_data_set.find(net::kDatatypeCache) !=
          clear_site_data_set.end() ||
      clear_site_data_set.find(net::kDatatypeClientHints) !=
          clear_site_data_set.end() ||
      clear_site_data_set.find(net::kDatatypeCookies) !=
          clear_site_data_set.end() ||
      clear_site_data_set.find(net::kDatatypeWildcard) !=
          clear_site_data_set.end()) {
    parsed_headers->client_hints_ignored_due_to_clear_site_data_header = true;
  }
  if (!features::ShouldBlockAcceptClientHintsFor(url::Origin::Create(url)) &&
      !parsed_headers->client_hints_ignored_due_to_clear_site_data_header) {
    std::string accept_ch;
    if (headers->GetNormalizedHeader("Accept-CH", &accept_ch)) {
      parsed_headers->accept_ch = ParseClientHintsHeader(accept_ch);
    }

    std::string critical_ch;
    if (headers->GetNormalizedHeader("Critical-CH", &critical_ch)) {
      parsed_headers->critical_ch = ParseClientHintsHeader(critical_ch);
    }
  }

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

  if (base::FeatureList::IsEnabled(network::features::kCookieIndicesHeader)) {
    parsed_headers->cookie_indices = net::ParseCookieIndices(*headers);
  }

  if (base::FeatureList::IsEnabled(network::features::kReduceAcceptLanguage)) {
    std::string avail_language;
    if (headers->GetNormalizedHeader("Avail-Language", &avail_language)) {
      parsed_headers->avail_language = ParseAvailLanguage(avail_language);
    }
    std::string content_language;
    if (headers->GetNormalizedHeader("Content-Language", &content_language)) {
      parsed_headers->content_language =
          ParseContentLanguages(content_language);
    }
  }

  // The code here only parses the No-Vary-Search header if it is present.
  parsed_headers->no_vary_search_with_parse_error = ParseNoVarySearch(*headers);

  parsed_headers->observe_browsing_topics =
      ParseObserveBrowsingTopicsFromHeader(*headers);

  parsed_headers->allow_cross_origin_event_reporting =
      ParseAllowCrossOriginEventReportingFromHeader(*headers);

  return parsed_headers;
}

}  // namespace network
