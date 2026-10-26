// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/aim/model/ios_chrome_aim_eligibility_service_factory.h"

#import <memory>
#import <optional>
#import <string>

#import "base/functional/bind.h"
#import "base/strings/strcat.h"
#import "base/test/scoped_feature_list.h"
#import "build/branding_buildflags.h"
#import "components/contextual_tasks/public/features.h"
#import "components/omnibox/browser/aim_eligibility_service.h"
#import "components/omnibox/browser/aim_eligibility_service_features.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "components/variations/scoped_variations_ids_provider.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/http/structured_headers.h"
#import "services/network/public/cpp/resource_request.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using ::testing::HasSubstr;

class IOSChromeAimEligibilityServiceFactoryTest : public PlatformTest {
 protected:
  void BuildProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeAimEligibilityServiceFactory::GetInstance(),
        base::BindOnce(
            [](scoped_refptr<network::SharedURLLoaderFactory>
                   shared_url_loader_factory,
               ProfileIOS* profile) {
              static_cast<TestProfileIOS*>(profile)->SetSharedURLLoaderFactory(
                  std::move(shared_url_loader_factory));
              return IOSChromeAimEligibilityServiceFactory::GetDefaultFactory()
                  .Run(profile);
            },
            test_url_loader_factory_.GetSafeWeakWrapper()));
    profile_ = std::move(builder).Build();

    TemplateURLService* template_url_service =
        ios::TemplateURLServiceFactory::GetForProfile(profile_.get());
    template_url_service->Load();
  }

  web::WebTaskEnvironment task_environment_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestProfileIOS> profile_;
};

// Tests that `Sec-CH-UA-Full-Version-List` is populated on eligibility requests
// even when `contextual_tasks::kContextualTasks` is disabled.
TEST_F(IOSChromeAimEligibilityServiceFactoryTest,
       SendsFullVersionListHeaderWhenContextualTasksDisabled) {
  scoped_feature_list_.InitWithFeatures(
      {omnibox::kAimEnabled,
       omnibox::kAimServerEligibilitySendFullVersionListEnabled},
      {contextual_tasks::kContextualTasks});

  BuildProfile();

  AimEligibilityService* service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  test_url_loader_factory_.pending_requests()->clear();
  service->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::optional<std::string> full_version_list =
      request.headers.GetHeader("Sec-CH-UA-Full-Version-List");
  ASSERT_TRUE(full_version_list.has_value());

  std::optional<net::structured_headers::List> parsed_list =
      net::structured_headers::ParseListStrict(*full_version_list);
  ASSERT_TRUE(parsed_list.has_value());
  EXPECT_FALSE(parsed_list->empty());

  // Per the UA-CH spec (matching `embedder_support::GenerateBrandVersionList`),
  // `"Chromium"` is always included as the base engine brand, and branded
  // builds additionally append the product brand (e.g. `"Google Chrome"`).
#if BUILDFLAG(CHROMIUM_BRANDING)
  const std::string expected_header = base::StrCat(
      {"\"Chromium\";v=\"", version_info::GetVersionNumber(), "\""});
#else
  const std::string expected_header =
      base::StrCat({"\"Chromium\";v=\"", version_info::GetVersionNumber(),
                    "\", \"", version_info::GetProductName(), "\";v=\"",
                    version_info::GetVersionNumber(), "\""});
#endif
  EXPECT_EQ(*full_version_list, expected_header);
}

// Tests that `Sec-CH-UA-Full-Version-List`, the Co-Browse `User-Agent` suffix,
// and `Chrome-Search-Capabilities-Version` are all populated when
// `contextual_tasks::kContextualTasks` is enabled.
TEST_F(IOSChromeAimEligibilityServiceFactoryTest,
       SendsAllHeadersWhenContextualTasksEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {omnibox::kAimEnabled, contextual_tasks::kContextualTasks,
       omnibox::kAimServerEligibilitySendFullVersionListEnabled,
       omnibox::kAimServerEligibilitySendCoBrowseUserAgentSuffixEnabled,
       omnibox::kAimServerEligibilitySendSearchCapabilitiesHeaderEnabled},
      {});

  BuildProfile();

  AimEligibilityService* service =
      IOSChromeAimEligibilityServiceFactory::GetForProfile(profile_.get());
  ASSERT_TRUE(service);

  test_url_loader_factory_.pending_requests()->clear();
  service->StartServerEligibilityRequestForDebugging();

  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;

  std::optional<std::string> full_version_list =
      request.headers.GetHeader("Sec-CH-UA-Full-Version-List");
  ASSERT_TRUE(full_version_list.has_value());
  EXPECT_THAT(*full_version_list,
              HasSubstr(std::string(version_info::GetVersionNumber())));

  std::optional<std::string> user_agent =
      request.headers.GetHeader("User-Agent");
  ASSERT_TRUE(user_agent.has_value());
  EXPECT_THAT(*user_agent,
              HasSubstr(contextual_tasks::GetContextualTasksUserAgentSuffix()));

  std::optional<std::string> search_capabilities = request.headers.GetHeader(
      contextual_tasks::kContextualTasksSearchCapabilitiesHeaderName);
  ASSERT_TRUE(search_capabilities.has_value());
  EXPECT_EQ(*search_capabilities,
            contextual_tasks::GetContextualTasksSearchCapabilitiesVersion());
}

}  // namespace
