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
#import "ios/chrome/browser/authentication/test/signin_earl_grey.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/content_suggestions/public/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/content_suggestions/set_up_list/public/set_up_list_constants.h"
#import "ios/chrome/browser/content_suggestions/test/new_tab_page_app_interface.h"
#import "ios/chrome/browser/first_run/public/first_run_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"
#import "ios/chrome/browser/home_customization/utils/home_customization_helper.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/test_constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/chrome_xcui_actions.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "net/base/apple/url_conversions.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

using ::chrome_test_util::ButtonWithAccessibilityLabel;

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
  id<GREYMatcher> button = chrome_test_util::ButtonStackSecondaryButton();
  [[EarlGrey selectElementWithMatcher:button] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Tap the ConfirmationAlertSecondaryAction Button.
void TapSecondaryActionButton() {
  id<GREYMatcher> button = chrome_test_util::ButtonStackSecondaryButton();
  [[EarlGrey selectElementWithMatcher:button] assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:button] performAction:grey_tap()];
}

// Swipe all the way over to the end of the Magic Stack and tap the edit button,
// which opens the customization menu at the Magic Stack page.
void TapMagicStackEditButton() {
  id<GREYMatcher> magicStackScrollView =
      grey_accessibilityID(kMagicStackScrollViewAccessibilityIdentifier);
  CGFloat moduleSwipeAmount = kMagicStackWideWidth * 0.6;
  [[[EarlGrey selectElementWithMatcher:
                  grey_allOf(grey_accessibilityID(
                                 kMagicStackEditButtonAccessibilityIdentifier),
                             grey_sufficientlyVisible(), nil)]
         usingSearchAction:GREYScrollInDirectionWithStartPoint(
                               kGREYDirectionRight, moduleSwipeAmount, 0.9, 0.5)
      onElementWithMatcher:magicStackScrollView] performAction:grey_tap()];
}

// Scrolls the most visited tiles all the way to the right.
void ScrollMostVisitedToRightEdge() {
  id<GREYMatcher> mostVisitedCollectionView = grey_allOf(
      grey_kindOfClassName(@"MostVisitedTilesCollectionView"),
      grey_ancestor(
          grey_accessibilityID(kContentSuggestionsCollectionIdentifier)),
      nil);
  // Avoid that the user starts scrolling from the rightmost 16px of the
  // collection view.
  [[EarlGrey selectElementWithMatcher:mostVisitedCollectionView]
      performAction:grey_scrollToContentEdgeWithStartPoint(
                        kGREYContentEdgeRight, 0.95, 0.5)];
}

// Helper function that returns the accessibility ID for the most visited cell
// at `index`.
NSString* AccessibilityIdentifierForMostVisitedCellAtIndex(int index) {
  return [NSString
      stringWithFormat:
          @"%@%d", kContentSuggestionsMostVisitedAccessibilityIdentifierPrefix,
          index];
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
  config.features_enabled.push_back(kMostVisitedTilesCustomizationIOS);
  config.features_disabled.push_back(kIOSExpandedSetupList);
  config.additional_args.push_back("--test-ios-module-ranker=mvt");
  if ([self isRunningTest:@selector(testMagicStackEditButton)] ||
      [self isRunningTest:@selector
            (testMagicStackCompactedSetUpListCompleteAllItems)]) {
    config.features_disabled.push_back(kContentPushNotifications);
  }

  return config;
}

+ (void)setUpForTestCase {
  [super setUpForTestCase];
  [self setUpHelper];
}

+ (void)setUpHelper {
  if (![ChromeTestCase forceRestartAndWipe]) {
    [self closeAllTabs];
  }
}

+ (void)tearDown {
  if (![ChromeTestCase forceRestartAndWipe]) {
    [self closeAllTabs];
  }

  [super tearDown];
}

- (void)setUp {
  [super setUp];
  [NewTabPageAppInterface disableTipsCards];
}

