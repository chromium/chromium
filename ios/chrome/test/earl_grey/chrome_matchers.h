// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#import <UIKit/UIKit.h>

#include <string>

@protocol GREYMatcher;

namespace chrome_test_util {

// Returns a matcher for a window with a given number.
// Window numbers are assigned at scene creation. Normally, each EGTest will
// start with exactly one window with number 0. Each time a window is created,
// it is assigned an accessibility identifier equal to the number of connected
// scenes (stored as NSString). This means typically any windows created in a
// test will have consecutive numbers.
id<GREYMatcher> WindowWithNumber(int window_number);

// Shorthand matcher for creating a matcher that ensures the given matcher
// matches elements under the given window.
id<GREYMatcher> MatchInWindowWithNumber(int window_number,
                                        id<GREYMatcher> matcher);

// Same as above, but for the blocking window which only appears when a blocking
// UI is shown in another window.
id<GREYMatcher> MatchInBlockerWindowWithNumber(int window_number,
                                               id<GREYMatcher> matcher);

// Returns a matcher for element with accessibility label corresponding to
// `message_id` and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id);

// Returns a matcher for element with accessibility label corresponding to
// `message_id`, `number` for the plural rule and accessibility trait
// UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabelIdAndNumberForPlural(int message_id,
                                                                 int number);

// Returns a matcher for element with accessibility label corresponding to
// `label` and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label);

// Returns a matcher for element with with foreground color corresponding to
// `colorName` and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithForegroundColor(NSString* colorName);

// Returns a matcher for element with with background color corresponding to
// `colorName` and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithBackgroundColor(NSString* colorName);

// Returns a matcher for context menu items with accessibility label
// corresponding to `label`.
id<GREYMatcher> ContextMenuItemWithAccessibilityLabel(NSString* label);

// Returns a matcher for context menu items with accessibility label
// corresponding to `message_id`.
id<GREYMatcher> ContextMenuItemWithAccessibilityLabelId(int message_id);

// Returns a matcher for element with an image corresponding to `image_id`.
id<GREYMatcher> ImageViewWithImage(UIImage* image);

// Returns a matcher for element with an image defined by its name in the main
// bundle.
id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName);

// Returns a matcher for an element with a custom symbol defined by its name and
// point size in the main bundle.
id<GREYMatcher> ImageViewWithCustomSymbolNameAndPointSize(NSString* symbolName,
                                                          CGFloat pointSize);

// Returns a matcher for element with an image corresponding to `image_id` and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithImage(int image_id);

// Returns a matcher for element with accessibility label corresponding to
// `message_id` and accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id);

// Returns a matcher for element with accessibility label corresponding to
// `label` and accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label);

// Returns a matcher for a text element (label, field, etc) whose text contains
// `text` as a substring. (contrast with grey_text() which tests for a complete
// string match).
id<GREYMatcher> ContainsPartialText(NSString* text);

// Returns a matcher for element with accessibility label corresponding to
// `message_id` and accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id);

// Returns a matcher for element with accessibility label corresponding to
// `label` and accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label);

// Returns a matcher for navigation bar title element with accessibility label
// corresponding to `label_id`.
id<GREYMatcher> NavigationBarTitleWithAccessibilityLabelId(int label_id);

// Returns a matcher for text field of a cell with `message_id`.
id<GREYMatcher> TextFieldForCellWithLabelId(int message_id);

// Returns a matcher for icon view of a cell with `message_id`.
id<GREYMatcher> IconViewForCellWithLabelId(int message_id, NSString* icon_type);

// Returns a matcher for the primary toolbar.
id<GREYMatcher> PrimaryToolbar();

// Returns a matcher for a cancel button.
id<GREYMatcher> CancelButton();

// Returns the matcher for an enabled cancel button in a navigation bar.
id<GREYMatcher> NavigationBarCancelButton();

// Returns a matcher for close tab menu button.
id<GREYMatcher> CloseTabMenuButton();

