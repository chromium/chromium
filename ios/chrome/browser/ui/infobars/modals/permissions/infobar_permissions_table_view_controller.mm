// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_table_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_modal_delegate.h"
#include "ios/chrome/browser/ui/infobars/modals/permissions/permission_info.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePermissionsDescription = kItemTypeEnumZero,
  ItemTypePermissionsCamera,
  ItemTypePermissionsMicrophone,
};

@interface InfobarPermissionsTableViewController ()

// InfobarPermissionsModalDelegate for this ViewController.
@property(nonatomic, weak) id<InfobarPermissionsModalDelegate>
    infobarModalDelegate;
// Used to build and record metrics.
@property(nonatomic, strong) InfobarMetricsRecorder* metricsRecorder;

// The permissions description.
@property(nonatomic, copy) NSString* permissionsDescription;

// The list of permissions info used to create switches.
@property(nonatomic, copy) NSArray<PermissionInfo*>* permissionsInfo;

@end

@implementation InfobarPermissionsTableViewController

- (instancetype)initWithDelegate:
    (id<InfobarPermissionsModalDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypePermissions];
    _infobarModalDelegate = modalDelegate;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  self.tableView.sectionHeaderHeight = 0;

  // Configure the NavigationBar.
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissInfobarModal)];
  doneButton.accessibilityIdentifier = kInfobarModalCancelButton;
  self.navigationItem.rightBarButtonItem = doneButton;
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  [self.tableViewModel addItem:[self permissionsDescriptionItem]
       toSectionWithIdentifier:SectionIdentifierContent];

  for (id permission in self.permissionsInfo) {
    [self updateSwitchForPermission:permission];
  }
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType =
      (ItemType)[self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePermissionsCamera:
    case ItemTypePermissionsMicrophone: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      switchCell.switchView.tag = itemType;
      [switchCell.switchView addTarget:self
                                action:@selector(permissionSwitchToggled:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypePermissionsDescription:
      break;
  }
  return cell;
}

#pragma mark - InfobarPermissionsModalConsumer

- (void)setPermissionsDescription:(NSString*)permissionsDescription {
  _permissionsDescription = permissionsDescription;
}

- (void)setPermissionsInfo:(NSArray<PermissionInfo*>*)permissionsInfo {
  _permissionsInfo = permissionsInfo;
}

- (void)permissionStateChanged:(PermissionInfo*)permissionInfo {
  [self updateSwitchForPermission:permissionInfo];
}

#pragma mark - Private Methods

// Helper that returns the permissionsDescription item.
- (TableViewTextItem*)permissionsDescriptionItem {
  TableViewTextItem* descriptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypePermissionsDescription];
  descriptionItem.text = self.permissionsDescription;
  descriptionItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionItem.textFont =
      [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
  descriptionItem.enabled = NO;
  return descriptionItem;
}

// Updates the switch of the given permission.
- (void)updateSwitchForPermission:(PermissionInfo*)permissionInfo {
  // TODO(crbug.com/1289645): Display permissions always in the same order.
  switch (permissionInfo.permission) {
    case web::PermissionCamera:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_CAMERA)
                                    toItem:ItemTypePermissionsCamera];
      break;
    case web::PermissionMicrophone:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_MICROPHONE)
                                    toItem:ItemTypePermissionsMicrophone];
      break;
  }
}

// Dismisses the infobar modal.
- (void)dismissInfobarModal {
  // TODO(crbug.com/1289645): Record some metrics.
  [self.infobarModalDelegate dismissInfobarModal:self];
}

// Invoked when a permission switch is toggled.
- (void)permissionSwitchToggled:(UISwitch*)sender {
  web::Permission permission;
  switch (sender.tag) {
    case ItemTypePermissionsCamera:
      permission = web::PermissionCamera;
      break;
    case ItemTypePermissionsMicrophone:
      permission = web::PermissionMicrophone;
      break;
    case ItemTypePermissionsDescription:
      NOTREACHED();
      return;
  }
  PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
  permissionsDescription.permission = permission;
  permissionsDescription.state =
      sender.isOn ? web::PermissionStateAllowed : web::PermissionStateBlocked;
  [self.infobarModalDelegate updateStateForPermission:permissionsDescription];
}

// Adds or removes a switch depending on the value of the PermissionState.
- (void)updateSwitchForPermissionState:(web::PermissionState)state
                             withLabel:(NSString*)label
                                toItem:(ItemType)itemType {
  if ([self.tableViewModel hasItemForItemType:itemType
                            sectionIdentifier:SectionIdentifierContent]) {
    // Remove the switch item if the permission is not accessible.
    if (state == web::PermissionStateNotAccessible) {
      [self.tableViewModel removeItemWithType:itemType
                    fromSectionWithIdentifier:SectionIdentifierContent];
      return;
    }

    NSIndexPath* index = [self.tableViewModel indexPathForItemType:itemType];
    TableViewSwitchItem* currentItem =
        base::mac::ObjCCastStrict<TableViewSwitchItem>(
            [self.tableViewModel itemAtIndexPath:index]);
    TableViewSwitchCell* currentCell =
        base::mac::ObjCCastStrict<TableViewSwitchCell>(
            [self.tableView cellForRowAtIndexPath:index]);
    currentItem.on = state == web::PermissionStateAllowed;

    // Reload the switch cell if its value is outdated.
    if (currentItem.isOn != currentCell.switchView.isOn) {
      [self.tableView reloadRowsAtIndexPaths:@[ index ]
                            withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  // Don't add a switch item if the permission is not accessible.
  if (state == web::PermissionStateNotAccessible) {
    return;
  }

  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:itemType];
  switchItem.text = label;
  switchItem.on = state == web::PermissionStateAllowed;
  [self.tableViewModel addItem:switchItem
       toSectionWithIdentifier:SectionIdentifierContent];
}

@end
