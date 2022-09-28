// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/promos_manager/constants.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/post_restore_signin/features.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kFakePreRestoreAccountEmail[] = "person@example.org";
const char kFakePreRestoreAccountGivenName[] = "Given";
const char kFakePreRestoreAccountFullName[] = "Full Name";
}  // namespace

// Tests the PostRestoreSignInProvider.
class PostRestoreSignInProviderTest : public PlatformTest {
 public:
  explicit PostRestoreSignInProviderTest() {
    SetFakePreRestoreAccountInfo();
    provider_ = [[PostRestoreSignInProvider alloc] init];
  }

  void SetFakePreRestoreAccountInfo() {
    AccountInfo accountInfo;
    accountInfo.email = std::string(kFakePreRestoreAccountEmail);
    accountInfo.given_name = std::string(kFakePreRestoreAccountGivenName);
    accountInfo.full_name = std::string(kFakePreRestoreAccountFullName);
    StorePreRestoreIdentity(accountInfo);
  }

  void SetupMockHandler() {
    mock_handler_ = OCMProtocolMock(@protocol(PromosManagerCommands));
    provider_.handler = mock_handler_;
  }

  void EnableFeatureVariationFullscreen() {
    scoped_feature_list_.InitAndEnableFeature(
        post_restore_signin::features::kIOSNewPostRestoreExperience);
  }

  void EnableFeatureVariationAlert() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {base::test::ScopedFeatureList::FeatureAndParams(
            post_restore_signin::features::kIOSNewPostRestoreExperience,
            {{post_restore_signin::features::kIOSNewPostRestoreExperienceParam,
              "true"}})},
        {});
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  id mock_handler_;
  PostRestoreSignInProvider* provider_;
};

TEST_F(PostRestoreSignInProviderTest, hasIdentifierFullscreen) {
  EnableFeatureVariationFullscreen();
  EXPECT_EQ(provider_.identifier,
            promos_manager::Promo::PostRestoreSignInFullscreen);
}

TEST_F(PostRestoreSignInProviderTest, hasIdentifierAlert) {
  EnableFeatureVariationAlert();
  EXPECT_EQ(provider_.identifier,
            promos_manager::Promo::PostRestoreSignInAlert);
}

TEST_F(PostRestoreSignInProviderTest, standardPromoAlertDefaultAction) {
  EnableFeatureVariationFullscreen();
  SetupMockHandler();
  OCMExpect([mock_handler_ showSignin:[OCMArg any]]);
  [provider_ standardPromoAlertDefaultAction];
  [mock_handler_ verify];
}

TEST_F(PostRestoreSignInProviderTest, title) {
  EnableFeatureVariationFullscreen();
  EXPECT_TRUE([[provider_ title] isEqualToString:@"Welcome back, Given"]);
}

TEST_F(PostRestoreSignInProviderTest, message) {
  EnableFeatureVariationFullscreen();
  EXPECT_TRUE([[provider_ message]
      isEqualToString:@"Sign in again to your account person@example.org"]);
}

TEST_F(PostRestoreSignInProviderTest, viewController) {
  EnableFeatureVariationFullscreen();
  EXPECT_TRUE(provider_.viewController != nil);
}