// Returns a matcher for the navigate forward button.
id<GREYMatcher> ForwardButton();

// Returns a matcher for the navigate backward button.
id<GREYMatcher> BackButton();

// Returns a matcher for the reload button.
id<GREYMatcher> ReloadButton();

// Returns a matcher for the stop loading button.
id<GREYMatcher> StopButton();

// Returns a matcher for the omnibox.
id<GREYMatcher> Omnibox();

// Returns a matcher for the omnibox at the bottom.
id<GREYMatcher> OmniboxAtBottom();

// Returns a matcher for the omnibox on the top.
id<GREYMatcher> OmniboxOnTop();

// Returns matcher for the omnibox popup list row views.
id<GREYMatcher> OmniboxPopupRow();

// Returns a matcher for a popup row containing `string` as accessibility label.
id<GREYMatcher> OmniboxPopupRowWithString(NSString* string);

// Returns matcher for the omnibox popup list view.
id<GREYMatcher> OmniboxPopupList();

// Returns a matcher for the location view.
id<GREYMatcher> DefocusedLocationView();

// Returns a matcher for the page security info button.
id<GREYMatcher> PageSecurityInfoButton();
// Returns a matcher for the page security info indicator.
id<GREYMatcher> PageSecurityInfoIndicator();

// Returns a matcher for omnibox containing `text`. Performs an exact match of
// the omnibox contents.
id<GREYMatcher> OmniboxText(const std::string& text);

// Returns a matcher for `text` being a substring of the text in the omnibox.
id<GREYMatcher> OmniboxContainingText(const std::string& text);

// Returns a matcher for `text` being inline autocomplete text in the omnibox.
id<GREYMatcher> OmniboxContainingAutocompleteText(NSString* text);

// Returns a matcher for `text` being a substring of the text in the location
// view.
id<GREYMatcher> LocationViewContainingText(const std::string& text);

// Returns a matcher for Tools menu button.
id<GREYMatcher> ToolsMenuButton();

// Returns a matcher for the New Tab button, which can be long-pressed for a
// menu.
id<GREYMatcher> NewTabButton();

// Returns a matcher for the Share menu button.
id<GREYMatcher> ShareButton();

// Returns a matcher for a context menu button that contains `text`.
id<GREYMatcher> ContextMenuButtonContainingText(NSString* text);

// Returns a matcher for the tab Share button (either in the omnibox or
// toolbar).
id<GREYMatcher> TabShareButton();

// Returns a matcher for show tabs button.
// DO NOT use this matcher to open the tab grid. Instead use one of the helpers:
// `[ChromeEarlGrey  showTabSwitcher]` or `[ChromeEarlGreyUI openTabGrid]`.
id<GREYMatcher> ShowTabsButton();

// Returns a matcher for Add to reading list button.
id<GREYMatcher> AddToReadingListButton();

// Returns a matcher for Add to bookmarks button.
id<GREYMatcher> AddToBookmarksButton();

// Returns a matcher for TableViewSwitchCell.
id<GREYMatcher> TableViewSwitchCell(NSString* accessibility_identifier,
                                    BOOL is_toggled_on);

// Returns a matcher for TableViewSwitchCell.
id<GREYMatcher> TableViewSwitchCell(NSString* accessibility_identifier,
                                    BOOL is_toggled_on,
                                    BOOL is_enabled);

// Returns a matcher for LegacySyncSwitchCell.
id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on);

// Returns a matcher for the Open in New Tab option in the context menu when
// long pressing a link.
id<GREYMatcher> OpenLinkInNewTabButton();

// Returns a matcher for the Open in Incognito option in the context menu when
// long pressing a link.
id<GREYMatcher> OpenLinkInIncognitoButton();

// Returns a matcher for the Open in New Window option in the context menu when
// long pressing a link.
id<GREYMatcher> OpenLinkInNewWindowButton();

// Returns a matcher for the done button on the navigation bar.
id<GREYMatcher> NavigationBarDoneButton();