- (void)tearDownHelper {
  [ChromeEarlGrey clearBrowsingHistory];
  [ChromeEarlGrey removeFirstRunSentinel];
  [NewTabPageAppInterface resetSetUpListPrefs];
  [super tearDownHelper];
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
  [ChromeEarlGrey waitForWebStateVisibleURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];
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
  [ChromeEarlGrey waitForWebStateVisibleURL:pageURL];
  [ChromeEarlGrey waitForWebStateContainingText:kPageLoadedString];

  GREYAssertTrue([ChromeEarlGrey isIncognitoMode],
                 @"Test did not switch to incognito");
}

// Tests the "Remove" action of the Most Visited context menu, and the "Undo"
// action.
- (void)testMostVisitedRemoveUndo {
  [self setupMostVisitedTileLongPress];
  const GURL pageURL = self.testServer->GetURL(kPageURL);
  NSString* pageTitle = base::SysUTF8ToNSString(kPageTitle);

  // Tap on remove.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_NEVER_SHOW_SITE)]
      performAction:grey_tap()];

  // Check the tile is removed.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(
              chrome_test_util::StaticTextWithAccessibilityLabel(pageTitle),
              grey_sufficientlyVisible(), nil)] assertWithMatcher:grey_nil()];

  // Check the snack bar notifying the user that an element has been removed is
  // displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
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

  // TODO:(crbug.com/480153437): Enable `kIOSExpandedSetupList` and update this
  // test to work with the new setup list.
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

  // Tap the notification item.
  TapView(set_up_list::kContentNotificationItemID);
  // Ensure the Notification opt-in screen is displayed
  id<GREYMatcher> notificationOptInView =
      grey_accessibilityID(@"NotificationsOptInScreenAxId");
  [[EarlGrey selectElementWithMatcher:notificationOptInView]
      assertWithMatcher:grey_notNil()];
  // Dismiss Notification opt-in screen.
  TapPromoStyleSecondaryActionButton();

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
  TapMagicStackEditButton();

  [[EarlGrey
      selectElementWithMatcher:
          grey_accessibilityID([HomeCustomizationHelper
              navigationBarTitleForPage:CustomizationMenuPage::kMagicStack])]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Turn off the Set Up list toggle.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_kindOfClassName(@"UISwitch"),
                                   grey_ancestor(grey_accessibilityID(
                                       kCustomizationToggleTipsIdentifier)),
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
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          set_up_list::kSetUpListContainerID)]
      assertWithMatcher:grey_notVisible()];
}

// Tests that the long-press hide action for the Set Up List card removes the
// card from the Magic Stack.
- (void)testMagicStackLongPressHide {
  [self prepareToTestSetUpListInMagicStack];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          set_up_list::kSetUpListContainerID)]
      performAction:grey_longPress()];

  NSString* setupListHideTitle = l10n_util::GetNSStringF(
      IDS_IOS_SET_UP_LIST_HIDE_MODULE_CONTEXT_MENU_DESCRIPTION,
      l10n_util::GetStringUTF16(IDS_IOS_MAGIC_STACK_TIP_TITLE));
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(chrome_test_util::ContextMenuItemWithAccessibilityLabel(
                         setupListHideTitle),
                     grey_interactable(), nullptr)] performAction:grey_tap()];
  GREYWaitForAppToIdle(@"App failed to idle");

  // Assert Set Up List card is not there.
  if (iOS26_OR_ABOVE()) {
    ConditionBlock condition = ^{
      NSError* error = nil;
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityID(
                                       set_up_list::kSetUpListContainerID)]
          assertWithMatcher:grey_notVisible()
                      error:&error];
      return error == nil;
    };
    GREYAssert(base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2),
                                                            condition),
               @"Timeout waiting for the Set Up List card to dismissing.");
  } else {
    [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                            set_up_list::kSetUpListContainerID)]
        assertWithMatcher:grey_notVisible()];
  }
}

