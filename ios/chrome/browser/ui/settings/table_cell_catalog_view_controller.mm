// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/table_cell_catalog_view_controller.h"

#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_signin_promo_item.h"
#import "ios/chrome/browser/ui/autofill/cells/autofill_edit_item.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/cells/account_sign_in_item.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_switch_item.h"
#import "ios/chrome/browser/ui/settings/cells/sync_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  ItemTypeTextAccessoryNoImage,
  ItemTypeTextEditItem,
  ItemTypeURLWithTimestamp,
  ItemTypeURLWithSize,
  ItemTypeURLWithSupplementalText,
  ItemTypeURLWithBadgeImage,
  ItemTypeTextSettingsDetail,
  ItemTypeLinkFooter,
  ItemTypeDetailText,
  ItemTypeMultiDetailText,
  ItemTypeAccountSignInItem,
  ItemTypeSettingsSwitch1,
  ItemTypeSettingsSwitch2,
  ItemTypeSyncSwitch,
  ItemTypeSettingsSyncError,
  ItemTypeAutofillData,
  ItemTypeAccount,
};
}

@implementation TableCellCatalogViewController

- (instancetype)init {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;
  return [super initWithTableViewStyle:style
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];
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

  TableViewTextItem* textItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"Simple Text Cell";
  textItem.textAlignment = NSTextAlignmentCenter;
  textItem.textColor = UIColor.cr_labelColor;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  textItem = [[TableViewTextItem alloc] initWithType:ItemTypeText];
  textItem.text = @"1234";
  textItem.masked = YES;
  [model addItem:textItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewImageItem* textImageItem =
      [[TableViewImageItem alloc] initWithType:ItemTypeTextAccessoryImage];
  textImageItem.title = @"Image Item with History Image";
  textImageItem.image = [UIImage imageNamed:@"show_history"];
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
    [UIColor colorNamed:@"settings_catalog_example_text"];
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

  TableViewDetailIconItem* detailIconItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTextSettingsDetail];
  detailIconItem.text = @"Detail Icon Item Cell";
  detailIconItem.iconImageName = @"settings_article_suggestions";
  detailIconItem.detailText = @"Short";
  [model addItem:detailIconItem toSectionWithIdentifier:SectionIdentifierText];

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
  textEditItem.textFieldName = @"Edit Text Item";
  textEditItem.textFieldValue = @" with no icons";
  textEditItem.hideIcon = YES;
  textEditItem.textFieldEnabled = YES;
  [model addItem:textEditItem toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemEditIcon =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemEditIcon.textFieldName = @"Edit Text Item";
  textEditItemEditIcon.textFieldValue = @" with edit icon";
  textEditItemEditIcon.textFieldEnabled = YES;
  [model addItem:textEditItemEditIcon
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemBothIcons =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemBothIcons.textFieldName = @"Edit Text Item";
  textEditItemBothIcons.textFieldValue = @" with edit and custom icons";
  textEditItemBothIcons.identifyingIcon =
      [UIImage imageNamed:@"table_view_cell_check_mark"];
  textEditItemBothIcons.textFieldEnabled = YES;
  [model addItem:textEditItemBothIcons
      toSectionWithIdentifier:SectionIdentifierText];

  TableViewTextEditItem* textEditItemIconButton =
      [[TableViewTextEditItem alloc] initWithType:ItemTypeTextEditItem];
  textEditItemIconButton.textFieldName = @"Edit Text Item";
  textEditItemIconButton.textFieldValue = @" icon is a button.";
  textEditItemIconButton.identifyingIcon =
      [UIImage imageNamed:@"table_view_cell_check_mark"];
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

  SettingsSwitchItem* settingsSwitchItem =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSettingsSwitch1];
  settingsSwitchItem.text = @"This is a settings switch item";
  [model addItem:settingsSwitchItem
      toSectionWithIdentifier:SectionIdentifierSettings];

  SettingsSwitchItem* settingsSwitchItem2 =
      [[SettingsSwitchItem alloc] initWithType:ItemTypeSettingsSwitch2];
  settingsSwitchItem2.text = @"This is a disabled settings switch item";
  settingsSwitchItem2.detailText = @"This is a switch item with detail text";
  settingsSwitchItem2.on = YES;
  settingsSwitchItem2.enabled = NO;
  [model addItem:settingsSwitchItem2
      toSectionWithIdentifier:SectionIdentifierSettings];

  SyncSwitchItem* syncSwitchItem =
      [[SyncSwitchItem alloc] initWithType:ItemTypeSyncSwitch];
  syncSwitchItem.text = @"This is a sync switch item";
  syncSwitchItem.detailText = @"This is a sync switch item with detail text";
  syncSwitchItem.on = YES;
  syncSwitchItem.enabled = NO;
  [model addItem:syncSwitchItem
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

  TableViewLinkHeaderFooterItem* linkFooter =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeLinkFooter];
  linkFooter.text =
      @"This is a footer text view with a BEGIN_LINKlinkEND_LINK in the middle";
  [model setFooter:linkFooter
      forSectionWithIdentifier:SectionIdentifierSettings];

  // SectionIdentifierAutofill.
  TableViewTextHeaderFooterItem* autofillHeader =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeTextHeader];
  autofillHeader.text = @"Autofill";
  [model setHeader:autofillHeader
      forSectionWithIdentifier:SectionIdentifierAutofill];

  CopiedToChromeItem* copiedToChrome =
      [[CopiedToChromeItem alloc] initWithType:ItemTypeAutofillData];
  [model addItem:copiedToChrome
      toSectionWithIdentifier:SectionIdentifierAutofill];

  // SectionIdentifierAccount.
  TableViewSigninPromoItem* signinPromo =
      [[TableViewSigninPromoItem alloc] initWithType:ItemTypeAccount];
  signinPromo.configurator = [[SigninPromoViewConfigurator alloc]
      initWithUserEmail:@"jonhdoe@example.com"
           userFullName:@"John Doe"
              userImage:nil
         hasCloseButton:NO];
  signinPromo.text = @"Signin promo text example";
  [model addItem:signinPromo toSectionWithIdentifier:SectionIdentifierAccount];

  TableViewAccountItem* accountItemDetailWithError =
      [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemDetailWithError.image = [UIImage imageNamed:@"ios_default_avatar"];
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
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemCheckMark.image = [UIImage imageNamed:@"ios_default_avatar"];
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
  item.URL = GURL("https://design.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLNoMetadata];
  item.URL = GURL("https://notitle.google.com");
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithTimestamp];
  item.title = @"Google";
  item.URL = GURL("https://www.google.com");
  item.metadata = @"3:42 PM";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSize];
  item.title = @"World Series 2017: Houston Astros Defeat Someone Else";
  item.URL = GURL("https://m.bbc.com");
  item.metadata = @"176 KB";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item =
      [[TableViewURLItem alloc] initWithType:ItemTypeURLWithSupplementalText];
  item.title = @"Chrome | Google Blog";
  item.URL = GURL("https://blog.google/products/chrome/");
  item.supplementalURLText = @"Read 4 days ago";
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];

  item = [[TableViewURLItem alloc] initWithType:ItemTypeURLWithBadgeImage];
  item.title = @"Photos - Google Photos";
  item.URL = GURL("https://photos.google.com/");
  item.badgeImage = [UIImage imageNamed:@"table_view_cell_check_mark"];
  [model addItem:item toSectionWithIdentifier:SectionIdentifierURL];
}

@end
