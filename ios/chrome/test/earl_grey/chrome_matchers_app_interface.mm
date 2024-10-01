// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/form_suggestion_constants.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/bookmark_ui_constants.h"
#import "ios/chrome/browser/download/ui_bundled/download_manager_constants.h"
#import "ios/chrome/browser/history/ui_bundled/history_ui_constants.h"
#import "ios/chrome/browser/incognito_interstitial/ui_bundled/incognito_interstitial_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_steady_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_accessibility_identifier_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/browser/ui/recent_tabs/recent_tabs_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_constants.h"
#import "ios/chrome/browser/ui/settings/notifications/tracking_price/tracking_price_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_ui_swift.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_constants.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/ui/settings/tabs/tabs_settings_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/swift_constants_for_objective_c.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/ui/confirmation_alert/constants.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/window_test_util.h"
#import "ios/testing/earl_grey/earl_grey_app.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/test/ios/ui_image_test_utils.h"

namespace {

// Identifier the for cell at given `index` in the tab grid.
NSString* IdentifierForGridCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u", kGridCellIdentifierPrefix, index];
}

// Identifier for the group cell at given `index` in the tab grid.
NSString* IdentifierForGridGroupCellAtIndex(unsigned int index) {
  return [NSString
      stringWithFormat:@"%@%u", kGroupGridCellIdentifierPrefix, index];
}

// Identifier the for cell at given `index` in the tab strip.
NSString* IdentifierForStripCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u",
                                    TabStripCollectionViewConstants
                                        .tabStripTabCellPrefixIdentifier,
                                    index];
}

// Identifier for the group cell at given `index` in the tab strip.
NSString* IdentifierForStripGroupCellAtIndex(unsigned int index) {
  return [NSString stringWithFormat:@"%@%u",
                                    TabStripCollectionViewConstants
                                        .tabStripTabCellPrefixIdentifier,
                                    index];
}

// Identifier for the tab groups panel cell at given `index` in the tab groups
// panel.
NSString* IdentifierForTabGroupsPanelCellAtIndex(unsigned int index) {
  return [NSString
      stringWithFormat:@"%@%u", kTabGroupsPanelCellIdentifierPrefix, index];
}

id<GREYMatcher> TableViewSwitchIsToggledOn(BOOL is_toggled_on) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    TableViewSwitchCell* switch_cell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.on && is_toggled_on) ||
           (!switch_view.on && !is_toggled_on);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"tableViewSwitchToggledState(%@)",
                                   is_toggled_on ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> TableViewSwitchIsEnabled(BOOL is_enabled) {
  GREYMatchesBlock matches = ^BOOL(id element) {
    TableViewSwitchCell* switch_cell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(element);
    UISwitch* switch_view = switch_cell.switchView;
    return (switch_view.enabled && is_enabled) ||
           (!switch_view.enabled && !is_enabled);
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"tableViewSwitchEnabledState(%@)",
                                   is_enabled ? @"YES" : @"NO"];
    [description appendText:name];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

// Returns the subview of `parentView` corresponding to the
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

UIWindow* WindowWithAccessibilityIdentifier(NSString* accessibility_id) {
  for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
    UIWindowScene* windowScene =
        base::apple::ObjCCastStrict<UIWindowScene>(scene);
    for (UIWindow* window in windowScene.windows) {
      if ([window.accessibilityIdentifier isEqualToString:accessibility_id]) {
        return window;
      }
    }
  }

  return nil;
}

}  // namespace

@implementation ChromeMatchersAppInterface

+ (id<GREYMatcher>)windowWithNumber:(int)windowNumber {
  return grey_allOf(
      grey_accessibilityID([NSString stringWithFormat:@"%d", windowNumber]),
      grey_kindOfClass([UIWindow class]), nil);
}

+ (id<GREYMatcher>)blockerWindowWithNumber:(int)windowNumber {
  return grey_allOf(grey_accessibilityID([NSString
                        stringWithFormat:@"blocker-%d", windowNumber]),
                    grey_kindOfClass([UIWindow class]), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabel:l10n_util::GetNSStringWithFixup(messageID)];
}

+ (id<GREYMatcher>)buttonWithAccessibilityLabelID:(int)messageID
                                  numberForPlural:(int)number {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabel:l10n_util::GetPluralNSStringF(messageID,
                                                                 number)];
}

+ (id<GREYMatcher>)buttonWithForegroundColor:(NSString*)colorName {
  GREYMatchesBlock matches = ^BOOL(id element) {
    if (![element isKindOfClass:UIButton.class]) {
      return NO;
    }
    UIButton* button = base::apple::ObjCCastStrict<UIButton>(element);
    return CGColorEqualToColor(
        [UIColor colorNamed:colorName].CGColor,
        button.configuration.baseForegroundColor.CGColor);
  };

  NSString* descriptionString =
      [NSString stringWithFormat:@"Foreground color %@", colorName];

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };

  id<GREYMatcher> foregroundColorMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    foregroundColorMatcher, nil);
}

+ (id<GREYMatcher>)buttonWithBackgroundColor:(NSString*)colorName {
  GREYMatchesBlock matches = ^BOOL(id element) {
    if (![element isKindOfClass:UIButton.class]) {
      return NO;
    }
    UIButton* button = base::apple::ObjCCastStrict<UIButton>(element);
    return CGColorEqualToColor(
        [UIColor colorNamed:colorName].CGColor,
        button.configuration.background.backgroundColor.CGColor);
  };

  NSString* descriptionString =
      [NSString stringWithFormat:@"Background color %@", colorName];

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };

  id<GREYMatcher> backgroundColorMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return grey_allOf(grey_accessibilityTrait(UIAccessibilityTraitButton),
                    backgroundColorMatcher, nil);
}

