// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/modals/permissions/infobar_permissions_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_info.h"
#import "ios/chrome/browser/permissions/ui_bundled/permission_metrics_util.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_constants.h"
#import "ios/chrome/browser/permissions/ui_bundled/permissions_delegate.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_constants.h"
#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"
#import "ios/chrome/browser/ui/infobars/presentation/infobar_modal_presentation_handler.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/permissions/permissions.h"
#import "ui/base/l10n/l10n_util.h"

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierContent = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypePermissionsDescription = kItemTypeEnumZero,
  ItemTypePermissionsCamera,
  ItemTypePermissionsMicrophone,
};

@implementation InfobarPermissionsTableViewController {
  // Whether the table view model has been loaded.
  BOOL _modelLoaded;

  // Delegate for this ViewController.
  __weak id<InfobarModalDelegate, PermissionsDelegate> _infobarModalDelegate;

  // Used to build and record metrics.
  InfobarMetricsRecorder* _metricsRecorder;

  // The permissions description.
  NSString* _permissionsDescription;

  // The list of permissions used to create switches. The first NSNumber
  // represents the `web::Permission` int value and the second its associated
  // `web::PermissionState`.
  NSDictionary<NSNumber*, NSNumber*>* _permissionsInfo;
}

- (instancetype)initWithDelegate:
    (id<InfobarModalDelegate, PermissionsDelegate>)modalDelegate {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _modelLoaded = NO;
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
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Presented];
}

- (void)viewDidDisappear:(BOOL)animated {
  [_infobarModalDelegate modalInfobarWasDismissed:self];
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Dismissed];
  [super viewDidDisappear:animated];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];
  [self.tableViewModel addSectionWithIdentifier:SectionIdentifierContent];

  [self.tableViewModel addItem:[self permissionsDescriptionItem]
       toSectionWithIdentifier:SectionIdentifierContent];

  for (NSNumber* key in _permissionsInfo.allKeys) {
    PermissionInfo* permissionInfo = [[PermissionInfo alloc] init];
    permissionInfo.permission = (web::Permission)key.unsignedIntValue;
    permissionInfo.state =
        (web::PermissionState)_permissionsInfo[key].unsignedIntValue;

    [self updateSwitchForPermission:permissionInfo tableViewLoaded:NO];
  }
  _modelLoaded = YES;
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
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
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

- (void)setPermissionsInfo:
    (NSDictionary<NSNumber*, NSNumber*>*)permissionsInfo {
  _permissionsInfo = [permissionsInfo copy];
}

- (void)permissionStateChanged:(PermissionInfo*)permissionInfo {
  [self updateSwitchForPermission:permissionInfo
                  tableViewLoaded:self.viewLoaded && _modelLoaded];
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
                                       _permissionsDescription,
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
  [_metricsRecorder recordModalEvent:MobileMessagesModalEvent::Canceled];
  [_infobarModalDelegate dismissInfobarModal:self];
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
      NOTREACHED_IN_MIGRATION();
      return;
  }
  PermissionInfo* permissionsDescription = [[PermissionInfo alloc] init];
  permissionsDescription.permission = permission;
  permissionsDescription.state =
      sender.isOn ? web::PermissionStateAllowed : web::PermissionStateBlocked;
  [_infobarModalDelegate updateStateForPermission:permissionsDescription];
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
          base::apple::ObjCCastStrict<TableViewSwitchItem>(
              [self.tableViewModel itemAtIndexPath:index]);
      TableViewSwitchCell* currentCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(
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
    CHECK_NE(index, nil, base::NotFatalUntil::M128);
    [self.tableView insertRowsAtIndexPaths:@[ index ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
    [self.presentationHandler resizeInfobarModal];
  }
}

@end
