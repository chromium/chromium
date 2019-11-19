// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/form_suggestion_constants.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/history/history_ui_constants.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_picker_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/credit_card_scanner/credit_card_scanner_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/advanced_signin_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/toolbar/keyboard_assist/toolbar_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Identifer for cell at given |index| in the tab grid.
NSString* IdentifierForCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

id<GREYMatcher> SettingsSwitchIsToggledOn(BOOL is_toggled_on) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    SettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && is_toggled_on) ||
           (!switch_view.on && !is_toggled_on);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchToggledState(%@)",
                                   is_toggled_on ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> SettingsSwitchIsEnabled(BOOL is_enabled) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    SettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<SettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.enabled && is_enabled) ||
           (!switch_view.enabled && !is_enabled);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchEnabledState(%@)",
                                   is_enabled ? @"YES" : @"NO"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns the subview of |parentView| corresponding to the
// ContentSuggestionsViewController. Returns nil if it is not in its subviews.
UIView* SubviewWithAccessibilityIdentifier(NSString* accessibility_id,
                                           UIView* parent_view) {
  if (parent_view.accessibilityIdentifier == accessibility_id) {
    return parent_view;
  }
  for (UIView* view in parent_view.subviews) {
    UIView* result_view =
        SubviewWithAccessibilityIdentifier(accessibility_id, view);
    if (result_view)
      return result_view;
  }
  return nil;
}

}  // namespace

@implementation ChromeMatchersAppInterface

+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabel:l10n_util::GetNSStringWithFixup(messageID)];
}

+ (id<GREYMatcher>)imageViewWithImageNamed:(NSString*)imageName {
  UIImage* expectedImage = [UIImage imageNamed:imageName];
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching image named %@", imageName];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)imageViewWithImage:(int)imageID {
  UIImage* expectedImage = NativeImage(imageID);
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageID];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)buttonWithImage:(int)imageID {
  UIImage* expectedImage = NativeImage(imageID);
  GREYMatchesBlock matches = ^BOOL(UIButton* button) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     [button currentImage]);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching %i", imageID];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    imageMatcher, nil);
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                           messageID))];
}

+ (id<GREYMatcher>)staticTextWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

+ (id<GREYMatcher>)headerWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      headerWithAccessibilityLabel:(l10n_util::GetNSStringWithFixup(
                                       messageID))];
}

+ (id<GREYMatcher>)headerWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitHeader), nil);
}

+ (id<GREYMatcher>)textFieldForCellWithLabelID:(int)messageID {
  return grey_allOf(grey_accessibilityID([l10n_util::GetNSStringWithFixup(
                        messageID) stringByAppendingString:@"_textField"]),
                    grey_kindOfClass([UITextField class]), nil);
}

+ (id<GREYMatcher>)iconViewForCellWithLabelID:(int)messageID
                                     iconType:(NSString*)iconType {
  return grey_allOf(grey_accessibilityID([l10n_util::GetNSStringWithFixup(
                        messageID) stringByAppendingString:iconType]),
                    grey_kindOfClass([UIImageView class]), nil);
}

+ (id<GREYMatcher>)primaryToolbar {
  return grey_kindOfClass([PrimaryToolbarView class]);
}

+ (id<GREYMatcher>)cancelButton {
  return
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_CANCEL)];
}

+ (id<GREYMatcher>)navigationBarCancelButton {
  return grey_allOf(
      grey_ancestor(grey_kindOfClass([UINavigationBar class])),
      [self cancelButton],
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

+ (id<GREYMatcher>)closeButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_CLOSE)],
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitNotEnabled)), nil);
}

+ (id<GREYMatcher>)forwardButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_ACCNAME_FORWARD)];
}

+ (id<GREYMatcher>)backButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_ACCNAME_BACK)];
}

+ (id<GREYMatcher>)reloadButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_ACCNAME_RELOAD)];
}

+ (id<GREYMatcher>)stopButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_ACCNAME_STOP)];
}

+ (id<GREYMatcher>)omnibox {
  return grey_allOf(grey_kindOfClass([OmniboxTextFieldIOS class]),
                    grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)defocusedLocationView {
  return grey_kindOfClass([LocationBarSteadyView class]);
}

+ (id<GREYMatcher>)pageSecurityInfoButton {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)pageSecurityInfoIndicator {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)omniboxText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::mac::ObjCCast<OmniboxTextFieldIOS>(element);
        return [omnibox.text isEqualToString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:@"Omnibox contains text '%@'",
                                                  text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)omniboxContainingText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(UITextField* element) {
        return [element.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:@"Omnibox contains text '%@'",
                                                  text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)locationViewContainingText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(LocationBarSteadyView* element) {
        return [element.locationLabel.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:
                               @"LocationBarSteadyView contains text '%@'",
                               text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)toolsMenuButton {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)shareButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_TOOLS_MENU_SHARE)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabletTabSwitcherOpenButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER)];
}

