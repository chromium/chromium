// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/safety_check/model/safety_check_utils.h"

#import <optional>
#import <string>
#import <vector>

#import "base/test/metrics/user_action_tester.h"
#import "base/test/task_environment.h"
#import "base/time/time.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "ios/chrome/browser/content_suggestions/safety_check/public/safety_check_constants.h"
#import "ios/chrome/browser/content_suggestions/safety_check/ui/safety_check_item_type.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

namespace {

using password_manager::CredentialUIEntry;
using password_manager::InsecurePasswordCounts;
using password_manager::InsecureType;
using password_manager::InsecurityMetadata;
using password_manager::PasswordCheckReferrer;
using password_manager::PasswordForm;
using password_manager::WarningType;

const char kTestUrl[] = "http://www.example.com/";

// Helper to create a dummy credential with specific insecure states.
CredentialUIEntry CreateCredential(
    const std::u16string& username,
    const std::vector<InsecureType>& issues = {}) {
  PasswordForm form;
  form.username_value = username;
  form.password_value = u"password";
  form.signon_realm = kTestUrl;
  form.url = GURL(kTestUrl);
  form.date_created = base::Time::Now();

  for (const auto& issue : issues) {
    form.password_issues.insert({issue, InsecurityMetadata()});
  }

  return CredentialUIEntry(form);
}

}  // namespace

class SafetyCheckUtilsTest : public PlatformTest {
 protected:
  SafetyCheckUtilsTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    PlatformTest::SetUp();
    mock_application_handler_ = OCMProtocolMock(@protocol(SceneCommands));
    mock_settings_handler_ = OCMProtocolMock(@protocol(SettingsCommands));
  }

  base::test::TaskEnvironment task_environment_;
  id mock_application_handler_;
  id mock_settings_handler_;
};

// Tests that tapping a check with multiple credentials of the same warning type
// opens that specific issue list.
TEST_F(SafetyCheckUtilsTest,
       HandlePasswordTapWithMultipleSameTypeOpensIssueList) {
  std::vector<CredentialUIEntry> credentials = {
      CreateCredential(u"user1", {InsecureType::kWeak}),
      CreateCredential(u"user2", {InsecureType::kWeak})};

  InsecurePasswordCounts counts = {.weak_count = 2};

  [[mock_application_handler_ expect]
      showPasswordIssuesWithWarningType:WarningType::kWeakPasswordsWarning
                               referrer:PasswordCheckReferrer::
                                            kSafetyCheckMagicStack];

  HandleSafetyCheckPasswordTap(
      credentials, counts, PasswordCheckReferrer::kSafetyCheckMagicStack,
      mock_application_handler_, mock_settings_handler_);

  [mock_application_handler_ verify];
}

// Tests that tapping a check with mixed warning types opens the main Password
// Checkup page.
TEST_F(SafetyCheckUtilsTest, HandlePasswordTapWithMixedTypesOpensCheckupPage) {
  base::UserActionTester user_action_tester;
  std::vector<CredentialUIEntry> credentials = {
      CreateCredential(u"user1", {InsecureType::kLeaked}),
      CreateCredential(u"user2", {InsecureType::kWeak})};

  InsecurePasswordCounts counts = {.compromised_count = 1, .weak_count = 1};

  [[mock_application_handler_ expect]
      dismissModalsAndShowPasswordCheckupPageForReferrer:
          PasswordCheckReferrer::kSafetyCheckMagicStack];

  HandleSafetyCheckPasswordTap(
      credentials, counts, PasswordCheckReferrer::kSafetyCheckMagicStack,
      mock_application_handler_, mock_settings_handler_);

  [mock_application_handler_ verify];
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "MobileMagicStackOpenPasswordCheckup"));
}

