// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <vector>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/first_run/first_run_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/mac/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

const char kPageLoadedString[] = "Page loaded!";
const char kPageURL[] = "/test-page.html";
const char kPageTitle[] = "Page title!";

// Provides responses for redirect and changed window location URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kPageURL) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("<html><head><title>" + std::string(kPageTitle) +
                             "</title></head><body>" +
                             std::string(kPageLoadedString) + "</body></html>");
  return std::move(http_response);
}

// Matcher for the SetUpList.
id<GREYMatcher> SetUpList() {
  return grey_allOf(grey_accessibilityID(set_up_list::kAccessibilityID),
                    grey_sufficientlyVisible(), nil);
}

// Returns matcher for the secondary action button.
id<GREYMatcher> SetUpListAllSet() {
  return grey_accessibilityID(set_up_list::kAllSetID);
}

// Scrolls to the SetUpList, if it is off-screen.
void ScrollToSetUpList() {
  [[[EarlGrey selectElementWithMatcher:SetUpList()]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
}

// Taps the view with the given `accessibility_id`.
void TapView(NSString* accessibility_id) {
  id<GREYMatcher> matcher = grey_accessibilityID(accessibility_id);
  [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
}

// Tap the PromoStylePrimaryActionButton.
void TapPromoStylePrimaryActionButton() {
  id<GREYMatcher> button =
      grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:button] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Tap the PromoStyleSecondaryActionButton.
void TapPromoStyleSecondaryActionButton() {
  id<GREYMatcher> button =
      grey_accessibilityID(kPromoStyleSecondaryActionAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:button] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Tap the ConfirmationAlertSecondaryAction Button.
void TapSecondaryActionButton() {
  id<GREYMatcher> button = grey_accessibilityID(
      kConfirmationAlertSecondaryActionAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:button] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Tap the SetUpList button to expand the list.
void TapSetUpListExpand() {
  id<GREYMatcher> expandButton =
      grey_allOf(grey_accessibilityID(set_up_list::kExpandButtonID),
                 grey_sufficientlyVisible(), nil);
  [[[EarlGrey selectElementWithMatcher:expandButton]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:chrome_test_util::NTPCollectionView()]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:expandButton] performAction:grey_tap()];
}

// Tap the "More" button if it is visible.
void TapMoreButtonIfVisible() {
  id<GREYInteraction> button =
      [EarlGrey selectElementWithMatcher:
                    grey_accessibilityID(
                        @"PromoStyleReadMoreActionAccessibilityIdentifier")];
  NSError* error;
  [button assertWithMatcher:grey_notNil() error:&error];
  if (!error) {
    [button performAction:grey_tap()];
  }
}

}  // namespace

#pragma mark - TestCase

// Test case for the ContentSuggestion UI.
@interface ContentSuggestionsTestCase : ChromeTestCase

@end

@implementation ContentSuggestionsTestCase

#pragma mark - Setup/Teardown

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(kEnableFeedAblation);
  if ([self isRunningTest:@selector
            (testSetUpListDismissItemsWithSyncToSigninDisabled)] ||
      [self isRunningTest:@selector
            (testSetUpListSigninWithSyncToSigninDisabled)] ||
      [self isRunningTest:@selector
            (testSetUpListSigninSwipeToDismissWithSyncToSigninDisabled)]) {
    config.features_disabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
  }
  if ([self isRunningTest:@selector
            (testSetUpListDismissItemsWithSyncToSigninEnabled)] ||
      [self isRunningTest:@selector
            (testSetUpListSigninWithSyncToSigninEnabled)]) {
    config.features_enabled.push_back(
        syncer::kReplaceSyncPromosWithSignInPromos);
    config.features_enabled.push_back(kConsistencyNewAccountInterface);
  }
  if ([self isRunningTest:@selector(testMagicStackSetUpListCompleteAllItems)] ||
      [self isRunningTest:@selector(testMagicStackEditButton)]) {
    config.features_enabled.push_back(kMagicStack);
  } else {
    config.features_disabled.push_back(kMagicStack);
  }
  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}

+ (void)setUpHelper {
  [self closeAllTabs];
}

+ (void)tearDown {
  [self closeAllTabs];

  [super tearDown];
}

- (void)setUp {
  [super setUp];
}

- (void)tearDown {
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGreyAppInterface removeFirstRunSentinel];
  [super tearDown];
}

#pragma mark - Tests

// Tests the "Open in New Tab" action of the Most Visited context menu.
- (void)testMostVisitedNewTab {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Open in new tab.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)]
      performAction:grey_tap()];

  // Check a new page in normal model is opened.
  [ChromeEarlGrey waitForMainTabCount:2];
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Check that the tab has been opened in background.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:chrome_test_util::NTPCollectionView()]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             @"Collection view not visible");

  // Check the page has been correctly opened.
  [ChromeEarlGrey selectTabAtIndex:1];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests the "Open in New Incognito Tab" action of the Most Visited context
