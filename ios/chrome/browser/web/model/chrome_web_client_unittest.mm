// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/chrome_web_client.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/command_line.h"
#import "base/run_loop.h"
#import "base/strings/string_split.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/captive_portal/core/captive_portal_detector.h"
#import "components/content_settings/core/browser/host_content_settings_map.h"
#import "components/lookalikes/core/lookalike_url_util.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_settings/model/host_content_settings_map_factory.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/safe_browsing/model/safe_browsing_blocking_page.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/ssl/model/captive_portal_tab_helper.h"
#import "ios/chrome/browser/web/model/error_page_util.h"
#import "ios/chrome/browser/web/model/features.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_container.h"
#import "ios/components/security_interstitials/https_only_mode/https_only_mode_error.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/common/features.h"
#import "ios/web/common/web_view_creation_util.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/js_test_util.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "net/http/http_status_code.h"
#import "net/ssl/ssl_info.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
const char kTestUrl[] = "http://chromium.test";

// Error used to test PrepareErrorPage method.
NSError* CreateTestError() {
  return web::testing::CreateTestNetError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorNetworkConnectionLost
             userInfo:@{
               NSURLErrorFailingURLStringErrorKey :
                   base::SysUTF8ToNSString(kTestUrl)
             }]);
}
}  // namespace

class ChromeWebClientTest : public PlatformTest {
 public:
  ChromeWebClientTest() { profile_ = TestProfileIOS::Builder().Build(); }

  ChromeWebClientTest(const ChromeWebClientTest&) = delete;
  ChromeWebClientTest& operator=(const ChromeWebClientTest&) = delete;

  ~ChromeWebClientTest() override = default;

  ProfileIOS* profile() { return profile_.get(); }

 protected:
  web::WebTaskEnvironment environment_{
      web::WebTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(ChromeWebClientTest, UserAgent) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  ChromeWebClient web_client;
  std::string buffer = web_client.GetUserAgent(web::UserAgentType::MOBILE);

  pieces = base::SplitStringUsingSubstr(
      buffer, "Mozilla/5.0 (", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  pieces = base::SplitStringUsingSubstr(
      buffer, ") AppleWebKit/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  pieces =
      base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ",
                                   base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  pieces = base::SplitStringUsingSubstr(
      buffer, " Safari/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  // Not sure what can be done to better check the OS string, since it's highly
  // platform-dependent.
  EXPECT_FALSE(os_str.empty());

  EXPECT_FALSE(webkit_version_str.empty());
  EXPECT_FALSE(safari_version_str.empty());

  EXPECT_EQ(0u, product_str.find("CriOS/"));
}

// Tests PrepareErrorPage wth non-post, not Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPageNonPostNonOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::FakeWebState web_state;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/std::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/false,
                           /*is_off_the_record=*/false),
              page);
}

// Tests PrepareErrorPage with post, not Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPagePostNonOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::FakeWebState web_state;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/true,
                              /*is_off_the_record=*/false,
                              /*info=*/std::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/true,
                           /*is_off_the_record=*/false),
              page);
}

// Tests PrepareErrorPage with non-post, Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPageNonPostOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::FakeWebState web_state;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/true,
                              /*info=*/std::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/false,
                           /*is_off_the_record=*/true),
              page);
}

// Tests PrepareErrorPage with post, Off The Record error.
TEST_F(ChromeWebClientTest, PrepareErrorPagePostOtr) {
  ChromeWebClient web_client;
  NSError* error = CreateTestError();
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::FakeWebState web_state;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/true,
                              /*is_off_the_record=*/true,
                              /*info=*/std::nullopt,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  EXPECT_NSEQ(GetErrorPage(GURL(kTestUrl), error, /*is_post=*/true,
                           /*is_off_the_record=*/true),
              page);
}