+ (id<GREYMatcher>)contextMenuItemWithAccessibilityLabel:(NSString*)label {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)contextMenuItemWithAccessibilityLabelID:(int)messageID {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabel:l10n_util::GetNSStringWithFixup(
                                                messageID)];
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

+ (id<GREYMatcher>)imageViewWithCustomSymbolNamed:(NSString*)symbolName
                                        pointSize:(CGFloat)pointSize {
  UIImage* expectedImage =
      CustomSymbolTemplateWithPointSize(symbolName, pointSize);
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(expectedImage,
                                                     imageView.image);
  };
  NSString* descriptionString = [NSString
      stringWithFormat:@"Images matching custom symbol named %@ of size %f",
                       symbolName, pointSize];
  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:descriptionString];
  };
  id<GREYMatcher> imageMatcher =
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe];
  return imageMatcher;
}

+ (id<GREYMatcher>)imageViewWithImage:(UIImage*)image {
  GREYMatchesBlock matches = ^BOOL(UIImageView* imageView) {
    return ui::test::uiimage_utils::UIImagesAreEqual(image, imageView.image);
  };
  NSString* descriptionString =
      [NSString stringWithFormat:@"Images matching image %@", image];
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
                    grey_accessibilityTrait(UIAccessibilityTraitHeader),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)navigationBarTitleWithAccessibilityLabelID:(int)labelID {
  return grey_allOf(
      grey_accessibilityID(l10n_util::GetNSStringWithFixup(labelID)),
      grey_kindOfClass([UINavigationBar class]), nil);
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

+ (id<GREYMatcher>)closeTabMenuButton {
  return grey_allOf(
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_CLOSETAB)],
      grey_sufficientlyVisible(), nil);
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

+ (id<GREYMatcher>)omniboxAtBottom {
  return grey_allOf(
      [ChromeMatchersAppInterface defocusedLocationView],
      grey_ancestor(grey_kindOfClassName(@"SecondaryToolbarView")),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)omniboxOnTop {
  return grey_allOf([ChromeMatchersAppInterface defocusedLocationView],
                    grey_ancestor(grey_kindOfClassName(@"PrimaryToolbarView")),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)defocusedLocationView {
  return grey_kindOfClass([LocationBarSteadyView class]);
}

+ (id<GREYMatcher>)pageSecurityInfoButton {
  return grey_accessibilityLabel(@"Page Security Info");
}

+ (id<GREYMatcher>)pageSecurityInfoIndicator {
  return grey_allOf(
      grey_accessibilityLabel(@"Page security info"),
      grey_ancestor(grey_kindOfClass([LocationBarSteadyView class])), nil);
}

+ (id<GREYMatcher>)omniboxText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::apple::ObjCCast<OmniboxTextFieldIOS>(element);
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
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::apple::ObjCCast<OmniboxTextFieldIOS>(element);
        return [omnibox.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:@"Omnibox contains text '%@'",
                                                  text]];
      }];
  return matcher;
}

+ (id<GREYMatcher>)omniboxContainingAutocompleteText:(NSString*)text {
  if (!text) {
    text = @"";
  }

  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(id element) {
        OmniboxTextFieldIOS* omnibox =
            base::apple::ObjCCast<OmniboxTextFieldIOS>(element);

        NSArray* textComponents =
            [omnibox.accessibilityValue componentsSeparatedByString:@"||||"];

        return textComponents.count >= 2 &&
               [textComponents[1] isEqualToString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString stringWithFormat:
                                     @"Omnibox contains autocomplete text '%@'",
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

+ (id<GREYMatcher>)openNewTabButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_TOOLS_MENU_NEW_TAB)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)shareButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          contextMenuItemWithAccessibilityLabelID:(IDS_IOS_TOOLS_MENU_SHARE)],
      grey_not([self tabShareButton]), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)contextMenuButtonContainingText:(NSString*)text {
  GREYElementMatcherBlock* matcher = [GREYElementMatcherBlock
      matcherWithMatchesBlock:^BOOL(UILabel* element) {
        return [element.text containsString:text];
      }
      descriptionBlock:^void(id<GREYDescription> description) {
        [description
            appendText:[NSString
                           stringWithFormat:@"Label contains text '%@'", text]];
      }];

  return grey_allOf(grey_kindOfClassName(@"UILabel"), matcher,
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabShareButton {
  return grey_allOf(
      grey_anyOf(grey_accessibilityID(kToolbarShareButtonIdentifier),
                 grey_accessibilityID(kOmniboxShareButtonIdentifier), nil),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)showTabsButton {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)addToReadingListButton {
  return grey_allOf([ChromeMatchersAppInterface
                        contextMenuItemWithAccessibilityLabelID:
                            (IDS_IOS_CONTENT_CONTEXT_ADDTOREADINGLIST)],
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)addToBookmarksButton {
  return grey_allOf(
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_ADDTOBOOKMARKS)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn {
  return [ChromeMatchersAppInterface tableViewSwitchCell:accessibilityIdentifier
                                             isToggledOn:isToggledOn
                                               isEnabled:YES];
}

+ (id<GREYMatcher>)tableViewSwitchCell:(NSString*)accessibilityIdentifier
                           isToggledOn:(BOOL)isToggledOn
                             isEnabled:(BOOL)isEnabled {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    TableViewSwitchIsToggledOn(isToggledOn),
                    TableViewSwitchIsEnabled(isEnabled),
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
  return
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_OPENLINKNEWTAB)];
}

+ (id<GREYMatcher>)openLinkInIncognitoButton {
  int stringId = IDS_IOS_OPEN_IN_INCOGNITO_ACTION_TITLE;
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:(stringId)];
}

+ (id<GREYMatcher>)openLinkInNewWindowButton {
  return
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      IDS_IOS_CONTENT_CONTEXT_OPENINNEWWINDOW];
}

+ (id<GREYMatcher>)navigationBarDoneButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabelID:(IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)],
      grey_userInteractionEnabled(), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)bookmarksNavigationBarDoneButton {
  return grey_accessibilityID(kBookmarksHomeNavigationBarDoneButtonIdentifier);
}

