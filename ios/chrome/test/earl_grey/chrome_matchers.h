// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_

#import <Foundation/Foundation.h>

#include <string>

@protocol GREYMatcher;

namespace chrome_test_util {

// Matcher for a window with a given number.
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

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label);

// Matcher for element with an image corresponding to |image_id|.
id<GREYMatcher> ImageViewWithImage(int image_id);

// Matcher for element with an image defined by its name in the main bundle.
id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName);

// Matcher for element with an image corresponding to |image_id| and
// accessibility trait UIAccessibilityTraitButton.
id<GREYMatcher> ButtonWithImage(int image_id);

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitStaticText.
id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label);

// Matcher for a text element (label, field, etc) whose text contains |text| as
// a substring. (contrast with grey_text() which tests for a complete string
// match).
id<GREYMatcher> ContainsPartialText(NSString* text);

// Matcher for element with accessibility label corresponding to |message_id|
// and accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabelId(int message_id);

// Matcher for element with accessibility label corresponding to |label| and
// accessibility trait UIAccessibilityTraitHeader.
id<GREYMatcher> HeaderWithAccessibilityLabel(NSString* label);

// Matcher for navigation bar title element with accessibility label
// corresponding to |label_id|.
id<GREYMatcher> NavigationBarTitleWithAccessibilityLabelId(int label_id);

// Matcher for text field of a cell with |message_id|.
id<GREYMatcher> TextFieldForCellWithLabelId(int message_id);

// Matcher for icon view of a cell with |message_id|.
id<GREYMatcher> IconViewForCellWithLabelId(int message_id, NSString* icon_type);

// Returns matcher for the primary toolbar.
id<GREYMatcher> PrimaryToolbar();

// Returns matcher for a cancel button.
id<GREYMatcher> CancelButton();

// Returns the matcher for an enabled cancel button in a navigation bar.
id<GREYMatcher> NavigationBarCancelButton();

// Returns matcher for a close button.
id<GREYMatcher> CloseButton();

// Returns matcher for close tab menu button.
id<GREYMatcher> CloseTabMenuButton();

// Matcher for the navigate forward button.
id<GREYMatcher> ForwardButton();

// Matcher for the navigate backward button.
id<GREYMatcher> BackButton();

// Matcher for the reload button.
id<GREYMatcher> ReloadButton();

// Matcher for the stop loading button.
id<GREYMatcher> StopButton();

// Returns a matcher for the omnibox.
id<GREYMatcher> Omnibox();

// Returns a matcher for the location view.
id<GREYMatcher> DefocusedLocationView();

// Returns a matcher for the page security info button.
id<GREYMatcher> PageSecurityInfoButton();
// Returns a matcher for the page security info indicator.
id<GREYMatcher> PageSecurityInfoIndicator();

// Returns matcher for omnibox containing |text|. Performs an exact match of the
// omnibox contents.
id<GREYMatcher> OmniboxText(const std::string& text);

// Returns matcher for |text| being a substring of the text in the omnibox.
id<GREYMatcher> OmniboxContainingText(const std::string& text);

// Returns matcher for |text| being a substring of the text in the location
// view.
id<GREYMatcher> LocationViewContainingText(const std::string& text);

// Matcher for Tools menu button.
id<GREYMatcher> ToolsMenuButton();

// Matcher for the Share menu button.
id<GREYMatcher> ShareButton();

// Matcher for the tab Share button (either in the omnibox or toolbar).
id<GREYMatcher> TabShareButton();

// Matcher for show tabs button.
id<GREYMatcher> ShowTabsButton();

// Matcher for Add to reading list button.
id<GREYMatcher> AddToReadingListButton();

// Matcher for Add to bookmarks button.
id<GREYMatcher> AddToBookmarksButton();

// Matcher for SettingsSwitchCell.
id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on);

// Matcher for SettingsSwitchCell.
id<GREYMatcher> SettingsSwitchCell(NSString* accessibility_identifier,
                                   BOOL is_toggled_on,
                                   BOOL is_enabled);

