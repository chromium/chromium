// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/table_cell_catalog_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_account_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/reading_list/ui_bundled/reading_list_table_view_item.h"
#import "ios/chrome/browser/settings/ui_bundled/address_bar_preference/cells/address_bar_options_item.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/inline_promo_item.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_check_cell.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_check_item.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/settings/ui_bundled/cells/sync_switch_item.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/password/passwords_table_view_constants.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_line_text_edit_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
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
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
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
  ItemTypeURL,
  ItemTypeTextAccessoryImage,
  ItemTypeSearchHistorySuggestedItem,
  ItemTypeTextAccessoryNoImage,
  ItemTypeTextEditItem,
  ItemTypeTextMultiLineEditItem,
  ItemTypeURLWithSize,
  ItemTypeURLWithThirdRowText,
  ItemTypeReadingList,
  ItemTypeTextSettingsDetail,
  ItemTypeTableViewWithBlueDot,
  ItemTypeLinkFooter,
  ItemAddressBarOptions,
  ItemTypeDetailText,
  ItemTypeMultiDetailText,
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
}  // namespace

@implementation TableCellCatalogViewController {
  raw_ptr<Browser> _browser;
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    _faviconLoader =
        IOSChromeFaviconLoaderFactory::GetForProfile(browser->GetProfile());
  }
  return self;
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

  TableViewDetailIconItem* tableViewNewBadgeItem =
      [[TableViewDetailIconItem alloc]
          initWithType:ItemTypeTableViewWithBlueDot];
  tableViewNewBadgeItem.badgeType = BadgeType::kNew;
  tableViewNewBadgeItem.text = @"I have a new badge!";
  tableViewNewBadgeItem.iconImage =
      DefaultSettingsRootSymbol(kDefaultBrowserSymbol);
  [model addItem:tableViewNewBadgeItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  textItem.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  textItem = [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"1234";
  textItem.masked = YES;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewImageItem* textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textImageItem.title = @"Image Item with History Image";
  textImageItem.detailText = @"and with a very very very long subtitle.";
  textImageItem.image =
      DefaultSymbolWithPointSize(kHistorySymbol, kSymbolActionPointSize);
  [model addItem:textImageItem toSectionWithIdentifier:SectionIdentifierText];

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
  textActionButtonItem.buttonText = @"Do something";
  [model addItem:textActionButtonItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtoExpandedItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
  textActionButtoExpandedItem.disableButtonIntrinsicWidth = YES;
  textActionButtoExpandedItem.buttonText = @"Expanded Button";
  [model addItem:textActionButtoExpandedItem
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextButtonItem* textActionButtonColorItem =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeTextButton];
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
  tableViewMultiDetailTextItem.text =
      @"Title label is very very very very very very very long";
  tableViewMultiDetailTextItem.leadingDetailText = @"Leading Detail Text";
  tableViewMultiDetailTextItem.trailingDetailText =
      @"Right label is very very very very very very very long";
  tableViewMultiDetailTextItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  tableViewMultiDetailTextItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text = @"L";
  tableViewMultiDetailTextItem.leadingDetailText = @"Leading Detail Text";
  tableViewMultiDetailTextItem.trailingDetailText =
      @"Right label is very very very very very very very long";
  tableViewMultiDetailTextItem.accessoryType =
      UITableViewCellAccessoryDisclosureIndicator;
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  tableViewMultiDetailTextItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text = @"L";
  tableViewMultiDetailTextItem.trailingDetailText =
      @"Right label is very very very very very very very long";
  [model addItem:tableViewMultiDetailTextItem
      toSectionWithIdentifier:SectionIdentifierText];

  tableViewMultiDetailTextItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypeMultiDetailText];
  tableViewMultiDetailTextItem.text =
      @"Title label is very very very very very very very long ";
  tableViewMultiDetailTextItem.leadingDetailText = @"Leading Detail Text";
  tableViewMultiDetailTextItem.trailingDetailText = @"R";
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
  tableViewInfoButtonItem.target = self;
  tableViewInfoButtonItem.selector = @selector(didTapManagedUIInfoButton:);
  [model addItem:tableViewInfoButtonItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewInfoButtonItem* tableViewInfoButtonItemWithDetailText =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeTableViewInfoButtonWithDetailText];
  tableViewInfoButtonItemWithDetailText.text = @"Info button item";
  tableViewInfoButtonItemWithDetailText.detailText = @"Detail text";
  tableViewInfoButtonItemWithDetailText.statusText = @"Status";
  tableViewInfoButtonItemWithDetailText.target = self;
  tableViewInfoButtonItemWithDetailText.selector =
      @selector(didTapManagedUIInfoButton:);
  [model addItem:tableViewInfoButtonItemWithDetailText
      toSectionWithIdentifier:SectionIdentifierSettings];

  TableViewInfoButtonItem* tableViewInfoButtonItemWithLeadingImage =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeTableViewInfoButtonWithImage];
  tableViewInfoButtonItemWithLeadingImage.text = @"Info button item";
  tableViewInfoButtonItemWithLeadingImage.statusText = @"Status";
  tableViewInfoButtonItemWithLeadingImage.iconImage =
      DefaultSettingsRootSymbol(kDiscoverSymbol);
  tableViewInfoButtonItemWithLeadingImage.target = self;
  tableViewInfoButtonItemWithLeadingImage.selector =
      @selector(didTapManagedUIInfoButton:);
  [model addItem:tableViewInfoButtonItemWithLeadingImage
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsImageDetailTextItem* imageDetailTextItem =
      [[SettingsImageDetailTextItem alloc]
          initWithType:ItemTypeSettingsSyncError];
  imageDetailTextItem.text = @"This is an error description about sync";
  imageDetailTextItem.detailText =
      @"This is more detail about the sync error description";
  imageDetailTextItem.image = DefaultSymbolTemplateWithPointSize(
      kInfoCircleSymbol, kSymbolActionPointSize);
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
  checkFinishedWithLeadingImage.leadingIcon =
      DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                         kSymbolActionPointSize);
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
  checkDisabledWithLeadingImage.leadingIcon =
      DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                         kSymbolActionPointSize);
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
  TableViewURLItem* item = [[TableViewURLItem alloc] initWithType:ItemTypeURL];
  item.title = @"Google Design";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://design.google.com")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURL];
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://notitle.google.com")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else and "
               @"Also Win Because They Won.";
  item.URL = [[CrURL alloc] initWithGURL:GURL("https://m.bbc.com")];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithThirdRowText];
  item.title = @"Web Channel with 3rd Row Text";
  item.URL =
      [[CrURL alloc] initWithGURL:GURL("https://blog.google/products/chrome/")];
  item.thirdRowText = @"Unavailable";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  ReadingListTableViewItem* readingListItem =
      [[ReadingListTableViewItem alloc] initWithType:ItemTypeReadingList];
  readingListItem.title = @"Reading List item";
  readingListItem.entryURL = GURL("https://lemonde.fr/my-article");
  readingListItem.distillationState = ReadingListUIDistillationStatusSuccess;
  readingListItem.distillationDateText = @"1min ago";
  [model addItem:readingListItem toSectionWithIdentifier:SectionIdentifierURL];

  readingListItem =
      [[ReadingListTableViewItem alloc] initWithType:ItemTypeReadingList];
  readingListItem.title = @"Local Reading List item";
  readingListItem.entryURL = GURL("https://lemonde.fr/my-article");
  readingListItem.distillationState = ReadingListUIDistillationStatusFailure;
  readingListItem.showCloudSlashIcon = YES;
  [model addItem:readingListItem toSectionWithIdentifier:SectionIdentifierURL];
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
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  if (itemType == ItemTypeURL) {
    TableViewURLItem* URLItem = base::apple::ObjCCastStrict<TableViewURLItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    if (!URLItem.faviconAttributes) {
      _faviconLoader->FaviconForPageUrl(
          URLItem.URL.gurl, 20, 20,
          /*fallback_to_google_server=*/true,
          ^(FaviconAttributes* attributes, bool cached) {
            URLItem.faviconAttributes = attributes;
            if (!cached && attributes.faviconImage) {
              [tableView reconfigureRowsAtIndexPaths:@[ indexPath ]];
            }
          });
    }
  }
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (itemType == ItemTypeCheck6) {
    SettingsCheckCell* checkCell =
        base::apple::ObjCCastStrict<SettingsCheckCell>(cell);
    [checkCell.infoButton addTarget:self
                             action:@selector(didTapCheckInfoButton:)
                   forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

@end