// Tests PrepareErrorPage with SSLInfo, which results in an SSL committed
// interstitial.
TEST_F(ChromeWebClientTest, PrepareErrorPageWithSSLInfo) {
  net::SSLInfo info;
  info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  info.is_fatal_cert_error = false;
  info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  std::optional<net::SSLInfo> ssl_info = info;
  ChromeWebClient web_client;
  NSError* error =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });
  web::FakeWebState web_state;
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state);

  // Use a test URLLoaderFactory so that the captive portal detector doesn't
  // make an actual network request.
  network::TestURLLoaderFactory test_loader_factory;
  test_loader_factory.AddResponse(
      captive_portal::CaptivePortalDetector::kDefaultURL, "",
      net::HTTP_NO_CONTENT);
  profile_->SetSharedURLLoaderFactory(
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_loader_factory));

  CaptivePortalTabHelper::GetOrCreateForWebState(&web_state);
  web_state.SetBrowserState(profile());
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/ssl_info,
                              /*navigation_id=*/0, std::move(callback));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return callback_called;
  }));
  NSString* error_string = base::SysUTF8ToNSString(
      net::ErrorToShortString(net::ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE([page containsString:error_string]);
}

// Tests PrepareErrorPage for a safe browsing error, which results in a
// committed safe browsing interstitial.
TEST_F(ChromeWebClientTest, PrepareErrorPageForSafeBrowsingError) {
  // Store an unsafe resource in `web_state`'s container.
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile());
  SafeBrowsingUrlAllowList::CreateForWebState(&web_state);
  SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state);
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state);

  security_interstitials::UnsafeResource resource;
  resource.threat_type =
      safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
  resource.url = GURL("http://www.chromium.test");
  resource.weak_web_state = web_state.GetWeakPtr();
  // Added to ensure that `threat_source` isn't considered UNKNOWN in this case.
  resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;
  SafeBrowsingUrlAllowList::FromWebState(&web_state)
      ->AddPendingUnsafeNavigationDecision(resource.url, resource.threat_type);
  SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state)
      ->StoreMainFrameUnsafeResource(resource);

  NSError* error = [NSError errorWithDomain:kSafeBrowsingErrorDomain
                                       code:kUnsafeResourceErrorCode
                                   userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });

  ChromeWebClient web_client;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/std::optional<net::SSLInfo>(),
                              /*navigation_id=*/0, std::move(callback));

  EXPECT_TRUE(callback_called);
  NSString* error_string = l10n_util::GetNSString(IDS_SAFEBROWSING_HEADING);
  EXPECT_TRUE([page containsString:error_string]);
}

// Tests PrepareErrorPage for a lookalike error, which results in a
// committed lookalike interstitial.
TEST_F(ChromeWebClientTest, PrepareErrorPageForLookalikeUrlError) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile());
  LookalikeUrlContainer::CreateForWebState(&web_state);
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state);
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web_state.SetNavigationManager(std::move(navigation_manager));

  LookalikeUrlContainer::FromWebState(&web_state)
      ->SetLookalikeUrlInfo(
          GURL("https://www.safe.test"), GURL(kTestUrl),
          lookalikes::LookalikeUrlMatchType::kSkeletonMatchTop5k);

  NSError* error = [NSError errorWithDomain:kLookalikeUrlErrorDomain
                                       code:kLookalikeUrlErrorCode
                                   userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });

  ChromeWebClient web_client;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/std::optional<net::SSLInfo>(),
                              /*navigation_id=*/0, std::move(callback));

  EXPECT_TRUE(callback_called);
  NSString* error_string =
      l10n_util::GetNSString(IDS_LOOKALIKE_URL_PRIMARY_PARAGRAPH);
  EXPECT_TRUE([page containsString:error_string])
      << base::SysNSStringToUTF8(page);
}

// Tests PrepareErrorPage for a lookalike error with no suggested URL,
// which results in a committed lookalike interstitial that has a 'Close page'
// button instead of 'Back to safety' (when there is no back item).
TEST_F(ChromeWebClientTest, PrepareErrorPageForLookalikeUrlErrorNoSuggestion) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile());
  LookalikeUrlContainer::CreateForWebState(&web_state);
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state);
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web_state.SetNavigationManager(std::move(navigation_manager));

  LookalikeUrlContainer::FromWebState(&web_state)
      ->SetLookalikeUrlInfo(
          GURL(""), GURL(kTestUrl),
          lookalikes::LookalikeUrlMatchType::kSkeletonMatchTop5k);

  NSError* error = [NSError errorWithDomain:kLookalikeUrlErrorDomain
                                       code:kLookalikeUrlErrorCode
                                   userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });

  ChromeWebClient web_client;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/std::optional<net::SSLInfo>(),
                              /*navigation_id=*/0, std::move(callback));

  EXPECT_TRUE(callback_called);
  NSString* close_page_string =
      l10n_util::GetNSString(IDS_LOOKALIKE_URL_CLOSE_PAGE);
  NSString* back_to_safety_string =
      l10n_util::GetNSString(IDS_LOOKALIKE_URL_BACK_TO_SAFETY);
  EXPECT_TRUE([page containsString:close_page_string])
      << base::SysNSStringToUTF8(page);
  EXPECT_FALSE([page containsString:back_to_safety_string])
      << base::SysNSStringToUTF8(page);
}

// Tests PrepareErrorPage for a HTTPS-Only Mode error, which results in a
// committed HTTPS-Only Mode interstitial that has a 'Go back'.
TEST_F(ChromeWebClientTest, PrepareErrorPageForHttpsOnlyModeError) {
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile());
  HttpsOnlyModeContainer::CreateForWebState(&web_state);
  security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
      &web_state);
  auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
  web_state.SetNavigationManager(std::move(navigation_manager));

  HttpsOnlyModeContainer::FromWebState(&web_state)->SetHttpUrl(GURL(kTestUrl));

  NSError* error = [NSError errorWithDomain:kHttpsOnlyModeErrorDomain
                                       code:kHttpsOnlyModeErrorCode
                                   userInfo:nil];
  __block bool callback_called = false;
  __block NSString* page = nil;
  base::OnceCallback<void(NSString*)> callback =
      base::BindOnce(^(NSString* error_html) {
        callback_called = true;
        page = error_html;
      });

  ChromeWebClient web_client;
  web_client.PrepareErrorPage(&web_state, GURL(kTestUrl), error,
                              /*is_post=*/false,
                              /*is_off_the_record=*/false,
                              /*info=*/std::optional<net::SSLInfo>(),
                              /*navigation_id=*/0, std::move(callback));

  EXPECT_TRUE(callback_called);
  NSString* back_to_safety_string =
      l10n_util::GetNSString(IDS_HTTPS_ONLY_MODE_BACK_BUTTON);
  EXPECT_TRUE([page containsString:back_to_safety_string])
      << base::SysNSStringToUTF8(page);
}