// Matcher for LegacySyncSwitchCell.
id<GREYMatcher> SyncSwitchCell(NSString* accessibility_label,
                               BOOL is_toggled_on);

// Matcher for the Open in New Tab option in the context menu when long pressing
// a link.
id<GREYMatcher> OpenLinkInNewTabButton();

// Matcher for the Open in Incognito option in the context menu when long
// pressing a link. |use_new_string| determines which string to use.
id<GREYMatcher> OpenLinkInIncognitoButton(BOOL use_new_string);

// Matcher for the Open in New Window option in the context menu when long
// pressing a link.
id<GREYMatcher> OpenLinkInNewWindowButton();

// Matcher for the done button on the navigation bar.
id<GREYMatcher> NavigationBarDoneButton();

// Matcher for the done button on the Bookmarks navigation bar.
id<GREYMatcher> BookmarksNavigationBarDoneButton();

// Matcher for the back button on the Bookmarks navigation bar.
id<GREYMatcher> BookmarksNavigationBarBackButton();

// Returns matcher for the add account accounts button.
id<GREYMatcher> AddAccountButton();

// Returns matcher for the sign out accounts button.
id<GREYMatcher> SignOutAccountsButton();

// Returns matcher for the Clear Browsing Data cell on the Privacy screen.
id<GREYMatcher> ClearBrowsingDataCell();

// Returns matcher for the clear browsing data button on the clear browsing data
// panel.
id<GREYMatcher> ClearBrowsingDataButton();

// Returns matcher for the clear browsing data view.
id<GREYMatcher> ClearBrowsingDataView();

// Matcher for the clear browsing data action sheet item.
id<GREYMatcher> ConfirmClearBrowsingDataButton();

// Returns matcher for the settings button in the tools menu.
id<GREYMatcher> SettingsMenuButton();

// Returns matcher for the "Done" button in the settings' navigation bar.
id<GREYMatcher> SettingsDoneButton();

// Returns matcher for the "Confirm" button in the Sync and Google Services
// settings' navigation bar.
id<GREYMatcher> SyncSettingsConfirmButton();

// Returns matcher for the Autofill Credit Card "Payment Methods" edit view.
id<GREYMatcher> AutofillCreditCardEditTableView();

// Returns matcher for the Autofill Credit Card "Payment Methods" view in the
// settings menu.
id<GREYMatcher> AutofillCreditCardTableView();

// Returns matcher for the "Addresses and More" button in the settings menu.
id<GREYMatcher> AddressesAndMoreButton();

// Returns matcher for the "Payment Methods" button in the settings menu.
id<GREYMatcher> PaymentMethodsButton();

// Returns matcher for the "Languages" button in the settings menu.
id<GREYMatcher> LanguagesButton();

// Returns matcher for the "Add Credit Card" view in the Settings menu.
id<GREYMatcher> AddCreditCardView();

// Returns matcher for the "Add Payment Method" button in the Settings Payment
// Methods view.
id<GREYMatcher> AddPaymentMethodButton();

// Returns matcher for the "Add" credit card button in the Payment
// Methods add credit card view.
id<GREYMatcher> AddCreditCardButton();

// Returns matcher for the "Cancel" button in the Payment Methods add credit
// card view.
id<GREYMatcher> AddCreditCardCancelButton();

// Returns matcher for the tools menu table view.
id<GREYMatcher> ToolsMenuView();

// Returns matcher for the OK button.
id<GREYMatcher> OKButton();

// Returns matcher for the primary button in the sign-in promo view. This is
// "Sign in into Chrome" button for a cold state, or "Continue as John Doe" for
// a warm state.
id<GREYMatcher> PrimarySignInButton();

// Returns matcher for the secondary button in the sign-in promo view. This is
// "Not johndoe@example.com" button.
id<GREYMatcher> SecondarySignInButton();

// Returns matcher for the button for the currently signed in account in the
// settings menu.
id<GREYMatcher> SettingsAccountButton();

// Returns matcher for the accounts collection view.
id<GREYMatcher> SettingsAccountsCollectionView();

