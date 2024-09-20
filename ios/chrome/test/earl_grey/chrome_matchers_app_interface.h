// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_

#import <UIKit/UIKit.h>

@protocol GREYMatcher;

// Helper class to return matchers for EG tests.  These helpers are compiled
// into the app binary and can be called from either app or test code.
// All calls of grey_... involve the App process, so it's more efficient to
// define the matchers in the app process.
@interface ChromeMatchersAppInterface : NSObject

// Matcher for a window with a given number.
// Window numbers are assigned at scene creation. Normally, each EGTest will
// start with exactly one window with number 0. Each time a window is created,
// it is assigned an accessibility identifier equal to the number of connected
// scenes (stored as NSString). This means typically any windows created in a
// test will have consecutive numbers.
+ (id<GREYMatcher>)windowWithNumber:(int)windowNumber;

// Same as above, but for the blocking window which only appears when a blocking
// UI is shown in another window.
+ (id<GREYMatcher>)blockerWindowWithNumber:(int)windowNumber;

// Matcher for element with accessibility label corresponding to `label` and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to `messageID`
// and accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID;

// Matcher for element with accessibility label corresponding to `messageID`,
// `number` for the plural rule and accessibility trait
// UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID
                                  numberForPlural:(int)number;

// Matcher for element with foreground color corresponding to `colorName`
// and accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithForegroundColor:(NSString*)colorName;

// Matcher for element with background color corresponding to `colorName`
// and accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithBackgroundColor:(NSString*)colorName;

// Matcher for context menu items with accessibility label
// corresponding to `label`.
+ (id<GREYMatcher>)contextMenuItemWithAccessibilityLabel:(NSString*)label;

// Matcher for context menu items with accessibility label
// corresponding to `messageID`.
+ (id<GREYMatcher>)contextMenuItemWithAccessibilityLabelID:(int)messageID;

// Matcher for element with an image corresponding to `image`.
+ (id<GREYMatcher>)imageViewWithImage:(UIImage*)image;

// Matcher for element with an image defined by its name in the main bundle.
+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName;

// Matcher for element with a custom symbol defined by its name and point size
// in the main bundle.
+ (id<GREYMatcher>)imageViewWithCustomSymbolNamed:(NSString*)symbolName
                                        pointSize:(CGFloat)pointSize;

// Matcher for element with an image corresponding to `imageID` and
// accessibility trait UIAccessibilityTraitButton.
+ (id<GREYMatcher>)buttonWithImage:(int)imageID;

// Matcher for element with accessibility label corresponding to `messageID`
// and accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabelID:(int)messageID;

// Matcher for element with accessibility label corresponding to `label` and
// accessibility trait UIAccessibilityTraitStaticText.
+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label;

// Matcher for element with accessibility label corresponding to `messageID`
// and accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabelID:(int)labelID;

// Matcher for navigation bar title element with accessibility label
// corresponding to `titleID`.
+ (id<GREYMatcher>)navigationBarTitleWithAccessibilityLabelID:(int)titleID;

// Matcher for text field of a cell with `messageID`.
+ (id<GREYMatcher>)textFieldForCellWithLabelID:(int)messageID;

// Matcher for icon view of a cell with `messageID`.
+ (id<GREYMatcher>)iconViewForCellWithLabelID:(int)messageID
                                     iconType:(NSString*)iconType;

// Matcher for element with accessibility label corresponding to `label` and
// accessibility trait UIAccessibilityTraitHeader.
+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label;

// Returns matcher for the primary toolbar.
+ (id<GREYMatcher>)primaryToolbar;

// Returns matcher for a cancel button.
+ (id<GREYMatcher>)cancelButton;

// Returns the matcher for an enabled cancel button in a navigation bar.
+ (id<GREYMatcher>)navigationBarCancelButton;

// Returns matcher for close tab context menu button.
+ (id<GREYMatcher>)closeTabMenuButton;