// menu.
- (void)testMostVisitedNewIncognitoTab {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);

  // Open in new incognito tab.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::OpenLinkInIncognitoButton()]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:1];
  [ChromeEarlGrey waitForIncognitoTabCount:1];

  // Check that the tab has been opened in foreground.
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Test did not switch to incognito");
}

// action.
- (void)testMostVisitedRemoveUndo {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Tap on remove.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_REMOVE)]
      performAction:grey_tap()];

  // Check the tile is removed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];

  // Check the snack bar notifying the user that an element has been removed is
  // displayed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier")]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Tap on undo.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_NEW_TAB_UNDO_THUMBNAIL_REMOVE)]
      performAction:grey_tap()];

  // Check the tile is back.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(
                chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
                grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  NSString* errorMessage =
      @"The tile wasn't added back after hitting 'Undo' on the snackbar";
  GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(
                 base::test::ios::kWaitForUIElementTimeout, condition),
             errorMessage);

  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(
                                pageTitle),
                            grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_notNil()];
}

// Tests that the context menu has the correct actions.
- (void)testMostVisitedLongPress {
  [self setupMostVisitedTileLongPress];

  // No "Add to Reading List" item.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Set Up List tests

// Tests that the SetUpList can be expanded and unexpanded by touching the
// "expand" button at the bottom of the list.
- (void)testSetUpListExpands {
  [self prepareToTestSetUpList];

  id<GREYMatcher> signinItem = grey_accessibilityID(set_up_list::kSignInItemID);
  id<GREYMatcher> defaultBrowserItem =
      grey_accessibilityID(set_up_list::kDefaultBrowserItemID);
  id<GREYMatcher> autofillItem =
      grey_accessibilityID(set_up_list::kAutofillItemID);
  [[EarlGrey selectElementWithMatcher:signinItem]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:defaultBrowserItem]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:autofillItem]
      assertWithMatcher:grey_nil()];

  TapSetUpListExpand();
  ScrollToSetUpList();

  // Autofill item should appear.
  [[EarlGrey selectElementWithMatcher:autofillItem]
      assertWithMatcher:grey_notNil()];

  TapSetUpListExpand();

  // Autofill item should disappear.
  [[EarlGrey selectElementWithMatcher:autofillItem]
      assertWithMatcher:grey_nil()];
}

