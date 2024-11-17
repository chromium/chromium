// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_coordinator.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_constants.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_coordinator.h"
#import "ios/chrome/browser/credential_provider_promo/ui_bundled/credential_provider_promo_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

using credential_provider_promo::IOSCredentialProviderPromoAction;

// Test fixture for testing the CredentialProviderPromoCoordinator class.
class CredentialProviderPromoCoordinatorTest : public PlatformTest {
 public:
  CredentialProviderPromoCoordinatorTest()
      : profile_(TestProfileIOS::Builder().Build()),
        browser_(std::make_unique<TestBrowser>(profile_.get())),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {
    coordinator_ = [[CredentialProviderPromoCoordinator alloc]
        initWithBaseViewController:nil
                           browser:browser_.get()];
    [coordinator_ start];

    credential_provider_promo_command_handler_ = HandlerForProtocol(
        browser_->GetCommandDispatcher(), CredentialProviderPromoCommands);
  }
  ~CredentialProviderPromoCoordinatorTest() override { [coordinator_ stop]; }

  // Returns the last action taken by the user on the promo that is stored in
  // local state.
  int LastActionTaken() {
    return local_state()->GetInteger(
        prefs::kIosCredentialProviderPromoLastActionTaken);
  }

  PrefService* local_state() {
    return GetApplicationContext()->GetLocalState();
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  CredentialProviderPromoCoordinator* coordinator_;
  id<CredentialProviderPromoCommands>
      credential_provider_promo_command_handler_;
};

#pragma mark - Tests

// Tests that impression is recorded with the correct source.
TEST_F(CredentialProviderPromoCoordinatorTest,
       CredentialProviderPromoImpressionRecorded) {
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoImpressionHistogram,
      IOSCredentialProviderPromoSource::kAutofillUsed, 0);

  // Coordinator will show the promo with PasswordCopied as source.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:
          CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword];

  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoImpressionHistogram,
      IOSCredentialProviderPromoSource::kAutofillUsed, 1);
}

// Tests that the remind-me-later promo's impression is recorded with the
// correct original source.
TEST_F(CredentialProviderPromoCoordinatorTest,
       CredentialProviderPromoImpressionFromReminderRecorded) {
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoImpressionIsReminderHistogram,
      IOSCredentialProviderPromoSource::kAutofillUsed, 0);

  // Coordinator will show the promo with AutofillUsed as source, and update
  // `prefs::kIosCredentialProviderPromoSource`.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:
          CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword];

  // Coordinator is called to again to show the promo as reminder.
  // `kAutofillUsed` will be used as the original source when recording
  // metric.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 RemindMeLater];

  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoImpressionIsReminderHistogram,
      IOSCredentialProviderPromoSource::kAutofillUsed, 1);
}

// Tests that tapping the primary CTA in both the first and second step of the
// promo will result in two primary actions being recorded correctly.
TEST_F(CredentialProviderPromoCoordinatorTest,
       CredentialProviderPromoTwoStepPrimaryActionRecorded) {
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kLearnMore,
      0);
  // Trigger the promo with PasswordSaved.
  // The primary CTA on the first step of the promo is 'learn more'.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:
          CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword];

  // Perform the action. Coordinator will record the action and set.
  // `promoContext` to 'learn more'
  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);
  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertPrimaryAction];

  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kLearnMore,
      1);
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::
          kGoToSettings,
      0);

  // When the `promoContext` is 'learn more', the primary CTA is 'go to
  // settings'.
  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertPrimaryAction];
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::
          kGoToSettings,
      1);
}

// Tests that tapping the secondary CTA is recorded correctly when the promo is
// shown from autofill.
TEST_F(CredentialProviderPromoCoordinatorTest,
       CredentialProviderPromoSecondaryActionRecorded) {
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kNo, 0);

  // Trigger the promo with SuccessfulLoginUsingExistingPassword.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:
          CredentialProviderPromoTrigger::SuccessfulLoginUsingExistingPassword];

  EXPECT_TRUE([coordinator_
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);
  // Perform the action, Coordinator will record the action.
  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertSecondaryAction];

  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnAutofillUsedHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kNo, 1);
}

// Tests the flow when the trigger is the SetUpList. It should go directly to
// LearnMore and the primary CTA should go to settings.
TEST_F(CredentialProviderPromoCoordinatorTest, SetUpListTrigger) {
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnSetUpListHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kLearnMore,
      0);
  // Trigger the promo with SetUpList. The primary CTA of the promo, when
  // triggered from SetUpList, is 'go to settings'.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SetUpList];

  // Perform the action. Coordinator will record the action 'go to settings'.
  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);
  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertPrimaryAction];

  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnSetUpListHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::kLearnMore,
      0);
  histogram_tester_->ExpectBucketCount(
      kIOSCredentialProviderPromoOnSetUpListHistogram,
      credential_provider_promo::IOSCredentialProviderPromoAction::
          kGoToSettings,
      1);
}

// Tests that the last action taken is recorded in local state.
TEST_F(CredentialProviderPromoCoordinatorTest, LastActionTaken) {
  // Trigger the promo with SetUpList. The primary CTA of the promo, when
  // triggered from SetUpList, is 'go to settings'.
  [credential_provider_promo_command_handler_
      showCredentialProviderPromoWithTrigger:CredentialProviderPromoTrigger::
                                                 SetUpList];
  EXPECT_EQ(LastActionTaken(), -1);

  // Perform the action. Coordinator will record the action 'go to settings'.
  ASSERT_TRUE([coordinator_
      conformsToProtocol:@protocol(ConfirmationAlertActionHandler)]);
  [(id<ConfirmationAlertActionHandler>)
          coordinator_ confirmationAlertPrimaryAction];
  EXPECT_EQ(LastActionTaken(),
            static_cast<int>(IOSCredentialProviderPromoAction::kGoToSettings));
}
