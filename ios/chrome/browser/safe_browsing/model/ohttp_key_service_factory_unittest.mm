// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/ohttp_key_service_factory.h"

#import "base/test/scoped_feature_list.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

class OhttpKeyServiceFactoryTest : public PlatformTest {
 protected:
  OhttpKeyServiceFactoryTest() = default;

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

class OhttpKeyServiceFactoryIncognitoLookupTest
    : public OhttpKeyServiceFactoryTest,
      public testing::WithParamInterface<bool> {
 protected:
  OhttpKeyServiceFactoryIncognitoLookupTest() {
    scoped_feature_list_.InitWithFeatureState(
        safe_browsing::kHashPrefixRealTimeLookupsOnIncognitoAndGuest,
        GetParam());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         OhttpKeyServiceFactoryIncognitoLookupTest,
                         testing::Bool());

// Checks that OhttpKeyServiceFactory returns a non-null instance for a regular
// profile, and returns a separate non-null instance for an off-the-record
// profile iff kHashPrefixRealTimeLookupsOnIncognitoAndGuest is enabled.
TEST_P(OhttpKeyServiceFactoryIncognitoLookupTest, GetForProfile) {
  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(OhttpKeyServiceFactory::GetInstance(),
                            OhttpKeyServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestProfileIOS> profile = std::move(builder).Build();

  safe_browsing::OhttpKeyService* regular_service =
      OhttpKeyServiceFactory::GetForProfile(profile.get());
  // There should be a non-null instance for a regular profile.
  EXPECT_TRUE(regular_service);

  ProfileIOS* otr_profile =
      profile->CreateOffTheRecordProfileWithTestingFactories(
          {TestProfileIOS::TestingFactory{
              OhttpKeyServiceFactory::GetInstance(),
              OhttpKeyServiceFactory::GetDefaultFactory()}});
  safe_browsing::OhttpKeyService* incognito_service =
      OhttpKeyServiceFactory::GetForProfile(otr_profile);
  EXPECT_EQ(GetParam(), incognito_service != nullptr);
  EXPECT_NE(regular_service, incognito_service);
}

// Checks that by default OhttpKeyServiceFactory returns null for both regular
// and off-the-record profiles for testing profiles if no testing factory is
// installed.
TEST_F(OhttpKeyServiceFactoryTest, GetForProfile_NoTestingFactory) {
  std::unique_ptr<TestProfileIOS> profile = TestProfileIOS::Builder().Build();

  // The factory should return null for an off-the-record profile.
  EXPECT_FALSE(
      OhttpKeyServiceFactory::GetForProfile(profile->GetOffTheRecordProfile()));

  // There should be a non-null instance for a regular profile.
  EXPECT_FALSE(OhttpKeyServiceFactory::GetForProfile(profile.get()));
}
