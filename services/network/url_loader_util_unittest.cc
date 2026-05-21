// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_util.h"

#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/cookie_settings.h"
#include "services/network/pervasive_resources/shared_resource_checker.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/originating_process_id.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace network::url_loader_util {
namespace {

class ConfigureUrlRequestTest : public testing::Test {
 protected:
  void SetUp() override {
    context_ = net::CreateTestURLRequestContextBuilder()->Build();
  }

  std::unique_ptr<net::URLRequest> CreateURLRequest(const GURL& url) {
    return context_->CreateRequest(url, net::DEFAULT_PRIORITY, &delegate_,
                                   TRAFFIC_ANNOTATION_FOR_TESTS);
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::URLRequestContext> context_;
  net::TestDelegate delegate_;
};

// Verifies that when `prefer_factory_site_for_cookies` is true and the
// factory's IsolationInfo has a non-null site_for_cookies, that value
// overrides the request's (null) site_for_cookies.
TEST_F(ConfigureUrlRequestTest,
       PreferFactorySiteForCookies_FlagTrue_UsesFactoryValue) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const GURL request_url("https://example.com/resource.js");
  const url::Origin extension_origin =
      url::Origin::Create(GURL("chrome-extension://abc123"));

  ResourceRequest request;
  request.url = request_url;
  request.method = "GET";
  // Simulate the renderer's incorrect null site_for_cookies.
  request.site_for_cookies = net::SiteForCookies();
  request.request_initiator = extension_origin;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = OriginatingProcessId::browser();
  factory_params->prefer_factory_site_for_cookies = true;
  // Set up factory IsolationInfo with extension-origin site_for_cookies.
  net::SiteForCookies extension_site_for_cookies =
      net::SiteForCookies::FromOrigin(extension_origin);
  factory_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, extension_origin,
      extension_origin, extension_site_for_cookies);

  cors::OriginAccessList origin_access_list;
  CookieSettings cookie_settings;
  SharedResourceChecker shared_resource_checker(cookie_settings);

  auto url_request = CreateURLRequest(request_url);
  ConfigureUrlRequest(request, *factory_params, origin_access_list,
                      *url_request, shared_resource_checker);

  // The url_request should use the factory's extension site_for_cookies.
  EXPECT_FALSE(url_request->site_for_cookies().IsNull());
  EXPECT_TRUE(
      url_request->site_for_cookies().IsFirstParty(extension_origin.GetURL()));
}

// Verifies that when `prefer_factory_site_for_cookies` is false, the
// request's original (null) site_for_cookies is preserved.
TEST_F(ConfigureUrlRequestTest,
       PreferFactorySiteForCookies_FlagFalse_UsesRequestValue) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const GURL request_url("https://example.com/resource.js");
  const url::Origin extension_origin =
      url::Origin::Create(GURL("chrome-extension://abc123"));

  ResourceRequest request;
  request.url = request_url;
  request.method = "GET";
  // Null site_for_cookies (as renderer would compute for cross-site).
  request.site_for_cookies = net::SiteForCookies();
  request.request_initiator = extension_origin;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = OriginatingProcessId::browser();
  factory_params->prefer_factory_site_for_cookies = false;
  // Factory still has the extension site_for_cookies, but flag is off.
  net::SiteForCookies extension_site_for_cookies =
      net::SiteForCookies::FromOrigin(extension_origin);
  factory_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, extension_origin,
      extension_origin, extension_site_for_cookies);

  cors::OriginAccessList origin_access_list;
  CookieSettings cookie_settings;
  SharedResourceChecker shared_resource_checker(cookie_settings);

  auto url_request = CreateURLRequest(request_url);
  ConfigureUrlRequest(request, *factory_params, origin_access_list,
                      *url_request, shared_resource_checker);

  // The url_request should retain the original null site_for_cookies.
  EXPECT_TRUE(url_request->site_for_cookies().IsNull());
}

// Verifies that `ShouldForceIgnoreSiteForCookies` uses the browser-computed
// `effective_site_for_cookies` (from factory IsolationInfo) rather than the
// renderer's null `request.site_for_cookies`.
//
// Scenario: An extension page embeds a web iframe. The web iframe makes a
// same-site request. The extension has host permissions (via OriginAccessList)
// for the web sites. The renderer computes null site_for_cookies because the
// extension is cross-site to the web content, but the factory provides the
// correct extension-origin site_for_cookies.
//
// This exercises the second code path in ShouldForceIgnoreSiteForCookies where
// site_for_cookies origin is checked for access, NOT the first path where
// request_initiator is checked directly.
TEST_F(ConfigureUrlRequestTest,
       PreferFactorySiteForCookies_ForceIgnoreSiteForCookies) {
  url::ScopedSchemeRegistryForTests scoped_registry;
  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);

  const GURL request_url("https://api.example.com/data");
  const url::Origin web_origin =
      url::Origin::Create(GURL("https://example.com"));
  const url::Origin extension_origin =
      url::Origin::Create(GURL("chrome-extension://abc123"));

  ResourceRequest request;
  request.url = request_url;
  request.method = "GET";
  // Renderer computes null site_for_cookies for extension subframes.
  request.site_for_cookies = net::SiteForCookies();
  // The web iframe is the initiator -- NOT the extension. This ensures the
  // first code path (initiator has direct access) is NOT triggered.
  request.request_initiator = web_origin;

  auto factory_params = mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = OriginatingProcessId::browser();
  factory_params->prefer_factory_site_for_cookies = true;
  net::SiteForCookies extension_site_for_cookies =
      net::SiteForCookies::FromOrigin(extension_origin);
  factory_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, extension_origin,
      extension_origin, extension_site_for_cookies);

  // Grant the extension origin access to https://*.example.com. This means
  // the extension's site_for_cookies origin can access both the initiator
  // (example.com) and the target (api.example.com). The web_origin itself
  // is NOT granted access, so the first code path will not match.
  cors::OriginAccessList origin_access_list;
  origin_access_list.AddAllowListEntryForOrigin(
      extension_origin, "https", "example.com",
      /*port=*/0, mojom::CorsDomainMatchMode::kAllowSubdomains,
      mojom::CorsPortMatchMode::kAllowAnyPort,
      mojom::CorsOriginAccessMatchPriority::kDefaultPriority);

  CookieSettings cookie_settings;
  SharedResourceChecker shared_resource_checker(cookie_settings);

  auto url_request = CreateURLRequest(request_url);
  ConfigureUrlRequest(request, *factory_params, origin_access_list,
                      *url_request, shared_resource_checker);

  // With the effective_site_for_cookies fix, the extension's site_for_cookies
  // is what `NetworkServiceNetworkDelegate::OnShouldForceIgnoreSiteForCookies`
  // will read off the URLRequest. The extension origin can access both the
  // initiator and target, and they are same-site, so the delegate path
  // should return true.
  EXPECT_TRUE(ShouldForceIgnoreSiteForCookies(
      url_request->url(), url_request->initiator(),
      url_request->site_for_cookies(), origin_access_list));
}

}  // namespace
}  // namespace network::url_loader_util