+ (id<GREYMatcher>)bookmarksNavigationBarBackButton {
  UINavigationBar* navBar = base::apple::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(kBookmarkNavigationBarIdentifier,
                                         GetAnyKeyWindow()));
  return grey_allOf(grey_buttonTitle(navBar.backItem.title),
                    grey_ancestor(grey_kindOfClass([UINavigationBar class])),
                    nil);
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
  return grey_allOf(grey_accessibilityLabel(l10n_util::GetNSString(
                        IDS_IOS_DELETE_BROWSING_DATA_BUTTON)),
                    grey_accessibilityTrait(UIAccessibilityTraitButton),
                    grey_userInteractionEnabled(), nil);
}

+ (id<GREYMatcher>)clearBrowsingDataView {
  return grey_allOf(
      grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier),
      grey_accessibilityLabel(
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE)),
      nil);
}

+ (id<GREYMatcher>)settingsDoneButton {
  return grey_accessibilityID(kSettingsDoneButtonId);
}

+ (id<GREYMatcher>)autofillCreditCardEditTableView {
  return grey_accessibilityID(kAutofillCreditCardEditTableViewId);
}

+ (id<GREYMatcher>)addressesAndMoreButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)paymentMethodsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_AUTOFILL_PAYMENT_METHODS)];
}

+ (id<GREYMatcher>)languagesButton {
  return grey_accessibilityID(kSettingsLanguagesCellId);
}

+ (id<GREYMatcher>)addCreditCardView {
  return grey_accessibilityID(kAddCreditCardViewID);
}

+ (id<GREYMatcher>)autofillCreditCardTableView {
  return grey_accessibilityID(kAutofillCreditCardTableViewId);
}

+ (id<GREYMatcher>)addCreditCardButton {
  return grey_accessibilityID(kSettingsAddCreditCardButtonID);
}

+ (id<GREYMatcher>)addCreditCardCancelButton {
  return grey_accessibilityID(kSettingsAddCreditCardCancelButtonID);
}

+ (id<GREYMatcher>)toolsMenuView {
  return grey_accessibilityID(kPopupMenuToolsMenuTableViewId);
}

+ (id<GREYMatcher>)omniboxPopupRow {
  return grey_kindOfClassName(@"UITableViewCell");
}

+ (id<GREYMatcher>)omniboxPopupRowWithString:(NSString*)string {
  id<GREYMatcher> textMatcher = grey_descendant(
      [ChromeMatchersAppInterface staticTextWithAccessibilityLabel:string]);
  id<GREYMatcher> popupRow =
      grey_allOf([ChromeMatchersAppInterface omniboxPopupRow], textMatcher,
                 grey_sufficientlyVisible(), nil);
  return popupRow;
}

+ (id<GREYMatcher>)omniboxPopupList {
  return grey_accessibilityID(kOmniboxPopupTableViewAccessibilityIdentifier);
}

+ (id<GREYMatcher>)OKButton {
  return [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:(IDS_OK)];
}

+ (id<GREYMatcher>)primarySignInButton {
  return grey_allOf(grey_accessibilityID(kSigninPromoPrimaryButtonId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)secondarySignInButton {
  return grey_allOf(grey_accessibilityID(kSigninPromoSecondaryButtonId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)identityChooserScrim {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_TOOLBAR_CLOSE_MENU)];
}

+ (id<GREYMatcher>)fakeFakeAddAccountScreenCancelButton {
  return grey_allOf(grey_buttonTitle(@"Cancel"), grey_sufficientlyVisible(),
                    nil);
}

+ (id<GREYMatcher>)settingsAccountButton {
  return grey_accessibilityID(kSettingsAccountCellId);
}

+ (id<GREYMatcher>)settingsAccountsCollectionView {
  return grey_accessibilityID(kSettingsLegacyAccountsTableViewId);
}

+ (id<GREYMatcher>)settingsSafetyCheckTableView {
  return grey_accessibilityID(
      SafetyCheckTableViewController.accessibilityIdentifier);
}

+ (id<GREYMatcher>)settingsPrivacyTableView {
  return grey_accessibilityID(kPrivacyTableViewId);
}

+ (id<GREYMatcher>)settingsPrivacySafeBrowsingTableView {
  return grey_accessibilityID(kPrivacySafeBrowsingTableViewId);
}

+ (id<GREYMatcher>)settingsNotificationsTableView {
  return grey_accessibilityID(kNotificationsBannerTableViewId);
}

+ (id<GREYMatcher>)settingsInactiveTabsTableView {
  return grey_accessibilityID(kInactiveTabsSettingsTableViewId);
}

+ (id<GREYMatcher>)settingsTabsTableView {
  return grey_accessibilityID(kTabsSettingsTableViewId);
}

+ (id<GREYMatcher>)settingsTrackingPriceTableView {
  return grey_accessibilityID(kTrackingPriceTableViewId);
}

+ (id<GREYMatcher>)contentSettingsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_CONTENT_SETTINGS_TITLE)];
}

