// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_permissions.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

TEST(ExtensionWebRequestPermissions, TestHideRequestForURL) {
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
  auto info_map = base::MakeRefCounted<extensions::InfoMap>();

  struct TestCase {
    const char* url;
    int expected_hide_request_mask;
  } cases[] = {
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
      // Unsupported scheme.
      {"blob:https://chrome.google.com/fc3f440b-78ed-469f-8af8-7a1717ff39ae",
       HIDE_ALL},
      {"notregisteredscheme://www.foobar.com", HIDE_ALL},
      {"https://chrome.google.com:80/webstore", HIDE_ALL},
      {"https://chrome.google.com/webstore?query", HIDE_ALL},
      {"http://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://clients2.google.com/service/update2/crx", HIDE_ALL},
      {"https://chrome.google.com/webstore/inlineinstall/detail/"
       "kcnhkahnjcbndmmehfkdnkjomaanaooo",
       HIDE_ALL},
  };
  const int kRendererProcessId = 1;
  const int kBrowserProcessId = -1;

  // Returns a WebRequestInfo instance constructed as per the given parameters.
  auto create_request = [](const GURL& url, content::ResourceType type,
                           int render_process_id) {
    WebRequestInfo request;
    request.url = url;
    request.type = type;
    request.render_process_id = render_process_id;

    request.web_request_type = ToWebRequestResourceType(type);
    request.is_browser_side_navigation =
        type == content::RESOURCE_TYPE_MAIN_FRAME ||
        type == content::RESOURCE_TYPE_SUB_FRAME;
    return request;
  };

  for (const TestCase& test_case : cases) {
    SCOPED_TRACE(test_case.url);

    GURL request_url(test_case.url);
    ASSERT_TRUE(request_url.is_valid());

    {
      SCOPED_TRACE("Renderer initiated sub-resource request");
      WebRequestInfo request = create_request(
          request_url, content::RESOURCE_TYPE_SUB_RESOURCE, kRendererProcessId);
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_RENDERER_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Browser initiated sub-resource request");
      WebRequestInfo request = create_request(
          request_url, content::RESOURCE_TYPE_SUB_RESOURCE, kBrowserProcessId);
      bool expect_hidden = test_case.expected_hide_request_mask &
                           HIDE_BROWSER_SUB_RESOURCE_REQUEST;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Main-frame navigation");
      WebRequestInfo request = create_request(
          request_url, content::RESOURCE_TYPE_MAIN_FRAME, kBrowserProcessId);
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_MAIN_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }

    {
      SCOPED_TRACE("Sub-frame navigation");
      WebRequestInfo request = create_request(
          request_url, content::RESOURCE_TYPE_SUB_FRAME, kBrowserProcessId);
      bool expect_hidden =
          test_case.expected_hide_request_mask & HIDE_SUB_FRAME_NAVIGATION;
      EXPECT_EQ(expect_hidden,
                WebRequestPermissions::HideRequest(info_map.get(), request));
    }
  }

  // Check protection of requests originating from the frame showing the Chrome
  // WebStore. Normally this request is not protected:
  GURL non_sensitive_url("http://www.google.com/test.js");
  WebRequestInfo non_sensitive_request_info = create_request(
      non_sensitive_url, content::RESOURCE_TYPE_SCRIPT, kRendererProcessId);
  EXPECT_FALSE(WebRequestPermissions::HideRequest(info_map.get(),
                                                  non_sensitive_request_info));

  // If the origin is labeled by the WebStoreAppId, it becomes protected.
  {
    const int kWebstoreProcessId = 42;
    const int kSiteInstanceId = 23;
    info_map->RegisterExtensionProcess(extensions::kWebStoreAppId,
                                       kWebstoreProcessId, kSiteInstanceId);
    WebRequestInfo sensitive_request_info = create_request(
        non_sensitive_url, content::RESOURCE_TYPE_SCRIPT, kWebstoreProcessId);
    EXPECT_TRUE(WebRequestPermissions::HideRequest(info_map.get(),
                                                   sensitive_request_info));
  }

  // Check that a request for a non-sensitive URL is rejected if it's a PAC
  // script fetch.
  non_sensitive_request_info.is_pac_request = true;
  EXPECT_TRUE(WebRequestPermissions::HideRequest(info_map.get(),
                                                 non_sensitive_request_info));
}

