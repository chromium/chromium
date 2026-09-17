// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_prefs.h"

#import "base/test/scoped_feature_list.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace gemini {

class GeminiPrefsTest : public PlatformTest {
 protected:
  GeminiPrefsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(GeminiPrefsTest, TestGetConversationId) {
  ASSERT_FALSE(GetConversationId(profile_->GetPrefs()).has_value());
  std::string server_id = "test_server_id";
  CreateOrUpdateConversationIdPrefs(server_id, "https://www.chromium.org",
                                    profile_->GetPrefs());
  ASSERT_TRUE(GetConversationId(profile_->GetPrefs()).has_value());
  ASSERT_EQ(server_id, GetConversationId(profile_->GetPrefs()).value());
}

TEST_F(GeminiPrefsTest, TestGetConversationId_Expired) {
  std::string server_id = "test_server_id";
  CreateOrUpdateConversationIdPrefs(server_id, "https://www.chromium.org",
                                    profile_->GetPrefs());
  ASSERT_TRUE(GetConversationId(profile_->GetPrefs()).has_value());

  // Fast forward time to expire the session.
  task_environment_.FastForwardBy(GetGeminiSessionValidityDuration() +
                                  base::Seconds(1));

  ASSERT_FALSE(GetConversationId(profile_->GetPrefs()).has_value());
}

TEST_F(GeminiPrefsTest, TestGetConversationId_Expired_CustomDuration) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kGeminiConfigParams, {{kGeminiSessionValidityDuration, "5"}});

  std::string server_id = "test_server_id";
  CreateOrUpdateConversationIdPrefs(server_id, "https://www.chromium.org",
                                    profile_->GetPrefs());
  ASSERT_TRUE(GetConversationId(profile_->GetPrefs()).has_value());
  ASSERT_EQ(GetGeminiSessionValidityDuration(), base::Minutes(5));

  // Fast forward by 4 minutes -> should still be valid.
  task_environment_.FastForwardBy(base::Minutes(4));
  ASSERT_TRUE(GetConversationId(profile_->GetPrefs()).has_value());

  // Fast forward by another 1 minute + 1 second -> should expire.
  task_environment_.FastForwardBy(base::Minutes(1) + base::Seconds(1));
  ASSERT_FALSE(GetConversationId(profile_->GetPrefs()).has_value());
}

// Tests that granting Live consent sets the Live consent pref.
TEST_F(GeminiPrefsTest, TestUpdateUserConsentToLivePrefs_ConsentGiven) {
  PrefService* prefs = profile_->GetPrefs();
  EXPECT_FALSE(DidUserConsentToGeminiLive(prefs));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));

  UpdateUserConsentToLivePrefs(/*consent=*/true, prefs);

  EXPECT_TRUE(DidUserConsentToGeminiLive(prefs));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));
}

// Tests that refusing Live consent updates the Live consent pref while keeping
// the default microphone setting.
TEST_F(GeminiPrefsTest, TestUpdateUserConsentToLivePrefs_ConsentRefused) {
  PrefService* prefs = profile_->GetPrefs();
  EXPECT_FALSE(DidUserConsentToGeminiLive(prefs));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));

  UpdateUserConsentToLivePrefs(/*consent=*/false, prefs);

  EXPECT_FALSE(DidUserConsentToGeminiLive(prefs));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kIOSGeminiLiveMicrophoneSetting));
}

}  // namespace gemini
