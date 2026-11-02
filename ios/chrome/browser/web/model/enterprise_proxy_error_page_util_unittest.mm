// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/enterprise_proxy_error_page_util.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/ios/ns_error_util.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/enterprise/net/core/enterprise_proxy_error_data.h"
#import "components/enterprise/net/core/enterprise_proxy_error_service.h"
#import "components/enterprise/net/core/features.h"
#import "components/enterprise/net/core/mock_enterprise_proxy_service.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_error_service_factory_ios.h"
#import "ios/chrome/browser/enterprise/proxy/model/enterprise_proxy_service_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web/model/error_page_util.h"
#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"
#import "ios/net/protocol_handler_util.h"
#import "ios/web/public/test/error_test_util.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "net/http/http_status_code.h"
#import "net/log/net_log_with_source.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

constexpr char kTestUrl[] = "https://destination.example.com/";
constexpr char kProxyUrl[] = "https://proxy.example.com/";
// Navigation failed with the enterprise proxy error.
constexpr int64_t kNavigationId = 1;
// Any code works, as only the domain of the enterprise proxy error matters.
constexpr NSInteger kEnterpriseProxyErrorCode = 1;

// Returns an error of `domain` with `code` for the request to `kTestUrl`.
NSError* CreateTestUrlError(NSErrorDomain domain, NSInteger code) {
  return [NSError errorWithDomain:domain
                             code:code
                         userInfo:@{
                           NSURLErrorFailingURLErrorKey : [NSURL
                               URLWithString:base::SysUTF8ToNSString(kTestUrl)]
                         }];
}

std::unique_ptr<KeyedService> BuildMockEnterpriseProxyService(ProfileIOS*) {
  return std::make_unique<
      testing::NiceMock<enterprise_net::MockEnterpriseProxyService>>();
}

}  // namespace

// Fixture with the enterprise proxy services that the Secure Gateway
// interstitial needs.
class EnterpriseProxyErrorPageUtilTest : public PlatformTest {
 protected:
  EnterpriseProxyErrorPageUtilTest() {
    scoped_feature_list_.InitWithFeatures(
        {enterprise_net::kEnableDynamicRouteFetching,
         enterprise_net::kEnterpriseProxyErrorHandling},
        {});

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(EnterpriseProxyServiceFactoryIOS::GetInstance(),
                              base::BindOnce(&BuildMockEnterpriseProxyService));
    builder.AddTestingFactory(
        EnterpriseProxyErrorServiceFactoryIOS::GetInstance(),
        EnterpriseProxyErrorServiceFactoryIOS::GetDefaultFactory());
    profile_ = std::move(builder).Build();
    error_service_ =
        EnterpriseProxyErrorServiceFactoryIOS::GetForProfile(profile_.get());

    web_state_.SetBrowserState(profile_.get());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    security_interstitials::IOSBlockingPageTabHelper::CreateForWebState(
        &web_state_);
  }

  // Records a disguised error for `kNavigationId`.
  void RecordDisguisedError() {
    error_service_->RecordDisguisedError(
        kNavigationId,
        enterprise_net::EnterpriseProxyErrorData(
            GURL(kTestUrl), GURL(kProxyUrl), net::HTTP_BAD_GATEWAY,
            enterprise_net::EnterpriseProxyErrorData::ErrorCategory::kOther),
        net::NetLogWithSource());
  }

  // Returns the error page HTML for `kNavigationId`. As in the web layer, the
  // enterprise proxy error is appended as the final underlying error of the
  // error of the cancelled request.
  NSString* GetErrorPageHtml() {
    NSError* error = base::ios::ErrorWithAppendedUnderlyingError(
        web::testing::CreateTestNetError(
            CreateTestUrlError(NSURLErrorDomain, NSURLErrorCancelled)),
        [NSError errorWithDomain:kEnterpriseProxyErrorDomain
                            code:kEnterpriseProxyErrorCode
                        userInfo:nil]);
    return GetEnterpriseProxyErrorPageHtml(&web_state_, GURL(kTestUrl), error,
                                           /*is_post=*/false,
                                           /*is_off_the_record=*/false,
                                           kNavigationId);
  }

  // Returns the generic error page HTML for a failed tunnel connection.
  NSString* GetTunnelConnectionFailedPageHtml() {
    return GetErrorPage(GURL(kTestUrl),
                        CreateTestUrlError(net::kNSErrorDomain,
                                           net::ERR_TUNNEL_CONNECTION_FAILED),
                        /*is_post=*/false, /*is_off_the_record=*/false);
  }

  // Declared first so that the feature state is set before any other member
  // inspects it.
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<enterprise_net::EnterpriseProxyErrorService> error_service_ = nullptr;
  web::FakeWebState web_state_;
};

// Tests that the Secure Gateway interstitial is shown for the disguised error
// recorded for the navigation, and that the error is taken.
TEST_F(EnterpriseProxyErrorPageUtilTest, SecureGatewayPage) {
  RecordDisguisedError();

  NSString* page = GetErrorPageHtml();

  NSString* heading =
      l10n_util::GetNSString(IDS_ENTERPRISE_PROXY_OTHER_ERROR_HEADING);
  EXPECT_TRUE([page containsString:heading]) << base::SysNSStringToUTF8(page);
  EXPECT_FALSE(error_service_->TakeDisguisedError(kNavigationId).has_value());
}

// Tests that the generic error page for a failed tunnel connection is shown
// when no disguised error is recorded for the navigation.
TEST_F(EnterpriseProxyErrorPageUtilTest, GenericPageWithoutRecordedError) {
  EXPECT_NSEQ(GetTunnelConnectionFailedPageHtml(), GetErrorPageHtml());
}

// Tests that the generic error page for a failed tunnel connection is shown
// when enterprise proxy error handling is disabled, as the Secure Gateway
// interstitial has no parameters then, and that the disguised error recorded
// for the navigation is still taken.
TEST_F(EnterpriseProxyErrorPageUtilTest, GenericPageWithErrorHandlingDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      enterprise_net::kEnterpriseProxyErrorHandling);
  RecordDisguisedError();

  EXPECT_NSEQ(GetTunnelConnectionFailedPageHtml(), GetErrorPageHtml());
  EXPECT_FALSE(error_service_->TakeDisguisedError(kNavigationId).has_value());
}
