// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/hashprefix_realtime/hash_realtime_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

using ExtensionWebRequestPermissionsTest = ExtensionsTest;

constexpr char kTestRelayUrl[] = "https://ohttp.endpoint.test/";

class ExtensionWebRequestPermissionsWithHashRealTimeDependenceTest
    : public ExtensionsTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    ExtensionsTest::SetUp();
    if (GetParam()) {
      feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{safe_browsing::kHashPrefixRealTimeLookups,
            {{"SafeBrowsingHashPrefixRealTimeLookupsRelayUrl",
              kTestRelayUrl}}}},
          /*disabled_features=*/{});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{safe_browsing::kHashPrefixRealTimeLookups});
    }
  }

 private:
  safe_browsing::hash_realtime_utils::GoogleChromeBrandingPretenderForTesting
      apply_branding_;
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    ExtensionWebRequestPermissionsWithHashRealTimeDependenceTest,
    testing::Bool());

TEST_P(ExtensionWebRequestPermissionsWithHashRealTimeDependenceTest,
       TestHideRequestForURL) {
  enum HideRequestMask {
    HIDE_NONE = 0,
    HIDE_RENDERER_REQUEST = 1,
    HIDE_SUB_FRAME_NAVIGATION = 2,
    HIDE_MAIN_FRAME_NAVIGATION = 4,
    HIDE_BROWSER_SUB_RESOURCE_REQUEST = 8,
    HIDE_ALL = HIDE_RENDERER_REQUEST | HIDE_SUB_FRAME_NAVIGATION |
               HIDE_MAIN_FRAME_NAVIGATION | HIDE_BROWSER_SUB_RESOURCE_REQUEST,
  };

  ExtensionsAPIClient api_client;
  auto* permission_helper = PermissionHelper::Get(browser_context());

  struct TestCase {
    const char* url;
    int expected_hide_request_mask;
  };
  std::vector<TestCase> cases = {
      {"https://www.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"http://clients4.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients4.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients9999.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://clients9999..google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients9999.example.google.com",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://clients.google.com.",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://.clients.google.com.",
       HIDE_BROWSER_SUB_RESOURCE_REQUEST | HIDE_SUB_FRAME_NAVIGATION},
      {"https://test.clients.google.com",
       HIDE_SUB_FRAME_NAVIGATION | HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://google.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://www.example.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://sb-ssl.google.com", HIDE_ALL},
      {"https://sb-ssl.random.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://safebrowsing.googleapis.com", HIDE_ALL},
      // Unsupported scheme.
      {"blob:https://safebrowsing.googleapis.com/"
       "fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       HIDE_ALL},
      {"filesystem:https://safebrowsing.googleapis.com/path", HIDE_ALL},
      {"https://safebrowsing.googleapis.com.", HIDE_ALL},
      {"https://safebrowsing.googleapis.com/v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com:80/v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com./v4", HIDE_ALL},
      {"https://safebrowsing.googleapis.com/v5", HIDE_ALL},
      {"https://safebrowsing.google.com/safebrowsing", HIDE_ALL},
      {"https://safebrowsing.google.com/safebrowsing/anything", HIDE_ALL},
      {"https://safebrowsing.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://chrome.google.com", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"http://www.google.com/", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
      {"https://chrome.google.com/webstore", HIDE_ALL},
      {"https://chrome.google.com./webstore", HIDE_ALL},
      {"https://chrome.google.com./webstore/", HIDE_ALL},
      {"https://chromewebstore.google.com", HIDE_ALL},
      {"https://chromewebstore.google.com/", HIDE_ALL},
      {"https://chromewebstore.google.com./", HIDE_ALL},
      {"https://chromewebstore.google.com:80/", HIDE_ALL},
      {"https://chromewebstore.google.com/?query", HIDE_ALL},
      // Unsupported scheme.
      {"blob:https://chrome.google.com/fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       HIDE_ALL},
      // Unsupported scheme.
      {"chrome://test/", HIDE_ALL},
      // Unsupported scheme.
      {"chrome-untrusted://test/", HIDE_ALL},
      {"notregisteredscheme://www.foobar.com", HIDE_ALL},
      {"https://chrome.google.com:80/webstore", HIDE_ALL},
      {"https://chrome.google.com/webstore?query", HIDE_ALL},
      {"http://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://chrome.google.com/webstore/inlineinstall/detail/"
       "kcnhkahnjcbndmmehfkdnkjomaanaooo",
       HIDE_ALL},
  };
  std::vector<TestCase> additional_cases;
  if (GetParam()) {
    additional_cases = {
        {"https://ohttp.endpoint.test", HIDE_ALL},
        {"https://ohttp.endpoint.test/", HIDE_ALL},
        {"https://ohttp.endpoint.test/path", HIDE_BROWSER_SUB_RESOURCE_REQUEST},
        {"https://endpoint.test/", HIDE_BROWSER_SUB_RESOURCE_REQUEST}};
  } else {
    additional_cases = {
        {"https://ohttp.endpoint.test/", HIDE_BROWSER_SUB_RESOURCE_REQUEST}};
  }
  cases.insert(cases.end(), additional_cases.begin(), additional_cases.end());

  const int kRendererProcessId = 1;
  const int kBrowserProcessId = -1;

  // Returns a WebRequestInfoInitParams instance constructed as per the given
  // parameters.
  auto create_request_params = [](const GURL& url,
                                  WebRequestResourceType web_request_type,
                                  int render_process_id) {
    WebRequestInfoInitParams request;
    request.url = url;
    request.render_process_id = render_process_id;
    request.web_request_type = web_request_type;
    request.is_navigation_request =
        web_request_type == WebRequestResourceType::MAIN_FRAME ||
        web_request_type == WebRequestResourceType::SUB_FRAME;
    return request;
  };

  for (const TestCase& test_case : cases) {
    SCOPED_TRACE(test_case.url);

    GURL request_url(test_case.url);
    ASSERT_TRUE(request_url.is_valid());

    {
      SCOPED_TRACE("Renderer initiated sub-resource request");
      WebRequestInfo request(create_request_params(
          request_url, WebRequestResourceType::OTHER, kRendererProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_RENDERER_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(permission_helper, request));
    }

    {
      SCOPED_TRACE(
          "Renderer initiated sub-resource request from "
          "chrome-untrusted://");
      auto request_init_params = create_request_params(
          request_url, WebRequestResourceType::OTHER, kRendererProcessId);
      GURL url("chrome-untrusted://test/");
      request_init_params.initiator = url::Origin::Create(url);

      WebRequestInfo request(std::move(request_init_params));
      // Always hide requests from chrome-untrusted://
      EXPECT_TRUE(
          WebRequestPermissions::HideRequest(permission_helper, request));
    }

    {
      SCOPED_TRACE("Browser initiated sub-resource request");
      WebRequestInfo request(create_request_params(
          request_url, WebRequestResourceType::OTHER, kBrowserProcessId));
      bool expect_hidden = test_case.expected_hide_request_mask &
                           HIDE_BROWSER_SUB_RESOURCE_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(permission_helper, request));
    }

    {
      SCOPED_TRACE("Main-frame navigation");
      WebRequestInfo request(create_request_params(
          request_url, WebRequestResourceType::MAIN_FRAME, kBrowserProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_MAIN_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(permission_helper, request));
    }

    {
      SCOPED_TRACE("Sub-frame navigation");
      WebRequestInfo request(create_request_params(
          request_url, WebRequestResourceType::SUB_FRAME, kBrowserProcessId));
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_SUB_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(permission_helper, request));
    }
  }

  // Check protection of requests originating from the frame showing the Chrome
  // WebStore. Normally this request is not protected:
  GURL non_sensitive_url("http://www.google.com/test.js");

  {
    WebRequestInfo non_sensitive_request(create_request_params(
        non_sensitive_url, WebRequestResourceType::SCRIPT, kRendererProcessId));
    EXPECT_FALSE(WebRequestPermissions::HideRequest(permission_helper,
                                                    non_sensitive_request));
  }

  // If the origin is labeled by the WebStoreAppId, it becomes protected.
  {
    const int kWebstoreProcessId = 42;
    ProcessMap::Get(browser_context())
        ->Insert(extensions::kWebStoreAppId, kWebstoreProcessId);
    WebRequestInfo sensitive_request_info(create_request_params(
        non_sensitive_url, WebRequestResourceType::SCRIPT, kWebstoreProcessId));
    EXPECT_TRUE(WebRequestPermissions::HideRequest(permission_helper,
                                                   sensitive_request_info));
  }
  // If the request is initiated by the new webstore domain it becomes
  // protected.
  {
    auto request_init_params = create_request_params(
        non_sensitive_url, WebRequestResourceType::SCRIPT, kRendererProcessId);
    GURL webstore_url("https://chromewebstore.google.com/");
    request_init_params.initiator = url::Origin::Create(webstore_url);

    WebRequestInfo sensitive_request_info(std::move(request_init_params));
    EXPECT_TRUE(WebRequestPermissions::HideRequest(permission_helper,
                                                   sensitive_request_info));
  }
  // Requests initiated in opaque origins with the webstore as a precursor will
  // also be protected.
  {
    auto request_init_params = create_request_params(
        non_sensitive_url, WebRequestResourceType::SCRIPT, kRendererProcessId);
    GURL webstore_url("https://chromewebstore.google.com/");
    auto opaque_origin =
        url::Origin::Create(webstore_url).DeriveNewOpaqueOrigin();
    EXPECT_TRUE(opaque_origin.opaque());
    request_init_params.initiator = std::move(opaque_origin);

    WebRequestInfo sensitive_request_info(std::move(request_init_params));
    EXPECT_TRUE(WebRequestPermissions::HideRequest(permission_helper,
                                                   sensitive_request_info));
  }
}

// Tests that subresource requests to Web origins initiated from
// chrome-untrusted:// pages can't be inspected.
TEST_F(ExtensionWebRequestPermissionsTest,
       CanNotAccessSubresourceRequestsFromChromeUntrustedPage) {
  ExtensionsAPIClient api_client;
  auto* permission_helper = PermissionHelper::Get(browser_context());

  auto create_sub_resource_request = [](const GURL& url,
                                        WebRequestResourceType type) {
    WebRequestInfoInitParams request;
    request.url = url;
    request.render_process_id = 1;
    request.web_request_type = type;
    request.initiator = url::Origin::Create(GURL("chrome-untrusted://test/"));

    return WebRequestInfo(std::move(request));
  };

  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_resource_request(GURL("https:://example.com/a.jpg"),
                                  WebRequestResourceType::IMAGE)));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_resource_request(GURL("https:://example.com/a.mp4"),
                                  WebRequestResourceType::MEDIA)));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_resource_request(GURL("https:://example.com/xhr"),
                                  WebRequestResourceType::XHR)));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_resource_request(GURL("https:://example.com/a.js"),
                                  WebRequestResourceType::SCRIPT)));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_resource_request(GURL("https:://example.com/a.css"),
                                  WebRequestResourceType::STYLESHEET)));
}

