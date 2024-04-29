// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_tabs_search_suggested_history_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/settings/address_bar_preference/cells/address_bar_options_item.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/inline_promo_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_table_view_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "url/gurl.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierText = kSectionIdentifierEnumZero,
  SectionIdentifierSettings,
  SectionIdentifierAutofill,
  SectionIdentifierAccount,
  SectionIdentifierURL,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeText = kItemTypeEnumZero,
  ItemTypeTextHeader,
  ItemTypeTextFooter,
  ItemTypeTextButton,
  ItemTypeURLNoMetadata,
  ItemTypeTextAccessoryImage,
  ItemTypeSearchHistorySuggestedItem,
  ItemTypeTextAccessoryNoImage,
  ItemTypeTextEditItem,
  ItemTypeTextMultiLineEditItem,
  ItemTypeURLWithActivityIndicator,
  ItemTypeURLWithActivityIndicatorStopped,
  ItemTypeURLWithTimestamp,
  ItemTypeURLWithSize,
  ItemTypeURLWithSupplementalText,
  ItemTypeURLWithThirdRowText,
  ItemTypeURLWithMetadata,
  ItemTypeURLWithMetadataImage,
  ItemTypeURLWithBadgeImage,
  ItemTypeTextSettingsDetail,
  ItemTypeTableViewWithBlueDot,
  ItemTypeLinkFooter,
  ItemAddressBarOptions,
  ItemTypeDetailText,
  ItemTypeMultiDetailText,
  ItemTypeAccountSignInItem,
  ItemTypeTableViewInfoButton,
  ItemTypeTableViewInfoButtonWithDetailText,
  ItemTypeTableViewInfoButtonWithImage,
  ItemTypeTableViewSwitch1,
  ItemTypeTableViewSwitch2,
  ItemTypeSyncSwitch,
  ItemTypeSettingsSyncError,
  ItemTypeAutofillData,
  ItemTypeAccount,
  ItemTypeCheck1,
  ItemTypeCheck2,
  ItemTypeCheck3,
  ItemTypeCheck4,
  ItemTypeCheck5,
  ItemTypeCheck6,
  ItemTypeInlinePromo,
};
}

