// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/url_request_mojom_traits.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/timer/lap_timer.h"
#include "base/unguessable_token.h"
#include "build/buildflag.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/load_flags.h"
#include "net/base/request_priority.h"
#include "net/cookies/site_for_cookies.h"
#include "net/filter/source_stream_type.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/device_bound_sessions.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/trust_token_access_observer.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_request.mojom.h"
#include "services/network/public/mojom/web_bundle_handle.mojom.h"
#include "testing/perf/perf_result_reporter.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
namespace {

constexpr char kPolicyUrlA[] = "https://a.test/index.html";
constexpr char kPolicyUrlB[] = "https://b.test/index.html";

network::ResourceRequest CreateResourceRequest() {
  network::ResourceRequest request;
  request.method = "POST";
  request.url = GURL("https://example.com/resources/dummy.xml");
  request.site_for_cookies =
      net::SiteForCookies::FromUrl(GURL("https://example.com/index.html"));
  request.update_first_party_url_on_redirect = false;
  request.request_initiator = url::Origin::Create(request.url);
  request.isolated_world_origin =
      url::Origin::Create(GURL("chrome-extension://blah"));
  request.referrer = GURL("https://referrer.com/");
  request.referrer_policy =
      net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN;
  request.headers.SetHeader("Accept", "text/xml");
  request.cors_exempt_headers.SetHeader("X-Requested-With", "ForTesting");
  request.load_flags = net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                       net::LOAD_SHOULD_BYPASS_HSTS;
  request.resource_type = 2;
  request.priority = net::IDLE;
  request.priority_incremental = net::kDefaultPriorityIncremental;
  request.cors_preflight_policy =
      mojom::CorsPreflightPolicy::kConsiderPreflight;
  request.originated_from_service_worker = false;
  request.skip_service_worker = false;
  request.mode = mojom::RequestMode::kNoCors;
  request.credentials_mode = mojom::CredentialsMode::kInclude;
  request.redirect_mode = mojom::RedirectMode::kFollow;
  request.fetch_integrity = "dummy_fetch_integrity";
  request.expected_public_keys = {};
  request.keepalive = true;
  request.browsing_topics = true;
  request.ad_auction_headers = true;
  request.shared_storage_writable_eligible = true;
  request.has_user_gesture = false;
  request.enable_load_timing = true;
  request.enable_upload_progress = false;
  request.do_not_prompt_for_login = true;
  request.is_outermost_main_frame = true;
  request.transition_type = 0;
  request.previews_state = 0;
  request.upgrade_if_insecure = true;
  request.is_revalidating = false;
  request.throttling_profile_id = base::UnguessableToken::Create();
  request.fetch_window_id = base::UnguessableToken::Create();
  request.web_bundle_token_params =
      std::make_optional(ResourceRequest::WebBundleTokenParams(
          GURL("https://bundle.test/"), base::UnguessableToken::Create(),
          mojo::PendingRemote<network::mojom::WebBundleHandle>()));
  request.net_log_create_info = std::make_optional(net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID()));
  request.net_log_reference_info = std::make_optional(net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID()));
  request.devtools_accepted_stream_types = std::vector<net::SourceStreamType>(
      {net::SourceStreamType::kBrotli, net::SourceStreamType::kGzip,
       net::SourceStreamType::kDeflate});
  request.storage_access_api_status =
      net::StorageAccessApiStatus::kAccessViaAPI;

  request.trusted_params = ResourceRequest::TrustedParams();
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame,
      url::Origin::Create(request.url), url::Origin::Create(request.url),
      request.site_for_cookies);
  request.trusted_params->disable_secure_dns = true;
  request.trusted_params->allow_cookies_from_browser = true;
  request.trusted_params->include_request_cookies_with_response = true;

  request.trust_token_params = network::mojom::TrustTokenParams();
  request.trust_token_params->issuers.push_back(
      url::Origin::Create(GURL("https://issuer.com")));
  request.trust_token_params->operation =
      mojom::TrustTokenOperationType::kRedemption;
  request.trust_token_params->include_timestamp_header = true;
  request.trust_token_params->sign_request_data =
      mojom::TrustTokenSignRequestData::kInclude;
  request.trust_token_params->additional_signed_headers.push_back(
      "some_header");
#if BUILDFLAG(IS_ANDROID)
  request.socket_tag = net::SocketTag(1, 2);
#else
  request.socket_tag = net::SocketTag();
#endif

  return request;
}

void MeasureRoundtrip(const network::ResourceRequest& request,
                      const std::string& story_name) {
  base::LapTimer timer;
  do {
    network::ResourceRequest copied;
    mojo::test::SerializeAndDeserialize<mojom::URLRequest>(request, copied);
    ::benchmark::DoNotOptimize(copied);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  perf_test::PerfResultReporter reporter("URLRequestMojomTraitsPerfTest",
                                         story_name);
  reporter.RegisterImportantMetric(".wall_time", "us");
  reporter.AddResult(".wall_time", timer.TimePerLap());
}
TEST(URLRequestMojomTraitsPerfTest,
     Roundtrips_ResourceRequest_WithoutPermissionsPolicy) {
  MeasureRoundtrip(CreateResourceRequest(),
                   "Roundtrips_ResourceRequest_WithoutPermissionsPolicy");
}

TEST(URLRequestMojomTraitsPerfTest,
     Roundtrips_ResourceRequest_WithPermissionsPolicy) {
  ParsedPermissionsPolicyDeclaration
      policy_declaration_with_reporting_endpoint = {
          network::mojom::PermissionsPolicyFeature::
              kCamera, /*allowed_origins=*/
          {*network::OriginWithPossibleWildcards::FromOrigin(
              url::Origin::Create(GURL(kPolicyUrlB)))},
          /*self_if_matches=*/std::nullopt,
          /*matches_all_origins=*/false,
          /*matches_opaque_src=*/false};
  policy_declaration_with_reporting_endpoint.reporting_endpoint =
      "https://example.com";

  std::unique_ptr<network::PermissionsPolicy> parent_policy =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/nullptr,
          /*header_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kAccelerometer, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kPolicyUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/true}}},
          /*container_policy=*/
          {{policy_declaration_with_reporting_endpoint}},
          url::Origin::Create(GURL(kPolicyUrlA)), true);

  std::unique_ptr<network::PermissionsPolicy> permissions_policy =
      PermissionsPolicy::CreateFromParentPolicy(
          /*parent_policy=*/parent_policy.get(),
          /*header_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kBrowsingTopics, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kPolicyUrlB)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false},
            {network::mojom::PermissionsPolicyFeature::
                 kSharedStorage, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kPolicyUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/true}}},
          /*container_policy=*/
          {{{network::mojom::PermissionsPolicyFeature::
                 kStorageAccessAPI, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kPolicyUrlA)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/false,
             /*matches_opaque_src=*/false},
            {network::mojom::PermissionsPolicyFeature::
                 kAutoplay, /*allowed_origins=*/
             {*network::OriginWithPossibleWildcards::FromOrigin(
                 url::Origin::Create(GURL(kPolicyUrlB)))},
             /*self_if_matches=*/std::nullopt,
             /*matches_all_origins=*/true,
             /*matches_opaque_src=*/false}}},
          url::Origin::Create(GURL(kPolicyUrlA)), false);

  network::ResourceRequest request = CreateResourceRequest();
  request.permissions_policy = *permissions_policy.get();

  MeasureRoundtrip(request, "Roundtrips_ResourceRequest_WithPermissionsPolicy");
}

}  // namespace
}  // namespace network