// Tests pinning and unpinning a Most Visited tile using the context menu.
- (void)testMostVisitedPinUnpinWithContextMenu {
  [self setupMostVisitedTileLongPress];
  // Pin the site.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE)]
      performAction:grey_tap()];
  // Verify it is pinned (check accessibility label,) snackbar is displayed, and
  // dismiss the snackbar.
  NSString* pinnedLabel = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
      base::UTF8ToUTF16(std::string_view(kPageTitle)));
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      performAction:grey_tap()];
  // Long press again.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabel)]
      performAction:grey_longPress()];
  // Tap on unpin.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_CONTENT_SUGGESTIONS_UNPIN_SITE)]
      performAction:grey_tap()];
  // Verify it is unpinned (accessibility label is just the title) and the
  // snackbar is visible.
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(
                                          base::SysUTF8ToNSString(kPageTitle))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that the user could use the "Add site" button to pin a site.
- (void)testMostVisitedPinWithAddSiteButton {
  NSString* firstTitle = @"First Site";
  NSString* firstUrl = @"https://first_url.com";
  GREYAssert([self addPinnedSiteWithTitle:firstTitle URL:firstUrl],
             @"Add pinned site form not dismissed.");
  // Verify that the site is pinned and snackbar is displayed. Dismiss the
  // snackbar.
  NSString* pinnedLabel = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(firstTitle));
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      performAction:grey_tap()];
  // Verify that an error message is displayed for invalid input.
  NSString* secondTitle = @"Second Site";
  NSString* secondUrl = @"chrome://second_url";
  NSString* invalidUrl = @"in://valid.url";
  id<GREYMatcher> saveButton = grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
      ButtonWithAccessibilityLabel(l10n_util::GetNSString(IDS_ADD)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  GREYAssertFalse([self addPinnedSiteWithTitle:secondTitle URL:firstUrl],
                  @"Add pinned site form should not be dismissed when URL is "
                  @"already pinned.");
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_EXISTS))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:saveButton] assertWithMatcher:grey_nil()];
  // Verify that the message disappears and the "Add" button is interactable
  // again after input is changed.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityValue(firstUrl),
                                          grey_kindOfClassName(@"UITextField"),
                                          nil)]
      performAction:grey_replaceText(invalidUrl)];
  [[EarlGrey selectElementWithMatcher:
                 grey_text(l10n_util::GetNSString(
                     IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_EXISTS))]
      assertWithMatcher:grey_nil()];
  // Verify that invalid URL could not be saved.
  [[[EarlGrey selectElementWithMatcher:saveButton]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_text(l10n_util::GetNSString(
              IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_URL_VALIDATION_FAILED))]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey selectElementWithMatcher:saveButton] assertWithMatcher:grey_nil()];
  // Save a valid URL. Verify that the "title" field is optional, and the URL
  // will be used as title.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityValue(invalidUrl),
                                          grey_kindOfClassName(@"UITextField"),
                                          nil)]
      performAction:grey_replaceText(secondUrl)];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityValue(secondTitle),
                                          grey_kindOfClassName(@"UITextField"),
                                          nil)] performAction:grey_clearText()];
  [[[EarlGrey selectElementWithMatcher:saveButton]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];
  pinnedLabel = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(secondUrl));
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabel)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests the "Edit" action of the Most Visited context menu.
- (void)testMostVisitedEditPinnedSite {
  [self setupMostVisitedTileLongPress];
  // Pin the site and dismiss the snackbar.
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      performAction:grey_tap()];
  // Long press again and tap on "Edit Site".
  NSString* pinnedLabel = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
      base::UTF8ToUTF16(std::string_view(kPageTitle)));
  [[EarlGrey selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabel)]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_EDIT_PINNED_SITE)]
      performAction:grey_tap()];
  // Check the form is displayed with the correct title and footer.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
              IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_EDIT_PINNED_SITE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  // Check the "Name" field and type a new value.
  NSString* newTitle = @"New Title";
  [[EarlGrey selectElementWithMatcher:grey_textFieldValue(
                                          base::SysUTF8ToNSString(kPageTitle))]
      performAction:grey_replaceText(newTitle)];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::NavigationBarSaveButton()]
      performAction:grey_tap()];
  // Verify that the title has changed.
  NSString* newPinnedTitle = l10n_util::GetNSStringF(
      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(newTitle));
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(newPinnedTitle)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that pinned sites can be reordered using drag and drop.
- (void)testMostVisitedPinnedSiteDragAndDrop {
  // Pin two sites.
  NSArray<NSString*>* titles = @[ @"First Site", @"Second Site" ];
  NSArray<NSString*>* URLs =
      @[ @"https://first_url.com", @"https://second_url.com" ];
  for (int i = 0; i < 2; i++) {
    ScrollMostVisitedToRightEdge();
    [[EarlGrey selectElementWithMatcher:
                   grey_accessibilityID(
                       AccessibilityIdentifierForMostVisitedCellAtIndex(-1))]
        performAction:grey_tap()];
    // Fill out form elements.
    [[EarlGrey
        selectElementWithMatcher:
            grey_allOf(grey_accessibilityValue(l10n_util::GetNSString(
                           IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_NAME)),
                       grey_kindOfClassName(@"UITextField"), nil)]
        performAction:grey_replaceText(titles[i])];
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_accessibilityValue(
                                         @"https://example.com"),
                                     grey_kindOfClassName(@"UITextField"), nil)]
        performAction:grey_replaceText(URLs[i])];
    id<GREYMatcher> saveButton = grey_allOf(
        grey_ancestor(grey_kindOfClass([UINavigationBar class])),
        ButtonWithAccessibilityLabel(l10n_util::GetNSString(IDS_ADD)),
        grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
    [[EarlGrey selectElementWithMatcher:saveButton] performAction:grey_tap()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
        performAction:grey_tap()];
  }

  NSMutableArray<NSString*>* tileAccessibilityIDs = [NSMutableArray array];
  NSMutableArray<NSString*>* pinnedLabels = [NSMutableArray array];
  for (int i = 0; i < 2; i++) {
    [tileAccessibilityIDs
        addObject:AccessibilityIdentifierForMostVisitedCellAtIndex(i)];
    [pinnedLabels
        addObject:l10n_util::GetNSStringF(
                      IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ACCESSIBILITY_LABEL,
                      base::SysNSStringToUTF16(titles[i]))];
  }
  // Verify initial order: "First Site" is displayed at index 0, "Second Site"
  // is displayed at index 1.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabels[0])]
      assertWithMatcher:grey_ancestor(
                            grey_accessibilityID(tileAccessibilityIDs[0]))];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabels[1])]
      assertWithMatcher:grey_ancestor(
                            grey_accessibilityID(tileAccessibilityIDs[1]))];
  // Drag Tile 0 to Tile 1.
  GREYAssertTrue(chrome_test_util::LongPressCellAndDragToOffsetOf(
                     tileAccessibilityIDs[0], 0, tileAccessibilityIDs[1], 0,
                     CGVectorMake(0.5, 0.5)),
                 @"Source or destination doesn't exist.");
  GREYWaitForAppToIdle(@"App failed to idle");
  // Verify new order: "Second Site" is displayed at index 0, "First Site"
  // is displayed at index 1.
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabels[1])]
      assertWithMatcher:grey_ancestor(
                            grey_accessibilityID(tileAccessibilityIDs[0]))];
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabels[0])]
      assertWithMatcher:grey_ancestor(
                            grey_accessibilityID(tileAccessibilityIDs[1]))];

  // Attempt to drag "First Site" to a non-pinned position. Verify that it has
  // failed.
  GREYAssertTrue(chrome_test_util::LongPressCellAndDragToOffsetOf(
                     tileAccessibilityIDs[1], 0,
                     AccessibilityIdentifierForMostVisitedCellAtIndex(2), 0,
                     CGVectorMake(0.5, 0.5)),
                 @"Source or destination doesn't exist.");
  GREYWaitForAppToIdle(@"App failed to idle");
  [[EarlGrey
      selectElementWithMatcher:ButtonWithAccessibilityLabel(pinnedLabels[0])]
      assertWithMatcher:grey_ancestor(
                            grey_accessibilityID(tileAccessibilityIDs[1]))];
}

