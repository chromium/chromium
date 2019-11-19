// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_constants.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_view_controller.h"
#import "ios/chrome/browser/ui/util/transparent_link_button.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

@interface FakeChromeSigninViewControllerDelegate
    : NSObject<ChromeSigninViewControllerDelegate>

@property(nonatomic) BOOL didSigninCalled;
@end

@implementation FakeChromeSigninViewControllerDelegate

@synthesize didSigninCalled = _didSigninCalled;

- (void)willStartSignIn:(ChromeSigninViewController*)controller {
}

- (void)willStartAddAccount:(ChromeSigninViewController*)controller {
}

- (void)didSkipSignIn:(ChromeSigninViewController*)controller {
}

- (void)didFailSignIn:(ChromeSigninViewController*)controller {
  FAIL();
}

- (void)didSignIn:(ChromeSigninViewController*)controller {
  ASSERT_FALSE(self.didSigninCalled);
  self.didSigninCalled = YES;
}

- (void)didUndoSignIn:(ChromeSigninViewController*)controller
             identity:(ChromeIdentity*)identity {
}

- (void)didAcceptSignIn:(ChromeSigninViewController*)controller
    showAccountsSettings:(BOOL)showAccountsSettings {
}

@end

namespace {

// Returns the first view in |mainView| with the accessibility identifier:
// |accessibilityID|.
UIView* FindViewWithAccessibilityID(UIView* mainView,
                                    NSString* accessibilityID) {
  NSMutableArray* views = [NSMutableArray array];
  [views addObject:mainView];
  while (views.count > 0) {
    UIView* view = [views objectAtIndex:0];
    [views removeObjectAtIndex:0];
    [views addObjectsFromArray:view.subviews];
    if ([view.accessibilityIdentifier isEqualToString:accessibilityID]) {
      return view;
    }
  }
  return nil;
}

// Returns the Settings link button to open the advanced sign-in settings view.
UIButton* FindLinkButton(UIView* mainView) {
  UIView* view = FindViewWithAccessibilityID(
      mainView, kAdvancedSigninSettingsLinkIdentifier);
  EXPECT_NE(nil, view);
  return base::mac::ObjCCastStrict<UIButton>(view);
}

static std::unique_ptr<KeyedService> CreateFakeConsentAuditor(
    web::BrowserState* context) {
  return std::make_unique<consent_auditor::FakeConsentAuditor>();
}

static std::unique_ptr<KeyedService> CreateFakeUnifiedConsentService(
    web::BrowserState* context) {
  return nullptr;
}

// These tests verify that Chrome correctly records user's consent to Chrome
// Sync, which is a GDPR requirement. None of those tests should be turned off.
// If one of those tests fails, one of the following methods should be updated
// with the added or removed strings:
//   - ExpectedConsentStringIds()
//   - WhiteListLocalizedStrings()
class ChromeSigninViewControllerTest
    : public PlatformTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    identity_ = [FakeChromeIdentity identityWithEmail:@"foo1@gmail.com"
                                               gaiaID:@"foo1ID"
                                                 name:@"Fake Foo 1"];
    // Setup services.
    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(
        AuthenticationServiceFactory::GetInstance(),
        base::BindRepeating(
            &AuthenticationServiceFake::CreateAuthenticationService));
    builder.AddTestingFactory(ConsentAuditorFactory::GetInstance(),
                              base::BindRepeating(&CreateFakeConsentAuditor));
    builder.AddTestingFactory(
        UnifiedConsentServiceFactory::GetInstance(),
        base::BindRepeating(&CreateFakeUnifiedConsentService));
    browser_state_ = builder.Build();
    WebStateList* web_state_list = nullptr;
    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), web_state_list);

    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentity(identity_);
    identity_manager_ =
        IdentityManagerFactory::GetForBrowserState(browser_state_.get());
    fake_consent_auditor_ = static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForBrowserState(browser_state_.get()));

    // Setup view controller.
    vc_ = [[ChromeSigninViewController alloc]
        initWithBrowser:browser_.get()
            accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
            promoAction:signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
         signInIdentity:identity_
             dispatcher:nil];
    vc_delegate_ = [[FakeChromeSigninViewControllerDelegate alloc] init];
    vc_.delegate = vc_delegate_;
    UIScreen* screen = [UIScreen mainScreen];
    UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
    [window makeKeyAndVisible];
    [window addSubview:[vc_ view]];
    window_ = window;
  }

  // Adds in |string_set|, all the strings displayed by |view| and its subviews,
  // recursively.
  static void AddStringsFromView(NSMutableSet<NSString*>* string_set,
                                 UIView* view) {
    for (UIView* subview in view.subviews)
      AddStringsFromView(string_set, subview);
    if ([view isKindOfClass:[UIButton class]]) {
      UIButton* button = static_cast<UIButton*>(view);
      if (button.currentTitle)
        [string_set addObject:button.currentTitle];
    } else if ([view isKindOfClass:[UILabel class]]) {
      UILabel* label = static_cast<UILabel*>(view);
      if (label.text)
        [string_set addObject:label.text];
    } else {
      NSString* view_name = NSStringFromClass([view class]);
      // Views that don't display strings.
      NSArray* other_views = @[
        @"LegacyAccountControlCell",
        @"CollectionViewFooterCell",
        @"IdentityPickerView",
        @"IdentityView",
        @"UIButtonLabel",
        @"MDCActivityIndicator",
        @"MDCButtonBar",
        @"MDCFlexibleHeaderView",
        @"MDCHeaderStackView",
        @"MDCInkView",
        @"MDCNavigationBar",
        @"UICollectionView",
        @"UICollectionViewControllerWrapperView",
        @"UIImageView",
        @"UIScrollView",
        @"UIView",
        @"_UIScrollViewScrollIndicator",
      ];
      // If this test fails, the unknown class should be added in other_views if
      // it doesn't display any strings, otherwise the strings diplay by this
      // class should be added in string_set.
      EXPECT_TRUE([other_views containsObject:view_name])
          << base::SysNSStringToUTF8(view_name);
    }
  }

  // Returns the set of strings displayed on the screen based on the views
  // displayed by the .
  NSSet<NSString*>* LocalizedStringOnScreen() const {
    NSMutableSet* string_set = [NSMutableSet set];
    AddStringsFromView(string_set, vc_.view);
    return string_set;
  }

  // Returns a localized string based on the string id.
  NSString* LocalizedStringFromID(int string_id) const {
    NSString* string = l10n_util::GetNSString(string_id);
    string = [string stringByReplacingOccurrencesOfString:@"BEGIN_LINK"
                                               withString:@""];
    string = [string stringByReplacingOccurrencesOfString:@"END_LINK"
                                               withString:@""];
    return string;
  }

  // Returns all the strings on screen that should be part of the user consent
  // and part of the white list strings.
  NSSet<NSString*>* LocalizedExpectedStringsOnScreen() const {
    const std::vector<int>& string_ids = ExpectedConsentStringIds();
    NSMutableSet<NSString*>* set = [NSMutableSet set];
    for (auto it = string_ids.begin(); it != string_ids.end(); ++it) {
      [set addObject:LocalizedStringFromID(*it)];
    }
    [set unionSet:WhiteListLocalizedStrings()];
    return set;
  }

  // Returns the list of string id that should be given to RecordGaiaConsent()
  // then the consent is given. The list is ordered according to the position
  // on the screen.
  const std::vector<int> ExpectedConsentStringIds() const {
    return {
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_TITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_SUBTITLE,
        IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS,
    };
  }

  // Returns the white list of strings that can be displayed on screen but
  // should not be part of ExpectedConsentStringIds().
  NSSet<NSString*>* WhiteListLocalizedStrings() const {
    return [NSSet setWithObjects:@"Fake Foo 1", @"foo1@gmail.com", @"CANCEL",
                                 @"YES, I'M IN", nil];
  }

  // Returns true if the primary button is visible and its tile is equal the
  // |string_id| (case insensitive).
  bool IsPrimaryButtonVisibleWithTitle(int string_id) {
    if (vc_.primaryButton.isHidden)
      return false;
    NSString* primary_title = vc_.primaryButton.currentTitle;
    return [primary_title
               caseInsensitiveCompare:l10n_util::GetNSString(string_id)] ==
           NSOrderedSame;
  }

  // Returns |view| if it is kind of UIScrollView or returns the UIScrollView-
  // kind in the subviews (recursive search). At most one UIScrollView is
  // expected.
  UIScrollView* FindConsentScrollView(UIView* view) {
    if ([view isKindOfClass:[UIScrollView class]])
      return base::mac::ObjCCastStrict<UIScrollView>(view);
    UIScrollView* found_scroll_view = nil;
    for (UIView* subview in view.subviews) {
      UIScrollView* scroll_view_from_subview = FindConsentScrollView(subview);
      if (scroll_view_from_subview) {
        EXPECT_EQ(nil, found_scroll_view);
        found_scroll_view = scroll_view_from_subview;
      }
    }
    return found_scroll_view;
  }

  // Scrolls to the bottom if needed and returns once the primary button is
  // found with the confirmation title.
  // The scroll is done without animation. Otherwise, the scroll view doesn't
  // scroll correctly inside WaitUntilConditionOrTimeout().
  void ScrollConsentViewToBottom() {
    ConditionBlock condition = ^bool() {
      if (IsPrimaryButtonVisibleWithTitle(
              IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SCROLL_BUTTON)) {
        UIScrollView* consent_scroll_view = FindConsentScrollView(vc_.view);
        CGPoint bottom_offset =
            CGPointMake(0, consent_scroll_view.contentSize.height -
                               consent_scroll_view.bounds.size.height +
                               consent_scroll_view.contentInset.bottom);
        [consent_scroll_view setContentOffset:bottom_offset animated:NO];
      }
      return IsPrimaryButtonVisibleWithTitle(
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON);
    };
    bool condition_met =
        WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
    EXPECT_TRUE(condition_met);
  }

  // Waits until all expected strings are on the screen.
  void WaitAndExpectAllStringsOnScreen() {
    __block NSSet<NSString*>* not_found_strings = nil;
    __block NSSet<NSString*>* not_expected_strings = nil;
    // Make sure the consent view is scrolled to the button to show the
    // confirmation button (instead of the "more" button).
    ScrollConsentViewToBottom();
    ConditionBlock condition = ^bool() {
      NSSet<NSString*>* found_strings = LocalizedStringOnScreen();
      NSSet<NSString*>* expected_strings = LocalizedExpectedStringsOnScreen();
      not_found_strings = [expected_strings
          objectsPassingTest:^BOOL(NSString* string, BOOL* stop) {
            return ![found_strings containsObject:string];
          }];
      not_expected_strings = [found_strings
          objectsPassingTest:^BOOL(NSString* string, BOOL* stop) {
            return ![expected_strings containsObject:string];
          }];
      return [found_strings isEqual:expected_strings];
    };
    bool condition_met =
        WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
    NSString* failureExplaination = [NSString
        stringWithFormat:@"Strings not found: %@, Strings not expected: %@",
                         not_found_strings, not_expected_strings];
    EXPECT_TRUE(condition_met) << base::SysNSStringToUTF8(failureExplaination);
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<Browser> browser_;
  FakeChromeIdentity* identity_;
  UIWindow* window_;
  ChromeSigninViewController* vc_;
  consent_auditor::FakeConsentAuditor* fake_consent_auditor_;
  signin::IdentityManager* identity_manager_;
  FakeChromeSigninViewControllerDelegate* vc_delegate_;
};

