// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/chrome_signin_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/consent_auditor/fake_consent_auditor.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/consent_auditor_factory.h"
#include "ios/chrome/browser/sync/ios_user_event_service_factory.h"
#include "ios/chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

const bool kUnifiedConsentParam[] = {
    false, true,
};

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
 public:
  ChromeSigninViewControllerTest()
      : unified_consent_enabled_(GetParam()),
        scoped_unified_consent_(
            unified_consent_enabled_
                ? unified_consent::UnifiedConsentFeatureState::kEnabledNoBump
                : unified_consent::UnifiedConsentFeatureState::kDisabled) {}

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
    context_ = builder.Build();
    ios::FakeChromeIdentityService* identity_service =
        ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
    identity_service->AddIdentity(identity_);
    account_tracker_service_ =
        ios::AccountTrackerServiceFactory::GetForBrowserState(context_.get());

    fake_consent_auditor_ = static_cast<consent_auditor::FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForBrowserState(context_.get()));

    // Setup view controller.
    vc_ = [[ChromeSigninViewController alloc]
        initWithBrowserState:context_.get()
                 accessPoint:signin_metrics::AccessPoint::ACCESS_POINT_SETTINGS
                 promoAction:signin_metrics::PromoAction::
                                 PROMO_ACTION_WITH_DEFAULT
              signInIdentity:identity_
                  dispatcher:nil];
    vc_delegate_ = [[FakeChromeSigninViewControllerDelegate alloc] init];
    vc_.delegate = vc_delegate_;
    __block base::MockOneShotTimer* mock_timer_ptr = nullptr;
    if (!unified_consent_enabled_) {
      vc_.timerGenerator = ^std::unique_ptr<base::OneShotTimer>() {
        auto mock_timer = std::make_unique<base::MockOneShotTimer>();
        mock_timer_ptr = mock_timer.get();
        return mock_timer;
      };
    }
    UIScreen* screen = [UIScreen mainScreen];
    UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
    [window makeKeyAndVisible];
    [window addSubview:[vc_ view]];
    if (!unified_consent_enabled_) {
      ASSERT_TRUE(mock_timer_ptr);
      mock_timer_ptr->Fire();
    }
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
        @"AccountControlCell",
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
    if (unified_consent_enabled_) {
      return {
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_TITLE,
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SYNC_DATA,
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_PERSONALIZED,
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_BETTER_BROWSER,
          IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS,
      };
    }
    return {
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_TITLE,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SYNC_DESCRIPTION,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_TITLE,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_SERVICES_DESCRIPTION,
        IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS,
    };
  }

  // Returns the white list of strings that can be displayed on screen but
  // should not be part of ExpectedConsentStringIds().
  NSSet<NSString*>* WhiteListLocalizedStrings() const {
    if (unified_consent_enabled_) {
      return [NSSet setWithObjects:@"Fake Foo 1", @"foo1@gmail.com", @"CANCEL",
                                   @"YES, I'M IN", nil];
    }
    return [NSSet setWithObjects:@"Hi, Fake Foo 1", @"foo1@gmail.com",
                                 @"OK, GOT IT", @"UNDO", nil];
  }

  int ConfirmationStringId() const {
    if (unified_consent_enabled_) {
      return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
    }
    return IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON;
  }

  int SettingsConfirmationStringId() const {
    if (unified_consent_enabled_) {
      return IDS_IOS_ACCOUNT_UNIFIED_CONSENT_SETTINGS;
    }
    return IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OPEN_SETTINGS;
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
  // found with the confirmation title (based on ConfirmationStringId()).
  // The scroll is done without animation. Otherwise, the scroll view doesn't
  // scroll correctly inside base::test::ios::WaitUntilConditionOrTimeout().
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
      return IsPrimaryButtonVisibleWithTitle(ConfirmationStringId());
    };
    bool condition_met =
        base::test::ios::WaitUntilConditionOrTimeout(10, condition);
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
        base::test::ios::WaitUntilConditionOrTimeout(10, condition);
    NSString* failureExplaination = [NSString
        stringWithFormat:@"Strings not found: %@, Strings not expected: %@",
                         not_found_strings, not_expected_strings];
    EXPECT_TRUE(condition_met) << base::SysNSStringToUTF8(failureExplaination);
  }

  bool unified_consent_enabled_;
  unified_consent::ScopedUnifiedConsent scoped_unified_consent_;
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> context_;
  FakeChromeIdentity* identity_;
  UIWindow* window_;
  ChromeSigninViewController* vc_;
  consent_auditor::FakeConsentAuditor* fake_consent_auditor_;
  AccountTrackerService* account_tracker_service_;
  base::MockOneShotTimer* mock_timer_ptr_ = nullptr;
  FakeChromeSigninViewControllerDelegate* vc_delegate_;
};

