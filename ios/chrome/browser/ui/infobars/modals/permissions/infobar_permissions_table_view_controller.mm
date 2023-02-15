// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_handler.h"
#import "ios/chrome/browser/ui/permissions/permission_info.h"
#import "ios/chrome/browser/ui/permissions/permission_metrics_util.h"
#import "ios/chrome/browser/ui/permissions/permissions_constants.h"
#import "ios/chrome/browser/ui/permissions/permissions_delegate.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"

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

// Delegate for this ViewController.
@property(nonatomic, weak) id<InfobarModalDelegate, PermissionsDelegate>
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
    (id<InfobarModalDelegate, PermissionsDelegate>)modalDelegate {
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

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.infobarModalDelegate modalInfobarWasDismissed:self];
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  [self.tableViewModel addItem:[self permissionsDescriptionItem]
       toSectionWithIdentifier:SectionIdentifierContent];

  for (id permission in self.permissionsInfo) {
    [self updateSwitchForPermission:permission tableViewLoaded:NO];
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
      cell.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
  }
  return cell;
}

#pragma mark - PermissionsConsumer

- (void)setPermissionsDescription:(NSString*)permissionsDescription {
  _permissionsDescription = permissionsDescription;
}

- (void)setPermissionsInfo:(NSArray<PermissionInfo*>*)permissionsInfo {
  _permissionsInfo = permissionsInfo;
}

- (void)permissionStateChanged:(PermissionInfo*)permissionInfo {
  [self updateSwitchForPermission:permissionInfo tableViewLoaded:YES];
}

#pragma mark - Private Methods

// Helper that returns the permissionsDescription item.
- (SettingsImageDetailTextItem*)permissionsDescriptionItem {
  SettingsImageDetailTextItem* descriptionItem =
      [[SettingsImageDetailTextItem alloc]
          initWithType:ItemTypePermissionsDescription];

  NSMutableAttributedString* descriptionAttributedString =
      [[NSMutableAttributedString alloc]
          initWithAttributedString:PutBoldPartInString(
                                       self.permissionsDescription,
                                       UIFontTextStyleFootnote)];

  NSDictionary* attrs = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  [descriptionAttributedString
      addAttributes:attrs
              range:NSMakeRange(0, descriptionAttributedString.length)];
  descriptionItem.attributedText = descriptionAttributedString;
  return descriptionItem;
}

// Updates the switch of the given permission.
- (void)updateSwitchForPermission:(PermissionInfo*)permissionInfo
                  tableViewLoaded:(BOOL)tableViewLoaded {
  switch (permissionInfo.permission) {
    case web::PermissionCamera:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_CAMERA)
                                    toItem:ItemTypePermissionsCamera
                           tableViewLoaded:tableViewLoaded];
      break;
    case web::PermissionMicrophone:
      [self updateSwitchForPermissionState:permissionInfo.state
                                 withLabel:l10n_util::GetNSString(
                                               IDS_IOS_PERMISSIONS_MICROPHONE)
                                    toItem:ItemTypePermissionsMicrophone
                           tableViewLoaded:tableViewLoaded];
      break;
  }
}

// Dismisses the infobar modal.
- (void)dismissInfobarModal {
  [self.metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
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
                                toItem:(ItemType)itemType
                       tableViewLoaded:(BOOL)tableViewLoaded {
  if ([self.tableViewModel hasItemForItemType:itemType
                            sectionIdentifier:SectionIdentifierContent]) {
    NSIndexPath* index = [self.tableViewModel indexPathForItemType:itemType];

    // Remove the switch item if the permission is not accessible.
    if (state == web::PermissionStateNotAccessible) {
      [self removeFromModelItemAtIndexPaths:@[ index ]];
      [self.tableView deleteRowsAtIndexPaths:@[ index ]
                            withRowAnimation:UITableViewRowAnimationAutomatic];
      [self.presentationHandler resizeInfobarModal];
    } else {
      TableViewSwitchItem* currentItem =
          base::mac::ObjCCastStrict<TableViewSwitchItem>(
              [self.tableViewModel itemAtIndexPath:index]);
      TableViewSwitchCell* currentCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(
              [self.tableView cellForRowAtIndexPath:index]);
      currentItem.on = state == web::PermissionStateAllowed;
      // Reload the switch cell if its value is outdated.
      if (currentItem.isOn != currentCell.switchView.isOn) {
        [self.tableView
            reloadRowsAtIndexPaths:@[ index ]
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
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
  switchItem.accessibilityIdentifier =
      itemType == ItemTypePermissionsCamera
          ? kInfobarModalCameraSwitchAccessibilityIdentifier
          : kInfobarModalMicrophoneSwitchAccessibilityIdentifier;

  // If ItemTypePermissionsMicrophone is already added, insert the
  // ItemTypePermissionsCamera before the ItemTypePermissionsMicrophone.
  if (itemType == ItemTypePermissionsCamera &&
      [self.tableViewModel hasItemForItemType:ItemTypePermissionsMicrophone
                            sectionIdentifier:SectionIdentifierContent]) {
    NSIndexPath* index = [self.tableViewModel
        indexPathForItemType:ItemTypePermissionsMicrophone];
    [self.tableViewModel insertItem:switchItem
            inSectionWithIdentifier:SectionIdentifierContent
                            atIndex:index.row];
  } else {
    [self.tableViewModel addItem:switchItem
         toSectionWithIdentifier:SectionIdentifierContent];
  }

  if (tableViewLoaded) {
    NSIndexPath* index = [self.tableViewModel indexPathForItemType:itemType];
    [self.tableView insertRowsAtIndexPaths:@[ index ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
    [self.presentationHandler resizeInfobarModal];
  }
}

@end