// Matcher for the navigate forward button.
+ (id<GREYMatcher>)forwardButton;

// Matcher for the navigate backward button.
+ (id<GREYMatcher>)backButton;

// Matcher for the reload button.
+ (id<GREYMatcher>)reloadButton;

// Matcher for the stop loading button.
+ (id<GREYMatcher>)stopButton;

// Returns a matcher for the omnibox.
+ (id<GREYMatcher>)omnibox;

// Returns a matcher for the omnibox at the bottom.
+ (id<GREYMatcher>)omniboxAtBottom;

// Returns a matcher for the omnibox on the top.
+ (id<GREYMatcher>)omniboxOnTop;

// Returns a matcher for the location view.
+ (id<GREYMatcher>)defocusedLocationView;

// Returns a matcher for the page security info button.
+ (id<GREYMatcher>)pageSecurityInfoButton;

// Returns a matcher for the page security info indicator.
+ (id<GREYMatcher>)pageSecurityInfoIndicator;

// Returns matcher for omnibox containing `text`. Performs an exact match of the
// omnibox contents.
+ (id<GREYMatcher>)omniboxText:(NSString*)text;

// Returns matcher for `text` being a substring of the text in the omnibox.
+ (id<GREYMatcher>)omniboxContainingText:(NSString*)text;

// Returns matcher for `text` being the inline autocomplete text in the omnibox.
+ (id<GREYMatcher>)omniboxContainingAutocompleteText:(NSString*)text;

// Returns matcher for `text` being a substring of the text in the location
// view.
+ (id<GREYMatcher>)locationViewContainingText:(NSString*)text;

// Matcher for Tools menu button.
+ (id<GREYMatcher>)toolsMenuButton;

// Matcher for the New Tab button, which can be long-pressed for a menu.
// (This method can't be named +newTabButton, because starting a class method
// with 'new' implicitly treats it as a constructor).
+ (id<GREYMatcher>)openNewTabButton;

// Matcher for the Share... button.
+ (id<GREYMatcher>)shareButton;

// Returns a matcher for a context menu button that contains `text`.
+ (id<GREYMatcher>)contextMenuButtonContainingText:(NSString*)text;

// Matcher for the tab Share button (either in the omnibox or the toolbar).
+ (id<GREYMatcher>)tabShareButton;

// Matcher for show tabs button.
+ (id<GREYMatcher>)showTabsButton;

// Matcher for Add to reading list button.
+ (id<GREYMatcher>)addToReadingListButton;

// Matcher for Add to bookmarks button.
+ (id<GREYMatcher>)addToBookmarksButton;

// Matcher for TableViewSwitchCell.
+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn;

// Matcher for TableViewSwitchCell.
+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn
                             isEnabled:(BOOL)isEnabled;

// Matcher for SyncSwitchCell.
+ (id<GREYMatcher>)syncSwitchCell:(NSString*)accessibilityLabel
                      isToggledOn:(BOOL)isToggledOn;

// Matcher for the Open in New Tab option in the context menu when long pressing
// a link.
+ (id<GREYMatcher>)openLinkInNewTabButton;

// Matcher for the Open in Incognito option in the context menu when long
// pressing a link.
+ (id<GREYMatcher>)openLinkInIncognitoButton;

// Matcher for the Open in New Window option in the context menu when long
// pressing a link.
+ (id<GREYMatcher>)openLinkInNewWindowButton;

// Matcher for the done button on the navigation bar.
+ (id<GREYMatcher>)navigationBarDoneButton;

// Matcher for the done button on the Bookmarks navigation bar.
+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton;

// Matcher for the back button on the Bookmarks navigation bar.
+ (id<GREYMatcher>)bookmarksNavigationBarBackButton;

// Returns matcher for the add account accounts button.
+ (id<GREYMatcher>)addAccountButton;

// Returns matcher for the sign out accounts button.
+ (id<GREYMatcher>)signOutAccountsButton;