TEST(ExtensionWebRequestPermissions,
     CanExtensionAccessURLWithWithheldPermissions) {
  // The InfoMap requires methods to be called on the IO thread. Fake it.
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddPermission("<all_urls>").Build();
  URLPatternSet all_urls(
      {URLPattern(Extension::kValidHostPermissionSchemes, "<all_urls>")});
  // Simulate withholding the <all_urls> permission.
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), all_urls,
          URLPatternSet()) /* withheld permissions */);

  scoped_refptr<InfoMap> info_map = base::MakeRefCounted<InfoMap>();
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  auto get_access = [extension, info_map](
                        const GURL& url,
                        base::Optional<url::Origin> initiator) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL;
    return WebRequestPermissions::CanExtensionAccessURL(
        info_map.get(), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator);
  };

  const GURL example_com("https://example.com");
  const GURL chromium_org("https://chromium.org");
  const url::Origin example_com_origin(url::Origin::Create(example_com));
  const url::Origin chromium_org_origin(url::Origin::Create(chromium_org));

  // With all permissions withheld, the result of any request should be
  // kWithheld.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, chromium_org_origin));

  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, chromium_org_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(chromium_org, example_com_origin));

  // Grant access to chromium.org.
  URLPatternSet chromium_org_patterns({URLPattern(
      Extension::kValidHostPermissionSchemes, "https://chromium.org/*")});
  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(),
                                      chromium_org_patterns, URLPatternSet()),
      std::make_unique<PermissionSet>(APIPermissionSet(),
                                      ManifestPermissionSet(), all_urls,
                                      URLPatternSet()));

  // example.com isn't granted, so without an initiator or with an initiator
  // that the extension doesn't have access to, access is withheld.
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kWithheld,
            get_access(example_com, example_com_origin));

  // However, if a request is made to example.com from an initiator that the
  // extension has access to, access is allowed. This is functionally necessary
  // for any extension with webRequest to work with the runtime host permissions
  // feature. See https://crbug.com/851722.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(example_com, chromium_org_origin));

  // With access to the requested origin, access is always allowed, independent
  // of initiator.
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, base::nullopt));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, chromium_org_origin));
  EXPECT_EQ(PermissionsData::PageAccess::kAllowed,
            get_access(chromium_org, example_com_origin));
}

TEST(ExtensionWebRequestPermissions,
     RequireAccessToURLAndInitiatorWithWithheldPermissions) {
  // The InfoMap requires methods to be called on the IO thread. Fake it.
  content::TestBrowserThreadBundle thread_bundle(
      content::TestBrowserThreadBundle::IO_MAINLOOP);
  const char* kGoogleCom = "https://google.com/";
  const char* kExampleCom = "https://example.com/";
  const char* kYahooCom = "https://yahoo.com";

  // Set up the extension to have access to kGoogleCom and withheld access to
  // kExampleCom.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("ext").AddPermissions({kGoogleCom, kExampleCom}).Build();

  URLPatternSet kActivePatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kGoogleCom)});
  URLPatternSet kWithheldPatternSet(
      {URLPattern(Extension::kValidHostPermissionSchemes, kExampleCom)});

  extension->permissions_data()->SetPermissions(
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), kActivePatternSet,
          kActivePatternSet),  // active permissions.
      std::make_unique<PermissionSet>(
          APIPermissionSet(), ManifestPermissionSet(), kWithheldPatternSet,
          kWithheldPatternSet) /* withheld permissions */);

  scoped_refptr<InfoMap> info_map = base::MakeRefCounted<InfoMap>();
  info_map->AddExtension(extension.get(), base::Time(), false, false);

  auto get_access = [extension, info_map](
                        const GURL& url,
                        base::Optional<url::Origin> initiator) {
    constexpr int kTabId = 42;
    constexpr WebRequestPermissions::HostPermissionsCheck kPermissionsCheck =
        WebRequestPermissions::REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR;
    return WebRequestPermissions::CanExtensionAccessURL(
        info_map.get(), extension->id(), url, kTabId,
        false /* crosses incognito */, kPermissionsCheck, initiator);
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
    base::Optional<url::Origin> initiator;
    GURL url;
    PermissionsData::PageAccess expected_access;
  } cases[] = {
      {base::nullopt, kAllowedUrl, PageAccess::kAllowed},
      {base::nullopt, kWithheldUrl, PageAccess::kWithheld},
      {base::nullopt, kDeniedUrl, PageAccess::kDenied},

      {kOpaqueOrigin, kAllowedUrl, PageAccess::kAllowed},
      {kOpaqueOrigin, kWithheldUrl, PageAccess::kWithheld},
      {kOpaqueOrigin, kDeniedUrl, PageAccess::kDenied},

      {kDeniedOrigin, kAllowedUrl, PageAccess::kDenied},
      {kDeniedOrigin, kWithheldUrl, PageAccess::kDenied},
      {kDeniedOrigin, kDeniedUrl, PageAccess::kDenied},
      {kAllowedOrigin, kDeniedUrl, PageAccess::kDenied},
      {kWithheldOrigin, kDeniedUrl, PageAccess::kDenied},

      {kWithheldOrigin, kWithheldUrl, PageAccess::kWithheld},
      {kWithheldOrigin, kAllowedUrl, PageAccess::kWithheld},
      {kAllowedOrigin, kWithheldUrl, PageAccess::kAllowed},
      {kAllowedOrigin, kAllowedUrl, PageAccess::kAllowed},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(base::StringPrintf(
        "url-%s initiator-%s", test_case.url.spec().c_str(),
        test_case.initiator ? test_case.initiator->Serialize().c_str()
                            : "empty"));
    EXPECT_EQ(get_access(test_case.url, test_case.initiator),
              test_case.expected_access);
  }
}

}  // namespace extensions