// Tests that subframe navigation requests to Web origins initiated from
// chrome-untrusted:// pages can't be inspected.
TEST_F(ExtensionWebRequestPermissionsTest,
       CanNotAccessSubframeNavigationRequestsFromChromeUntrustedPage) {
  ExtensionsAPIClient api_client;
  auto* permission_helper = PermissionHelper::Get(browser_context());

  auto create_sub_frame_navigation_request = [](const GURL& url) {
    WebRequestInfoInitParams request;
    request.url = url;
    request.render_process_id = 1;
    request.web_request_type = WebRequestResourceType::SUB_FRAME;
    request.is_navigation_request = true;
    request.initiator = url::Origin::Create(GURL("chrome-untrusted://test/"));

    return WebRequestInfo(std::move(request));
  };

  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_sub_frame_navigation_request(GURL("https:://example.com/"))));
}

// Tests that main frame navigations to non-WebUI origins initiated from
// chrome-untrusted:// pages can be inspected.
TEST_F(ExtensionWebRequestPermissionsTest,
       CanAccessMainFrameNavigationsToWebOriginsFromChromeUntrustedPage) {
  ExtensionsAPIClient api_client;
  auto* permission_helper = PermissionHelper::Get(browser_context());

  auto create_main_frame_request_info = [](const GURL& url) {
    WebRequestInfoInitParams request;
    request.url = url;
    request.render_process_id = 1;
    request.web_request_type = WebRequestResourceType::MAIN_FRAME;
    request.is_navigation_request = true;
    request.initiator = url::Origin::Create(GURL("chrome-untrusted://test/"));

    return WebRequestInfo(std::move(request));
  };

  EXPECT_FALSE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_main_frame_request_info(GURL("https://example.com/"))));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_main_frame_request_info(GURL("chrome://version/"))));
  EXPECT_TRUE(WebRequestPermissions::HideRequest(
      permission_helper,
      create_main_frame_request_info(GURL("chrome-untrusted://test2/"))));
}

