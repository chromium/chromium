// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#import <OCHamcrest/OCHamcrest.h>

#import <WebKit/WebKit.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/unified_consent/feature.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_ui_constants.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_view_controller.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_error_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_picker_view_controller.h"
#import "ios/chrome/browser/ui/payments/payment_request_view_controller.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/settings/accounts_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/import_data_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync_settings_collection_view_controller.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/web/public/block_types.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ios/ui_image_test_utils.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

id<GREYMatcher> SettingsSwitchIsToggledOn(BOOL isToggledOn) {
  MatchesBlock matches = ^BOOL(id element) {
    LegacySettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && isToggledOn) || (!switch_view.on && !isToggledOn);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchToggledState(%@)",
                                   isToggledOn ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> SettingsSwitchIsEnabled(BOOL isEnabled) {
  MatchesBlock matches = ^BOOL(id element) {
    LegacySettingsSwitchCell* switch_cell =
        base::mac::ObjCCastStrict<LegacySettingsSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.enabled && isEnabled) ||
           (!switch_view.enabled && !isEnabled);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"settingsSwitchEnabledState(%@)",
                                   isEnabled ? @"YES" : @"NO"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

}  // namespace

namespace chrome_test_util {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> ImageViewWithImageNamed(NSString* imageName) {
  UIImage* expected_image = [UIImage imageNamed:imageName];
  MatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expected_image,
                                                     imageView.image);
  };
  NSString* description_string =
      [NSString stringWithFormat:@"Images matching image named %@", imageName];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:description_string];
  };
  id<GREYMatcher> image_matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return image_matcher;
}

id<GREYMatcher> ImageViewWithImage(int image_id) {
  UIImage* expected_image = NativeImage(image_id);
  MatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expected_image,
                                                     imageView.image);
  };
  NSString* description_string =
      [NSString stringWithFormat:@"Images matching %i", image_id];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:description_string];
  };
  id<GREYMatcher> image_matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return image_matcher;
}

id<GREYMatcher> ButtonWithImage(int image_id) {
  UIImage* expected_image = NativeImage(image_id);
  MatchesBlock matches = ^BOOL(UIButton* button) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expected_image,
                                                     [button currentImage]);
  };
  NSString* description_string =
      [NSString stringWithFormat:@"Images matching %i", image_id];
  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:description_string];
  };
  id<GREYMatcher> image_matcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    image_matcher, nil);
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return StaticTextWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> CancelButton() {
  return ButtonWithAccessibilityLabelId(IDS_CANCEL);
}

id<GREYMatcher> CloseButton() {
  return ButtonWithAccessibilityLabelId(IDS_CLOSE);
}

id<GREYMatcher> ForwardButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_FORWARD);
}

id<GREYMatcher> BackButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_BACK);
}

id<GREYMatcher> ReloadButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
}

id<GREYMatcher> StopButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_STOP);
}

id<GREYMatcher> Omnibox() {
  return grey_kindOfClass([OmniboxTextFieldIOS class]);
}

id<GREYMatcher> DefocusedLocationView() {
  return grey_kindOfClass([LocationBarSteadyView class]);
}

id<GREYMatcher> PageSecurityInfoButton() {
  return grey_accessibilityLabel(@"Page Security Info");
}

id<GREYMatcher> PageSecurityInfoIndicator() {
  return grey_accessibilityLabel(@"Page Security Info");
}

id<GREYMatcher> OmniboxText(std::string text) {
  return grey_allOf(Omnibox(),
                    hasProperty(@"text", base::SysUTF8ToNSString(text)), nil);
}

id<GREYMatcher> OmniboxContainingText(std::string text) {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(UITextField* element) {
        return [element.text containsString:base::SysUTF8ToNSString(text)];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:@"Omnibox contains text \"%@\"",
                                            base::SysUTF8ToNSString(text)]];
      }];
  return matcher;
}

id<GREYMatcher> LocationViewContainingText(std::string text) {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(LocationBarSteadyView* element) {
        return [element.locationLabel.text
            containsString:base::SysUTF8ToNSString(text)];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:
                               @"LocationBarSteadyView contains text \"%@\"",
                               base::SysUTF8ToNSString(text)]];
      }];
  return matcher;
}

id<GREYMatcher> ToolsMenuButton() {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ShareButton() {
  return grey_allOf(ButtonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_SHARE),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabletTabSwitcherOpenButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER);
}

id<GREYMatcher> ShowTabsButton() {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> LegacySettingsSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isToggledOn) {
  return LegacySettingsSwitchCell(accessibilityIdentifier, isToggledOn, YES);
}

id<GREYMatcher> LegacySettingsSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isToggledOn,
                                         BOOL isEnabled) {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    SettingsSwitchIsToggledOn(isToggledOn),
                    SettingsSwitchIsEnabled(isEnabled),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> SyncSwitchCell(NSString* accessibilityLabel, BOOL isToggledOn) {
  return grey_allOf(
      grey_accessibilityLabel(accessibilityLabel),
      grey_accessibilityValue(
          isToggledOn ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                      : l10n_util::GetNSString(IDS_IOS_SETTING_OFF)),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> OpenLinkInNewTabButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB);
}

id<GREYMatcher> NavigationBarDoneButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON);
}

id<GREYMatcher> BookmarksNavigationBarDoneButton() {
  return grey_accessibilityID(kBookmarkHomeNavigationBarDoneButtonIdentifier);
}

id<GREYMatcher> AccountConsistencySetupSigninButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_ACCOUNT_CONSISTENCY_SETUP_SIGNIN_BUTTON);
}