// Tests that all strings on the screen are either part of the consent string
// list defined in FakeConsentAuditor::ExpectedConsentStringIds()), or are part
// of the white list strings defined in
// FakeConsentAuditor::WhiteListLocalizedStrings().
TEST_F(ChromeSigninViewControllerTest, TestAllStrings) {
  WaitAndExpectAllStringsOnScreen();
}

// Tests when the user taps on "OK GOT IT", that RecordGaiaConsent() is called
// with the expected list of string ids, and confirmation string id.
TEST_F(ChromeSigninViewControllerTest, TestConsentWithOKGOTIT) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  ConditionBlock condition = ^bool() {
    return this->vc_delegate_.didSigninCalled;
  };
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition));
  const std::vector<int>& recorded_ids =
      fake_consent_auditor_->recorded_id_vectors().at(0);
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON,
            fake_consent_auditor_->recorded_confirmation_ids().at(0));
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->recorded_statuses().at(0));
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->recorded_features().at(0));
  EXPECT_EQ(identity_manager_->PickAccountIdForAccount(
                base::SysNSStringToUTF8([identity_ gaiaID]),
                base::SysNSStringToUTF8([identity_ userEmail])),
            fake_consent_auditor_->account_id());
}

// Tests that RecordGaiaConsent() is not called when the user taps on UNDO.
TEST_F(ChromeSigninViewControllerTest, TestRefusingConsent) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(0ul, fake_consent_auditor_->recorded_id_vectors().size());
  EXPECT_EQ(0ul, fake_consent_auditor_->recorded_confirmation_ids().size());
}

// Tests that RecordGaiaConsent() is called with the expected list of string
// ids, and settings confirmation string id.
TEST_F(ChromeSigninViewControllerTest, TestConsentWithSettings) {
  WaitAndExpectAllStringsOnScreen();
  UIButton* linkButton = FindLinkButton(vc_.view);
  EXPECT_NE(nil, linkButton);
  [linkButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  ConditionBlock condition = ^bool() {
    return this->vc_delegate_.didSigninCalled;
  };
  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition));
  const std::vector<int>& recorded_ids =
      fake_consent_auditor_->recorded_id_vectors().at(0);
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS,
            fake_consent_auditor_->recorded_confirmation_ids().at(0));
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->recorded_statuses().at(0));
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->recorded_features().at(0));
  EXPECT_EQ(identity_manager_->PickAccountIdForAccount(
                base::SysNSStringToUTF8([identity_ gaiaID]),
                base::SysNSStringToUTF8([identity_ userEmail])),
            fake_consent_auditor_->account_id());
}

}  // namespace
