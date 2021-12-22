// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller.h"

#import "ios/chrome/browser/ui/settings/content_settings/default_page_mode_table_view_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierMode = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeMobile = kItemTypeEnumZero,
  ItemTypeDesktop,
};

}  // namespace

@interface DefaultPageModeTableViewController ()

@property(nonatomic, assign) ItemType chosenItemType;

@end

@implementation DefaultPageModeTableViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierMode];

  TableViewDetailIconItem* mobileItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeMobile];
  mobileItem.text = @"TEST - Mobile";
  [model addItem:mobileItem toSectionWithIdentifier:SectionIdentifierMode];

  TableViewDetailIconItem* desktopItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeDesktop];
  desktopItem.text = @"TEST - Desktop";
  [model addItem:desktopItem toSectionWithIdentifier:SectionIdentifierMode];

  for (TableViewItem* item in [self.tableViewModel
           itemsInSectionWithIdentifier:SectionIdentifierMode]) {
    if (item.type == self.chosenItemType) {
      item.accessoryType = UITableViewCellAccessoryCheckmark;
    }
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewModel* model = self.tableViewModel;
  NSInteger itemType = [model itemTypeForIndexPath:indexPath];

  DefaultPageMode chosenMode = itemType == ItemTypeMobile
                                   ? DefaultPageModeMobile
                                   : DefaultPageModeDesktop;

  [tableView deselectRowAtIndexPath:indexPath animated:YES];

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

  [self reloadCellsForItems:[self.tableViewModel itemsInSectionWithIdentifier:
                                                     SectionIdentifierMode]
           withRowAnimation:UITableViewRowAnimationAutomatic];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/1276922): Add UserAction recording.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/1276922): Add UserAction recording.
}

@end