// Returns a matcher for the done button on the Bookmarks navigation bar.
id<GREYMatcher> BookmarksNavigationBarDoneButton();

// Returns a matcher for the back button on the Bookmarks navigation bar.
id<GREYMatcher> BookmarksNavigationBarBackButton();

// Returns a matcher for the add account accounts button.
id<GREYMatcher> AddAccountButton();

// Returns a matcher for the sign out accounts button.
id<GREYMatcher> SignOutAccountsButton();

// Returns a matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell();

// Returns a matcher for the clear browsing data button on the clear browsing
// data panel.
id<GREYMatcher> ClearBrowsingDataButton();

// Returns a matcher for the clear browsing data view.
id<GREYMatcher> ClearBrowsingDataView();

// Returns a matcher for the "Done" button in the settings' navigation bar.
id<GREYMatcher> SettingsDoneButton();

// Returns a matcher for the Autofill Credit Card "Payment Methods" edit view.
id<GREYMatcher> AutofillCreditCardEditTableView();

// Returns a matcher for the Autofill Credit Card "Payment Methods" view in the
// settings menu.
id<GREYMatcher> AutofillCreditCardTableView();

// Returns a matcher for the "Addresses and More" button in the settings menu.
id<GREYMatcher> AddressesAndMoreButton();

// Returns a matcher for the "Payment Methods" button in the settings menu.
id<GREYMatcher> PaymentMethodsButton();

// Returns a matcher for the "Languages" button in the settings menu.
id<GREYMatcher> LanguagesButton();

// Returns a matcher for the "Add Credit Card" view in the Settings menu.
id<GREYMatcher> AddCreditCardView();

// Returns a matcher for the "Add" credit card button in the Payment
// Methods add credit card view.
id<GREYMatcher> AddCreditCardButton();

// Returns a matcher for the "Cancel" button in the Payment Methods add credit
// card view.
id<GREYMatcher> AddCreditCardCancelButton();

// Returns a matcher for the tools menu table view.
id<GREYMatcher> ToolsMenuView();

// Returns a matcher for the OK button.
id<GREYMatcher> OKButton();

// Returns a matcher for the primary button in the sign-in promo view. This is
// "Sign in into Chrome" button for a cold state, or "Continue as John Doe" for
// a warm state.
id<GREYMatcher> PrimarySignInButton();

// Returns a matcher for the secondary button in the sign-in promo view. This is
// "Not johndoe@example.com" button.
id<GREYMatcher> SecondarySignInButton();

// Returns matcher for the identity chooser scrim that is shown behind the
// identity chooser dialog. Tapping on the scrim dismisses the dialog.
id<GREYMatcher> IdentityChooserScrim();

// Returns matcher for the cancel button in the fake add account flow.
id<GREYMatcher> FakeAddAccountScreenCancelButton();

// Returns a matcher for the button for the currently signed in account in the
// settings menu.
id<GREYMatcher> SettingsAccountButton();

// Returns a matcher for the accounts collection view.
id<GREYMatcher> SettingsAccountsCollectionView();

// Returns a matcher for the safety check table view.
id<GREYMatcher> SettingsSafetyCheckTableView();

// Returns a matcher for the privacy settings table view.
id<GREYMatcher> SettingsPrivacyTableView();

// Returns a matcher for the privacy safe browsing settings table view.
id<GREYMatcher> SettingsPrivacySafeBrowsingTableView();

// Returns a matcher for the notifications settings table view.
id<GREYMatcher> SettingsNotificationsTableView();

// Returns a matcher for the inactive tabs settings table view.
id<GREYMatcher> SettingsInactiveTabsTableView();

// Returns a matcher for the tabs settings table view.
id<GREYMatcher> SettingsTabsTableView();

// Returns a matcher for the tracking price settings table view.
id<GREYMatcher> SettingsTrackingPriceTableView();

// Returns a matcher for the Content Settings button on the main Settings
// screen.
id<GREYMatcher> ContentSettingsButton();