+ (id<GREYMatcher>)googleServicesSettingsButton {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_accessibilityID(kSettingsGoogleServicesCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)inactiveTabsSettingsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_OPTIONS_MOVE_INACTIVE_TABS)];
}

+ (id<GREYMatcher>)tabsSettingsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_TABS_MANAGEMENT_SETTINGS)];
}

+ (id<GREYMatcher>)manageSyncSettingsButton {
  return grey_allOf(grey_kindOfClass([UITableViewCell class]),
                    grey_accessibilityID(kSettingsGoogleSyncAndServicesCellId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)googleServicesSettingsView {
  return grey_accessibilityID(kGoogleServicesSettingsViewIdentifier);
}

+ (id<GREYMatcher>)settingsNavigationBar {
  return grey_allOf(
      grey_kindOfClass([UINavigationBar class]),
      grey_accessibilityID(
          password_manager::kSettingsNavigationBarAccessibilityID),
      nil);
}

+ (id<GREYMatcher>)settingsMenuBackButton:(NSString*)buttonTitle {
  return grey_allOf(
      grey_anyOf(grey_accessibilityLabel(buttonTitle),
                 grey_accessibilityLabel(@"Back"), grey_buttonTitle(@"Back"),
                 grey_descendant(grey_buttonTitle(buttonTitle)), nil),
      grey_kindOfClassName(@"_UIButtonBarButton"),
      grey_ancestor(grey_kindOfClass([UINavigationBar class])), nil);
}

+ (id<GREYMatcher>)settingsMenuBackButton {
  UINavigationBar* navBar = base::apple::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(
          password_manager::kSettingsNavigationBarAccessibilityID,
          GetAnyKeyWindow()));
  return
      [ChromeMatchersAppInterface settingsMenuBackButton:navBar.backItem.title];
}

+ (id<GREYMatcher>)settingsMenuBackButtonInWindowWithNumber:(int)windowNumber {
  UINavigationBar* navBar = base::apple::ObjCCastStrict<UINavigationBar>(
      SubviewWithAccessibilityIdentifier(
          password_manager::kSettingsNavigationBarAccessibilityID,
          WindowWithAccessibilityIdentifier(
              [NSString stringWithFormat:@"%d", windowNumber])));
  return
      [ChromeMatchersAppInterface settingsMenuBackButton:navBar.backItem.title];
}

+ (id<GREYMatcher>)settingsMenuPrivacyButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_SETTINGS_PRIVACY_TITLE)];
}

+ (id<GREYMatcher>)settingsMenuNotificationsButton {
  return [ChromeMatchersAppInterface
      buttonWithAccessibilityLabelID:(IDS_IOS_NOTIFICATIONS_TITLE)];
}

+ (id<GREYMatcher>)settingsMenuPasswordsButton {
  return grey_accessibilityID(kSettingsPasswordsCellId);
}

+ (id<GREYMatcher>)settingsMenuSafetyCheckButton {
  return grey_accessibilityID(kSettingsSafetyCheckCellId);
}

// TODO(crbug.com/40106317): Remove this stub.
+ (id<GREYMatcher>)paymentRequestView {
  return nil;
}

// TODO(crbug.com/40106317): Remove this stub.
+ (id<GREYMatcher>)paymentRequestErrorView {
  return nil;
}

+ (id<GREYMatcher>)voiceSearchButton {
  return grey_allOf(grey_accessibilityID(kSettingsVoiceSearchCellId),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

+ (id<GREYMatcher>)voiceSearchInputAccessoryButton {
  return grey_accessibilityID(kVoiceSearchInputAccessoryViewID);
}

+ (id<GREYMatcher>)settingsCollectionView {
  return grey_allOf(grey_accessibilityID(kSettingsTableViewId),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)browsingDataButtonMatcher {
  return grey_accessibilityID(kQuickDeleteBrowsingDataButtonIdentifier);
}

+ (id<GREYMatcher>)browsingDataConfirmButtonMatcher {
  return grey_accessibilityID(kQuickDeleteBrowsingDataConfirmButtonIdentifier);
}

+ (id<GREYMatcher>)clearBrowsingHistoryButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataHistoryIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)historyTableView {
  return grey_accessibilityID(kHistoryTableViewIdentifier);
}

+ (id<GREYMatcher>)clearCookiesButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataSiteDataIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearCacheButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataCacheIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearSavedPasswordsButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataPasswordsIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)clearAutofillButton {
  // Needs to use grey_sufficientlyVisible() to make the difference between a
  // cell used by the tableview and a invisible recycled cell.
  return grey_allOf(
      grey_accessibilityID(kQuickDeleteBrowsingDataAutofillIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)contentSuggestionCollectionView {
  return grey_accessibilityID(kContentSuggestionsCollectionIdentifier);
}

+ (id<GREYMatcher>)ntpCollectionView {
  return grey_accessibilityID(kNTPCollectionViewIdentifier);
}

+ (id<GREYMatcher>)ntpIncognitoView {
  return grey_accessibilityID(kNTPIncognitoViewIdentifier);
}

+ (id<GREYMatcher>)ntpFeedMenuEnableButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:
                                      IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM],
      grey_interactable(), nil);
}