// Tests that each item opens the appropriate UI flow and that dismissing that
// UI marks the item complete. Also tests that the "All Set" view appears when
// all items are complete.
- (void)testSetUpListDismissItemsWithSyncToSigninDisabled {
  [self prepareToTestSetUpList];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  // Verify the signin screen appears and touch "Don't Sign In".
  id<GREYMatcher> signinView = grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:signinView]
      assertWithMatcher:grey_notNil()];
  // Dismiss the signin view.
  TapPromoStyleSecondaryActionButton();
  // Verify the signin item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemSignInSyncIsComplete],
                 @"SetUpList item SignIn not completed.");

  // Tap the default browser item.
  TapView(set_up_list::kDefaultBrowserItemID);
  // Ensure the Default Browser Promo is displayed.
  id<GREYMatcher> defaultBrowserView = grey_accessibilityID(
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:defaultBrowserView]
      assertWithMatcher:grey_notNil()];
  // Dismiss Default Browser Promo.
  TapPromoStyleSecondaryActionButton();
  // Verify the default browser item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemDefaultBrowserIsComplete],
                 @"SetUpList item Default Browser not completed.");

  TapSetUpListExpand();
  ScrollToSetUpList();

  // Tap the autofill item.
  TapView(set_up_list::kAutofillItemID);
  // TODO - verify the CPE promo is displayed.
  id<GREYMatcher> CPEPromoView =
      grey_accessibilityID(@"kCredentialProviderPromoAccessibilityId");
  [[EarlGrey selectElementWithMatcher:CPEPromoView]
      assertWithMatcher:grey_notNil()];
  // Dismiss the CPE promo.
  TapSecondaryActionButton();
  // Verify the Autofill item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemAutofillIsComplete],
                 @"SetUpList item Autofill not completed.");

  // Verify All Set view appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SetUpListAllSet()];

  // Close NTP and reopen.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  // SetUpList is still visible.
  [[EarlGrey selectElementWithMatcher:SetUpList()]
      assertWithMatcher:grey_notNil()];

  // Close NTP and reopen. SetUpList should not be visible.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  // SetUpList is not visible.
  [[EarlGrey selectElementWithMatcher:SetUpList()]
      assertWithMatcher:grey_nil()];
}

// Tests that each item opens the appropriate UI flow and that dismissing that
// UI marks the item complete. Also tests that the "All Set" view appears when
// all items are complete.
- (void)testSetUpListDismissItemsWithSyncToSigninEnabled {
  [self prepareToTestSetUpList];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  // Verify the signin screen appears.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Dismiss the signin view.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];
  // Verify the signin item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemSignInSyncIsComplete],
                 @"SetUpList item SignIn not completed.");

  // Tap the default browser item.
  TapView(set_up_list::kDefaultBrowserItemID);
  // Ensure the Default Browser Promo is displayed.
  id<GREYMatcher> defaultBrowserView = grey_accessibilityID(
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:defaultBrowserView]
      assertWithMatcher:grey_notNil()];
  // Dismiss Default Browser Promo.
  TapPromoStyleSecondaryActionButton();
  // Verify the default browser item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemDefaultBrowserIsComplete],
                 @"SetUpList item Default Browser not completed.");

  TapSetUpListExpand();
  ScrollToSetUpList();

  // Tap the autofill item.
  TapView(set_up_list::kAutofillItemID);
  // TODO - verify the CPE promo is displayed.
  id<GREYMatcher> CPEPromoView =
      grey_accessibilityID(@"kCredentialProviderPromoAccessibilityId");
  [[EarlGrey selectElementWithMatcher:CPEPromoView]
      assertWithMatcher:grey_notNil()];
  // Dismiss the CPE promo.
  TapSecondaryActionButton();
  // Verify the Autofill item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemAutofillIsComplete],
                 @"SetUpList item Autofill not completed.");

  // Verify All Set view appears.
  [ChromeEarlGrey waitForUIElementToAppearWithMatcher:SetUpListAllSet()];

  // Close NTP and reopen.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  // SetUpList is still visible.
  [[EarlGrey selectElementWithMatcher:SetUpList()]
      assertWithMatcher:grey_notNil()];

  // Close NTP and reopen. SetUpList should not be visible.
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  // SetUpList is not visible.
  [[EarlGrey selectElementWithMatcher:SetUpList()]
      assertWithMatcher:grey_nil()];
}