// Verifies that only the OutOfDate state is considered invalid for the Update
// Chrome check.
TEST_F(SafetyCheckUtilsTest, InvalidUpdateChromeState) {
  const std::vector<UpdateChromeSafetyCheckState> all_states = {
      UpdateChromeSafetyCheckState::kDefault,
      UpdateChromeSafetyCheckState::kUpToDate,
      UpdateChromeSafetyCheckState::kOutOfDate,
      UpdateChromeSafetyCheckState::kManaged,
      UpdateChromeSafetyCheckState::kRunning,
      UpdateChromeSafetyCheckState::kOmahaError,
      UpdateChromeSafetyCheckState::kNetError,
      UpdateChromeSafetyCheckState::kChannel,
  };

  for (auto state : all_states) {
    bool expected_invalid = false;
    switch (state) {
      case UpdateChromeSafetyCheckState::kOutOfDate:
        expected_invalid = true;
        break;
      case UpdateChromeSafetyCheckState::kDefault:
      case UpdateChromeSafetyCheckState::kUpToDate:
      case UpdateChromeSafetyCheckState::kManaged:
      case UpdateChromeSafetyCheckState::kRunning:
      case UpdateChromeSafetyCheckState::kOmahaError:
      case UpdateChromeSafetyCheckState::kNetError:
      case UpdateChromeSafetyCheckState::kChannel:
        expected_invalid = false;
        break;
    }
    EXPECT_EQ(InvalidUpdateChromeState(state), expected_invalid)
        << "Failed for state: " << (int)state;
  }
}

// Verifies that compromised, reused, and weak password states are considered
// invalid.
TEST_F(SafetyCheckUtilsTest, InvalidPasswordState) {
  const std::vector<PasswordSafetyCheckState> all_states = {
      PasswordSafetyCheckState::kDefault,
      PasswordSafetyCheckState::kSafe,
      PasswordSafetyCheckState::kUnmutedCompromisedPasswords,
      PasswordSafetyCheckState::kReusedPasswords,
      PasswordSafetyCheckState::kWeakPasswords,
      PasswordSafetyCheckState::kDismissedWarnings,
      PasswordSafetyCheckState::kRunning,
      PasswordSafetyCheckState::kDisabled,
      PasswordSafetyCheckState::kError,
      PasswordSafetyCheckState::kSignedOut,
  };

  for (auto state : all_states) {
    bool expected_invalid = false;
    switch (state) {
      case PasswordSafetyCheckState::kUnmutedCompromisedPasswords:
      case PasswordSafetyCheckState::kReusedPasswords:
      case PasswordSafetyCheckState::kWeakPasswords:
        expected_invalid = true;
        break;
      case PasswordSafetyCheckState::kDefault:
      case PasswordSafetyCheckState::kSafe:
      case PasswordSafetyCheckState::kDismissedWarnings:
      case PasswordSafetyCheckState::kRunning:
      case PasswordSafetyCheckState::kDisabled:
      case PasswordSafetyCheckState::kError:
      case PasswordSafetyCheckState::kSignedOut:
        expected_invalid = false;
        break;
    }
    EXPECT_EQ(InvalidPasswordState(state), expected_invalid)
        << "Failed for state: " << (int)state;
  }
}

// Verifies that only the Unsafe state is considered invalid for the Safe
// Browsing check.
TEST_F(SafetyCheckUtilsTest, InvalidSafeBrowsingState) {
  const std::vector<SafeBrowsingSafetyCheckState> all_states = {
      SafeBrowsingSafetyCheckState::kDefault,
      SafeBrowsingSafetyCheckState::kManaged,
      SafeBrowsingSafetyCheckState::kRunning,
      SafeBrowsingSafetyCheckState::kSafe,
      SafeBrowsingSafetyCheckState::kUnsafe,
  };

  for (auto state : all_states) {
    bool expected_invalid = false;
    switch (state) {
      case SafeBrowsingSafetyCheckState::kUnsafe:
        expected_invalid = true;
        break;
      case SafeBrowsingSafetyCheckState::kDefault:
      case SafeBrowsingSafetyCheckState::kManaged:
      case SafeBrowsingSafetyCheckState::kRunning:
      case SafeBrowsingSafetyCheckState::kSafe:
        expected_invalid = false;
        break;
    }
    EXPECT_EQ(InvalidSafeBrowsingState(state), expected_invalid)
        << "Failed for state: " << (int)state;
  }
}

// Tests that the safety check runs only if it hasn't run recently, respecting
// the auto-run delay.
TEST_F(SafetyCheckUtilsTest, CanRunSafetyCheck) {
  // Never run before.
  EXPECT_TRUE(CanRunSafetyCheck(std::nullopt));

  const base::Time now = base::Time::Now();

  // Run very recently (now).
  EXPECT_FALSE(CanRunSafetyCheck(now));

  // Run just within the delay period.
  EXPECT_FALSE(CanRunSafetyCheck(
      now - safety_check::kTimeDelayForSafetyCheckAutorun + base::Seconds(1)));

  // Run exactly at the moment the delay period ends.
  EXPECT_FALSE(
      CanRunSafetyCheck(now - safety_check::kTimeDelayForSafetyCheckAutorun));

  // Run just after the delay period.
  EXPECT_TRUE(CanRunSafetyCheck(
      now - safety_check::kTimeDelayForSafetyCheckAutorun - base::Seconds(1)));

  // Run a long time ago (> autorun delay).
  EXPECT_TRUE(CanRunSafetyCheck(
      now - safety_check::kTimeDelayForSafetyCheckAutorun - base::Days(1)));
}