id<GREYMatcher> AccountConsistencyConfirmationOkButton() {
  int labelID = base::FeatureList::IsEnabled(unified_consent::kUnifiedConsent)
                    ? IDS_IOS_ACCOUNT_UNIFIED_CONSENT_OK_BUTTON
                    : IDS_IOS_ACCOUNT_CONSISTENCY_CONFIRMATION_OK_BUTTON;
  return ButtonWithAccessibilityLabelId(labelID);
}

id<GREYMatcher> AddAccountButton() {
  return grey_accessibilityID(kSettingsAccountsAddAccountCellId);
}

id<GREYMatcher> SignOutAccountsButton() {
  return grey_accessibilityID(kSettingsAccountsSignoutCellId);
}

id<GREYMatcher> ClearBrowsingDataCollectionView() {
  return grey_accessibilityID(
      kClearBrowsingDataCollectionViewAccessibilityIdentifier);
}

id<GREYMatcher> ConfirmClearBrowsingDataButton() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)),
      grey_accessibilityTrait(UIAccessibilityTraitButton),
      grey_not(grey_accessibilityID(kClearBrowsingDataButtonIdentifier)), nil);
}

id<GREYMatcher> SettingsMenuButton() {
  return grey_accessibilityID(kToolsMenuSettingsId);
}

id<GREYMatcher> SettingsDoneButton() {
  return grey_accessibilityID(kSettingsDoneButtonId);
}

id<GREYMatcher> ToolsMenuView() {
  return grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
}

id<GREYMatcher> OKButton() {
  return ButtonWithAccessibilityLabelId(IDS_OK);
}

id<GREYMatcher> PrimarySignInButton() {
  return grey_accessibilityID(kSigninPromoPrimaryButtonId);
}

id<GREYMatcher> SecondarySignInButton() {
  return grey_accessibilityID(kSigninPromoSecondaryButtonId);
}

id<GREYMatcher> SettingsAccountButton() {
  return grey_accessibilityID(kSettingsAccountCellId);
}

id<GREYMatcher> SettingsAccountsCollectionView() {
  return grey_accessibilityID(kSettingsAccountsId);
}

id<GREYMatcher> SettingsImportDataImportButton() {
  return grey_accessibilityID(kImportDataImportCellId);
}

id<GREYMatcher> SettingsImportDataKeepSeparateButton() {
  return grey_accessibilityID(kImportDataKeepSeparateCellId);
}

id<GREYMatcher> SettingsSyncManageSyncedDataButton() {
  return grey_accessibilityID(kSettingsSyncId);
}

id<GREYMatcher> AccountsSyncButton() {
  return grey_allOf(grey_accessibilityID(kSettingsAccountsSyncCellId),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ContentSettingsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_SETTINGS_TITLE);
}

id<GREYMatcher> GoogleServicesSettingsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_GOOGLE_SERVICES_SETTINGS_TITLE);
}

id<GREYMatcher> SettingsMenuBackButton() {
  return grey_allOf(grey_accessibilityID(@"ic_arrow_back"),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> SettingsMenuPrivacyButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_OPTIONS_ADVANCED_SECTION_TITLE_PRIVACY);
}

id<GREYMatcher> SettingsMenuPasswordsButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_PASSWORDS);
}

id<GREYMatcher> PaymentRequestView() {
  return grey_accessibilityID(kPaymentRequestCollectionViewID);
}

// Returns matcher for the error confirmation view for payment request.
id<GREYMatcher> PaymentRequestErrorView() {
  return grey_accessibilityID(kPaymentRequestErrorCollectionViewID);
}

id<GREYMatcher> VoiceSearchButton() {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> SettingsCollectionView() {
  return grey_accessibilityID(kSettingsCollectionViewId);
}

id<GREYMatcher> ClearBrowsingHistoryButton() {
  return grey_accessibilityID(kClearBrowsingHistoryCellAccessibilityIdentifier);
}

id<GREYMatcher> ClearCookiesButton() {
  return grey_accessibilityID(kClearCookiesCellAccessibilityIdentifier);
}

id<GREYMatcher> ClearCacheButton() {
  return grey_accessibilityID(kClearCacheCellAccessibilityIdentifier);
}

id<GREYMatcher> ClearSavedPasswordsButton() {
  return grey_accessibilityID(kClearSavedPasswordsCellAccessibilityIdentifier);
}

id<GREYMatcher> ContentSuggestionCollectionView() {
  return grey_accessibilityID(
      [ContentSuggestionsViewController collectionAccessibilityIdentifier]);
}

id<GREYMatcher> WarningMessageView() {
  return grey_accessibilityID(kWarningMessageAccessibilityID);
}

id<GREYMatcher> PaymentRequestPickerRow() {
  return grey_accessibilityID(kPaymentRequestPickerRowAccessibilityID);
}

id<GREYMatcher> PaymentRequestPickerSearchBar() {
  return grey_accessibilityID(kPaymentRequestPickerSearchBarAccessibilityID);
}

id<GREYMatcher> BookmarksMenuButton() {
  return grey_accessibilityID(kToolsMenuBookmarksId);
}

id<GREYMatcher> RecentTabsMenuButton() {
  return grey_accessibilityID(kToolsMenuOtherDevicesId);
}

id<GREYMatcher> SystemSelectionCallout() {
  return grey_kindOfClass(NSClassFromString(@"UICalloutBarButton"));
}

id<GREYMatcher> SystemSelectionCalloutCopyButton() {
  return grey_accessibilityLabel(@"Copy");
}

id<GREYMatcher> ContextMenuCopyButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_COPY);
}

id<GREYMatcher> NewTabPageOmnibox() {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
}

id<GREYMatcher> WebViewMatcher() {
  return web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
}

}  // namespace chrome_test_util
