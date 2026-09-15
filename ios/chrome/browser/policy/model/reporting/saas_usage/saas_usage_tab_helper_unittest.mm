// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_tab_helper.h"

#import <string>
#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/test_future.h"
#import "components/enterprise/browser/reporting/reporting_features.cc"
#import "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_encryption_protocol_provider.h"
#import "ios/chrome/browser/policy/model/reporting/saas_usage/saas_usage_reporting_controller_factory_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace enterprise_reporting {

namespace {

class MockSaasUsageReportingController : public SaasUsageReportingController {
 public:
  explicit MockSaasUsageReportingController(PrefService* pref_service)
      : SaasUsageReportingController(
            /*local_state_pref_service=*/pref_service,
            /*profile_pref_service=*/pref_service,
            std::make_unique<PrefURLListMatcher>(pref_service, "test_pref"),
            std::make_unique<PrefURLListMatcher>(pref_service, "test_pref")) {}
  MOCK_METHOD(void,
              RecordNavigation,
              (const SaasUsageReportingController::NavigationDataDelegate&),
              (const, override));
};

class FakeProber : public SaasUsageEncryptionProtocolProvider::Prober {
 public:
  FakeProber() = default;
  ~FakeProber() override = default;

  void SetProtocol(std::string protocol) { protocol_ = std::move(protocol); }

  void ProbeUrl(const GURL& url,
                SaasUsageEncryptionProtocolProvider::EncryptionProtocolCallback
                    callback) override {
    std::move(callback).Run(protocol_);
  }

 private:
  std::string protocol_ = "TLS 1.3";
};

std::string GetEncryptionProtocol(
    const SaasUsageReportingController::NavigationDataDelegate& delegate) {
  base::test::TestFuture<std::string_view> result;
  delegate.GetEncryptionProtocol(result.GetCallback());
  return std::string(result.Get());
}

auto SaasUsageNavigationMatcher(
    const testing::Matcher<GURL>& url,
    const testing::Matcher<std::string>& encryption_protocol) {
  return testing::AllOf(
      testing::Property(
          &SaasUsageReportingController::NavigationDataDelegate::GetUrl, url),
      testing::ResultOf(&GetEncryptionProtocol, encryption_protocol));
}

}  // namespace

class SaasUsageTabHelperTest : public PlatformTest {
 protected:
  SaasUsageTabHelperTest() : PlatformTest() {
    feature_list_.InitAndEnableFeature(kSaasUsageReporting);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    testing_pref_service_.registry()->RegisterListPref("test_pref");

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        SaasUsageReportingControllerFactoryIOS::GetInstance(),
        base::BindRepeating(&SaasUsageTabHelperTest::BuildMockController,
                            base::Unretained(this)));
    profile_ = std::move(builder).Build();

    web_state_.SetBrowserState(profile_.get());
    auto fake_prober = std::make_unique<FakeProber>();
    fake_prober_ = fake_prober.get();
    SaasUsageEncryptionProtocolProvider::GetInstance().SetProberForTesting(
        std::move(fake_prober));
    auto* controller =
        SaasUsageReportingControllerFactoryIOS::GetForProfile(profile_.get());
    SaasUsageTabHelper::CreateForWebState(&web_state_, controller);
  }

  void TearDown() override {
    fake_prober_ = nullptr;
    SaasUsageEncryptionProtocolProvider::GetInstance().SetProberForTesting(
        nullptr);
    PlatformTest::TearDown();
  }

  std::unique_ptr<KeyedService> BuildMockController(ProfileIOS* profile) {
    auto controller = std::make_unique<MockSaasUsageReportingController>(
        &testing_pref_service_);
    mock_controller_ = controller.get();
    return controller;
  }

  MockSaasUsageReportingController* mock_controller() {
    return mock_controller_;
  }

  FakeProber* fake_prober() { return fake_prober_; }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  TestingPrefServiceSimple testing_pref_service_;
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<FakeProber> fake_prober_ = nullptr;
  web::FakeWebState web_state_;
  raw_ptr<MockSaasUsageReportingController> mock_controller_ = nullptr;
};

TEST_F(SaasUsageTabHelperTest, ReportNavigation) {
  GURL test_url("https://example.com/app");
  fake_prober()->SetProtocol("TLS 1.3");

  EXPECT_CALL(*mock_controller(),
              RecordNavigation(SaasUsageNavigationMatcher(test_url, "TLS 1.3")))
      .Times(1);

  web::FakeNavigationContext context;
  context.SetUrl(test_url);
  context.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context);
}

TEST_F(SaasUsageTabHelperTest, ReportReload) {
  GURL test_url("https://example.com/app");
  fake_prober()->SetProtocol("TLS 1.3");

  EXPECT_CALL(*mock_controller(),
              RecordNavigation(SaasUsageNavigationMatcher(test_url, "TLS 1.3")))
      .Times(2);

  web::FakeNavigationContext context1;
  context1.SetUrl(test_url);
  context1.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context1);

  web::FakeNavigationContext context2;
  context2.SetUrl(test_url);
  context2.SetHasCommitted(true);
  web_state_.OnNavigationFinished(&context2);
}

TEST_F(SaasUsageTabHelperTest, DoNotReportSameDocumentNavigation) {
  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/#section"));
  context.SetHasCommitted(true);
  context.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&context);
}

TEST_F(SaasUsageTabHelperTest, DoNotReportErrorNavigation) {
  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/error"));
  context.SetHasCommitted(true);
  context.SetError([NSError errorWithDomain:@"test" code:-1 userInfo:nil]);
  web_state_.OnNavigationFinished(&context);
}

TEST_F(SaasUsageTabHelperTest, DoNotReportUncommittedNavigation) {
  EXPECT_CALL(*mock_controller(), RecordNavigation).Times(0);

  web::FakeNavigationContext context;
  context.SetUrl(GURL("https://example.com/notcommitted"));
  context.SetHasCommitted(false);
  web_state_.OnNavigationFinished(&context);
}

TEST_F(SaasUsageTabHelperTest, DoNotReportForIncognito) {
  ProfileIOS* otr_profile = profile_->GetOffTheRecordProfile();
  EXPECT_EQ(SaasUsageReportingControllerFactoryIOS::GetForProfile(otr_profile),
            nullptr);
}

}  // namespace enterprise_reporting