@implementation TableCellCatalogViewController

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Table Cell Catalog";
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 56.0;
  self.tableView.estimatedSectionHeaderHeight = 56.0;
  self.tableView.estimatedSectionFooterHeight = 56.0;

  [self loadModel];
}

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierText];
  [model addSectionWithIdentifier:SectionIdentifierSettings];
  [model addSectionWithIdentifier:SectionIdentifierAutofill];
  [model addSectionWithIdentifier:SectionIdentifierAccount];
  [model addSectionWithIdentifier:SectionIdentifierURL];

  // SectionIdentifierText.
  TableViewTextHeaderFooterItem* textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  textHeaderFooterItem.text = @"Simple Text Header";
  [model setHeader:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* symbolItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  symbolItem.text = @"Detail Icon using custom background";
  symbolItem.iconImage = DefaultSymbolWithPointSize(kSettingsFilledSymbol, 18);
  symbolItem.iconBackgroundColor = UIColorFromRGB(0xFBBC04);
  symbolItem.iconTintColor = UIColor.whiteColor;
  symbolItem.iconCornerRadius = 7;
  [model addItem:symbolItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* tableViewBlueDotItem =
      [[TableViewDetailIconItem alloc]
          initWithType:ItemTypeTableViewWithBlueDot];
  tableViewBlueDotItem.badgeType = BadgeType::kNotificationDot;
  tableViewBlueDotItem.text = @"I have a blue dot badge!";
  tableViewBlueDotItem.iconImage =
      DefaultSettingsRootSymbol(kDefaultBrowserSymbol);
  [model addItem:tableViewBlueDotItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  textItem.textAlignment = NSTextAlignmentCenter;
  textItem.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  textItem = [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"1234";
  textItem.masked = YES;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewImageItem* textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textImageItem.title = @"Image Item with History Image";
  textImageItem.image =
      DefaultSymbolWithPointSize(kHistorySymbol, kSymbolActionPointSize);
  [model addItem:textImageItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewTabsSearchSuggestedHistoryItem* searchHistorySuggestedItem =
      [[TableViewTabsSearchSuggestedHistoryItem alloc]
          initWithType:ItemTypeSearchHistorySuggestedItem];
  [model addItem:searchHistorySuggestedItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewImageItem* textImageItem2 =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textImageItem2.title = @"Image item without image, and disabled";
  textImageItem2.textColor = [UIColor colorNamed:kRedColor];
  textImageItem2.detailText =
      @"Very very very long detail text for the image cell without image";
  textImageItem2.detailTextColor = [UIColor colorNamed:kRedColor];
  textImageItem2.enabled = NO;
  [model addItem:textImageItem2 toSectionWithIdentifier:SectionIdentifierText];

  textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryNoImage];
  textImageItem.title = @"Image Item with No Image";
  [model addItem:textImageItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItemDefault =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItemDefault.text = @"Simple Text Cell with Defaults";
  [model addItem:textItemDefault toSectionWithIdentifier:SectionIdentifierText];

  textHeaderFooterItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextFooter];
  textHeaderFooterItem.text = @"Simple Text Footer";
  [model setFooter:textHeaderFooterItem
      forSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonItem.text = @"Hello, you should do something. Also as you "
                              @"can see this text is centered.";
  textActionButtonItem.buttonText = @"Do something";
  [model addItem:textActionButtonItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonAlignmentItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonAlignmentItem.text =
      @"Hello, you should do something. Also as you can see this text is using "
      @"NSTextAlignmentNatural";
  textActionButtonAlignmentItem.textAlignment = NSTextAlignmentNatural;
  textActionButtonAlignmentItem.buttonText = @"Do something";
  [model addItem:textActionButtonAlignmentItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtoExpandedItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtoExpandedItem.text = @"Hello, you should do something.";
  textActionButtoExpandedItem.disableButtonIntrinsicWidth = YES;
  textActionButtoExpandedItem.buttonText = @"Expanded Button";
  [model addItem:textActionButtoExpandedItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonNoTextItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonNoTextItem.buttonText = @"Do something, no Top Text";
  [model addItem:textActionButtonNoTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonColorItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtonColorItem.text = @"Hello, you should do something.";
  textActionButtonColorItem.disableButtonIntrinsicWidth = YES;
  textActionButtonColorItem.buttonBackgroundColor = [UIColor lightGrayColor];
  textActionButtonColorItem.buttonTextColor =
      [UIColor colorNamed:kSolidBlackColor];
  textActionButtonColorItem.buttonText = @"Do something, different Colors";
  [model addItem:textActionButtonColorItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* detailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  detailTextItem.text = @"Item with two labels";
  detailTextItem.detailText =
      @"The second label is optional and is mostly displayed on one line";
  [model addItem:detailTextItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* noDetailTextItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  noDetailTextItem.text = @"Detail item on one line.";
  [model addItem:noDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* chevronAccessorySymbolItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  chevronAccessorySymbolItem.text = @"Text on first line.";
  chevronAccessorySymbolItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolChevron;
  [model addItem:chevronAccessorySymbolItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailTextItem* externalLinkAccessorySymbolItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeDetailText];
  externalLinkAccessorySymbolItem.text = @"Text on first line.";
  externalLinkAccessorySymbolItem.detailText = @"Detail item on second line";
  externalLinkAccessorySymbolItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  [model addItem:externalLinkAccessorySymbolItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* detailIconItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItem.text = @"Detail Icon Item Cell";
  detailIconItem.iconImage = DefaultSettingsRootSymbol(kDiscoverSymbol);
  detailIconItem.detailText = @"Short";
  [model addItem:detailIconItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* detailIconItemVerticalTextLayout =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItemVerticalTextLayout.iconImage =
      DefaultSettingsRootSymbol(kDiscoverSymbol);
  detailIconItemVerticalTextLayout.text = @"Detail Icon Item Cell";
  detailIconItemVerticalTextLayout.detailText = @"Short subtitle";
  detailIconItemVerticalTextLayout.textLayoutConstraintAxis =
      UILayoutConstraintAxisVertical;
  [model addItem:detailIconItemVerticalTextLayout
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* detailIconItemLeftLong =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItemLeftLong.text =
      @"Left label is very very very very very very very long";
  detailIconItemLeftLong.detailText = @"R";
  [model addItem:detailIconItemLeftLong
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* detailIconItemRightLong =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItemRightLong.text = @"L";
  detailIconItemRightLong.detailText =
      @"Right label is very very very very very very very long";
  [model addItem:detailIconItemRightLong
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewDetailIconItem* detailIconItemBothLong =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItemBothLong.text = @"Left label occupy 75% of row space";
  detailIconItemBothLong.detailText = @"Right label occupy 25% of row space";
  [model addItem:detailIconItemBothLong
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItem =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItem.fieldNameLabelText = @"Edit Text Item";
  textEditItem.textFieldValue = @" with no icons";
  textEditItem.hideIcon = YES;
  textEditItem.textFieldEnabled = YES;
  [model addItem:textEditItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemEditIcon =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemEditIcon.fieldNameLabelText = @"Edit Text Item";
  textEditItemEditIcon.textFieldValue = @" with edit icon";
  textEditItemEditIcon.textFieldEnabled = YES;
  [model addItem:textEditItemEditIcon
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemBothIcons =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemBothIcons.fieldNameLabelText = @"Edit Text Item";
  textEditItemBothIcons.textFieldValue = @" with edit and custom icons";
  textEditItemBothIcons.identifyingIcon =
      DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 13);
  textEditItemBothIcons.textFieldEnabled = YES;
  [model addItem:textEditItemBothIcons
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemIconButton =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemIconButton.fieldNameLabelText = @"Edit Text Item";
  textEditItemIconButton.textFieldValue = @" icon is a button.";
  textEditItemIconButton.identifyingIcon =
      DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 13);
  textEditItemIconButton.identifyingIconEnabled = YES;
  textEditItemIconButton.textFieldEnabled = NO;
  [model addItem:textEditItemIconButton
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewMultiDetailTextItem* tableViewMultiDetailTextItem =
      [[TableViewMultiDetailTextItem alloc]
          initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text = @"Main Text";
  tableViewMultiDetailTextItem.trailingDetailText = @"Trailing Detail Text";
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  tableViewMultiDetailTextItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text = @"Main Text";
  tableViewMultiDetailTextItem.leadingDetailText = @"Leading Detail Text";
  tableViewMultiDetailTextItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  tableViewMultiDetailTextItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text = @"Main Text";
  tableViewMultiDetailTextItem.leadingDetailText = @"Leading Detail Text";
  tableViewMultiDetailTextItem.trailingDetailText = @"Trailing Detail Text";
  tableViewMultiDetailTextItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewMultiLineTextEditItem* tableViewMultiLineTextEditItem =
      [[TableViewMultiLineTextEditItem alloc]
          initWithType:ItemTypeTextMultiLineEditItem];
  tableViewMultiLineTextEditItem.label = @"Multi Line Edit Text Item";
  tableViewMultiLineTextEditItem.text =
      @"This is possibly a very very very very very very long multi line text";
  [model addItem:tableViewMultiLineTextEditItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewSwitchItem* tableViewSwitchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeTableViewSwitch1];
  tableViewSwitchItem.text = @"This is a switch item";
  [model addItem:tableViewSwitchItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewSwitchItem* tableViewSwitchItem2 =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeTableViewSwitch2];
  tableViewSwitchItem2.text = @"This is a disabled switch item";
  tableViewSwitchItem2.detailText = @"This is a switch item with detail text";
  tableViewSwitchItem2.on = YES;
  tableViewSwitchItem2.enabled = NO;
  [model addItem:tableViewSwitchItem2
      toSectionWithIdentifier:SectionIdentifierText];

  // SectionIdentifierSettings.
  TableViewTextHeaderFooterItem* settingsHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  settingsHeader.text = @"Settings";
  [model setHeader:settingsHeader
      forSectionWithIdentifier:SectionIdentifierSettings];

  AccountSignInItem* accountSignInItem =
      [[AccountSignInItem alloc] initWithType:ItemTypeAccountSignInItem];
  accountSignInItem.detailText = @"Get cool stuff on all your devices";
  [model addItem:accountSignInItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SyncSwitchItem* syncSwitchItem =
      [[SyncSwitchItem alloc] initWithType:ItemTypeSyncSwitch];
  syncSwitchItem.text = @"This is a sync switch item";
  syncSwitchItem.detailText = @"This is a sync switch item with detail text";
  syncSwitchItem.on = YES;
  syncSwitchItem.enabled = NO;
  [model addItem:syncSwitchItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewInfoButtonItem* tableViewInfoButtonItem =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeTableViewInfoButton];
  tableViewInfoButtonItem.text = @"Info button item";
  tableViewInfoButtonItem.statusText = @"Status";
  [model addItem:tableViewInfoButtonItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewInfoButtonItem* tableViewInfoButtonItemWithDetailText =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeTableViewInfoButtonWithDetailText];
  tableViewInfoButtonItemWithDetailText.text = @"Info button item";
  tableViewInfoButtonItemWithDetailText.detailText = @"Detail text";
  tableViewInfoButtonItemWithDetailText.statusText = @"Status";
  [model addItem:tableViewInfoButtonItemWithDetailText
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewInfoButtonItem* tableViewInfoButtonItemWithLeadingImage =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeTableViewInfoButtonWithImage];
  tableViewInfoButtonItemWithLeadingImage.text = @"Info button item";
  tableViewInfoButtonItemWithLeadingImage.statusText = @"Status";
  tableViewInfoButtonItemWithLeadingImage.iconImage =
      DefaultSettingsRootSymbol(kDiscoverSymbol);
  [model addItem:tableViewInfoButtonItemWithLeadingImage
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsImageDetailTextItem* imageDetailTextItem =
      [[SettingsImageDetailTextItem alloc]
          initWithType:ItemTypeSettingsSyncError];
  imageDetailTextItem.text = @"This is an error description about sync";
  imageDetailTextItem.detailText =
      @"This is more detail about the sync error description";
  imageDetailTextItem.image = [[ChromeIcon infoIcon]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [model addItem:imageDetailTextItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkInProcess =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck1];
  checkInProcess.text = @"This is running check item";
  checkInProcess.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkInProcess.enabled = YES;
  checkInProcess.indicatorHidden = NO;
  [model addItem:checkInProcess
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkFinished =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck2];
  checkFinished.text = @"This is finished check item";
  checkFinished.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkFinished.enabled = YES;
  checkFinished.indicatorHidden = YES;
  checkFinished.trailingImage =
      DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 13);
  [model addItem:checkFinished
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkFinishedWithLeadingImage =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck3];
  checkFinishedWithLeadingImage.text = @"Check item leading image";
  checkFinishedWithLeadingImage.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkFinishedWithLeadingImage.leadingIcon = [[ChromeIcon infoIcon]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  checkFinishedWithLeadingImage.enabled = YES;
  checkFinishedWithLeadingImage.indicatorHidden = YES;
  checkFinishedWithLeadingImage.trailingImage =
      DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 13);
  [model addItem:checkFinishedWithLeadingImage
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkDisabled =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck4];
  checkDisabled.text = @"This is disabled check item";
  checkDisabled.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkDisabled.enabled = NO;
  [model addItem:checkDisabled
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkDisabledWithLeadingImage =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck5];
  checkDisabledWithLeadingImage.text = @"Disabled check item leading image";
  checkDisabledWithLeadingImage.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkDisabledWithLeadingImage.leadingIcon = [[ChromeIcon infoIcon]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  checkDisabledWithLeadingImage.enabled = NO;
  [model addItem:checkDisabledWithLeadingImage
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsCheckItem* checkWithInfoButton =
      [[SettingsCheckItem alloc] initWithType:ItemTypeCheck6];
  checkWithInfoButton.text = @"Check item with info ";
  checkWithInfoButton.detailText =
      @"This is very long description of check item. Another line of "
      @"description.";
  checkWithInfoButton.enabled = YES;
  checkWithInfoButton.indicatorHidden = YES;
  checkWithInfoButton.infoButtonHidden = NO;
  [model addItem:checkWithInfoButton
      toSectionWithIdentifier:SectionIdentifierSettings];

  AddressBarOptionsItem* addressBarOptions =
      [[AddressBarOptionsItem alloc] initWithType:ItemAddressBarOptions];
  addressBarOptions.bottomAddressBarOptionSelected = YES;
  [model addItem:addressBarOptions
      toSectionWithIdentifier:SectionIdentifierSettings];

  InlinePromoItem* inlinePromoItem =
      [[InlinePromoItem alloc] initWithType:ItemTypeInlinePromo];
  inlinePromoItem.promoImage = [UIImage imageNamed:WidgetPromoImageName()];
  inlinePromoItem.promoText =
      @"Text to promote some cool stuff in Settings. Can be on multiple lines.";
  inlinePromoItem.moreInfoButtonTitle = @"Show Me How";
  [model addItem:inlinePromoItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewLinkHeaderFooterItem* linkFooter =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkFooter];
  linkFooter.text =
      @"This is a footer text view with a BEGIN_LINKlinkEND_LINK in the middle";
  linkFooter.urls =
      @[ [[CrURL alloc] initWithGURL:GURL("http://www.example.com")] ];
  [model setFooter:linkFooter
      forSectionWithIdentifier:SectionIdentifierSettings];

  // SectionIdentifierAutofill.
  TableViewTextHeaderFooterItem* autofillHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  autofillHeader.text = @"Autofill";
  [model setHeader:autofillHeader
      forSectionWithIdentifier:SectionIdentifierAutofill];

  // SectionIdentifierAccount.
  UIImage* signinPromoAvatar = ios::provider::GetSigninDefaultAvatar();
  CGSize avatarSize =
      GetSizeForIdentityAvatarSize(IdentityAvatarSize::SmallSize);
  signinPromoAvatar =
      ResizeImage(signinPromoAvatar, avatarSize, ProjectionMode::kFill);
  // Sign-in promo with an email and a name.
  TableViewSigninPromoItem* signinPromo =
      [[TableViewSigninPromoItem alloc] initWithType:ItemTypeAccount];
  signinPromo.configurator = [[SigninPromoViewConfigurator alloc]
      initWithSigninPromoViewMode:SigninPromoViewModeSigninWithAccount
                        userEmail:@"jonhdoe@example.com"
                    userGivenName:@"John Doe"
                        userImage:signinPromoAvatar
                   hasCloseButton:NO
                 hasSignInSpinner:NO];
  signinPromo.text = @"Signin promo text example";
  [model addItem:signinPromo toSectionWithIdentifier:SectionIdentifierAccount];
  // Sign-in promo without an email and name.
  signinPromo = [[TableViewSigninPromoItem alloc] initWithType:ItemTypeAccount];
  signinPromo.configurator = [[SigninPromoViewConfigurator alloc]
      initWithSigninPromoViewMode:SigninPromoViewModeNoAccounts
                        userEmail:nil
                    userGivenName:nil
                        userImage:nil
                   hasCloseButton:YES
                 hasSignInSpinner:NO];
  signinPromo.text = @"Signin promo text example";
  [model addItem:signinPromo toSectionWithIdentifier:SectionIdentifierAccount];
  // Sign-in promo with spinner.
  signinPromo = [[TableViewSigninPromoItem alloc] initWithType:ItemTypeAccount];
  signinPromo.configurator = [[SigninPromoViewConfigurator alloc]
      initWithSigninPromoViewMode:SigninPromoViewModeSigninWithAccount
                        userEmail:@"jonhdoe@example.com"
                    userGivenName:@"John Doe"
                        userImage:signinPromoAvatar
                   hasCloseButton:NO
                 hasSignInSpinner:YES];
  signinPromo.text = @"Signin promo with spinner";
  [model addItem:signinPromo toSectionWithIdentifier:SectionIdentifierAccount];

  UIImage* defaultAvatar = ios::provider::GetSigninDefaultAvatar();

  TableViewAccountItem* accountItemDetailWithError =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  accountItemDetailWithError.image = defaultAvatar;
  accountItemDetailWithError.text = @"Account User Name";
  accountItemDetailWithError.detailText =
      @"Syncing to AccountUserNameAccount@example.com";
  accountItemDetailWithError.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  accountItemDetailWithError.shouldDisplayError = YES;
  [model addItem:accountItemDetailWithError
      toSectionWithIdentifier:SectionIdentifierAccount];

  TableViewAccountItem* accountItemCheckMark =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  accountItemCheckMark.image = defaultAvatar;
  accountItemCheckMark.text = @"Lorem ipsum dolor sit amet, consectetur "
                              @"adipiscing elit, sed do eiusmod tempor "
                              @"incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.detailText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.accessoryType = UITableViewCellAccessoryCheckmark;
  [model addItem:accountItemCheckMark
      toSectionWithIdentifier:SectionIdentifierAccount];

  // SectionIdentifierURL.
  TableViewURLItem* item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.title = @"Google Design";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://design.google.com")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://notitle.google.com")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithTimestamp];
  item.title = @"Google";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://www.google.com")];
  item.metadata = @"3:42 PM";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://m.bbc.com")];
  item.metadata = @"176 KB";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSupplementalText];
  item.title = @"Chrome | Google Blog";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.supplementalURLText = @"Read 4 days ago";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithBadgeImage];
  item.title = @"Photos - Google Photos";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://photos.google.com/")];
  item.badgeImage =
      DefaultSymbolTemplateWithPointSize(kCheckmarkCircleFillSymbol, 13);
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLWithActivityIndicator];
  item.title = @"Sent Request to Server";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://started.spinner.com/")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc]
      initWithType:ItemTypeURLWithActivityIndicatorStopped];
  item.title = @"Received Response from Server";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://stopped.spinner.com/")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithThirdRowText];
  item.title = @"Web Channel with 3rd Row Text";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.thirdRowText = @"Unavailable";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithThirdRowText];
  item.title = @"Web Channel with 3rd Row Red Text";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.thirdRowText = @"Unavailable";
  item.thirdRowTextColor = UIColor.redColor;
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithMetadata];
  item.title = @"Web Channel with metadata image and label";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.metadata = @"176 KB";
  item.metadataImage = CustomSymbolTemplateWithPointSize(kCloudSlashSymbol, 18);
  item.metadataImageColor = [UIColor colorNamed:kTextSecondaryColor];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithMetadataImage];
  item.title = @"Web Channel with metadata image";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.metadataImage = CustomSymbolTemplateWithPointSize(kCloudSlashSymbol, 18);
  item.metadataImageColor = [UIColor colorNamed:kTextSecondaryColor];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

// Called when the user clicks on the information button of the check item
// setting's UI. Shows a textual bubble with the detailed information.
- (void)didTapCheckInfoButton:(UIButton*)buttonView {
  PopoverLabelViewController* popoverViewController =
      [[PopoverLabelViewController alloc]
          initWithMessage:@"You clicked settings check item. Here you can see "
                          @"detailed information."];

  // Set the anchor and arrow direction of the bubble.
  popoverViewController.popoverPresentationController.sourceView = buttonView;
  popoverViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  popoverViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:popoverViewController
                     animated:YES
                   completion:nil];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  if (itemType == ItemTypeTableViewInfoButton ||
      itemType == ItemTypeTableViewInfoButtonWithDetailText ||
      itemType == ItemTypeTableViewInfoButtonWithImage) {
    TableViewInfoButtonCell* managedCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [managedCell.trailingButton addTarget:self
                                   action:@selector(didTapManagedUIInfoButton:)
                         forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeCheck6) {
    SettingsCheckCell* checkCell =
        base::apple::ObjCCastStrict<SettingsCheckCell>(cell);
    [checkCell.infoButton addTarget:self
                             action:@selector(didTapCheckInfoButton:)
                   forControlEvents:UIControlEventTouchUpInside];
  } else if (itemType == ItemTypeSearchHistorySuggestedItem) {
    TableViewTabsSearchSuggestedHistoryCell* searchHistoryCell =
        base::apple::ObjCCastStrict<TableViewTabsSearchSuggestedHistoryCell>(
            cell);
    [searchHistoryCell updateHistoryResultsCount:7];
  } else if (itemType == ItemTypeURLWithActivityIndicator) {
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    [URLCell startAnimatingActivityIndicator];
  } else if (itemType == ItemTypeURLWithActivityIndicatorStopped) {
    TableViewURLCell* URLCell =
        base::apple::ObjCCastStrict<TableViewURLCell>(cell);
    [URLCell startAnimatingActivityIndicator];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 4 * NSEC_PER_SEC),
                   dispatch_get_main_queue(), ^{
                     [URLCell stopAnimatingActivityIndicator];
                   });
  }
  return cell;
}

@end