// Returns matcher for the Clear Browsing Data cell on the Privacy screen.
+ (id<GREYMatcher>)clearBrowsingDataCell;

// Returns matcher for the clear browsing data button on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearBrowsingDataButton;

// Returns matcher for the clear browsing data view.
+ (id<GREYMatcher>)clearBrowsingDataView;

// Returns matcher for the "Done" button in the settings' navigation bar.
+ (id<GREYMatcher>)settingsDoneButton;

// Returns matcher for the Autofill Credit Card "Payment Methods" edit view.
+ (id<GREYMatcher>)autofillCreditCardEditTableView;

// Returns matcher for the Autofill Credit Card "Payment Methods" view in the
// settings menu.
+ (id<GREYMatcher>)autofillCreditCardTableView;

// Returns matcher for the "Addresses and More" button in the settings menu.
+ (id<GREYMatcher>)addressesAndMoreButton;

// Returns matcher for the "Payment Methods" button in the settings menu.
+ (id<GREYMatcher>)paymentMethodsButton;

// Returns matcher for the "Languages" button in the settings menu.
+ (id<GREYMatcher>)languagesButton;

// Returns matcher for the "Add Credit Card" view in the Settings menu.
+ (id<GREYMatcher>)addCreditCardView;

// Returns matcher for the "Add" credit card button in the Payment
// Methods add credit card view.
+ (id<GREYMatcher>)addCreditCardButton;

// Returns matcher for the "Cancel" button in the Payment Methods add credit
// card view.
+ (id<GREYMatcher>)addCreditCardCancelButton;

// Returns matcher for the tools menu table view.
+ (id<GREYMatcher>)toolsMenuView;

// Returns matcher for the omnibox popup list row views.
+ (id<GREYMatcher>)omniboxPopupRow;

// Returns a matcher for a popup row containing `string` as accessibility label.
+ (id<GREYMatcher>)omniboxPopupRowWithString:(NSString*)string;

// Returns matcher for the omnibox popup list view.
+ (id<GREYMatcher>)omniboxPopupList;

// Returns matcher for the OK button.
+ (id<GREYMatcher>)OKButton;

// Returns matcher for the primary button in the sign-in promo view. This is
// "Sign in into Chrome" button for a cold state, or "Continue as John Doe" for
// a warm state.
+ (id<GREYMatcher>)primarySignInButton;

// Returns matcher for the secondary button in the sign-in promo view. This is
// "Not johndoe@example.com" button.
+ (id<GREYMatcher>)secondarySignInButton;

// Returns matcher for the identity chooser scrim that is shown behind the
// identity chooser dialog. Tapping on the scrim dismisses the dialog.
+ (id<GREYMatcher>)identityChooserScrim;

// Returns matcher for the cancel button in the fake add account flow.
+ (id<GREYMatcher>)fakeFakeAddAccountScreenCancelButton;

// Returns matcher for the button for the currently signed in account in the
// settings menu.
+ (id<GREYMatcher>)settingsAccountButton;

// Returns matcher for the accounts collection view.
+ (id<GREYMatcher>)settingsAccountsCollectionView;

// Returns matcher for the safety check table view.
+ (id<GREYMatcher>)settingsSafetyCheckTableView;

// Returns matcher for the privacy table view.
+ (id<GREYMatcher>)settingsPrivacyTableView;

// Returns matcher for the privacy safe browsing table view.
+ (id<GREYMatcher>)settingsPrivacySafeBrowsingTableView;

// Returns matcher for the notifications table view.
+ (id<GREYMatcher>)settingsNotificationsTableView;

// Returns matcher for the inactive tabs table view.
+ (id<GREYMatcher>)settingsInactiveTabsTableView;

// Returns matcher for the tabs table view.
+ (id<GREYMatcher>)settingsTabsTableView;

// Returns matcher for the tracking price table view.
+ (id<GREYMatcher>)settingsTrackingPriceTableView;