// Tests that the signin UI flow works and that the signin item is marked
// complete when signin is completed.
- (void)testSetUpListSigninWithSyncToSigninDisabled {
  [self prepareToTestSetUpList];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  // Verify the signin screen appears and touch "Continue as ...".
  id<GREYMatcher> signinView = grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:signinView]
      assertWithMatcher:grey_notNil()];
  // Tap "Continue as ...".
  TapPromoStylePrimaryActionButton();

  // Verify the tangible sync screen appears.
  id<GREYMatcher> syncView =
      grey_accessibilityID(kTangibleSyncViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:syncView]
      assertWithMatcher:grey_notNil()];
  // On small screens, we may need to tap the "More" button.
  TapMoreButtonIfVisible();
  // Tap "Yes, I'm in".
  TapPromoStylePrimaryActionButton();

  // Verify the signin item is complete.
  GREYAssertTrue([NewTabPageAppInterface setUpListItemSignInSyncIsComplete],
                 @"SetUpList item SignIn not completed.");
}

// Tests that the signin UI flow works and that the signin item is marked
// complete when signin is completed.
- (void)testSetUpListSigninWithSyncToSigninEnabled {
  [self prepareToTestSetUpList];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);

  // The signin screen should appear. Tap it.
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kWebSigninPrimaryButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  GREYAssertTrue([NewTabPageAppInterface setUpListItemSignInSyncIsComplete],
                 @"SetUpList item SignIn not completed.");
}

// Tests that the signin and sync screens can be dismissed by a swipe.
// Note that if SyncToSignin is enabled, then the signin screen is replaced
// by a bottom sheet which can't be dismissed by swiping, so this test
// doesn't apply.
- (void)testSetUpListSigninSwipeToDismissWithSyncToSigninDisabled {
  [self prepareToTestSetUpList];
  [SigninEarlGrey addFakeIdentity:[FakeSystemIdentity fakeIdentity1]];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  // Verify the signin screen appears.
  id<GREYMatcher> signinView = grey_accessibilityID(
      first_run::kFirstRunSignInScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:signinView]
      assertWithMatcher:grey_notNil()];
  // Swipe to dismiss the signin screen.
  [[EarlGrey selectElementWithMatcher:signinView]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Verify that the signin screen is gone.
  [[EarlGrey selectElementWithMatcher:signinView] assertWithMatcher:grey_nil()];

  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  // Verify the signin screen appears.
  [[EarlGrey selectElementWithMatcher:signinView]
      assertWithMatcher:grey_notNil()];
  // Tap "Continue as ...".
  TapPromoStylePrimaryActionButton();
  // Verify the tangible sync screen appears.
  id<GREYMatcher> syncView =
      grey_accessibilityID(kTangibleSyncViewAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:syncView]
      assertWithMatcher:grey_notNil()];
  // Swipe to dismiss the sync screen.
  [[EarlGrey selectElementWithMatcher:syncView]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  // Verify that the sync screen is gone.
  [[EarlGrey selectElementWithMatcher:syncView] assertWithMatcher:grey_nil()];
}

