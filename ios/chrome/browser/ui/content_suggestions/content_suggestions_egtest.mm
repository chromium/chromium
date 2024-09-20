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
#import "components/segmentation_platform/public/constants.h"
#import "components/segmentation_platform/public/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/first_run/ui_bundled/first_run_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/signin_earl_grey.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/new_tab_page_app_interface.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
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

// Taps the view with the given `accessibility_id`.
void TapView(NSString* accessibility_id) {
  id<GREYMatcher> matcher = grey_accessibilityID(accessibility_id);
  [[EarlGrey selectElementWithMatcher:matcher] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];
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
  config.additional_args.push_back("--test-ios-module-ranker=mvt");
  if ([self isRunningTest:@selector
            (DISABLED_testMagicStackSetUpListCompleteAllItems)] ||
      [self isRunningTest:@selector(testMagicStackEditButton)] ||
      [self isRunningTest:@selector
            (testMagicStackCompactedSetUpListCompleteAllItems)]) {
    config.features_disabled.push_back(kContentPushNotifications);
    config.features_disabled.push_back(kIOSTipsNotifications);
  }
  if ([self isRunningTest:@selector(testMVTInMagicStack)]) {
    std::string enable_mvt_arg = std::string(kMagicStack.name) + ":" +
                                 kMagicStackMostVisitedModuleParam + "/true";
    config.additional_args.push_back("--enable-features=" + enable_mvt_arg);
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
  [ChromeEarlGrey removeFirstRunSentinel];
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

// Tests the "Remove" action of the Most Visited context menu, and the "Undo"
// action.
// TODO(crbug.com/337064665): Test is flaky on simluator. Re-enable when fixed.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testMostVisitedRemoveUndo FLAKY_testMostVisitedRemoveUndo
#else
#define MAYBE_testMostVisitedRemoveUndo testMostVisitedRemoveUndo
#endif
- (void)MAYBE_testMostVisitedRemoveUndo {
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

// Tests that the "All Set" module is shown after completing all Set Up List
// Hero Cell modules in the Magic Stack.
// TODO(crbug.com/41493926): Test is flaky, re-enable when fixed.
- (void)DISABLED_testMagicStackSetUpListCompleteAllItems {
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
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
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
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"Timeout waiting for Sign in Set Up List Item expired.");

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  [ChromeEarlGreyUI waitForAppToIdle];
  // The fake signin UI appears. Dismiss it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

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

// Attempts to complete the Set Up List through the Compacted Magic Stack
// module.
- (void)testMagicStackCompactedSetUpListCompleteAllItems {
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
    return [NewTabPageAppInterface
        setUpListItemDefaultBrowserInMagicStackIsComplete];
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"SetUpList item Default Browser not completed.");

  // Tap the autofill item.
  TapView(set_up_list::kAutofillItemID);
  id<GREYMatcher> CPEPromoView =
      grey_accessibilityID(@"kCredentialProviderPromoAccessibilityId");
  [[EarlGrey selectElementWithMatcher:CPEPromoView]
      assertWithMatcher:grey_notNil()];
  // Dismiss the CPE promo.
  TapSecondaryActionButton();

  condition = ^{
    return [NewTabPageAppInterface setUpListItemAutofillInMagicStackIsComplete];
  };
  GREYAssert(
      base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), condition),
      @"SetUpList item Autofill not completed.");

  // Completed Set Up List items last one impression
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGrey closeAllTabs];
  [ChromeEarlGrey openNewTab];

  // Tap the signin item.
  TapView(set_up_list::kSignInItemID);
  [ChromeEarlGreyUI waitForAppToIdle];
  // The fake signin UI appears. Dismiss it.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kFakeAuthCancelButtonIdentifier)]
      performAction:grey_tap()];

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
  // Enable customization.
  AppLaunchConfiguration config = [self appConfigurationForTestCase];
  config.relaunch_policy = ForceRelaunchByCleanShutdown;
  config.features_enabled.push_back(kHomeCustomization);
  [[AppLaunchManager sharedManager] ensureAppLaunchedWithConfiguration:config];

  [self prepareToTestSetUpListInMagicStack];

  // Swipe all the way over to the end of the Magic Stack and tap the edit
  // button, which opens the customization menu at the Magic Stack page.
  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(grey_accessibilityID(
                                 kMagicStackEditButtonAccessibilityIdentifier),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_swipeFastInDirection(kGREYDirectionLeft)
      onElementWithMatcher:grey_accessibilityID(
                               kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_tap()];

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([HomeCustomizationHelper
              navigationBarTitleForPage:CustomizationMenuPage::kMagicStack])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Turn off the Set Up list toggle.
  [[EarlGrey selectElementWithMatcher:
                 grey_allOf(grey_kindOfClassName(@"UISwitch"),
                            grey_ancestor(grey_accessibilityID(
                                kCustomizationToggleSetUpListIdentifier)),
                            nil)] performAction:grey_turnSwitchOn(NO)];

  // Dismiss the menu.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   kNavigationBarDismissButtonIdentifier)]
      performAction:grey_tap()];

  // Swipe back to first module
  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier)]
      performAction:grey_swipeFastInDirection(kGREYDirectionRight)];

  // Assert Set Up List is not there. If it is, it is always the first module.
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(
                                   [NewTabPageAppInterface setUpListTitle])]
      assertWithMatcher:grey_notVisible()];
}

// Test that MVT navigation and removal works when the module is put in the
// Magic Stack.
- (void)testMVTInMagicStack {
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

  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_MOST_VISITED_MODULE_TITLE))]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Navigate to MVT
  id<GREYMatcher> matcher =
      grey_allOf(chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
                 grey_sufficientlyVisible(), nil);
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_tap()];

  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          pageURL.GetContent())]
      assertWithMatcher:grey_notNil()];

  [ChromeEarlGrey goBack];
  [[EarlGrey selectElementWithMatcher:matcher] performAction:grey_longPress()];

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
}

#pragma mark - Test utils

- (void)prepareToTestSetUpListInMagicStack {
  [ChromeEarlGrey writeFirstRunSentinel];
  [ChromeEarlGrey clearDefaultBrowserPromoData];
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