// Returns matcher for the Import Data cell in switch sync account view.
id<GREYMatcher> SettingsImportDataImportButton();

// Returns matcher for the Keep Data Separate cell in switch sync account view.
id<GREYMatcher> SettingsImportDataKeepSeparateButton();

// Returns matcher for the Keep Data Separate cell in switch sync account view.
id<GREYMatcher> SettingsImportDataContinueButton();

// Returns matcher for the privacy settings table view.
id<GREYMatcher> SettingsPrivacyTableView();

// Returns matcher for the menu button to sync accounts.
id<GREYMatcher> AccountsSyncButton();

// Returns matcher for the Content Settings button on the main Settings screen.
id<GREYMatcher> ContentSettingsButton();

// Returns matcher for the Google Services Settings button on the main Settings
// screen.
id<GREYMatcher> GoogleServicesSettingsButton();

// Returns matcher for the Google Services Settings view.
id<GREYMatcher> GoogleServicesSettingsView();

// Returns matcher for the back button on a settings menu.
id<GREYMatcher> SettingsMenuBackButton();

// Returns matcher for the back button on a settings menu in given window
// number.
id<GREYMatcher> SettingsMenuBackButton(int window_number);

// Returns matcher for the Privacy cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPrivacyButton();

// Returns matcher for the Save passwords cell on the main Settings screen.
id<GREYMatcher> SettingsMenuPasswordsButton();

// Returns matcher for the payment request collection view.
id<GREYMatcher> PaymentRequestView();

// Returns matcher for the error confirmation view for payment request.
id<GREYMatcher> PaymentRequestErrorView();

// Returns matcher for the voice search button on the main Settings screen.
id<GREYMatcher> VoiceSearchButton();

// Returns matcher for the voice search button on the omnibox input accessory.
id<GREYMatcher> VoiceSearchInputAccessoryButton();

// Returns matcher for the settings main menu view.
id<GREYMatcher> SettingsCollectionView();

// Returns matcher for the clear browsing history cell on the clear browsing
// data panel.
id<GREYMatcher> ClearBrowsingHistoryButton();

// Returns matcher for the clear cookies cell on the clear browsing data panel.
id<GREYMatcher> ClearCookiesButton();

// Returns matcher for the clear cache cell on the clear browsing data panel.
id<GREYMatcher> ClearCacheButton();

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
id<GREYMatcher> ClearSavedPasswordsButton();

// Returns matcher for the clear saved passwords cell on the clear browsing data
// panel.
id<GREYMatcher> ClearAutofillButton();

// Returns matcher for the collection view of content suggestion.
id<GREYMatcher> ContentSuggestionsCollectionView();

// Returns matcher for the collection view of the NTP.
id<GREYMatcher> NTPCollectionView();

// Returns matcher for the warning message while filling in payment requests.
id<GREYMatcher> WarningMessageView();

// Returns matcher for the payment picker cell.
id<GREYMatcher> PaymentRequestPickerRow();

// Returns matcher for the payment request search bar.
id<GREYMatcher> PaymentRequestPickerSearchBar();

// Returns matcher for the New Window button on the Tools menu.
id<GREYMatcher> OpenNewWindowMenuButton();

// Returns matcher for the reading list on the Tools menu.
id<GREYMatcher> ReadingListMenuButton();

// Returns matcher for the bookmarks button on the Tools menu.
id<GREYMatcher> BookmarksMenuButton();

// Returns matcher for the recent tabs button on the Tools menu.
id<GREYMatcher> RecentTabsMenuButton();

// Returns matcher for the system selection callout.
id<GREYMatcher> SystemSelectionCallout();

// Returns a matcher for the Link to text button in the edit menu.
id<GREYMatcher> SystemSelectionCalloutLinkToTextButton();

// Returns matcher for the copy button on the system selection callout.
id<GREYMatcher> SystemSelectionCalloutCopyButton();

// Returns matcher for the system selection callout overflow button to show more
// menu items.
id<GREYMatcher> SystemSelectionCalloutOverflowButton();