// Returns a matcher for the Google Services Settings button on the main
// Settings screen.
id<GREYMatcher> GoogleServicesSettingsButton();

// Returns a matcher for the Manage Sync Settings button on the main Settings
// screen.
id<GREYMatcher> ManageSyncSettingsButton();

// Returns a matcher for the Inactive Tabs Settings button on the Tabs Settings
// screen.
id<GREYMatcher> InactiveTabsSettingsButton();

// Returns a matcher for the Tabs Settings button on the main Settings screen.
id<GREYMatcher> TabsSettingsButton();

// Returns a matcher for the Google Services Settings view.
id<GREYMatcher> GoogleServicesSettingsView();

// Returns matcher for the Navigation Bar embedded in the Settings Navigation
// Controller.
id<GREYMatcher> SettingsNavigationBar();

// Returns a matcher for the back button on a settings menu.
id<GREYMatcher> SettingsMenuBackButton();

// Returns a matcher for the back button on a settings menu in given window
// number.
id<GREYMatcher> SettingsMenuBackButton(int window_number);

// Returns a matcher for the Privacy cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPrivacyButton();

// Returns a matcher for the Save passwords cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPasswordsButton();

// Returns matcher for the Safety Check cell on the main Settings screen.
id<GREYMatcher> SettingsMenuSafetyCheckButton();

// Returns a matcher for the Notifications cell on the main Settings
// screen.
id<GREYMatcher> SettingsMenuNotificationsButton();

// Returns a matcher for the payment request collection view.
id<GREYMatcher> PaymentRequestView();

// Returns a matcher for the error confirmation view for payment request.
id<GREYMatcher> PaymentRequestErrorView();

// Returns a matcher for the voice search button on the main Settings screen.
id<GREYMatcher> VoiceSearchButton();

// Returns a matcher for the voice search button on the omnibox input accessory.
id<GREYMatcher> VoiceSearchInputAccessoryButton();

// Returns a matcher for the settings main menu view.
id<GREYMatcher> SettingsCollectionView();

// Returns the matcher for the quick delete browsing data button.
id<GREYMatcher> BrowsingDataButtonMatcher();

// Returns the matcher for the quick delete browsing data confirmation button.
id<GREYMatcher> BrowsingDataConfirmButtonMatcher();

// Returns a matcher for the clear browsing history cell on the clear browsing
// data panel.
id<GREYMatcher> ClearBrowsingHistoryButton();

// Returns a matcher for the History table view.
id<GREYMatcher> HistoryTableView();

// Returns a matcher for the clear cookies cell on the clear browsing data
// panel.
id<GREYMatcher> ClearCookiesButton();

// Returns a matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> ClearCacheButton();

// Returns a matcher for the clear saved passwords cell on the clear browsing
// data panel.
id<GREYMatcher> ClearSavedPasswordsButton();

// Returns a matcher for the clear saved passwords cell on the clear browsing
// data panel.
id<GREYMatcher> ClearAutofillButton();

// Returns a matcher for the collection view of content suggestion.
id<GREYMatcher> ContentSuggestionsCollectionView();

// Returns a matcher for the collection view of the NTP.
id<GREYMatcher> NTPCollectionView();

// Returns a matcher for the NTP view when the user is in incognito mode.
id<GREYMatcher> NTPIncognitoView();

// Returns a matcher for the NTP Feed menu button which enables the feed.
id<GREYMatcher> NTPFeedMenuEnableButton();

// Returns a matcher for the NTP Feed menu button which disables the feed.
id<GREYMatcher> NTPFeedMenuDisableButton();

// Returns a matcher for the warning message while filling in payment requests.
id<GREYMatcher> WarningMessageView();

// Returns a matcher for the payment picker cell.
id<GREYMatcher> PaymentRequestPickerRow();

// Returns a matcher for the payment request search bar.
id<GREYMatcher> PaymentRequestPickerSearchBar();

