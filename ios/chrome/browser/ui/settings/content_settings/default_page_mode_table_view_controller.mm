// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMode = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMobile = kItemTypeEnumZero,
  ItemTypeDesktop,
  ItemTypeFooter,
};

}  // namespace

@interface DefaultPageModeTableViewController ()

@property(nonatomic, assign) ItemType chosenItemType;

@end

@implementation DefaultPageModeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_TITLE);
  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierMode];

  TableViewDetailIconItem* mobileItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeMobile];
  mobileItem.text = l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_MOBILE);
  [model addItem:mobileItem toSectionWithIdentifier:SectionIdentifierMode];

  TableViewDetailIconItem* desktopItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeDesktop];
  desktopItem.text = l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_DESKTOP);
  [model addItem:desktopItem toSectionWithIdentifier:SectionIdentifierMode];

  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierMode]) {
    if (item.type == self.chosenItemType) {
      item.accessoryType = UITableViewCellAccessoryCheckmark;
    }
  }

  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  if (self.chosenItemType == ItemTypeDesktop) {
    footer.text =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_DESKTOP_SUBTITLE);
  } else {
    footer.text =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_MOBILE_SUBTITLE);
  }
  [model setFooter:footer forSectionWithIdentifier:SectionIdentifierMode];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];

  DefaultPageMode chosenMode = itemType == ItemTypeMobile
                                   ? DefaultPageModeMobile
                                   : DefaultPageModeDesktop;

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

  base::RecordAction(
      base::UserMetricsAction("MobileDefaultPageModeSettingsClose"));

  [self.delegate didSelectMode:chosenMode];
}

#pragma mark - DefaultPageModeConsumer

- (void)setDefaultPageMode:(DefaultPageMode)mode {
  ItemType chosenType =
      mode == DefaultPageModeMobile ? ItemTypeMobile : ItemTypeDesktop;

  self.chosenItemType = chosenType;

  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierMode]) {
    if (item.type == chosenType) {
      item.accessoryType = UITableViewCellAccessoryCheckmark;
    } else {
      item.accessoryType = UITableViewCellAccessoryNone;
    }
  }

  TableViewLinkHeaderFooterItem* footer =
      base::apple::ObjCCastStrict<TableViewLinkHeaderFooterItem>(
          [self.tableViewModel
              footerForSectionWithIdentifier:SectionIdentifierMode]);
  if (mode == DefaultPageModeDesktop) {
    footer.text =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_DESKTOP_SUBTITLE);
  } else {
    footer.text =
        l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_MOBILE_SUBTITLE);
  }

  base::RecordAction(
      base::UserMetricsAction("MobileDefaultPageModeSettingsToggled"));
  NSIndexSet* section = [NSIndexSet
      indexSetWithIndex:[self.tableViewModel
                            sectionForSectionIdentifier:SectionIdentifierMode]];
  [self.tableView reloadSections:section
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileDefaultPageModeSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileDefaultPageModeSettingsBack"));
}

@end