// Matcher for a Copy button, such as the one in the Activity View. This matcher
// is very broad and will look for any button with a matching string.
// Only the iOS 13 Activity View is reachable by EarlGrey.
id<GREYMatcher> CopyActivityButton();

// Matcher for the Copy Link option in the updated context menus when long
// pressing on a link. |use_new_string| determines which string to use.
id<GREYMatcher> CopyLinkButton(BOOL use_new_string);

// Matcher for the Edit option on the updated context menus. |use_new_string|
// determines which string to use.
id<GREYMatcher> EditButton(BOOL use_new_string);

// Matcher for the Move option on the updated context menus.
id<GREYMatcher> MoveButton();

// Matcher for the Mark as Read option on the Reading List's context menus.
id<GREYMatcher> ReadingListMarkAsReadButton();

// Matcher for the Mark as Unread option on the Reading List's context menus.
id<GREYMatcher> ReadingListMarkAsUnreadButton();

// Matcher for the Delete option on the updated context menus.
id<GREYMatcher> DeleteButton();

// Returns matcher for the Copy item on the old-style context menu.
id<GREYMatcher> ContextMenuCopyButton();

// Returns matcher for defoucesed omnibox on a new tab.
id<GREYMatcher> NewTabPageOmnibox();

// Returns matcher for a fake omnibox on a new tab page.
id<GREYMatcher> FakeOmnibox();

// Returns matcher for a header label of the Discover feed.
id<GREYMatcher> DiscoverHeaderLabel();

// Returns matcher for a logo on a new tab page.
id<GREYMatcher> NTPLogo();

// Returns a matcher for the current WebView.
id<GREYMatcher> WebViewMatcher();

// Returns a matcher for the current WebState's scroll view.
id<GREYMatcher> WebStateScrollViewMatcher();

// Returns a matcher for the current WebState's scroll view in the given
// |window_number|.
id<GREYMatcher> WebStateScrollViewMatcherInWindowWithNumber(int window_number);

// Returns a matcher for the Clear Browsing Data button in the History UI.
id<GREYMatcher> HistoryClearBrowsingDataButton();

// Returns a matcher for "Open In..." button.
id<GREYMatcher> OpenInButton();

// Returns the GREYMatcher for the cell at |index| in the tab grid.
id<GREYMatcher> TabGridCellAtIndex(unsigned int index);

// Returns the GREYMatcher for the button that closes the tab grid.
id<GREYMatcher> TabGridDoneButton();

// Returns the GREYMatcher for the button that closes all the tabs in the tab
// grid.
id<GREYMatcher> TabGridCloseAllButton();

// Returns the GREYMatcher for the button that reverts the close all tabs action
// in the tab grid.
id<GREYMatcher> TabGridUndoCloseAllButton();

// Returns the GREYMatcher for the cell that opens History in Recent Tabs.
id<GREYMatcher> TabGridSelectShowHistoryCell();

// Returns the GREYMatcher for the regular tabs empty state view.
id<GREYMatcher> TabGridRegularTabsEmptyStateView();

// Returns the GREYMatcher for the button that creates new non incognito tabs
// from within the tab grid.
id<GREYMatcher> TabGridNewTabButton();

// Returns the GREYMatcher for the button that creates new incognito tabs from
// within the tab grid.
id<GREYMatcher> TabGridNewIncognitoTabButton();

// Returns the GREYMatcher for the button to go to the non incognito panel in
// the tab grid.
id<GREYMatcher> TabGridOpenTabsPanelButton();

// Returns the GREYMatcher for the button to go to the incognito panel in
// the tab grid.
id<GREYMatcher> TabGridIncognitoTabsPanelButton();

// Returns the GREYMatcher for the button to go to the other devices panel in
// the tab grid.
id<GREYMatcher> TabGridOtherDevicesPanelButton();

// Returns the GREYMatcher for the tab grid background.
id<GREYMatcher> TabGridBackground();

// Returns the GREYMatcher for the regular tab grid.
id<GREYMatcher> RegularTabGrid();

// Returns the GREYMatcher for the incognito tab grid.
id<GREYMatcher> IncognitoTabGrid();