// Returns a matcher for the New Window button on the Tools menu.
id<GREYMatcher> OpenNewWindowMenuButton();

// Returns a matcher for the system selection callout.
id<GREYMatcher> SystemSelectionCallout();

// Returns a matcher for the Link to text button in the edit menu.
id<GREYMatcher> SystemSelectionCalloutLinkToTextButton();

// Returns a matcher for the copy button on the system selection callout.
id<GREYMatcher> SystemSelectionCalloutCopyButton();

// Returns a matcher for the cut button on the system selection callout.
id<GREYMatcher> SystemSelectionCalloutCutButton();

// Returns a matcher for the paste button on the system selection callout.
id<GREYMatcher> SystemSelectionCalloutPasteButton();

// Returns a matcher for the system selection callout overflow button to show
// more menu items.
id<GREYMatcher> SystemSelectionCalloutOverflowButton();

// Returns a matcher for a Copy button, such as the one in the Activity View.
// This matcher is very broad and will look for any button with a matching
// string. Only the iOS 13 Activity View is reachable by EarlGrey.
id<GREYMatcher> CopyActivityButton();

// Returns a matcher for the Copy Link option in the updated context menus when
// long pressing on a link.
id<GREYMatcher> CopyLinkButton();

// Returns a matcher for the Edit option on the context menus.
id<GREYMatcher> EditButton();

// Returns a matcher for the Move option on the updated context menus.
id<GREYMatcher> MoveButton();

// Returns a matcher for the Mark as Read option on the Reading List's context
// menus.
id<GREYMatcher> ReadingListMarkAsReadButton();

// Returns a matcher for the Mark as Unread option on the Reading List's context
// menus.
id<GREYMatcher> ReadingListMarkAsUnreadButton();

// Returns a matcher for the Delete option on the updated context menus.
id<GREYMatcher> DeleteButton();

// Returns a matcher for the Copy item on the old-style context menu.
id<GREYMatcher> ContextMenuCopyButton();

// Returns a matcher for defocused omnibox on a new tab.
id<GREYMatcher> NewTabPageOmnibox();

// Returns a matcher for a fake omnibox on a new tab page.
id<GREYMatcher> FakeOmnibox();

// Returns a matcher for a header label of the Discover feed.
id<GREYMatcher> DiscoverHeaderLabel();

// Returns a matcher for a logo on a new tab page.
id<GREYMatcher> NTPLogo();

// Returns a matcher for the current WebView.
id<GREYMatcher> WebViewMatcher();

// Returns a matcher for the current WebState's scroll view.
id<GREYMatcher> WebStateScrollViewMatcher();

// Returns a matcher for the current WebState's scroll view in the given
// `window_number`.
id<GREYMatcher> WebStateScrollViewMatcherInWindowWithNumber(int window_number);

// Returns a matcher for the Clear Browsing Data button in the History UI.
id<GREYMatcher> HistoryClearBrowsingDataButton();

// Returns a matcher for "Open In..." button.
id<GREYMatcher> OpenInButton();

// Returns a matcher for "Open" button.
id<GREYMatcher> OpenPDFButton();

// Returns a matcher for the cell at `index` in the tab grid.
id<GREYMatcher> TabGridCellAtIndex(unsigned int index);

// Returns a matcher for the group cell at `index` in the tab grid.
id<GREYMatcher> TabGridGroupCellAtIndex(unsigned int index);

// Returns a matcher for the group cell for the given `group_name` and
// `tab_count`.
id<GREYMatcher> TabGridGroupCellWithName(NSString* group_name,
                                         NSInteger tab_count);

// Returns a matcher for the cell at `index` in the tab strip.
id<GREYMatcher> TabStripCellAtIndex(unsigned int index);

// Returns a matcher for the group cell at `index` in the tab strip.
id<GREYMatcher> TabStripGroupCellAtIndex(unsigned int index);

// Returns a matcher for the group cell at `index` in the tab groups panel.
id<GREYMatcher> TabGroupsPanelCellAtIndex(unsigned int index);