// Returns matcher for the Content Settings button on the main Settings screen.
+ (id<GREYMatcher>)contentSettingsButton;

// Returns matcher for the Google Services Settings button on the main Settings
// screen.
+ (id<GREYMatcher>)googleServicesSettingsButton;

// Returns matcher for the Inactive Tabs Settings button on the Tabs Settings
// screen.
+ (id<GREYMatcher>)inactiveTabsSettingsButton;

// Returns matcher for the Tabs Settings button on the main Settings screen.
+ (id<GREYMatcher>)tabsSettingsButton;

// Returns matcher for the Manage Sync Settings button on the main Settings
// screen.
+ (id<GREYMatcher>)manageSyncSettingsButton;

// Returns matcher for the Google Services Settings view.
+ (id<GREYMatcher>)googleServicesSettingsView;

// Returns matcher for the Navigation Bar embedded in the Settings Navigation
// Controller.
+ (id<GREYMatcher>)settingsNavigationBar;

// Returns matcher for the back button on a settings menu.
+ (id<GREYMatcher>)settingsMenuBackButton;

// Returns matcher for the back button on a settings menu in given window
// number.
+ (id<GREYMatcher>)settingsMenuBackButtonInWindowWithNumber:(int)windowNumber;

// Returns matcher for the Privacy cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuPrivacyButton;

// Returns matcher for the Notifications cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuNotificationsButton;

// Returns matcher for the Save passwords cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuPasswordsButton;

// Returns matcher for the Safety Check cell on the main Settings screen.
+ (id<GREYMatcher>)settingsMenuSafetyCheckButton;

// Returns matcher for the payment request collection view.
+ (id<GREYMatcher>)paymentRequestView;

// Returns matcher for the error confirmation view for payment request.
+ (id<GREYMatcher>)paymentRequestErrorView;

// Returns matcher for the voice search button on the main Settings screen.
+ (id<GREYMatcher>)voiceSearchButton;

// Returns matcher for the voice search button on the omnibox input accessory.
+ (id<GREYMatcher>)voiceSearchInputAccessoryButton;

// Returns matcher for the settings main menu view.
+ (id<GREYMatcher>)settingsCollectionView;

// Returns matcher for the History table view.
+ (id<GREYMatcher>)historyTableView;

// Returns the matcher for the quick delete browsing data button.
+ (id<GREYMatcher>)browsingDataButtonMatcher;

// Returns the matcher for the quick delete browsing data button.
+ (id<GREYMatcher>)browsingDataConfirmButtonMatcher;

// Returns matcher for the clear browsing history cell on the clear browsing
// data panel.
+ (id<GREYMatcher>)clearBrowsingHistoryButton;

// Returns matcher for the clear cookies cell on the clear browsing data panel.
+ (id<GREYMatcher>)clearCookiesButton;

// Returns matcher for the clear cache cell on the clear browsing data panel.
+ (id<GREYMatcher>)clearCacheButton;

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearSavedPasswordsButton;

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
+ (id<GREYMatcher>)clearAutofillButton;

// Returns matcher for the collection view of content suggestion.
+ (id<GREYMatcher>)contentSuggestionCollectionView;

// Returns matcher for the collection view of the NTP.
+ (id<GREYMatcher>)ntpCollectionView;

// Returns matcher for the NTP view when the user is in incognito mode.
+ (id<GREYMatcher>)ntpIncognitoView;

// Returns matcher for the NTP Feed menu button which enables the feed.
+ (id<GREYMatcher>)ntpFeedMenuEnableButton;

// Returns matcher for the NTP Feed menu button which disables the feed.
+ (id<GREYMatcher>)ntpFeedMenuDisableButton;

// Returns matcher for the warning message while filling in payment requests.
+ (id<GREYMatcher>)warningMessageView;

// Returns matcher for the payment picker cell.
+ (id<GREYMatcher>)paymentRequestPickerRow;