// Returns the GREYMatcher for the button to close the cell at |index| in the
// tab grid.
id<GREYMatcher> TabGridCloseButtonForCellAtIndex(unsigned int index);

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

// Returns a matcher for an autofill suggestion view.
id<GREYMatcher> AutofillSuggestionViewMatcher();

// Returns a matcher to test whether the element is a scroll view with a content
// smaller than the scroll view bounds.
id<GREYMatcher> ContentViewSmallerThanScrollView();

// Returns a matcher for the infobar asking to save a credit card locally.
id<GREYMatcher> AutofillSaveCardLocallyInfobar();

// Returns a matcher for the infobar asking to upload a credit card.
id<GREYMatcher> AutofillUploadCardInfobar();

// Returns a matcher for a history entry with |url| and |title|.
id<GREYMatcher> HistoryEntry(const std::string& url, const std::string& title);

#pragma mark - Manual Fallback

// Returns a matcher for the scroll view in keyboard accessory bar.
id<GREYMatcher> ManualFallbackFormSuggestionViewMatcher();

// Returns a matcher for the keyboard icon in the keyboard accessory bar.
id<GREYMatcher> ManualFallbackKeyboardIconMatcher();

// Returns a matcher for the password icon in the keyboard accessory bar.
id<GREYMatcher> ManualFallbackPasswordIconMatcher();

// Returns a matcher for the password table view in manual fallback.
id<GREYMatcher> ManualFallbackPasswordTableViewMatcher();

// Returns a matcher for the password search bar in manual fallback.
id<GREYMatcher> ManualFallbackPasswordSearchBarMatcher();

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManualFallbackManagePasswordsMatcher();

// Returns a matcher for the button to open all passwords in manual fallback.
id<GREYMatcher> ManualFallbackOtherPasswordsMatcher();

// Returns a matcher for the button to dismiss all passwords in manual fallback.
id<GREYMatcher> ManualFallbackOtherPasswordsDismissMatcher();

// Returns a matcher for the a password in the manual fallback list.
id<GREYMatcher> ManualFallbackPasswordButtonMatcher();

// Returns a matcher for the PasswordTableView window.
id<GREYMatcher> ManualFallbackPasswordTableViewWindowMatcher();

// Returns a matcher for the profiles icon in the keyboard accessory bar.
id<GREYMatcher> ManualFallbackProfilesIconMatcher();

// Returns a matcher for the profiles table view in manual fallback.
id<GREYMatcher> ManualFallbackProfilesTableViewMatcher();
// Returns a matcher for the button to open profile settings in manual
// fallback.
id<GREYMatcher> ManualFallbackManageProfilesMatcher();

// Returns a matcher for the profiles settings collection view.
id<GREYMatcher> SettingsProfileMatcher();

// Returns a matcher for the ProfileTableView window.
id<GREYMatcher> ManualFallbackProfileTableViewWindowMatcher();

// Returns a matcher for the credit card icon in the keyboard accessory bar.
id<GREYMatcher> ManualFallbackCreditCardIconMatcher();

// Returns a matcher for the credit card table view in manual fallback.
id<GREYMatcher> ManualFallbackCreditCardTableViewMatcher();

// Returns a matcher for the button to open password settings in manual
// fallback.
id<GREYMatcher> ManualFallbackManageCreditCardsMatcher();

// Returns a matcher for the button to add credit cards settings in manual
// fallback.
id<GREYMatcher> ManualFallbackAddCreditCardsMatcher();

// Returns a matcher for the CreditCardTableView window.
id<GREYMatcher> ManualFallbackCreditCardTableViewWindowMatcher();

// Returns the matcher for the iOS 13+ Activity View header.
id<GREYMatcher> ActivityViewHeader(NSString* url_host, NSString* page_title);

// Returns a matcher for the button to trigger password generation on manual
// fallback.
id<GREYMatcher> ManualFallbackSuggestPasswordMatcher();

// Returns a matcher for the button to accept the generated password.
id<GREYMatcher> UseSuggestedPasswordMatcher();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_MATCHERS_H_