// Returns a matcher for the group cell created just now in the tab groups panel
// for the given `group_name` and `tab_count`.
id<GREYMatcher> TabGroupsPanelCellWithName(NSString* group_name,
                                           NSInteger tab_count);

// Returns a matcher for the button that closes the tab grid.
id<GREYMatcher> TabGridDoneButton();

// Returns a matcher for the button that closes all the tabs in the tab
// grid.
id<GREYMatcher> TabGridCloseAllButton();

// Returns a matcher for the button that reverts the close all tabs action
// in the tab grid.
id<GREYMatcher> TabGridUndoCloseAllButton();

// Returns a matcher for the cell that opens History in Recent Tabs.
id<GREYMatcher> TabGridSelectShowHistoryCell();

// Returns a matcher for the regular tabs empty state view.
id<GREYMatcher> TabGridRegularTabsEmptyStateView();

// Returns a matcher for the button that creates new non incognito tabs
// from within the tab grid.
id<GREYMatcher> TabGridNewTabButton();

// Returns a matcher for the button that creates new incognito tabs from
// within the tab grid.
id<GREYMatcher> TabGridNewIncognitoTabButton();

// Returns a matcher for the button to go to the non incognito panel in
// the tab grid.
id<GREYMatcher> TabGridOpenTabsPanelButton();

// Returns a matcher for the button to go to the incognito panel in
// the tab grid.
id<GREYMatcher> TabGridIncognitoTabsPanelButton();

// Returns a matcher for the button to go to the other devices panel in
// the tab grid.
id<GREYMatcher> TabGridOtherDevicesPanelButton();

// Returns the matcher for the tab group snack bar.
id<GREYMatcher> TabGroupSnackBar(int tabGroupCount);

// Returns the matcher for the tab group snackbar action.
id<GREYMatcher> TabGroupSnackBarAction();

// Returns a matcher for the button to go to the Tab Groups panel in
// the tab grid.
id<GREYMatcher> TabGridTabGroupsPanelButton();

// Returns a matcher for the button to go to the third panel in
// the tab grid. If Tab Group Sync is enabled, it's equivalent to
// `TabGridTabGroupsPanelButton`. Otherwise, it's equivalent to
// `TabGridOtherDevicesPanelButton`.
id<GREYMatcher> TabGridThirdPanelButton();

// Returns a matcher that matches tab grid normal mode page control - The
// PageControl panel always exist only on the tab grid normal mode, So this can
// be used to validate that the tab grid normal mode is active.
id<GREYMatcher> TabGridNormalModePageControl();

// Returns a matcher for the Inactive Tabs button of the tab grid.
id<GREYMatcher> TabGridInactiveTabsButton();

// Returns a matcher for the tab grid background.
id<GREYMatcher> TabGridBackground();

// Returns a matcher for the regular tab grid.
id<GREYMatcher> RegularTabGrid();

// Returns a matcher for the incognito tab grid.
id<GREYMatcher> IncognitoTabGrid();

// Returns a matcher for the button to close the cell at `index` in the
// tab grid.
id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index);

// Returns a matcher for the button to close the group cell at `index` in the
// tab grid.
id<GREYMatcher> TabGridCloseButtonForGroupCellAtIndex(unsigned int index);

// Returns a matcher for the password settings collection view.
id<GREYMatcher> SettingsPasswordMatcher();

// Returns a matcher for the search bar in password settings.
id<GREYMatcher> SettingsPasswordSearchMatcher();

// Returns a matcher for the profiles settings collection view.
id<GREYMatcher> SettingsProfileMatcher();

// Returns a matcher for the credit card settings collection view.
id<GREYMatcher> SettingsCreditCardMatcher();

// Returns a matcher for the delete button at the bottom of settings collection
// views.
id<GREYMatcher> SettingsBottomToolbarDeleteButton();

// Returns a matcher for the search engine button in the main settings view.
id<GREYMatcher> SettingsSearchEngineButton();