// Returns matcher for the payment request search bar.
+ (id<GREYMatcher>)paymentRequestPickerSearchBar;

// Returns matcher for the New Window button on the Tools menu.
+ (id<GREYMatcher>)openNewWindowMenuButton;

// Matcher for a Copy button, such as the one in the Activity View. This matcher
// is very broad and will look for any button with a matching string.
+ (id<GREYMatcher>)copyActivityButton;

// Matcher for the Copy Link option in the updated context menus when long
// pressing on a link.
+ (id<GREYMatcher>)copyLinkButton;

// Matcher for the Edit option on the context menus.
+ (id<GREYMatcher>)editButton;

// Matcher for the Move option on the updated context menus.
+ (id<GREYMatcher>)moveButton;

// Matcher for the Mark as Read option on the Reading List's context menus.
+ (id<GREYMatcher>)readingListMarkAsReadButton;

// Matcher for the Mark as Unread option on the Reading List's context menus.
+ (id<GREYMatcher>)readingListMarkAsUnreadButton;

// Matcher for the Share option on the updated context menus.
+ (id<GREYMatcher>)deleteButton;

// Returns matcher for the Copy item on the old-style context menu.
+ (id<GREYMatcher>)contextMenuCopyButton;

// Returns matcher for defocused omnibox on a new tab.
+ (id<GREYMatcher>)NTPOmnibox;

// Returns matcher for a fake omnibox on a new tab page.
+ (id<GREYMatcher>)fakeOmnibox;

// Returns matcher for a label of a Discover feed header.
+ (id<GREYMatcher>)discoverHeaderLabel;

// Returns matcher for a logo on a new tab page.
+ (id<GREYMatcher>)ntpLogo;

// Returns a matcher for the current WebView.
+ (id<GREYMatcher>)webViewMatcher;

// Returns a matcher for the current WebState's scroll view.
+ (id<GREYMatcher>)webStateScrollViewMatcher;

// Returns a matcher for the current WebState's scroll view in the given
// `windowNumber`.
+ (id<GREYMatcher>)webStateScrollViewMatcherInWindowWithNumber:
    (int)windowNumber;

// Returns a matcher for the Clear Browsing Data button in the History UI.
+ (id<GREYMatcher>)historyClearBrowsingDataButton;

// Returns a matcher for "Open In..." button.
+ (id<GREYMatcher>)openInButton;

// Returns a matcher for "Open" button.
+ (id<GREYMatcher>)openPDFButton;

// Returns the GREYMatcher for the cell at `index` in the tab grid.
+ (id<GREYMatcher>)tabGridCellAtIndex:(unsigned int)index;

// Returns the GREYMatcher for the group cell at `index` in the tab grid.
+ (id<GREYMatcher>)tabGridGroupCellAtIndex:(unsigned int)index;

// Returns a matcher for the group cell for the given `groupName` and `count`.
+ (id<GREYMatcher>)tabGridGroupCellWithName:(NSString*)groupName
                                      count:(NSInteger)count;

// Returns the GREYMatcher for the cell at `index` in the tab strip.
+ (id<GREYMatcher>)tabStripCellAtIndex:(unsigned int)index;

// Returns the GREYMatcher for the group cell at `index` in the tab strip.
+ (id<GREYMatcher>)tabStripGroupCellAtIndex:(unsigned int)index;

// Returns a matcher for the group cell at `index` in the tab groups panel.
+ (id<GREYMatcher>)tabGroupsPanelCellAtIndex:(unsigned int)index;

// Returns a matcher for the group cell created just now in the tab groups panel
// for the given `group_name` and `tab_count`.
+ (id<GREYMatcher>)tabGroupsPanelCellWithName:(NSString*)groupName
                                        count:(NSInteger)count;

// Returns the GREYMatcher for the button that closes the tab grid.
+ (id<GREYMatcher>)tabGridDoneButton;