// Tests the default user agent for different views.
TEST_F(ChromeWebClientTest, DefaultUserAgent) {
  ChromeWebClient web_client;
  web::FakeWebState web_state;
  web_state.SetBrowserState(profile());

  scoped_refptr<HostContentSettingsMap> settings_map(
      ios::HostContentSettingsMapFactory::GetForProfile(profile()));
  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REQUEST_DESKTOP_SITE, CONTENT_SETTING_BLOCK);

  EXPECT_EQ(web::UserAgentType::MOBILE,
            web_client.GetDefaultUserAgent(&web_state, GURL()));

  settings_map->SetContentSettingCustomScope(
      ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
      ContentSettingsType::REQUEST_DESKTOP_SITE, CONTENT_SETTING_ALLOW);

  EXPECT_EQ(web::UserAgentType::DESKTOP,
            web_client.GetDefaultUserAgent(&web_state, GURL()));
}

// Tests if two online URLs are correctly processed.
TEST_F(ChromeWebClientTest, IsPointingToSameDocumentOnline) {
  ChromeWebClient web_client;
  GURL same_url1 = GURL("http://chromium.org/foo");
  GURL same_url2 = GURL("http://chromium.org/foo");

  EXPECT_TRUE(web_client.IsPointingToSameDocument(same_url1, same_url2));

  GURL different_url1 = GURL("http://chromium.org/foo");
  GURL different_url2 = GURL("http://chromium.org/bar");

  EXPECT_FALSE(
      web_client.IsPointingToSameDocument(different_url1, different_url2));
}

// Tests if one online URL and one offline reload URL are correctly processed.
TEST_F(ChromeWebClientTest, IsPointingToSameDocumentOnlineOfflineReload) {
  ChromeWebClient web_client;
  GURL same_url1 = GURL("http://chromium.org/foo");
  GURL same_url2 =
      reading_list::OfflineReloadURLForURL(GURL("http://chromium.org/foo"));

  EXPECT_TRUE(web_client.IsPointingToSameDocument(same_url1, same_url2));

  GURL different_url1 = GURL("http://chromium.org/foo");
  GURL different_url2 =
      reading_list::OfflineReloadURLForURL(GURL("http://chromium.org/bar"));

  EXPECT_FALSE(
      web_client.IsPointingToSameDocument(different_url1, different_url2));
}

// Tests if one online URL and one offline Entry URL are correctly processed.
TEST_F(ChromeWebClientTest, IsPointingToSameDocumentOnlineOfflineEntry) {
  ChromeWebClient web_client;
  GURL same_url1 = GURL("http://chromium.org/foo");
  GURL same_url2 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));

  EXPECT_TRUE(web_client.IsPointingToSameDocument(same_url1, same_url2));

  GURL different_url1 = GURL("http://chromium.org/foo");
  GURL different_url2 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/bar"));

  EXPECT_FALSE(
      web_client.IsPointingToSameDocument(different_url1, different_url2));
}

// Tests if two offline URLs are correctly processed.
TEST_F(ChromeWebClientTest, IsPointingToSameDocumentOfflineEntry) {
  ChromeWebClient web_client;
  GURL same_url1 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));
  GURL same_url2 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));

  EXPECT_TRUE(web_client.IsPointingToSameDocument(same_url1, same_url2));

  GURL different_url1 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));
  GURL different_url2 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/bar"));

  EXPECT_FALSE(
      web_client.IsPointingToSameDocument(different_url1, different_url2));

  GURL same_url3 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));
  GURL same_url4 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));

  EXPECT_TRUE(web_client.IsPointingToSameDocument(same_url3, same_url4));

  GURL different_url3 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));
  GURL different_url4 =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/bar"));

  EXPECT_FALSE(
      web_client.IsPointingToSameDocument(different_url3, different_url4));
}

// Tests if URLs with one empty is working as expected.
TEST_F(ChromeWebClientTest, IsPointingToSameDocumentEmpty) {
  ChromeWebClient web_client;
  GURL offline_url =
      reading_list::OfflineURLForURL(GURL("http://chromium.org/foo"));
  GURL online_url = GURL("http://chromium.org/foo");

  EXPECT_FALSE(web_client.IsPointingToSameDocument(GURL(), offline_url));
  EXPECT_FALSE(web_client.IsPointingToSameDocument(GURL(), online_url));
}