// Returns a matcher for the address bar button in the main settings view.
id<GREYMatcher> SettingsAddressBarButton();

// Returns a matcher for an autofill suggestion view.
id<GREYMatcher> AutofillSuggestionViewMatcher();

// Returns a matcher to test whether the element is a scroll view with a content
// smaller than the scroll view bounds.
id<GREYMatcher> ContentViewSmallerThanScrollView();

// Returns a matcher for a history entry with `url` and `title`.
id<GREYMatcher> HistoryEntry(const std::string& url, const std::string& title);

// Returns a matcher to the add button in the toolbar in the settings view.
id<GREYMatcher> SettingsToolbarAddButton();

// Returns a matcher matching cells that can be swiped-to-dismiss.
id<GREYMatcher> CellCanBeSwipedToDismissed();

// Returns a matcher for passwords table view.
id<GREYMatcher> PasswordsTableViewMatcher();

// Returns a mather for default browser settings table view.
id<GREYMatcher> DefaultBrowserSettingsTableViewMatcher();

// Returns a matcher for safety check table view.
id<GREYMatcher> SafetyCheckTableViewMatcher();

// Returns a matcher for action in an AlertCoordinator.
id<GREYMatcher> AlertAction(NSString* title);

// Returns the matcher for the iOS 13+ Activity View header.
id<GREYMatcher> ActivityViewHeader(NSString* url_host, NSString* page_title);

// Returns a matcher for the button to accept the generated password.
id<GREYMatcher> UseSuggestedPasswordMatcher();

#pragma mark - Promo style view controller

// Returns matcher for the primary action button.
id<GREYMatcher> PromoStylePrimaryActionButtonMatcher();

// Returns matcher for the secondary action button.
id<GREYMatcher> PromoStyleSecondaryActionButtonMatcher();

#pragma mark - Incognito Interstitial

// Returns a matcher for the Incognito Interstitial view controller.
id<GREYMatcher> IncognitoInterstitialMatcher();

// Returns a matcher for the subtitle of the Incognito Interstitial,
// as it should appear when `URL` was given to the Interstitial.
id<GREYMatcher> IncognitoInterstitialLabelForURL(const std::string& url);

// Returns a matcher for the primary action button in the Incognito
// Interstitial.
id<GREYMatcher> IncognitoInterstitialOpenInChromeIncognitoButton();

// Returns a matcher for the secondary action button in the Incognito
// Interstitial.
id<GREYMatcher> IncognitoInterstitialOpenInChromeButton();

// Returns a matcher for the Cancel button in the Incognito Interstitial.
id<GREYMatcher> IncognitoInterstitialCancelButton();

#pragma mark - Overflow Menu Destinations

// Returns a matcher for the bookmarks destination button in the overflow menu.
id<GREYMatcher> BookmarksDestinationButton();

// Returns a matcher for the history destination button in the overflow menu.
id<GREYMatcher> HistoryDestinationButton();

// Returns a matcher for the reading list destination button in the overflow
// menu.
id<GREYMatcher> ReadingListDestinationButton();

// Returns a matcher for the passwords destination button in the overflow menu.
id<GREYMatcher> PasswordsDestinationButton();

// Returns a matchwer for the price notifications destination button in the
// overflow menu.
id<GREYMatcher> PriceNotificationsDestinationButton();

// Returns a matcher for the downloads destination button in the overflow menu.
id<GREYMatcher> DownloadsDestinationButton();

// Returns a matcher for the recent tabs destination button in the overflow
// menu.
id<GREYMatcher> RecentTabsDestinationButton();

// Returns a matcher for the site info destination button in the overflow menu.
id<GREYMatcher> SiteInfoDestinationButton();

// Returns a matcher for the settings destination button in the overflow menu.
id<GREYMatcher> SettingsDestinationButton();

// Returns a matcher for the What's New destination button in the overflow menu.
id<GREYMatcher> WhatsNewDestinationButton();

#pragma mark - Overflow Menu Actions