+ (id<GREYMatcher>)showTabsButton {
  if (IsIPadIdiom()) {
    return grey_accessibilityID(@"Enter Tab Switcher");
  }
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn {
  return [ChromeMatchersAppInterface settingsSwitchCell:accessibilityIdentifier
                                            isToggledOn:isToggledOn
                                              isEnabled:YES];
}

+ (id<GREYMatcher>)settingsSwitchCell:(NSString*)accessibilityIdentifier
                          isToggledOn:(BOOL)isToggledOn
                            isEnabled:(BOOL)isEnabled {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    SettingsSwitchIsToggledOn(isToggledOn),
                    SettingsSwitchIsEnabled(isEnabled),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)syncSwitchCell:(NSString*)accessibilityLabel
                      isToggledOn:(BOOL)isToggledOn {
  return grey_allOf(
      grey_accessibilityLabel(accessibilityLabel),
      grey_accessibilityValue(
          isToggledOn ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                      : l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)openLinkInNewTabButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)];
}

+ (id<GREYMatcher>)navigationBarDoneButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)],
      grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

+ (id<GREYMatcher>)accountConsistencyConfirmationOKButton {
  int labelID = IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON;
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:labelID];
}

+ (id<GREYMatcher>)unifiedConsentAddAccountButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          (IDS_IOS_ACCOUNT_UNIFIED_CONSENT_ADD_ACCOUNT)];
}

+ (id<GREYMatcher>)addAccountButton {
  return grey_accessibilityID(kSettingsAccountsTableViewAddAccountCellId);
}

+ (id<GREYMatcher>)signOutAccountsButton {
  return grey_accessibilityID(kSettingsAccountsTableViewSignoutCellId);
}

+ (id<GREYMatcher>)clearBrowsingDataCell {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)];
}

+ (id<GREYMatcher>)clearBrowsingDataButton {
  return grey_accessibilityID(kClearBrowsingDataButtonIdentifier);
}

+ (id<GREYMatcher>)clearBrowsingDataView {
  return grey_accessibilityID(kClearBrowsingDataViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)confirmClearBrowsingDataButton {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityID(kClearBrowsingDataButtonIdentifier)),
      grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)settingsMenuButton {
  return grey_accessibilityID(kToolsMenuSettingsId);
}

+ (id<GREYMatcher>)settingsDoneButton {
  return grey_accessibilityID(kSettingsDoneButtonId);
}

+ (id<GREYMatcher>)syncSettingsConfirmButton {
  return grey_accessibilityID(kSyncSettingsConfirmButtonId);
}

+ (id<GREYMatcher>)paymentMethodsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_AUTOFILL_PAYMENT_METHODS)];
}

+ (id<GREYMatcher>)addCreditCardView {
  return grey_accessibilityID(kAddCreditCardViewID);
}

+ (id<GREYMatcher>)autofillCreditCardTableView {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

+ (id<GREYMatcher>)addPaymentMethodButton {
  return grey_accessibilityID(kSettingsAddPaymentMethodButtonId);
}

+ (id<GREYMatcher>)addCreditCardButton {
  return grey_accessibilityID(kSettingsAddCreditCardButtonID);
}

+ (id<GREYMatcher>)addCreditCardCancelButton {
  return grey_accessibilityID(kSettingsAddCreditCardCancelButtonID);
}

+ (id<GREYMatcher>)creditCardScannerView {
  return grey_accessibilityID(kCreditCardScannerViewID);
}

+ (id<GREYMatcher>)toolsMenuView {
  return grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
}

+ (id<GREYMatcher>)OKButton {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_OK)];
}

+ (id<GREYMatcher>)primarySignInButton {
  return grey_accessibilityID(kSigninPromoPrimaryButtonId);
}

+ (id<GREYMatcher>)secondarySignInButton {
  return grey_accessibilityID(kSigninPromoSecondaryButtonId);
}

+ (id<GREYMatcher>)settingsAccountButton {
  return grey_accessibilityID(kSettingsAccountCellId);
}

+ (id<GREYMatcher>)settingsAccountsCollectionView {
  return grey_accessibilityID(kSettingsAccountsTableViewId);
}

+ (id<GREYMatcher>)settingsImportDataImportButton {
  return grey_accessibilityID(kImportDataImportCellId);
}