// Tests pinning 8 sites and verifying the "Add site" button disappears after 8
// sites are added, and reappears after unpinning one.
- (void)testMostVisitedPinEightSites {
  id<GREYMatcher> addSiteButton = grey_accessibilityID(
      AccessibilityIdentifierForMostVisitedCellAtIndex(-1));
  // Add 8 pinned sites. Before pinning each, verify that the "Add site" button
  // is still visible.
  for (int i = 0; i < 8; ++i) {
    NSString* title = [NSString stringWithFormat:@"site %d", i];
    NSString* URL = [NSString stringWithFormat:@"http://page%d.com", i];
    GREYAssert([self addPinnedSiteWithTitle:title URL:URL],
               @"Add pinned site form not dismissed.");
    // Dismiss the snackbar after it appears.
    [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
        performAction:grey_tap()];
  }
  // After 8 sites, the "+" button should be gone.
  ScrollMostVisitedToRightEdge();
  [[EarlGrey selectElementWithMatcher:addSiteButton]
      assertWithMatcher:grey_nil()];
  // Unpin the last site.
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     AccessibilityIdentifierForMostVisitedCellAtIndex(7))]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:
                 chrome_test_util::ContextMenuItemWithAccessibilityLabelId(
                     IDS_IOS_CONTENT_SUGGESTIONS_UNPIN_SITE)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::SnackbarViewMatcher()]
      performAction:grey_tap()];
  // Now the "+" button should be back.
  ScrollMostVisitedToRightEdge();
  [[EarlGrey selectElementWithMatcher:addSiteButton]
      assertWithMatcher:grey_interactable()];
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
  if (![ChromeTestCase forceRestartAndWipe]) {
    [ChromeEarlGrey clearBrowsingHistory];
  }
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

