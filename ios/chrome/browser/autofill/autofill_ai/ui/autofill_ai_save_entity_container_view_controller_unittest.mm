// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_container_view_controller.h"

#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_mutator.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface FakeAutofillAISaveEntityContainerViewControllerDelegate
    : NSObject <AutofillAISaveEntityContainerViewControllerDelegate>
@property(nonatomic, strong) CrURL* tappedURL;
@end

@implementation FakeAutofillAISaveEntityContainerViewControllerDelegate
- (void)didTapLinkWithURL:(CrURL*)url {
  self.tappedURL = url;
}
@end

@interface FakeAutofillAISaveEntityMutator
    : NSObject <AutofillAISaveEntityMutator>
@property(nonatomic, assign) BOOL acceptSavingCalled;
@property(nonatomic, assign) BOOL cancelSavingCalled;
@end

@implementation FakeAutofillAISaveEntityMutator
- (void)acceptSaving {
  self.acceptSavingCalled = YES;
}
- (void)cancelSaving {
  self.cancelSavingCalled = YES;
}
- (void)dismissSaving {
}
@end

class AutofillAISaveEntityContainerViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = [[AutofillAISaveEntityContainerViewController alloc] init];
    autofill::test::VehicleOptions options;
    options.make = u"Car Maker";
    [controller_ setNewEntity:autofill::test::GetVehicleEntityInstance(options)
                    oldEntity:std::nullopt
                    userEmail:u"test@example.com"
            saveIsSynchronous:YES];
  }

  AutofillAISaveEntityContainerViewController* controller_;
};

// Tests that legal disclosure text view is displayed when legal messages are
// set.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LegalDisclosureShownWhenLegalMessagesPresent) {
  autofill::LegalMessageLines lines = {
      autofill::TestLegalMessageLine("Test legal disclosure")};
  NSArray<AutofillLegalMessageLine*>* messages =
      [AutofillLegalMessageLine convertFrom:lines];

  [controller_ setLegalMessages:messages];
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_NE(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));
}

// Tests that legal disclosure text view is not displayed when no legal messages
// are set.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LegalDisclosureNotShownWhenLegalMessagesNotPresent) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));
}

// Tests that legal disclosure text view is added dynamically when legal
// messages are set after the view is already loaded.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LegalDisclosureAddedDynamicallyAfterViewLoaded) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));

  autofill::LegalMessageLines lines = {
      autofill::TestLegalMessageLine("Test dynamic legal disclosure")};
  NSArray<AutofillLegalMessageLine*>* messages =
      [AutofillLegalMessageLine convertFrom:lines];

  [controller_ setLegalMessages:messages];
  [controller_.view layoutIfNeeded];

  EXPECT_NE(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));
}

// Tests that tapping a link in the disclosure notifies the delegate.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LinkTapNotifiesDelegate) {
  FakeAutofillAISaveEntityContainerViewControllerDelegate* delegate =
      [[FakeAutofillAISaveEntityContainerViewControllerDelegate alloc] init];
  controller_.delegate = delegate;

  CrURL* test_url =
      [[CrURL alloc] initWithGURL:GURL("https://www.example.com/legal")];
  [(id<AutofillAISaveEntityTableViewControllerDelegate>)controller_
      didTapLinkWithURL:test_url];
  EXPECT_EQ(delegate.tappedURL.gurl, GURL("https://www.example.com/legal"));
}

// Tests that standard layout is used when `showsExplicitDeclineButton` is NO.
TEST_F(AutofillAISaveEntityContainerViewControllerTest, StandardLayout) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  ASSERT_NE(nil, controller_.navigationItem.leftBarButtonItem);
  EXPECT_NSEQ(
      kAutofillAISaveEntityCancelButtonId,
      controller_.navigationItem.leftBarButtonItem.accessibilityIdentifier);
  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityNoThanksButtonId));
}

// Tests that explicit decline layout is used when `showsExplicitDeclineButton`
// is YES.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       ExplicitDeclineButtonLayout) {
  [controller_ setShowsExplicitDeclineButton:YES];
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, controller_.navigationItem.leftBarButtonItem);
  EXPECT_NE(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityNoThanksButtonId));
}

// Tests that dynamic transition occurs when `showsExplicitDeclineButton` is
// changed after load.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       DynamicExplicitDeclineButtonLayoutTransition) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_NE(nil, controller_.navigationItem.leftBarButtonItem);
  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityNoThanksButtonId));

  [controller_ setShowsExplicitDeclineButton:YES];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, controller_.navigationItem.leftBarButtonItem);
  EXPECT_NE(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityNoThanksButtonId));

  [controller_ setShowsExplicitDeclineButton:NO];
  [controller_.view layoutIfNeeded];

  EXPECT_NE(nil, controller_.navigationItem.leftBarButtonItem);
  EXPECT_EQ(nil, chrome_test_util::FindViewById(
                     controller_.view, kAutofillAISaveEntityNoThanksButtonId));
}