+ (id<GREYMatcher>)ntpFeedMenuDisableButton {
  return grey_allOf(
      [ChromeMatchersAppInterface buttonWithAccessibilityLabelID:
                                      IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM],
      grey_interactable(), nil);
}

// TODO(crbug.com/40106317): Remove this stub.
+ (id<GREYMatcher>)warningMessageView {
  return nil;
}

// TODO(crbug.com/40106317): Remove this stub.
+ (id<GREYMatcher>)paymentRequestPickerRow {
  return nil;
}

// TODO(crbug.com/40106317): Remove this stub.
+ (id<GREYMatcher>)paymentRequestPickerSearchBar {
  return nil;
}

+ (id<GREYMatcher>)openNewWindowMenuButton {
  return grey_accessibilityID(kToolsMenuNewWindowId);
}

+ (id<GREYMatcher>)copyActivityButton {
  id<GREYMatcher> copyStaticText = [ChromeMatchersAppInterface
      staticTextWithAccessibilityLabel:l10n_util::GetNSString(
                                           IDS_IOS_SHARE_MENU_COPY)];
  return grey_allOf(
      copyStaticText,
      grey_ancestor(grey_kindOfClassName(@"UIActivityActionGroupCell")), nil);
}

+ (id<GREYMatcher>)copyLinkButton {
  int stringId = IDS_IOS_COPY_LINK_ACTION_TITLE;
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:stringId];
}

+ (id<GREYMatcher>)editButton {
  int stringId = IDS_IOS_EDIT_ACTION_TITLE;
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:stringId];
}

+ (id<GREYMatcher>)moveButton {
  return [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                         IDS_IOS_BOOKMARK_CONTEXT_MENU_MOVE];
}

+ (id<GREYMatcher>)readingListMarkAsReadButton {
  return
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      IDS_IOS_READING_LIST_MARK_AS_READ_ACTION];
}

+ (id<GREYMatcher>)readingListMarkAsUnreadButton {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:
          IDS_IOS_READING_LIST_MARK_AS_UNREAD_ACTION];
}

+ (id<GREYMatcher>)deleteButton {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:IDS_IOS_DELETE_ACTION_TITLE];
}

+ (id<GREYMatcher>)contextMenuCopyButton {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:(IDS_IOS_CONTENT_CONTEXT_COPY)];
}

+ (id<GREYMatcher>)NTPOmnibox {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT)),
      grey_minimumVisiblePercent(0.2), nil);
}

+ (id<GREYMatcher>)fakeOmnibox {
  return grey_accessibilityID(ntp_home::FakeOmniboxAccessibilityID());
}

+ (id<GREYMatcher>)discoverHeaderLabel {
  return grey_accessibilityID(ntp_home::DiscoverHeaderTitleAccessibilityID());
}

+ (id<GREYMatcher>)ntpLogo {
  return grey_accessibilityID(ntp_home::NTPLogoAccessibilityID());
}