TEST_F(ExtensionWebRequestPermissionsTest,
       CanExtensionAccessURLWithWithheldPermissions) {
  ExtensionsAPIClient api_client;
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddHostPermission("<all_urls>").Build();
  URLPatternSet all_urls(
      {URLPattern(Extension::kValidHostPermissionSchemes, "<all_urls>")});
  // Simulate withholding the <all_urls> permission.
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), all_urls.Clone(),
          URLPatternSet()) /* withheld permissions */);

  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  auto get_access = [extension, this](
                        const GURL& url,
                        const std::optional<url::Origin>& initiator,
                        const WebRequestResourceType type) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL;
    return WebRequestPermissions::CanExtensionAccessURL(
        PermissionHelper::Get(browser_context()), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator, type);
  };

  const GURL example_com("https://example.com");
  const GURL chromium_org("https://chromium.org");
  const url::Origin example_com_origin(url::Origin::Create(example_com));
  const url::Origin chromium_org_origin(url::Origin::Create(chromium_org));

  GURL urls[] = {example_com, chromium_org};
  std::optional<url::Origin> initiators[] = {std::nullopt, example_com_origin,
                                             chromium_org_origin};
  WebRequestResourceType types[] = {WebRequestResourceType::OTHER,
                                    WebRequestResourceType::MAIN_FRAME};

  // With all permissions withheld, the result of any request should be
  // kWithheld.
  for (const auto& url : urls) {
    for (const auto& initiator : initiators) {
      for (const auto& type : types) {
        EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
                  get_access(url, initiator, type));
      }
    }
  }

  // Grant access to chromium.org.
  URLPatternSet chromium_org_patterns({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://chromium.org/*")});
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          std::move(chromium_org_patterns), URLPatternSet()),
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(), all_urls.Clone(),
                                      URLPatternSet()));

  // example.com isn't granted, so without an initiator or with an initiator
  // that the extension doesn't have access to, access is withheld.
  EXPECT_EQ(
      PermissionsData::PageAccess::kWithheld,
      get_access(example_com, std::nullopt, WebRequestResourceType::OTHER));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin,
                       WebRequestResourceType::MAIN_FRAME));

  // However, if a sub-resource request is made to example.com from an initiator
  // that the extension has access to, access is allowed. This is functionally
  // necessary for any extension with webRequest to work with the runtime host
  // permissions feature. See https://crbug.com/851722.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(example_com, chromium_org_origin,
                       WebRequestResourceType::OTHER));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin,
                       WebRequestResourceType::SUB_FRAME));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin,
                       WebRequestResourceType::MAIN_FRAME));

  // With access to the requested origin, access is always allowed for
  // REQUIRE_HOST_PERMISSION_FOR_URL, independent of initiator.
  for (const auto& initiator : initiators) {
    for (const auto& type : types) {
      EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
                get_access(chromium_org, initiator, type));
    }
  }
}