INSTANTIATE_TEST_CASE_P(,
                        ChromeSigninViewControllerTest,
                        ::testing::ValuesIn(kUnifiedConsentParam));

// Tests that all strings on the screen are either part of the consent string
// list defined in FakeConsentAuditor::ExpectedConsentStringIds()), or are part
// of the white list strings defined in
// FakeConsentAuditor::WhiteListLocalizedStrings().
TEST_P(ChromeSigninViewControllerTest, TestAllStrings) {
  WaitAndExpectAllStringsOnScreen();
}

// Tests when the user taps on "OK GOT IT", that RecordGaiaConsent() is called
// with the expected list of string ids, and confirmation string id.
TEST_P(ChromeSigninViewControllerTest, TestConsentWithOKGOTIT) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.primaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  ConditionBlock condition = ^bool() {
    return this->vc_delegate_.didSigninCalled;
  };
  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(10, condition));
  const std::vector<int>& recorded_ids =
      fake_consent_auditor_->recorded_id_vectors().at(0);
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(ConfirmationStringId(),
            fake_consent_auditor_->recorded_confirmation_ids().at(0));
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->recorded_statuses().at(0));
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->recorded_features().at(0));
  EXPECT_EQ(account_tracker_service_->PickAccountIdForAccount(
                base::SysNSStringToUTF8([identity_ gaiaID]),
                base::SysNSStringToUTF8([identity_ userEmail])),
            fake_consent_auditor_->account_id());
}

// Tests that RecordGaiaConsent() is not called when the user taps on UNDO.
TEST_P(ChromeSigninViewControllerTest, TestRefusingConsent) {
  WaitAndExpectAllStringsOnScreen();
  [vc_.secondaryButton sendActionsForControlEvents:UIControlEventTouchUpInside];
  EXPECT_EQ(0ul, fake_consent_auditor_->recorded_id_vectors().size());
  EXPECT_EQ(0ul, fake_consent_auditor_->recorded_confirmation_ids().size());
}

// Tests that RecordGaiaConsent() is called with the expected list of string
// ids, and settings confirmation string id.
TEST_P(ChromeSigninViewControllerTest, TestConsentWithSettings) {
  WaitAndExpectAllStringsOnScreen();
  [vc_ signinConfirmationControllerDidTapSettingsLink:vc_.confirmationVC];
  const std::vector<int>& recorded_ids =
      fake_consent_auditor_->recorded_id_vectors().at(0);
  EXPECT_EQ(ExpectedConsentStringIds(), recorded_ids);
  EXPECT_EQ(SettingsConfirmationStringId(),
            fake_consent_auditor_->recorded_confirmation_ids().at(0));
  EXPECT_EQ(consent_auditor::ConsentStatus::GIVEN,
            fake_consent_auditor_->recorded_statuses().at(0));
  EXPECT_EQ(consent_auditor::Feature::CHROME_SYNC,
            fake_consent_auditor_->recorded_features().at(0));
  EXPECT_EQ(account_tracker_service_->PickAccountIdForAccount(
                base::SysNSStringToUTF8([identity_ gaiaID]),
                base::SysNSStringToUTF8([identity_ userEmail])),
            fake_consent_auditor_->account_id());
}

}  // namespace