// Returns the GREYMatcher for the button that closes all the tabs in the tab
// grid.
+ (id<GREYMatcher>)tabGridCloseAllButton;

// Returns the GREYMatcher for the button that reverts the close all tabs action
// in the tab grid.
+ (id<GREYMatcher>)tabGridUndoCloseAllButton;

// Returns the GREYMatcher for the cell that opens History in Recent Tabs.
+ (id<GREYMatcher>)tabGridSelectShowHistoryCell;

// Returns the GREYMatcher for the regular tabs empty state view.
+ (id<GREYMatcher>)tabGridRegularTabsEmptyStateView;

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab grid.
+ (id<GREYMatcher>)tabGridNewTabButton;

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab grid.
+ (id<GREYMatcher>)tabGridNewIncognitoTabButton;

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridOpenTabsPanelButton;

// Returns the GREYMatcher for the button to go to the incognito panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridIncognitoTabsPanelButton;

// Returns the GREYMatcher for the button to go to the other devices panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridOtherDevicesPanelButton;

// Returns the matcher for the tab group snack bar.
+ (id<GREYMatcher>)tabGroupSnackBar:(NSInteger)tabGroupCount;

// Returns the matcher for the tab group snackbar action.
+ (id<GREYMatcher>)tabGroupSnackBarAction;

// Returns the GREYMatcher for the button to go to the tab groups panel in
// the tab grid.
+ (id<GREYMatcher>)tabGridTabGroupsPanelButton;

// Returns the GREYMatcher for the button to go to the third panel in
// the tab grid. If Tab Group Sync is enabled, it's equivalent to
// `tabGridTabGroupsPanelButton`. Otherwise, it's equivalent to
// `tabGridOtherDevicesPanelButton`.
+ (id<GREYMatcher>)tabGridThirdPanelButton;

// Returns a matcher that matches tab grid normal mode page control - The
// PageControl panel always exist only on the tab grid normal mode, So this can
// be used to validate that the tab grid normal mode is active.
+ (id<GREYMatcher>)tabGridNormalModePageControl;

// Returns the GREYMatcher for the Inactive Tabs button of the tab grid.
+ (id<GREYMatcher>)tabGridInactiveTabsButton;

// Returns the GREYMatcher for the background of the tab grid.
+ (id<GREYMatcher>)tabGridBackground;

// Returns the GREYMatcher for the regular tab grid.
+ (id<GREYMatcher>)regularTabGrid;

// Returns the GREYMatcher for the incognito tab grid.
+ (id<GREYMatcher>)incognitoTabGrid;

// Returns the GREYMatcher for the button to close the cell at `index` in the
// tab grid.
+ (id<GREYMatcher>)tabGridCloseButtonForCellAtIndex:(unsigned int)index;

// Returns the GREYMatcher for the button to close the tab group cell at `index`
// in the tab grid.
+ (id<GREYMatcher>)tabGridCloseButtonForGroupCellAtIndex:(unsigned int)index;

// Returns a matcher for the password settings collection view.
+ (id<GREYMatcher>)settingsPasswordMatcher;

// Returns a matcher for the search bar in password settings.
+ (id<GREYMatcher>)settingsPasswordSearchMatcher;

// Returns a matcher for the profiles settings collection view.
+ (id<GREYMatcher>)settingsProfileMatcher;

// Returns a matcher for the credit card settings collection view.
+ (id<GREYMatcher>)settingsCreditCardMatcher;

// Returns a matcher for the delete button at the bottom of settings collection
// views.
+ (id<GREYMatcher>)settingsBottomToolbarDeleteButton;

// Returns a matcher for the search engine button in the main settings view.
+ (id<GREYMatcher>)settingsSearchEngineButton;

// Returns a matcher for the address bar button in the main settings view.
+ (id<GREYMatcher>)settingsAddressBarButton;

