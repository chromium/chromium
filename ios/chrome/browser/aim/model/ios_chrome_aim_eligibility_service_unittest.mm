// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service.h"

#import <memory>

#import "base/memory/raw_ptr.h"
#import "base/memory/scoped_refptr.h"
#import "base/test/metrics/histogram_tester.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/identity_test_environment.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/testing_application_context.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/http_status_code.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/omnibox_proto/aim_eligibility_response.pb.h"

namespace {

class TestIOSChromeAimEligibilityService
    : public IOSChromeAimEligibilityService {
 public:
  using IOSChromeAimEligibilityService::GetLocale;
  using IOSChromeAimEligibilityService::IOSChromeAimEligibilityService;
};

class IOSChromeAimEligibilityServiceTest : public PlatformTest {
 protected:
  IOSChromeAimEligibilityServiceTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    profile_ = std::move(builder).Build();

    template_url_service_ =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service_->Load();

    TemplateURLData data;
    data.SetShortName(u"Test");
    data.SetKeyword(u"test");
    data.SetURL("https://www.google.com/search?q={searchTerms}");
    TemplateURL* template_url =
        template_url_service_->Add(std::make_unique<TemplateURL>(data));
    template_url_service_->SetUserSelectedDefaultSearchProvider(template_url);
  }

  void SetUp() override {
    PlatformTest::SetUp();
    TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
        "en-US");
    service_ = std::make_unique<TestIOSChromeAimEligibilityService>(
        profile_->GetPrefs(), template_url_service_, shared_url_loader_factory_,
        identity_test_env_.identity_manager(),
        AimEligibilityService::Configuration());
  }

  void TearDown() override {
    service_.reset();
    test_url_loader_factory_.pending_requests()->clear();
    TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
        "en-US");
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  std::unique_ptr<TestProfileIOS> profile_;
  raw_ptr<TemplateURLService> template_url_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<TestIOSChromeAimEligibilityService> service_;
};

// Tests that GetLocale retrieves the current application locale.
TEST_F(IOSChromeAimEligibilityServiceTest, GetLocaleReturnsCurrentLocale) {
  EXPECT_EQ(service_->GetLocale(), "en-US");
}

// Tests that GetLocale returns en-US when ShouldIgnoreDeviceLocaleConditions is
// set.
TEST_F(IOSChromeAimEligibilityServiceTest,
       GetLocaleIgnoresDeviceLocaleWhenFlagEnabled) {
  [[NSUserDefaults standardUserDefaults]
      setBool:YES
       forKey:@"IgnoreDeviceLocaleConditions"];
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "fr-FR");

  EXPECT_EQ(service_->GetLocale(), "en-US");

  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:@"IgnoreDeviceLocaleConditions"];
}

// Tests that changing the application locale triggers an eligibility fetch.
TEST_F(IOSChromeAimEligibilityServiceTest, FetchEligibilityOnLocaleChange) {
  base::HistogramTester histogram_tester;
  omnibox::AimEligibilityResponse response;
  response.set_is_eligible(true);

  // Clear startup request.
  test_url_loader_factory_.pending_requests()->clear();

  // Changing application locale in storage.
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "fr-FR");

  EXPECT_EQ(service_->GetLocale(), "fr-FR");
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);

  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  std::string response_string;
  response.SerializeToString(&response_string);
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      request.url.spec(), response_string, net::HTTP_OK);

  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.LocaleChange.is_eligible",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Omnibox.AimEligibility.EligibilityResponse.is_eligible", true, 1);
}

// Tests that GetLocale falls back to a non-empty fallback when storage is
// empty.
TEST_F(IOSChromeAimEligibilityServiceTest, GetLocaleFallsBackWhenStorageEmpty) {
  test_url_loader_factory_.pending_requests()->clear();
  TestingApplicationContext::GetGlobal()->GetApplicationLocaleStorage()->Set(
      "");
  std::string fallback_locale = service_->GetLocale();
  EXPECT_FALSE(fallback_locale.empty());
  EXPECT_EQ(fallback_locale.find('_'), std::string::npos);
}

}  // namespace