+ (id<GREYMatcher>)settingsImportDataKeepSeparateButton {
  return grey_accessibilityID(kImportDataKeepSeparateCellId);
}

+ (id<GREYMatcher>)settingsImportDataContinueButton {
  return grey_accessibilityID(kImportDataContinueButtonId);
}

+ (id<GREYMatcher>)settingsPrivacyTableView {
  return grey_accessibilityID(kPrivacyTableViewId);
}

+ (id<GREYMatcher>)accountsSyncButton {
  return grey_allOf(grey_accessibilityID(kSettingsAccountsTableViewSyncCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)contentSettingsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)googleServicesSettingsButton {
  NSString* syncAndGoogleServicesTitle =
      l10n_util::GetNSStringWithFixup(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
  id<GREYMatcher> mainTextLabelMatcher =
      grey_allOf(grey_accessibilityLabel(syncAndGoogleServicesTitle),
                 grey_sufficientlyVisible(), nil);
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_sufficientlyVisible(),
                    grey_descendant(mainTextLabelMatcher), nil);
}

+ (id<GREYMatcher>)settingsMenuBackButton {
  UINavigationBar* navBar = base::mac::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(
          @"SettingNavigationBar",
          [[UIApplication sharedApplication] keyWindow]));
  return grey_allOf(grey_anyOf(grey_accessibilityLabel(navBar.backItem.title),
                               grey_accessibilityLabel(@"Back"), nil),
                    grey_kindOfClass([UIButton class]),
                    grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                    nil);
}

+ (id<GREYMatcher>)settingsMenuPrivacyButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:
          (IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY)];
}

+ (id<GREYMatcher>)settingsMenuPasswordsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_PASSWORDS)];
}

+ (id<GREYMatcher>)paymentRequestView {
  return grey_accessibilityID(kPaymentRequestCollectionViewID);
}

// Returns matcher for the error confirmation view for payment request.
+ (id<GREYMatcher>)paymentRequestErrorView {
  return grey_accessibilityID(kPaymentRequestErrorCollectionViewID);
}

+ (id<GREYMatcher>)voiceSearchButton {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)voiceSearchInputAccessoryButton {
  return grey_accessibilityID(kVoiceSearchInputAccessoryViewID);
}

+ (id<GREYMatcher>)settingsCollectionView {
  return grey_accessibilityID(kSettingsTableViewId);
}