- (void)testMagicStackSetUpListCompleteAllItems {
  [self prepareToTestSetUpListInMagicStack];

  // Tap the default browser item.
  TapView(set_up_list::kDefaultBrowserItemID);
  // Ensure the Default Browser Promo is displayed.
  id<GREYMatcher> defaultBrowserView = grey_accessibilityID(
      first_run::kFirstRunDefaultBrowserScreenAccessibilityIdentifier);
  [[EarlGrey selectElementWithMatcher:defaultBrowserView]
      assertWithMatcher:grey_notNil()];
  // Dismiss Default Browser Promo.
  TapPromoStyleSecondaryActionButton();

  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                set_up_list::kAutofillItemID),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for Autofill Set Up List Item expired.");
  // Tap the autofill item.
  TapView(set_up_list::kAutofillItemID);
  // TODO - verify the CPE promo is displayed.
  id<GREYMatcher> CPEPromoView =
      grey_accessibilityID(@"kCredentialProviderPromoAccessibilityId");
  [[EarlGrey selectElementWithMatcher:CPEPromoView]
      assertWithMatcher:grey_notNil()];
  // Dismiss the CPE promo.
  TapSecondaryActionButton();

  condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                set_up_list::kSignInItemID),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(1), condition),
      @"Timeout waiting for Sign in Set Up List Item expired.");

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  [ChromeEarlGreyUI waitForAppToIdle];
  if ([ChromeEarlGrey isReplaceSyncWithSigninEnabled]) {
    // The fake signin UI appears. Dismiss it.
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            kFakeAuthCancelButtonIdentifier)]
        performAction:grey_tap()];
  } else {
    // The full-screen signin promo appears. Dismiss it.
    TapPromoStyleSecondaryActionButton();
  }

  // Verify the All Set item is shown.
  condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                                set_up_list::kAllSetItemID),
                                            grey_sufficientlyVisible(), nil)]
        assertWithMatcher:grey_sufficientlyVisible()
                    error:&error];
    return error == nil;
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"Timeout waiting for the All Set Module to show expired.");
}

// Tests the edit button in the Magic Stack. Opens the edit half sheet, disables
// Set Up List, returns to the Magic Stack and ensures Set Up List is not in the
// Magic Stack anymore.
- (void)testMagicStackEditButton {
  [self prepareToTestSetUpListInMagicStack];

  // Swipe all the way over to the end of the Magic Stack.
  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(grey_accessibilityID(
                                 kMagicStackEditButtonAccessibilityIdentifier),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeFastInDirection(kGREYDirectionLeft)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Verify edit half sheet is visible.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(l10n_util::GetNSString(
                                   IDS_IOS_MAGIC_STACK_EDIT_MODAL_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> setUpToggle = grey_allOf(
      grey_accessibilityID(l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TITLE)),
      grey_sufficientlyVisible(), nil);
  // Assert Set Up List toggle is on, and then turn if off.
  [[EarlGrey selectElementWithMatcher:setUpToggle]
      performAction:chrome_test_util::TurnTableViewSwitchOn(NO)];

  // Dismiss
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kMagicStackEditHalfSheetDoneButtonAccessibilityIdentifier)]
      performAction:grey_tap()];

  // Swipe back to first module
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionRight)];

  // Assert Set Up List is not there. If it is, it is always the first module.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(l10n_util::GetNSString(
                                   IDS_IOS_SET_UP_LIST_TITLE))]
      assertWithMatcher:grey_nil()];
}

#pragma mark - Test utils

// Sets up the test case to test SetUpList.
- (void)prepareToTestSetUpList {
  [ChromeEarlGreyAppInterface writeFirstRunSentinel];
  [ChromeEarlGreyAppInterface clearDefaultBrowserPromoData];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  ScrollToSetUpList();

  // SetUpList is visible
  [[EarlGrey selectElementWithMatcher:SetUpList()]
      assertWithMatcher:grey_notNil()];
}

- (void)prepareToTestSetUpListInMagicStack {
  [ChromeEarlGreyAppInterface writeFirstRunSentinel];
  [ChromeEarlGreyAppInterface clearDefaultBrowserPromoData];
  [ChromeEarlGrey resetDataForLocalStatePref:
                      prefs::kIosCredentialProviderPromoLastActionTaken];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
}

// Setup a most visited tile, and open the context menu by long pressing on it.
- (void)setupMostVisitedTileLongPress {
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Clear history and verify that the tile does not exist.
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  // After loading URL, need to do another action before opening a new tab
  // with the icon present.
  [ChromeEarlGrey goBack];

  [[self class] closeAllTabs];
  [ChromeEarlGrey openNewTab];

  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()];
}

@end