// Scrolls all the way to the edge, taps the "Add Site" button, fills out the
// form and taps the "Add" button, which may be inactive. Returns `NO` if the
// form is not dismissed at the end of the process, which strongly indicates
// that the inputs are invalid.
- (BOOL)addPinnedSiteWithTitle:(NSString*)title URL:(NSString*)URL {
  ScrollMostVisitedToRightEdge();
  id<GREYMatcher> addSiteButton = grey_accessibilityID(
      AccessibilityIdentifierForMostVisitedCellAtIndex(-1));
  [[EarlGrey selectElementWithMatcher:addSiteButton] performAction:grey_tap()];
  // Verify form title.
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::NavigationBarTitleWithAccessibilityLabelId(
              IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ADD_PINNED_SITE_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];
  id<GREYMatcher> saveButton = grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
      ButtonWithAccessibilityLabel(l10n_util::GetNSString(IDS_ADD)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
  // Fill the "Name" field.
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityValue(l10n_util::GetNSString(
                         IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_FORM_NAME)),
                     grey_kindOfClassName(@"UITextField"), nil)]
      performAction:grey_replaceText(title)];
  // Check that "Save" button is disabled when the URL is empty.
  [[EarlGrey selectElementWithMatcher:saveButton] assertWithMatcher:grey_nil()];
  // Fill the "URL" field.
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityValue(
                                              @"https://example.com"),
                                          grey_kindOfClassName(@"UITextField"),
                                          nil)]
      performAction:grey_replaceText(URL)];
  // Tap "Add". Check if that dismisses the form.
  [[EarlGrey selectElementWithMatcher:saveButton] performAction:grey_tap()];
  NSError* err;
  [[EarlGrey
      selectElementWithMatcher:
          chrome_test_util::HeaderWithAccessibilityLabelId(
              IDS_IOS_CONTENT_SUGGESTIONS_PIN_SITE_ADD_PINNED_SITE_TITLE)]
      assertWithMatcher:grey_nil()
                  error:&err];
  return !err;
}

@end