+ (id<GREYMatcher>)clearBrowsingHistoryButton {
  return grey_allOf(
      grey_accessibilityID(kClearBrowsingHistoryCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearCookiesButton {
  return grey_accessibilityID(kClearCookiesCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)clearCacheButton {
  return grey_allOf(
      grey_accessibilityID(kClearCacheCellAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearSavedPasswordsButton {
  return grey_accessibilityID(kClearSavedPasswordsCellAccessibilityIdentifier);
}

+ (id<GREYMatcher>)contentSuggestionCollectionView {
  return grey_accessibilityID(kContentSuggestionsCollectionIdentifier);
}

+ (id<GREYMatcher>)warningMessageView {
  return grey_accessibilityID(kWarningMessageAccessibilityID);
}

+ (id<GREYMatcher>)paymentRequestPickerRow {
  return grey_accessibilityID(kPaymentRequestPickerRowAccessibilityID);
}

+ (id<GREYMatcher>)paymentRequestPickerSearchBar {
  return grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID);
}

+ (id<GREYMatcher>)readingListMenuButton {
  return grey_accessibilityID(kToolsMenuReadingListId);
}

+ (id<GREYMatcher>)bookmarksMenuButton {
  return grey_accessibilityID(kToolsMenuBookmarksId);
}

+ (id<GREYMatcher>)recentTabsMenuButton {
  return grey_accessibilityID(kToolsMenuOtherDevicesId);
}

+ (id<GREYMatcher>)systemSelectionCallout {
  return grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"));
}

+ (id<GREYMatcher>)systemSelectionCalloutCopyButton {
  return grey_allOf(grey_accessibilityLabel(@"Copy"),
                    [self systemSelectionCallout], nil);
}

+ (id<GREYMatcher>)contextMenuCopyButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_COPY)];
}

+ (id<GREYMatcher>)NTPOmnibox {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
}

+ (id<GREYMatcher>)fakeOmnibox {
  return grey_accessibilityID(ntp_home::FakeOmniboxAccessibilityID());
}

+ (id<GREYMatcher>)webViewMatcher {
  return web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webStateScrollViewMatcher {
  return web::WebViewScrollView(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)historyClearBrowsingDataButton {
  return grey_accessibilityID(kHistoryToolbarClearBrowsingButtonIdentifier);
}

+ (id<GREYMatcher>)openInButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:IDS_IOS_OPEN_IN];
}

+ (id<GREYMatcher>)tabGridOpenButton {
  if (IsRegularXRegularSizeClass()) {
    return [self
        buttonWithAccessibilityLabelID:IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER];
  } else {
    return [self buttonWithAccessibilityLabelID:IDS_IOS_TOOLBAR_SHOW_TABS];
  }
}

+ (id<GREYMatcher>)tabGridCellAtIndex:(unsigned int)index {
  return grey_allOf(grey_accessibilityID(IdentifierForCellAtIndex(index)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridDoneButton {
  return grey_allOf(grey_accessibilityID(kTabGridDoneButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridCloseAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridUndoCloseAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridUndoCloseAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSelectShowHistoryCell {
  return grey_allOf(grey_accessibilityID(
                        kRecentTabsShowFullHistoryCellAccessibilityIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridRegularTabsEmptyStateView {
  return grey_allOf(
      grey_accessibilityID(kTabGridRegularTabsEmptyStateIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridNewTabButton {
  return grey_allOf(
      [self buttonWithAccessibilityLabelID:IDS_IOS_TAB_GRID_CREATE_NEW_TAB],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridNewIncognitoTabButton {
  return grey_allOf([self buttonWithAccessibilityLabelID:
                              IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB],
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridOpenTabsPanelButton {
  return grey_accessibilityID(kTabGridRegularTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridIncognitoTabsPanelButton {
  return grey_accessibilityID(kTabGridIncognitoTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridOtherDevicesPanelButton {
  return grey_accessibilityID(kTabGridRemoteTabsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridCloseButtonForCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(IdentifierForCellAtIndex(index))),
      grey_accessibilityID(kGridCellCloseButtonIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsPasswordMatcher {
  return grey_accessibilityID(kPasswordsTableViewId);
}

+ (id<GREYMatcher>)settingsPasswordSearchMatcher {
  return grey_accessibilityID(kPasswordsSearchBarId);
}

+ (id<GREYMatcher>)settingsProfileMatcher {
  return grey_accessibilityID(kAutofillProfileTableViewID);
}

+ (id<GREYMatcher>)settingsCreditCardMatcher {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

+ (id<GREYMatcher>)autofillSuggestionViewMatcher {
  return grey_accessibilityID(kFormSuggestionLabelAccessibilityIdentifier);
}

+ (id<GREYMatcher>)settingsBottomToolbarDeleteButton {
  return grey_accessibilityID(kSettingsToolbarDeleteButtonId);
}

#pragma mark - Manual Fallback

+ (id<GREYMatcher>)manualFallbackFormSuggestionViewMatcher {
  return grey_accessibilityID(kFormSuggestionsViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryPasswordAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackKeyboardIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryKeyboardAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordSearchBarMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordSearchBarAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManagePasswordsMatcher {
  return grey_accessibilityID(
      manual_fill::ManagePasswordsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackOtherPasswordsMatcher {
  return grey_accessibilityID(
      manual_fill::OtherPasswordsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackOtherPasswordsDismissMatcher {
  return grey_accessibilityID(
      manual_fill::kPasswordDoneButtonAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackPasswordButtonMatcher {
  return grey_buttonTitle(kMaskedPasswordTitle);
}

+ (id<GREYMatcher>)manualFallbackPasswordTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackPasswordTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYMatcher>)manualFallbackProfilesIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryAddressAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackProfilesTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::AddressTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManageProfilesMatcher {
  return grey_accessibilityID(
      manual_fill::ManageAddressAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackProfileTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackProfilesTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

+ (id<GREYMatcher>)manualFallbackCreditCardIconMatcher {
  return grey_accessibilityID(
      manual_fill::AccessoryCreditCardAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackCreditCardTableViewMatcher {
  return grey_accessibilityID(
      manual_fill::CardTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackManageCreditCardsMatcher {
  return grey_accessibilityID(manual_fill::ManageCardsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackAddCreditCardsMatcher {
  return grey_accessibilityID(
      manual_fill::kAddCreditCardsAccessibilityIdentifier);
}

+ (id<GREYMatcher>)manualFallbackCreditCardTableViewWindowMatcher {
  id<GREYMatcher> classMatcher = grey_kindOfClass([UIWindow class]);
  id<GREYMatcher> parentMatcher =
      grey_descendant([self manualFallbackCreditCardTableViewMatcher]);
  return grey_allOf(classMatcher, parentMatcher, nil);
}

@end