// Returns a matcher for an autofill suggestion view.
+ (id<GREYMatcher>)autofillSuggestionViewMatcher;

// Returns a matcher to test whether the element is a scroll view with a content
// smaller than the scroll view bounds.
+ (id<GREYMatcher>)contentViewSmallerThanScrollView;

// Returns a matcher for a history entry with `url` and `title`.
+ (id<GREYMatcher>)historyEntryForURL:(NSString*)URL title:(NSString*)title;

// Returns a matcher to the add button in the toolbar of the settings view.
+ (id<GREYMatcher>)settingsToolbarAddButton;

// Returns a matcher matching cells that can be swiped-to-dismiss.
+ (id<GREYMatcher>)cellCanBeSwipedToDismissed;

// Returns a matcher to password table view.
+ (id<GREYMatcher>)passwordsTableViewMatcher;

// Returns a matcher to default browser settings table view.
+ (id<GREYMatcher>)defaultBrowserSettingsTableViewMatcher;

// Returns a matcher to safety check table view.
+ (id<GREYMatcher>)safetyCheckTableViewMatcher;

#pragma mark - Overflow Menu Destinations

// Returns matcher for the bookmarks destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)bookmarksDestinationButton;

// Returns matcher for the history destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)historyDestinationButton;

// Returns matcher for the passwords destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)passwordsDestinationButton;

// Returns matcher for the price notifications destination button in the
// overflow menu carousel.
+ (id<GREYMatcher>)priceNotificationsDestinationButton;

// Returns matcher for the reading list destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)readingListDestinationButton;

// Returns matcher for the recent tabs destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)recentTabsDestinationButton;

// Returns matcher for the settings destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)settingsDestinationButton;

// Returns matcher for the site info destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)siteInfoDestinationButton;

// Returns matcher for the downloads destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)downloadsDestinationButton;

// Returns matcher for the What's New destination button in the overflow menu
// carousel.
+ (id<GREYMatcher>)whatsNewDestinationButton;

#pragma mark - Overflow Menu Actions

// Returns matcher for the settings action button in the overflow menu
// carousel.
+ (id<GREYMatcher>)settingsActionButton;

#pragma mark - Promo style view controller

// Returns matcher for the primary action button.
+ (id<GREYMatcher>)promoStylePrimaryActionButtonMatcher;

// Returns matcher for the secondary action button.
+ (id<GREYMatcher>)promoStyleSecondaryActionButtonMatcher;

#pragma mark - Incognito Interstitial

// Returns a matcher for the Incognito Interstitial view controller.
+ (id<GREYMatcher>)incognitoInterstitial;

// Returns a matcher for the subtitle of the Incognito Interstitial,
// as it should appear when `URL` was given to the Interstitial.
+ (id<GREYMatcher>)incognitoInterstitialLabelForURL:(NSString*)url;

// Returns a matcher for the primary action button in the Incognito
// Interstitial.
+ (id<GREYMatcher>)incognitoInterstitialOpenInChromeIncognitoButton;

// Returns a matcher for the secondary action button in the Incognito
// Interstitial.
+ (id<GREYMatcher>)incognitoInterstitialOpenInChromeButton;

// Returns a matcher for the Cancel button in the Incognito Interstitial.
+ (id<GREYMatcher>)incognitoInterstitialCancelButton;

#pragma mark - Manual Fallback

// Returns the matcher for the Activity View header.
+ (id<GREYMatcher>)activityViewHeaderWithURLHost:(NSString*)host
                                           title:(NSString*)pageTitle;

// Returns a matcher for the button to accept the generated password.
+ (id<GREYMatcher>)useSuggestedPasswordMatcher;

#pragma mark - Tab Grid Edit Mode

// Returns a matcher for the button to open the context menu for edit actions.
+ (id<GREYMatcher>)tabGridEditButton;

// Returns a matcher for the context menu button to close all tabs.
+ (id<GREYMatcher>)tabGridEditMenuCloseAllButton;

