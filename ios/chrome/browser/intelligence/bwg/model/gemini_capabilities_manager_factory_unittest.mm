// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager_factory.h"

#import <memory>
#import <utility>

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/intelligence/bwg/model/fake_gemini_service.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_capabilities_manager.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/sync/model/test_sync_service_utils.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for GeminiCapabilitiesManagerFactory.
class GeminiCapabilitiesManagerFactoryTest : public PlatformTest {
 protected:
  // Creates a test profile with necessary testing factories.
  std::unique_ptr<TestProfileIOS> CreateProfile() {
    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        AuthenticationServiceFactory::GetFactoryWithDelegateForTesting(
            std::make_unique<FakeAuthenticationServiceDelegate>()));
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              base::BindRepeating(&CreateTestSyncService));
    builder.AddTestingFactory(GeminiServiceFactory::GetInstance(),
                              base::BindOnce([](ProfileIOS* profile) {
                                return std::unique_ptr<KeyedService>(
                                    std::make_unique<FakeGeminiService>());
                              }));
    builder.AddTestingFactory(
        GeminiCapabilitiesManagerFactory::GetInstance(),
        GeminiCapabilitiesManagerFactory::GetDefaultFactory());
    return std::move(builder).Build();
  }

  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;
  web::WebTaskEnvironment task_environment_;
};

// Tests that the factory successfully creates a service instance.
TEST_F(GeminiCapabilitiesManagerFactoryTest, ServiceCreatedSuccessfully) {
  auto profile = CreateProfile();
  EXPECT_THAT(GeminiCapabilitiesManagerFactory::GetForProfile(profile.get()),
              testing::NotNull());
}

}  // namespace
