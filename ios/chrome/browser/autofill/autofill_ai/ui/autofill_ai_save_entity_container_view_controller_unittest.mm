// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_container_view_controller.h"

#import "components/autofill/core/browser/payments/test_legal_message_line.h"
#import "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_constants.h"
#import "ios/chrome/browser/autofill/autofill_ai/ui/autofill_ai_save_entity_table_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/model/message/autofill_legal_message_line.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

UIView* FindViewWithAccessibilityIdentifier(UIView* root_view,
                                            NSString* identifier) {
  if ([root_view.accessibilityIdentifier isEqualToString:identifier]) {
    return root_view;
  }
  for (UIView* subview in root_view.subviews) {
    UIView* found = FindViewWithAccessibilityIdentifier(subview, identifier);
    if (found) {
      return found;
    }
  }
  return nil;
}

}  // namespace

@interface FakeAutofillAISaveEntityContainerViewControllerDelegate
    : NSObject <AutofillAISaveEntityContainerViewControllerDelegate>
@property(nonatomic, strong) CrURL* tappedURL;
@end

@implementation FakeAutofillAISaveEntityContainerViewControllerDelegate
- (void)didTapLinkWithURL:(CrURL*)url {
  self.tappedURL = url;
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

  EXPECT_NE(nil, FindViewWithAccessibilityIdentifier(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));
}

// Tests that legal disclosure text view is not displayed when no legal messages
// are set.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LegalDisclosureNotShownWhenLegalMessagesNotPresent) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, FindViewWithAccessibilityIdentifier(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));
}

// Tests that legal disclosure text view is added dynamically when legal
// messages are set after the view is already loaded.
TEST_F(AutofillAISaveEntityContainerViewControllerTest,
       LegalDisclosureAddedDynamicallyAfterViewLoaded) {
  [controller_ loadViewIfNeeded];
  [controller_.view layoutIfNeeded];

  EXPECT_EQ(nil, FindViewWithAccessibilityIdentifier(
                     controller_.view, kAutofillAISaveEntityLegalDisclosureId));

  autofill::LegalMessageLines lines = {
      autofill::TestLegalMessageLine("Test dynamic legal disclosure")};
  NSArray<AutofillLegalMessageLine*>* messages =
      [AutofillLegalMessageLine convertFrom:lines];

  [controller_ setLegalMessages:messages];
  [controller_.view layoutIfNeeded];

  EXPECT_NE(nil, FindViewWithAccessibilityIdentifier(
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