// Tests that pressing "No thanks" button invokes `cancelSaving` on the
// `mutator`.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       NoThanksButtonTriggersCancelSaving) {
  FakeAutofillAISaveEntityMutator* mutator =
      [[FakeAutofillAISaveEntityMutator alloc] init];
  controller_.mutator = mutator;
  [controller_ setShowsExplicitDeclineButton:YES];
  [controller_ loadViewIfNeeded];

  UIButton* noThanksButton = chrome_test_util::FindViewById<UIButton>(
      controller_.view, kAutofillAISaveEntityNoThanksButtonId);
  ASSERT_NE(nil, noThanksButton);
  [noThanksButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_TRUE(mutator.cancelSavingCalled);
}

// Tests that button states transition correctly in loading and confirmation
// states with explicit decline layout.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       ExplicitDeclineButtonsStateTransitions) {
  [controller_ setShowsExplicitDeclineButton:YES];
  [controller_ loadViewIfNeeded];

  UIButton* saveButton = chrome_test_util::FindViewById<UIButton>(
      controller_.view, kAutofillAISaveEntitySaveButtonId);
  UIButton* noThanksButton = chrome_test_util::FindViewById<UIButton>(
      controller_.view, kAutofillAISaveEntityNoThanksButtonId);

  ASSERT_NE(nil, saveButton);
  ASSERT_NE(nil, noThanksButton);
  EXPECT_TRUE(saveButton.enabled);
  EXPECT_TRUE(noThanksButton.enabled);

  [controller_ showLoadingState];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_FALSE(noThanksButton.enabled);
  EXPECT_TRUE(controller_.modalInPresentation);

  [controller_ showConfirmationState];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_FALSE(noThanksButton.enabled);
  EXPECT_FALSE(controller_.modalInPresentation);
}

// Tests that button states transition correctly in loading and confirmation
// states with standard layout.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       StandardButtonsStateTransitions) {
  [controller_ loadViewIfNeeded];

  UIButton* saveButton = chrome_test_util::FindViewById<UIButton>(
      controller_.view, kAutofillAISaveEntitySaveButtonId);
  UIBarButtonItem* cancelButton = controller_.navigationItem.leftBarButtonItem;

  ASSERT_NE(nil, saveButton);
  ASSERT_NE(nil, cancelButton);
  EXPECT_TRUE(saveButton.enabled);
  EXPECT_TRUE(cancelButton.enabled);

  [controller_ showLoadingState];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_FALSE(cancelButton.enabled);
  EXPECT_TRUE(controller_.modalInPresentation);

  [controller_ showConfirmationState];
  EXPECT_FALSE(saveButton.enabled);
  EXPECT_FALSE(cancelButton.enabled);
  EXPECT_FALSE(controller_.modalInPresentation);
}

// Tests that tapping the cancel button invokes `cancelSaving` on the `mutator`.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       CancelButtonTriggersCancelSavingWhenEnabled) {
  FakeAutofillAISaveEntityMutator* mutator =
      [[FakeAutofillAISaveEntityMutator alloc] init];
  controller_.mutator = mutator;
  [controller_ loadViewIfNeeded];

  UIBarButtonItem* cancelButton = controller_.navigationItem.leftBarButtonItem;
  ASSERT_NE(nil, cancelButton);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
  [cancelButton.target performSelector:cancelButton.action
                            withObject:cancelButton];
#pragma clang diagnostic pop

  EXPECT_TRUE(mutator.cancelSavingCalled);
}

// Tests that dynamic layout transitions during loading state keep newly
// created buttons disabled.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       DynamicLayoutUpdateDuringLoadingKeepsButtonsDisabled) {
  [controller_ setShowsExplicitDeclineButton:YES];
  [controller_ loadViewIfNeeded];
  [controller_ showLoadingState];

  // Transition to standard layout: newly created cancel button must be
  // disabled.
  [controller_ setShowsExplicitDeclineButton:NO];
  UIBarButtonItem* cancelButton = controller_.navigationItem.leftBarButtonItem;
  ASSERT_NE(nil, cancelButton);
  EXPECT_FALSE(cancelButton.enabled);

  // Transition back to explicit decline: newly created No Thanks button must
  // be disabled.
  [controller_ setShowsExplicitDeclineButton:YES];
  UIButton* noThanksButton = chrome_test_util::FindViewById<UIButton>(
      controller_.view, kAutofillAISaveEntityNoThanksButtonId);
  ASSERT_NE(nil, noThanksButton);
  EXPECT_FALSE(noThanksButton.enabled);
}