// Returns a matcher for the settings action button in the overflow menu.
id<GREYMatcher> SettingsActionButton();

#pragma mark - Tab Grid Edit Mode

// Returns a matcher for the button to open the context menu for edit actions.
id<GREYMatcher> TabGridEditButton();

// Returns a matcher for the context menu button to close all tabs.
id<GREYMatcher> TabGridEditMenuCloseAllButton();

// Returns a matcher for the context menu button to enter the tab grid tab
// selection mode.
id<GREYMatcher> TabGridSelectTabsMenuButton();

// Returns a matcher for the button to act on the selected tabs.
id<GREYMatcher> TabGridEditAddToButton();

// Returns a matcher for the button to close the selected tabs.
id<GREYMatcher> TabGridEditCloseTabsButton();

// Returns a matcher for the button to select all tabs.
id<GREYMatcher> TabGridEditSelectAllButton();

// Returns a matcher for the button to share tabs.
id<GREYMatcher> TabGridEditShareButton();

#pragma mark - Tab Grid Search Mode

// Returns a matcher for the button to enter the tab grid search mode.
id<GREYMatcher> TabGridSearchTabsButton();

// Returns a matcher for the tab grid search bar text field.
id<GREYMatcher> TabGridSearchBar();

// Returns a matcher for the tab grid search cancel button.
id<GREYMatcher> TabGridSearchCancelButton();

// Returns a matcher for the tab grid search mode toolbar.
id<GREYMatcher> TabGridSearchModeToolbar();

#pragma mark - Create Tab Group View

// Returns the matcher for the tab group creation view.
id<GREYMatcher> TabGroupCreationView();

// Returns the matcher for the text field in the tab group creation view.
id<GREYMatcher> CreateTabGroupTextField();

// Returns the matcher for the text field's clear button in the tab group
// creation view.
id<GREYMatcher> CreateTabGroupTextFieldClearButton();

// Returns the matcher for `Create Group` button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCreateButton();

// Returns the matcher for the cancel button in the tab group creation view.
id<GREYMatcher> CreateTabGroupCancelButton();

#pragma mark - Tab Group View

// Returns the matcher for the tab group view.
id<GREYMatcher> TabGroupView();

// Returns the matcher for the title on the tab group view.
id<GREYMatcher> TabGroupViewTitle(NSString* title);

// Returns the matcher for the overflow menu button in the tab group view.
id<GREYMatcher> TabGroupOverflowMenuButton();

// Returns the matcher for the back button in the tab group view.
id<GREYMatcher> TabGroupBackButton();

#pragma mark - Tab Groups Context Menus

// Returns the matcher for `Add Tab to New Group` button in the context menu.
id<GREYMatcher> AddTabToNewGroupButton();

// Returns the matcher for the sub menu button `New Tab Group` in the `Add Tab
// To Group` button.
id<GREYMatcher> AddTabToGroupSubMenuButton();

// Returns the matcher for `Rename Group` button in the context menu of a tab
// group.
id<GREYMatcher> RenameGroupButton();

// Returns the matcher for `Ungroup` button in the context menu of a tab group.
id<GREYMatcher> UngroupButton();

// Returns the matcher for `Ungroup` button in the confirmation dialog of a tab
// group. It's displayed only when tab groups sync is enabled.
id<GREYMatcher> UngroupConfirmationButton();

// Returns the matcher for `Delete Group` button in the context menu of a tab
// group.
id<GREYMatcher> DeleteGroupButton();

// Returns the matcher for `Delete Group` button in the confirmation dialog of a
// tab group. It's displayed only when tab groups sync is enabled.
id<GREYMatcher> DeleteGroupConfirmationButton();

// Returns the matcher for `Close Group` button in the context menu of a tab
// group.
id<GREYMatcher> CloseGroupButton();

#pragma mark - Tab Groups Panel

// Returns the matcher for the tab groups panel view.
id<GREYMatcher> TabGroupsPanel();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