// Tests the localized string formatting for the time elapsed since the last
// safety check.
TEST_F(SafetyCheckUtilsTest, FormatElapsedTimeSinceLastSafetyCheck) {
  // Case: Never run.
  EXPECT_NSEQ(FormatElapsedTimeSinceLastSafetyCheck(std::nullopt),
              l10n_util::GetNSString(IDS_IOS_CHECK_NEVER_RUN));

  // Case: Just now (< 1 min).
  base::Time now = base::Time::Now();
  EXPECT_NSEQ(FormatElapsedTimeSinceLastSafetyCheck(now - base::Seconds(30)),
              l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW));

  // Case: Elapsed time (> 1 min).
  base::TimeDelta delta = base::Minutes(10);
  base::Time past_time = now - delta;

  std::u16string timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT, delta,
      true);
  NSString* expected = l10n_util::GetNSStringF(
      IDS_IOS_SAFETY_CHECK_LAST_COMPLETED_CHECK, timestamp);

  EXPECT_NSEQ(FormatElapsedTimeSinceLastSafetyCheck(past_time), expected);
}

// Tests the bidirectional mapping between `SafetyCheckItemType` enums and their
// string representations.
TEST_F(SafetyCheckUtilsTest, SafetyCheckItemTypeNameMapping) {
  const std::vector<SafetyCheckItemType> all_types = {
      SafetyCheckItemType::kAllSafe,      SafetyCheckItemType::kRunning,
      SafetyCheckItemType::kUpdateChrome, SafetyCheckItemType::kPassword,
      SafetyCheckItemType::kSafeBrowsing, SafetyCheckItemType::kDefault,
  };

  for (SafetyCheckItemType type : all_types) {
    NSString* expected_name = nil;

    switch (type) {
      case SafetyCheckItemType::kAllSafe:
        expected_name = @"SafetyCheckItemType::kAllSafe";
        break;
      case SafetyCheckItemType::kRunning:
        expected_name = @"SafetyCheckItemType::kRunning";
        break;
      case SafetyCheckItemType::kUpdateChrome:
        expected_name = @"SafetyCheckItemType::kUpdateChrome";
        break;
      case SafetyCheckItemType::kPassword:
        expected_name = @"SafetyCheckItemType::kPassword";
        break;
      case SafetyCheckItemType::kSafeBrowsing:
        expected_name = @"SafetyCheckItemType::kSafeBrowsing";
        break;
      case SafetyCheckItemType::kDefault:
        expected_name = @"SafetyCheckItemType::kDefault";
        break;
    }

    EXPECT_NSEQ(NameForSafetyCheckItemType(type), expected_name);
    EXPECT_EQ(SafetyCheckItemTypeForName(expected_name), type);
  }
}

// Tests the correct metric specific to the referrer provided (Magic Stack vs.
// Safety Check Notification) is logged.
TEST_F(SafetyCheckUtilsTest, HandlePasswordTapRecordsCorrectMetricForReferrer) {
  const struct {
    PasswordCheckReferrer referrer;
    std::string expected_metric;
  } referrer_mappings[] = {
      {PasswordCheckReferrer::kSafetyCheckMagicStack,
       "MobileMagicStackOpenPasswordCheckup"},
      {PasswordCheckReferrer::kSafetyCheckNotification,
       "MobileSafetyCheckNotificationOpenPasswordCheckup"},
  };

  for (const auto& mapping : referrer_mappings) {
    base::UserActionTester user_action_tester;

    std::vector<CredentialUIEntry> empty_credentials;
    InsecurePasswordCounts empty_counts = {};

    [[mock_application_handler_ expect]
        dismissModalsAndShowPasswordCheckupPageForReferrer:mapping.referrer];

    HandleSafetyCheckPasswordTap(empty_credentials, empty_counts,
                                 mapping.referrer, mock_application_handler_,
                                 mock_settings_handler_);

    [mock_application_handler_ verify];

    EXPECT_EQ(1, user_action_tester.GetActionCount(mapping.expected_metric))
        << "Failed to record correct metric for referrer: "
        << (int)mapping.referrer;
  }
}
