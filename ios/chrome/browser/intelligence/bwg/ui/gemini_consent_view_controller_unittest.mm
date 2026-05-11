// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/ui/gemini_consent_view_controller.h"

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
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

}  // namespace

// Test fixture for GeminiConsentViewController.
class GeminiConsentViewControllerTest : public PlatformTest {
 public:
  GeminiConsentViewController* CreateViewController(
      BOOL is_account_managed,
      NSString* country = nil,
      BOOL use_strict_consent = NO) {
    GeminiConsentViewController* controller =
        [[GeminiConsentViewController alloc]
            initWithIsAccountManaged:is_account_managed
               useStrictLegalConsent:use_strict_consent
                             FREType:GeminiFREType::kNewUser
                             country:country];
    // Force view initialization since this view controller is never added into
    // the hierarchy in this unit test.
    [controller view];
    [controller viewWillLayoutSubviews];
    return controller;
  }
};

// Tests initialization with a managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(YES, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests initialization with a non-managed account.
TEST_F(GeminiConsentViewControllerTest, InitializationWithNonManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");

  EXPECT_NE(nil, view_controller);
  EXPECT_TRUE(view_controller.view);
  EXPECT_TRUE(view_controller.navigationItem.hidesBackButton);
}

// Tests footnote links for non-managed accounts.
TEST_F(GeminiConsentViewControllerTest, TestFootnoteLinksForNonManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"us");

  UITextView* footnoteView =
      static_cast<UITextView*>(GetViewWithAccessibilityIdentifier(
          view_controller.view,
          kGeminiFootNoteTextViewAccessibilityIdentifier));
  ASSERT_NE(nil, footnoteView);

  EXPECT_TRUE(HasLinkWithURL(footnoteView, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(HasLinkWithURL(footnoteView, kGeminiSecondFootnoteLinkAction));
}

// Tests footnote links for managed accounts.
TEST_F(GeminiConsentViewControllerTest, TestFootnoteLinksForManagedAccount) {
  GeminiConsentViewController* view_controller =
      CreateViewController(YES, @"us");

  UITextView* footnoteView =
      static_cast<UITextView*>(GetViewWithAccessibilityIdentifier(
          view_controller.view,
          kGeminiFootNoteTextViewAccessibilityIdentifier));
  ASSERT_NE(nil, footnoteView);

  EXPECT_TRUE(
      HasLinkWithURL(footnoteView, kGeminiFootnoteLinkActionManagedAccount));
}

// Tests footnote links for South Korea.
TEST_F(GeminiConsentViewControllerTest, TestFootnoteLinksForSouthKorea) {
  GeminiConsentViewController* view_controller =
      CreateViewController(NO, @"kr");

  UITextView* footnoteView =
      static_cast<UITextView*>(GetViewWithAccessibilityIdentifier(
          view_controller.view,
          kGeminiFootNoteTextViewAccessibilityIdentifier));
  ASSERT_NE(nil, footnoteView);

  EXPECT_TRUE(HasLinkWithURL(footnoteView, kGeminiFirstFootnoteLinkAction));
  EXPECT_TRUE(HasLinkWithURL(footnoteView, kGeminiKoreanTermsLinkAction));
  EXPECT_TRUE(HasLinkWithURL(footnoteView, kGeminiSecondFootnoteLinkAction));
}
