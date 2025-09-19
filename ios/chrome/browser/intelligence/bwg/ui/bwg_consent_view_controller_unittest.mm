// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_view_controller.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/bwg_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/ui/bwg_consent_mutator.h"
#import "ios/chrome/browser/intelligence/bwg/utils/bwg_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

namespace {

// Helper function to find a view by accessibility identifier.
UIView* GetViewWithAccessibilityIdentifier(UIView* view, NSString* identifier) {
  if ([view.accessibilityIdentifier isEqualToString:identifier]) {
    return view;
  }
  for (UIView* subview in view.subviews) {
    UIView* result = GetViewWithAccessibilityIdentifier(subview, identifier);
    if (result) {
      return result;
    }
  }
  return nil;
}

// Helper function to check for a link in a text view.
BOOL HasLinkWithURL(UITextView* text_view, NSString* url) {
  __block BOOL found_link = NO;
  [text_view.attributedText
      enumerateAttribute:NSLinkAttributeName
                 inRange:NSMakeRange(0, text_view.attributedText.length)
                 options:0
              usingBlock:^(id value, NSRange range, BOOL* stop) {
                if ([value isKindOfClass:[NSString class]] &&
                    [static_cast<NSString*>(value) isEqualToString:url]) {
                  found_link = YES;
                  *stop = YES;
                }
              }];
  return found_link;
}

// Expected minimum content height.
constexpr CGFloat kExpectedMinimumContentHeight = 300.0;

}  // namespace

// Test fixture for BWGConsentViewController.
class BWGConsentViewControllerTest : public PlatformTest {
 public:
  BWGConsentViewController* CreateViewController(BOOL is_account_managed) {
    BWGConsentViewController* controller = [[BWGConsentViewController alloc]
        initWithIsAccountManaged:is_account_managed];
    mock_mutator_ = OCMProtocolMock(@protocol(BWGConsentMutator));
    controller.mutator = mock_mutator_;
    // Force view initialization since this view controller is never added into
    // the hierarchy in this unit test.
    [controller view];
    [controller viewWillLayoutSubviews];
    return controller;
  }

 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  id mock_mutator_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests initialization with a managed account.
TEST_F(BWGConsentViewControllerTest, InitializationWithManagedAccount) {
  BWGConsentViewController* view_controller = CreateViewController(YES);

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests initialization with a non-managed account.
TEST_F(BWGConsentViewControllerTest, InitializationWithNonManagedAccount) {
  BWGConsentViewController* view_controller = CreateViewController(NO);

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests that contentHeight returns a value greater than the expected minimum
// content height.
TEST_F(BWGConsentViewControllerTest, ContentHeightReturnsValidValue) {
  BWGConsentViewController* view_controller = CreateViewController(NO);

  CGFloat contentHeight = [view_controller contentHeight];
  EXPECT_GT(contentHeight, kExpectedMinimumContentHeight);
}

// Tests that the primary button action calls the correct mutator method.
TEST_F(BWGConsentViewControllerTest, TestPrimaryButtonAction) {
  BWGConsentViewController* view_controller = CreateViewController(NO);
  OCMExpect([mock_mutator_ didConsentBWG]);

  UIButton* primaryButton =
      static_cast<UIButton*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgPrimaryButtonAccessibilityIdentifier));
  ASSERT_NE(nil, primaryButton);

  [primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that the secondary button action calls the correct mutator method.
TEST_F(BWGConsentViewControllerTest, TestSecondaryButtonAction) {
  BWGConsentViewController* view_controller = CreateViewController(NO);
  OCMExpect([mock_mutator_ didRefuseBWGConsent]);

  UIButton* secondaryButton =
      static_cast<UIButton*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgSecondaryButtonAccessibilityIdentifier));
  ASSERT_NE(nil, secondaryButton);

  [secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(mock_mutator_);
}

// Tests that tapping the primary button records the correct metrics.
TEST_F(BWGConsentViewControllerTest, PrimaryButtonRecordsMetrics) {
  BWGConsentViewController* view_controller = CreateViewController(NO);

  UIButton* primaryButton =
      static_cast<UIButton*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgPrimaryButtonAccessibilityIdentifier));
  ASSERT_NE(nil, primaryButton);

  [primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  histogram_tester_->ExpectUniqueSample(
      kConsentActionHistogram, static_cast<int>(IOSGeminiFREAction::kAccept),
      1);
}

// Tests that tapping the secondary button records the correct metrics.
TEST_F(BWGConsentViewControllerTest, SecondaryButtonRecordsMetrics) {
  BWGConsentViewController* view_controller = CreateViewController(NO);

  UIButton* secondaryButton =
      static_cast<UIButton*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgSecondaryButtonAccessibilityIdentifier));
  ASSERT_NE(nil, secondaryButton);

  [secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];

  histogram_tester_->ExpectUniqueSample(
      kConsentActionHistogram, static_cast<int>(IOSGeminiFREAction::kDismiss),
      1);
}

// Tests footnote links for non-managed accounts.
TEST_F(BWGConsentViewControllerTest, TestFootnoteLinksForNonManagedAccount) {
  BWGConsentViewController* view_controller = CreateViewController(NO);

  UITextView* footnoteView =
      static_cast<UITextView*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgFootNoteTextViewAccessibilityIdentifier));
  ASSERT_NE(nil, footnoteView);

  EXPECT_TRUE(HasLinkWithURL(footnoteView, kBwgFirstFootnoteLinkAction));
  EXPECT_TRUE(HasLinkWithURL(footnoteView, kBwgSecondFootnoteLinkAction));
}

// Tests footnote links for managed accounts.
TEST_F(BWGConsentViewControllerTest, TestFootnoteLinksForManagedAccount) {
  BWGConsentViewController* view_controller = CreateViewController(YES);

  UITextView* footnoteView =
      static_cast<UITextView*>(GetViewWithAccessibilityIdentifier(
          view_controller.view, kBwgFootNoteTextViewAccessibilityIdentifier));
  ASSERT_NE(nil, footnoteView);

  EXPECT_TRUE(
      HasLinkWithURL(footnoteView, kBwgFootnoteLinkActionManagedAccount));
}