// Returns a matcher for the context menu button to enter the tab grid tab
// selection mode.
+ (id<GREYMatcher>)tabGridSelectTabsMenuButton;

// Returns a matcher for the button to act on the selected tabs.
+ (id<GREYMatcher>)tabGridEditAddToButton;

// Returns a matcher for the button to close the selected tabs.
+ (id<GREYMatcher>)tabGridEditCloseTabsButton;

// Returns a matcher for the button to select all tabs.
+ (id<GREYMatcher>)tabGridEditSelectAllButton;

// Returns a matcher for the button to share tabs.
+ (id<GREYMatcher>)tabGridEditShareButton;

#pragma mark - Tab Grid Search Mode

// Returns a matcher for the button to enter the tab grid search mode.
+ (id<GREYMatcher>)tabGridSearchTabsButton;

// Returns a matcher for the tab grid search bar.
+ (id<GREYMatcher>)tabGridSearchBar;

// Returns a matcher for the tab grid search cancel button.
+ (id<GREYMatcher>)tabGridSearchCancelButton;

// Returns a matcher for the tab grid search mode toolbar.
+ (id<GREYMatcher>)tabGridSearchModeToolbar;

#pragma mark - Create Tab Group View

// Returns the matcher for the tab group creation view.
+ (id<GREYMatcher>)tabGroupCreationView;

// Returns the matcher for the text field's clear button in the tab group
// creation view.
+ (id<GREYMatcher>)createTabGroupTextField;

// Returns the matcher for the text field's clear button in the tab group
+ (id<GREYMatcher>)createTabGroupTextFieldClearButton;

// Returns the matcher for `Create Group` button in the tab group creation view.
+ (id<GREYMatcher>)createTabGroupCreateButton;

// Returns the matcher for the cancel button in the tab group creation view.
+ (id<GREYMatcher>)createTabGroupCancelButton;

#pragma mark - Tab Group View

// Returns the matcher for the tab group view.
+ (id<GREYMatcher>)tabGroupView;

// Returns the matcher for the title on the tab group view.
+ (id<GREYMatcher>)tabGroupViewTitle:(NSString*)title;

// Returns the matcher for the overflow menu button in the tab group view.
+ (id<GREYMatcher>)tabGroupOverflowMenuButton;

// Returns the matcher for the back button in the tab group view.
+ (id<GREYMatcher>)tabGroupBackButton;

#pragma mark - Tab Groups Context Menus

// Returns the matcher for `Add Tab to New Group` button in the context menu.
+ (id<GREYMatcher>)addTabToNewGroupButton;

// Returns the matcher for the sub menu button `New Tab Group` in the `Add Tab
// To Group` button.
+ (id<GREYMatcher>)addTabToGroupSubMenuButton;

// Returns the matcher for `Rename Group` button in the context menu of a tab
// group.
+ (id<GREYMatcher>)renameGroupButton;

// Returns the matcher for `Ungroup` button in the context menu of a tab group.
+ (id<GREYMatcher>)ungroupButton;

// Returns the matcher for `Ungroup` button in the confirmation dialog of a tab
// group. It's displayed only when tab groups sync is enabled.
+ (id<GREYMatcher>)ungroupConfirmationButton;

// Returns the matcher for `Delete Group` button in the context menu of a tab
// group.
+ (id<GREYMatcher>)deleteGroupButton;

// Returns the matcher for `Delete Group` button in the confirmation dialog of a
// tab group. It's displayed only when tab groups sync is enabled.
+ (id<GREYMatcher>)deleteGroupConfirmationButton;

// Returns the matcher for `Close Group` button in the context menu of a tab
// group.
+ (id<GREYMatcher>)closeGroupButton;

#pragma mark - Tab Groups Panel

// Returns the matcher for the tab groups panel view.
+ (id<GREYMatcher>)tabGroupsPanel;

@end

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_APP_INTERFACE_H_