+ (id<GREYMatcher>)webViewMatcher {
  return web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webStateScrollViewMatcher {
  return web::WebViewScrollView(chrome_test_util::GetCurrentWebState());
}

+ (id<GREYMatcher>)webStateScrollViewMatcherInWindowWithNumber:
    (int)windowNumber {
  return web::WebViewScrollView(
      chrome_test_util::GetCurrentWebStateForWindowWithNumber(windowNumber));
}

+ (id<GREYMatcher>)historyClearBrowsingDataButton {
  return grey_accessibilityID(kHistoryToolbarClearBrowsingButtonIdentifier);
}

+ (id<GREYMatcher>)openInButton {
  return grey_allOf(
      grey_accessibilityID(kDownloadManagerOpenInAccessibilityIdentifier),
      grey_interactable(), nil);
}

+ (id<GREYMatcher>)openPDFButton {
  return grey_allOf(
      grey_accessibilityID(kDownloadManagerOpenAccessibilityIdentifier),
      grey_interactable(), nil);
}

+ (id<GREYMatcher>)tabGridCellAtIndex:(unsigned int)index {
  return grey_allOf(grey_accessibilityID(IdentifierForGridCellAtIndex(index)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridGroupCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_accessibilityID(IdentifierForGridGroupCellAtIndex(index)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridGroupCellWithName:(NSString*)groupName
                                      count:(NSInteger)count {
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_TAB_GROUP_CELL_ACCESSIBILITY_TITLE,
          base::SysNSStringToUTF16(groupName), base::NumberToString16(count))),
      grey_kindOfClassName(@"GroupGridCell"), grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabStripCellAtIndex:(unsigned int)index {
  return grey_allOf(grey_accessibilityID(IdentifierForStripCellAtIndex(index)),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabStripGroupCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_accessibilityID(IdentifierForStripGroupCellAtIndex(index)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGroupsPanelCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_accessibilityID(IdentifierForTabGroupsPanelCellAtIndex(index)),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGroupsPanelCellWithName:(NSString*)groupName
                                        count:(NSInteger)count {
  NSString* numberOfTabsString =
      l10n_util::GetPluralNSStringF(IDS_IOS_TAB_GROUP_TABS_NUMBER, count);
  return grey_allOf(
      grey_accessibilityLabel(l10n_util::GetNSStringF(
          IDS_IOS_TAB_GROUPS_PANEL_CELL_ACCESSIBILITY_LABEL_FORMAT,
          base::SysNSStringToUTF16(groupName),
          base::SysNSStringToUTF16(numberOfTabsString),
          base::SysNSStringToUTF16(@"Created just now"))),
      grey_kindOfClassName(@"TabGroupsPanelCell"), grey_sufficientlyVisible(),
      nil);
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

+ (id<GREYMatcher>)tabGroupSnackBar:(NSInteger)tabGroupCount {
  NSString* messageLabel =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          IDS_IOS_TAB_GROUP_SNACKBAR_LABEL, tabGroupCount));
  return grey_allOf(
      grey_accessibilityID(@"MDCSnackbarMessageTitleAutomationIdentifier"),
      grey_text(messageLabel), nil);
}

+ (id<GREYMatcher>)tabGroupSnackBarAction {
  return grey_allOf(grey_kindOfClassName(@"M3CButton"),
                    grey_buttonTitle(l10n_util::GetNSString(
                        IDS_IOS_TAB_GROUP_SNACKBAR_ACTION)),
                    nil);
}

+ (id<GREYMatcher>)tabGridTabGroupsPanelButton {
  return grey_accessibilityID(kTabGridTabGroupsPageButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridThirdPanelButton {
  if (IsTabGroupSyncEnabled()) {
    return [self tabGridTabGroupsPanelButton];
  }
  return [self tabGridOtherDevicesPanelButton];
}

+ (id<GREYMatcher>)tabGridNormalModePageControl {
  return grey_allOf(
      grey_kindOfClassName(@"UIControl"),
      grey_descendant(
          [ChromeMatchersAppInterface tabGridIncognitoTabsPanelButton]),
      grey_descendant([ChromeMatchersAppInterface tabGridOpenTabsPanelButton]),
      grey_descendant([ChromeMatchersAppInterface tabGridThirdPanelButton]),
      grey_ancestor(grey_kindOfClassName(@"UIToolbar")),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridInactiveTabsButton {
  return grey_allOf(
      grey_accessibilityID(kInactiveTabsButtonAccessibilityIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridBackground {
  return grey_accessibilityID(kGridBackgroundIdentifier);
}

+ (id<GREYMatcher>)regularTabGrid {
  return grey_accessibilityID(kRegularTabGridIdentifier);
}

+ (id<GREYMatcher>)incognitoTabGrid {
  return grey_accessibilityID(kIncognitoTabGridIdentifier);
}

+ (id<GREYMatcher>)tabGridCloseButtonForCellAtIndex:(unsigned int)index {
  return grey_allOf(
      grey_ancestor(grey_accessibilityID(IdentifierForGridCellAtIndex(index))),
      grey_accessibilityID(kGridCellCloseButtonIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridCloseButtonForGroupCellAtIndex:(unsigned int)index {
  return grey_allOf(grey_ancestor(grey_accessibilityID(
                        IdentifierForGridGroupCellAtIndex(index))),
                    grey_accessibilityID(kGridCellCloseButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsPasswordMatcher {
  return grey_accessibilityID(kPasswordsSettingsTableViewId);
}

+ (id<GREYMatcher>)settingsPasswordSearchMatcher {
  return grey_accessibilityID(kPasswordsSearchBarID);
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

+ (id<GREYMatcher>)settingsSearchEngineButton {
  return grey_accessibilityID(kSettingsSearchEngineCellId);
}

+ (id<GREYMatcher>)settingsAddressBarButton {
  return grey_accessibilityID(kSettingsAddressBarCellId);
}

+ (id<GREYMatcher>)contentViewSmallerThanScrollView {
  GREYMatchesBlock matches = ^BOOL(UIView* view) {
    UIScrollView* scrollView = base::apple::ObjCCast<UIScrollView>(view);
    return scrollView &&
           scrollView.contentSize.height < scrollView.bounds.size.height;
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:@"Not a scroll view or the scroll view content is bigger "
                   @"than the scroll view bounds"];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

+ (id<GREYMatcher>)historyEntryForURL:(NSString*)URL title:(NSString*)title {
  GREYMatchesBlock matches = ^BOOL(TableViewURLCell* cell) {
    return [cell.titleLabel.text isEqualToString:title] &&
           [cell.URLLabel.text isEqualToString:URL];
  };

  GREYDescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"view containing URL text: "];
    [description appendText:URL];
    [description appendText:@" title text: "];
    [description appendText:title];
  };
  return grey_allOf(
      grey_kindOfClass([TableViewURLCell class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)settingsToolbarAddButton {
  return grey_accessibilityID(kSettingsToolbarAddButtonId);
}

+ (id<GREYMatcher>)cellCanBeSwipedToDismissed {
  GREYMatchesBlock matches = ^BOOL(id element) {
    if (![element isKindOfClass:UITableViewCell.class])
      return NO;

    UITableViewCell* cell =
        base::apple::ObjCCastStrict<UITableViewCell>(element);

    // Try to find the TableView containing the cell.
    UIView* potential_table_view = [cell superview];
    while (![potential_table_view isKindOfClass:UITableView.class] &&
           potential_table_view.superview) {
      potential_table_view = potential_table_view.superview;
    }

    if (![potential_table_view isKindOfClass:UITableView.class])
      return NO;

    UITableView* table_view =
        base::apple::ObjCCastStrict<UITableView>(potential_table_view);

    NSIndexPath* index_path = [table_view indexPathForCell:cell];

    return [table_view.dataSource tableView:table_view
                      canEditRowAtIndexPath:index_path];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:@"cellCanBeSwipedToDismissed"];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

+ (id<GREYMatcher>)passwordsTableViewMatcher {
  return grey_accessibilityID(kPasswordsTableViewID);
}

+ (id<GREYMatcher>)defaultBrowserSettingsTableViewMatcher {
  return grey_accessibilityID(kDefaultBrowserSettingsTableViewId);
}

+ (id<GREYMatcher>)safetyCheckTableViewMatcher {
  return grey_accessibilityID(
      SafetyCheckTableViewController.accessibilityIdentifier);
}

#pragma mark - Overflow Menu Destinations

+ (id<GREYMatcher>)bookmarksDestinationButton {
  return grey_accessibilityID(kToolsMenuBookmarksId);
}

+ (id<GREYMatcher>)downloadsDestinationButton {
  return grey_accessibilityID(kToolsMenuDownloadsId);
}

+ (id<GREYMatcher>)historyDestinationButton {
  return grey_accessibilityID(kToolsMenuHistoryId);
}

+ (id<GREYMatcher>)passwordsDestinationButton {
  return grey_accessibilityID(kToolsMenuPasswordsId);
}

+ (id<GREYMatcher>)priceNotificationsDestinationButton {
  return grey_anyOf(
      grey_accessibilityID(kToolsMenuPriceNotifications),
      grey_accessibilityID([NSString
          stringWithFormat:@"%@-newBadge", kToolsMenuPriceNotifications]),
      nullptr);
}

+ (id<GREYMatcher>)readingListDestinationButton {
  return grey_accessibilityID(kToolsMenuReadingListId);
}

+ (id<GREYMatcher>)recentTabsDestinationButton {
  return grey_accessibilityID(kToolsMenuOtherDevicesId);
}

+ (id<GREYMatcher>)settingsDestinationButton {
  // The settings button could have a dot or something on it, changing its ID to
  // "kToolsMenuSettingsId - Dot" or something else. The matcher needs to be
  // adapted to match both.
  GREYMatchesBlock matches = ^BOOL(id element) {
    return [[element accessibilityIdentifier] hasPrefix:kToolsMenuSettingsId];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description appendText:[NSString stringWithFormat:@"starts with('%@')",
                                                       kToolsMenuSettingsId]];
  };
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

+ (id<GREYMatcher>)siteInfoDestinationButton {
  return grey_accessibilityID(kToolsMenuSiteInformation);
}

+ (id<GREYMatcher>)whatsNewDestinationButton {
  return grey_accessibilityID(kToolsMenuWhatsNewId);
}

#pragma mark - Overflow Menu Actions

+ (id<GREYMatcher>)settingsActionButton {
  return grey_accessibilityID(kToolsMenuSettingsActionId);
}

#pragma mark - Promo style view controller

// Returns matcher for the primary action button.
+ (id<GREYMatcher>)promoStylePrimaryActionButtonMatcher {
  return grey_accessibilityID(kPromoStylePrimaryActionAccessibilityIdentifier);
}

// Returns matcher for the secondary action button.
+ (id<GREYMatcher>)promoStyleSecondaryActionButtonMatcher {
  return grey_accessibilityID(
      kPromoStyleSecondaryActionAccessibilityIdentifier);
}

#pragma mark - Incognito Interstitial

+ (id<GREYMatcher>)incognitoInterstitial {
  return grey_accessibilityID(kIncognitoInterstitialAccessibilityIdentifier);
}

+ (id<GREYMatcher>)incognitoInterstitialLabelForURL:(NSString*)url {
  return grey_allOf(grey_accessibilityID(
                        kIncognitoInterstitialURLLabelAccessibilityIdentifier),
                    grey_accessibilityLabel(url), nullptr);
}

+ (id<GREYMatcher>)incognitoInterstitialOpenInChromeIncognitoButton {
  return grey_allOf(
      [ChromeMatchersAppInterface promoStylePrimaryActionButtonMatcher],
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME_INCOGNITO)),
      nullptr);
}

+ (id<GREYMatcher>)incognitoInterstitialOpenInChromeButton {
  return grey_allOf(
      [ChromeMatchersAppInterface promoStyleSecondaryActionButtonMatcher],
      grey_accessibilityLabel(l10n_util::GetNSString(
          IDS_IOS_INCOGNITO_INTERSTITIAL_OPEN_IN_CHROME)),
      nullptr);
}

+ (id<GREYMatcher>)incognitoInterstitialCancelButton {
  return grey_accessibilityID(
      kIncognitoInterstitialCancelButtonAccessibilityIdentifier);
}

#pragma mark - Manual Fallback

+ (id<GREYMatcher>)activityViewHeaderWithURLHost:(NSString*)host
                                           title:(NSString*)pageTitle {
  // The title of the activity view starts as the URL, then asynchronously
  // changes to the page title. Sometimes, the activity view fails to update
  // the text to the page title, causing test flake. Allow matcher to pass
  // with either value for the activity view title.

  GREYMatchesBlock matches = ^BOOL(id element) {
    NSString* label = [element accessibilityLabel];
    NSLog(@"label is %@ %d %d", label, [label containsString:host],
          [label containsString:pageTitle]);
    return [label containsString:host] || [label containsString:pageTitle];
  };
  GREYDescribeToBlock describe = ^void(id<GREYDescription> description) {
    [description
        appendText:[NSString stringWithFormat:
                                 @"accessibilityLabel containing %@ or %@",
                                 host, pageTitle]];
  };

  return grey_allOf(
      grey_kindOfClassName(@"LPLinkView"),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

+ (id<GREYMatcher>)useSuggestedPasswordMatcher {
  return grey_allOf(
      [self
          buttonWithAccessibilityLabelID:IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD],
      grey_interactable(), nullptr);
}

#pragma mark - Tab Grid Selection Mode

+ (id<GREYMatcher>)tabGridEditButton {
  return grey_accessibilityID(kTabGridEditButtonIdentifier);
}

+ (id<GREYMatcher>)tabGridEditMenuCloseAllButton {
  int ID = IsTabGroupSyncEnabled()
               ? IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABSANDGROUPS
               : IDS_IOS_CONTENT_CONTEXT_CLOSEALLTABS;
  return grey_allOf(
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:ID],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSelectTabsMenuButton {
  return grey_allOf(
      [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                      (IDS_IOS_CONTENT_CONTEXT_SELECTTABS)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditAddToButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditAddToButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditCloseTabsButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditCloseTabsButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditSelectAllButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditSelectAllButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridEditShareButton {
  return grey_allOf(grey_accessibilityID(kTabGridEditShareButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

#pragma mark - Tab Grid Search Mode

+ (id<GREYMatcher>)tabGridSearchTabsButton {
  return grey_allOf(grey_accessibilityID(kTabGridSearchButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSearchBar {
  return grey_allOf(grey_accessibilityID(kTabGridSearchBarIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSearchCancelButton {
  return grey_allOf(grey_accessibilityID(kTabGridCancelButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGridSearchModeToolbar {
  return grey_allOf(
      grey_kindOfClassName(@"UIToolbar"),
      grey_descendant([ChromeMatchersAppInterface tabGridSearchBar]),
      grey_descendant([ChromeMatchersAppInterface tabGridSearchCancelButton]),
      grey_sufficientlyVisible(), nil);
}

#pragma mark - Create Tab Group View

+ (id<GREYMatcher>)tabGroupCreationView {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)createTabGroupTextField {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupTextFieldIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)createTabGroupTextFieldClearButton {
  return grey_allOf(
      grey_accessibilityID(kCreateTabGroupTextFieldClearButtonIdentifier),
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)createTabGroupCreateButton {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCreateButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)createTabGroupCancelButton {
  return grey_allOf(grey_accessibilityID(kCreateTabGroupCancelButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

#pragma mark - Tab Group View

+ (id<GREYMatcher>)tabGroupView {
  return grey_allOf(grey_accessibilityID(kTabGroupViewIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGroupViewTitle:(NSString*)title {
  return grey_allOf(grey_accessibilityID(kTabGroupViewTitleIdentifier),
                    grey_accessibilityLabel(title), nil);
}

+ (id<GREYMatcher>)tabGroupOverflowMenuButton {
  return grey_allOf(grey_accessibilityID(kTabGroupOverflowMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)tabGroupBackButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          buttonWithAccessibilityLabel:l10n_util::GetNSString(
                                           IDS_IOS_ICON_ARROW_BACK)],
      grey_kindOfClassName(@"UIAccessibilityBackButtonElement"), nil);
}

#pragma mark - Tab Groups Context Menus

+ (id<GREYMatcher>)addTabToNewGroupButton {
  return grey_allOf(
      [ChromeMatchersAppInterface
          contextMenuItemWithAccessibilityLabel:
              l10n_util::GetPluralNSStringF(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1)],
      grey_sufficientlyVisible(), nil);
}

+ (id<GREYMatcher>)addTabToGroupSubMenuButton {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:
          IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU];
}

+ (id<GREYMatcher>)renameGroupButton {
  return [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                         IDS_IOS_CONTENT_CONTEXT_RENAMEGROUP];
}

+ (id<GREYMatcher>)ungroupButton {
  return [ChromeMatchersAppInterface
      contextMenuItemWithAccessibilityLabelID:IDS_IOS_CONTENT_CONTEXT_UNGROUP];
}

+ (id<GREYMatcher>)ungroupConfirmationButton {
  return grey_allOf(grey_accessibilityID(
                        [l10n_util::GetNSString(IDS_IOS_CONTENT_CONTEXT_UNGROUP)
                            stringByAppendingString:@"AlertAction"]),
                    grey_interactable(), nil);
}

+ (id<GREYMatcher>)deleteGroupButton {
  return [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                         IDS_IOS_CONTENT_CONTEXT_DELETEGROUP];
}

+ (id<GREYMatcher>)deleteGroupConfirmationButton {
  return grey_allOf(grey_accessibilityID([l10n_util::GetNSString(
                        IDS_IOS_CONTENT_CONTEXT_DELETEGROUP)
                        stringByAppendingString:@"AlertAction"]),
                    grey_interactable(), nil);
}

+ (id<GREYMatcher>)closeGroupButton {
  return [ChromeMatchersAppInterface contextMenuItemWithAccessibilityLabelID:
                                         IDS_IOS_CONTENT_CONTEXT_CLOSEGROUP];
}

#pragma mark - Tab Groups Panel

+ (id<GREYMatcher>)tabGroupsPanel {
  return grey_allOf(grey_accessibilityID(kTabGroupsPanelIdentifier),
                    grey_sufficientlyVisible(), nil);
}

@end
