// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/tab_usage_recorder_service.h"

#import <array>

#import "base/check_deref.h"
#import "base/memory/raw_ref.h"
#import "base/test/task_environment.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/sessions/model/test_session_restoration_service.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {

// Maps a BrowserType to the NSUserDefaults used to store the number of
// tabs of that type.
struct BrowserTypeMapping {
  Browser::Type browser_type;
  NSString* user_defaults_key;
};

// Subclass of TestBrowser that automatically register self with the
// given `browser_list` on construction and unregister itself upon
// destruction.
class ScopedTestBrowser {
 public:
  ScopedTestBrowser(ProfileIOS* profile,
                    Browser::Type browser_type,
                    BrowserList* browser_list)
      : browser_list_(CHECK_DEREF(browser_list)),
        browser_(std::make_unique<TestBrowser>(
            profile,
            /*scene_state=*/nil,
            std::make_unique<FakeWebStateListDelegate>(),
            browser_type)) {
    browser_list_->AddBrowser(browser_.get());
  }

  ~ScopedTestBrowser() { browser_list_->RemoveBrowser(browser_.get()); }

  WebStateList* GetWebStateList() { return browser_->GetWebStateList(); }

 private:
  const raw_ref<BrowserList> browser_list_;
  const std::unique_ptr<Browser> browser_;
};

}  // namespace

// Test fixture for TabUsageRecorderService.
class TabUsageRecorderServiceTest : public PlatformTest {
 public:
  TabUsageRecorderServiceTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_list_ = std::make_unique<BrowserList>();
    session_restoration_service_ =
        std::make_unique<TestSessionRestorationService>();
    tab_usage_recorder_service_ = std::make_unique<TabUsageRecorderService>(
        browser_list_.get(), session_restoration_service_.get());
  }

  ~TabUsageRecorderServiceTest() override {
    tab_usage_recorder_service_->Shutdown();
    session_restoration_service_->Shutdown();
    browser_list_->Shutdown();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<BrowserList> browser_list_;
  std::unique_ptr<SessionRestorationService> session_restoration_service_;
  std::unique_ptr<TabUsageRecorderService> tab_usage_recorder_service_;
};

// Check that the tabs count are correctly reported to crash keys and
// NSUserDefaults for each tab types.
TEST_F(TabUsageRecorderServiceTest, CountTabs) {
  const auto kBrowserTypeMappings = std::to_array<BrowserTypeMapping>({
      {
          Browser::Type::kRegular,
          previous_session_info_constants::kPreviousSessionInfoTabCount,
      },
      {
          Browser::Type::kInactive,
          previous_session_info_constants::kPreviousSessionInfoInactiveTabCount,
      },
      {
          Browser::Type::kIncognito,
          previous_session_info_constants::kPreviousSessionInfoOTRTabCount,
      },
      {
          Browser::Type::kTemporary,
          nil,
      },
      // Browser::Type::kIncognito is listed twice to ensure that destroying
      // and recreating the off-the-record profile does not crash.
      {
          Browser::Type::kIncognito,
          previous_session_info_constants::kPreviousSessionInfoOTRTabCount,
      },
  });

  const auto kUserDefaultsKeys = std::to_array<NSString*>({
      previous_session_info_constants::kPreviousSessionInfoTabCount,
      previous_session_info_constants::kPreviousSessionInfoInactiveTabCount,
      previous_session_info_constants::kPreviousSessionInfoOTRTabCount,
  });

  constexpr int kTabCount = 5;
  constexpr int kTabCountRegular = 7;
  NSString* regular_tab_count_key =
      previous_session_info_constants::kPreviousSessionInfoTabCount;
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  for (const auto& mapping : kBrowserTypeMappings) {
    // Clears the values stored in the NSUserDefaults.
    for (NSString* key : kUserDefaultsKeys) {
      [defaults setInteger:0 forKey:key];
    }

    ProfileIOS* profile = mapping.browser_type == Browser::Type::kIncognito
                              ? profile_.get()
                              : profile_->GetOffTheRecordProfile();

    // Create a Browser of the specified type and insert kTabCount tabs.
    auto browser1 = std::make_unique<ScopedTestBrowser>(
        profile, mapping.browser_type, browser_list_.get());

    for (int count = 0; count < kTabCount; ++count) {
      browser1->GetWebStateList()->InsertWebState(
          std::make_unique<web::FakeWebState>());
    }

    // Check that only the expected key has been modified.
    for (NSString* key : kUserDefaultsKeys) {
      int expected = 0;
      if ([key isEqualToString:mapping.user_defaults_key]) {
        expected += kTabCount;
      }

      EXPECT_EQ([defaults integerForKey:key], expected);
    }

    // Create a Browser of regular type and insert kTabCountRegular tabs.
    auto browser2 = std::make_unique<ScopedTestBrowser>(
        profile, Browser::Type::kRegular, browser_list_.get());

    for (int count = 0; count < kTabCountRegular; ++count) {
      browser2->GetWebStateList()->InsertWebState(
          std::make_unique<web::FakeWebState>());
    }

    // Check that only the regular key has been modified.
    for (NSString* key : kUserDefaultsKeys) {
      int expected = 0;
      if ([key isEqualToString:mapping.user_defaults_key]) {
        expected += kTabCount;
      }
      if ([key isEqualToString:regular_tab_count_key]) {
        expected += kTabCountRegular;
      }

      EXPECT_EQ([defaults integerForKey:key], expected);
    }

    // Destroy the Browser of the requested type and check that the expected
    // keys has been modified.
    browser1.reset();

    // Destroy the off-the-record profile if we closed all the incognito tabs.
    if (mapping.browser_type == Browser::Type::kIncognito) {
      profile_->DestroyOffTheRecordProfile();
    }

    for (NSString* key : kUserDefaultsKeys) {
      int expected = 0;
      if ([key isEqualToString:regular_tab_count_key]) {
        expected += kTabCountRegular;
      }

      EXPECT_EQ([defaults integerForKey:key], expected);
    }

    // Destroy the regular Browser and check that only the regular key has
    // been modified.
    browser2.reset();

    for (NSString* key : kUserDefaultsKeys) {
      int expected = 0;

      EXPECT_EQ([defaults integerForKey:key], expected);
    }
  }
}