TEST_F(ExtensionWebRequestPermissionsTest,
       RequireAccessToURLAndInitiatorWithWithheldPermissions) {
  ExtensionsAPIClient api_client;

  const char* kGoogleCom = "https://google.com/";
  const char* kExampleCom = "https://example.com/";
  const char* kYahooCom = "https://yahoo.com";

  // Set up the extension to have access to kGoogleCom and withheld access to
  // kExampleCom.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext")
          .AddHostPermissions({kGoogleCom, kExampleCom})
          .Build();

  URLPatternSet kActivePatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kGoogleCom)});
  URLPatternSet kWithheldPatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kExampleCom)});

  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          kActivePatternSet.Clone(),
          kActivePatternSet.Clone()),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(),
          kWithheldPatternSet.Clone(),
          kWithheldPatternSet.Clone()) /* withheld permissions */);

  ExtensionRegistry::Get(browser_context())->AddEnabled(extension);

  auto get_access = [extension, this](
                        const GURL& url,
                        const std::optional<url::Origin>& initiator,
                        WebRequestResourceType type) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR;
    return WebRequestPermissions::CanExtensionAccessURL(
        PermissionHelper::Get(browser_context()), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator, type);
  };

  using PageAccess = PermissionsData::PageAccess;
  const GURL kAllowedUrl(kGoogleCom);
  const GURL kWithheldUrl(kExampleCom);
  const GURL kDeniedUrl(kYahooCom);
  const url::Origin kAllowedOrigin(url::Origin::Create(kAllowedUrl));
  const url::Origin kWithheldOrigin(url::Origin::Create(kWithheldUrl));
  const url::Origin kDeniedOrigin(url::Origin::Create(kDeniedUrl));
  const url::Origin kOpaqueOrigin;
  struct {
    std::optional<url::Origin> initiator;
    GURL url;
    PermissionsData::PageAccess expected_access_subresource;
    PermissionsData::PageAccess expected_access_navigation;
  } cases[] = {
      {std::nullopt, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
      {std::nullopt, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {std::nullopt, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kOpaqueOrigin, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
      {kOpaqueOrigin, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {kOpaqueOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kDeniedOrigin, kAllowedUrl, PageAccess::kDenied, PageAccess::kAllowed},
      {kDeniedOrigin, kWithheldUrl, PageAccess::kDenied, PageAccess::kWithheld},
      {kDeniedOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},
      {kAllowedOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},
      {kWithheldOrigin, kDeniedUrl, PageAccess::kDenied, PageAccess::kDenied},

      {kWithheldOrigin, kWithheldUrl, PageAccess::kWithheld,
       PageAccess::kWithheld},
      {kWithheldOrigin, kAllowedUrl, PageAccess::kWithheld,
       PageAccess::kAllowed},
      {kAllowedOrigin, kWithheldUrl, PageAccess::kAllowed,
       PageAccess::kWithheld},
      {kAllowedOrigin, kAllowedUrl, PageAccess::kAllowed, PageAccess::kAllowed},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf(
        "url-%s initiator-%s", test_case.url.spec().c_str(),
        test_case.initiator ? test_case.initiator->Serialize().c_str()
                            : "empty"));
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         WebRequestResourceType::OTHER),
              test_case.expected_access_subresource);
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         WebRequestResourceType::SUB_FRAME),
              test_case.expected_access_navigation);
    EXPECT_EQ(get_access(test_case.url, test_case.initiator,
                         WebRequestResourceType::MAIN_FRAME),
              test_case.expected_access_navigation);
  }
}

}  // namespace extensions
