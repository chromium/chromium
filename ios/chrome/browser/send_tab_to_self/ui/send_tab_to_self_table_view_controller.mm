// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_table_view_controller.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/features.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_constants.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_image_detail_text_item.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_manage_devices_item.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

constexpr CGFloat kSymbolSize = 22;

}  // namespace

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSend = kItemTypeEnumZero,
  ItemTypeDevice,
  ItemTypeNoTargetDevice,
  ItemTypeManageDevices,
};

@interface SendTabToSelfTableViewController ()

// Item that holds the currently selected device.
@property(nonatomic, strong) SendTabToSelfImageDetailTextItem* selectedItem;

// Delegate to handle dismissal and event actions.
@property(nonatomic, weak) id<SendTabToSelfModalDelegate> delegate;

// Avatar of the account sharing a tab.
@property(nonatomic, strong) UIImage* accountAvatar;
// Email of the account sharing a tab.
@property(nonatomic, copy) NSString* accountEmail;

// Item that holds the cancel Button for this modal dialog.
@property(nonatomic, strong) TableViewTextButtonItem* sendToDevice;
@end

@implementation SendTabToSelfTableViewController {
  // The list of devices with their names, cache_guids, device types,
  // and active times.
  std::vector<send_tab_to_self::TargetDeviceInfo> _targetDeviceList;
}

- (instancetype)initWithDeviceList:
                    (std::vector<send_tab_to_self::TargetDeviceInfo>)
                        targetDeviceList
                          delegate:(id<SendTabToSelfModalDelegate>)delegate
                     accountAvatar:(UIImage*)accountAvatar
                      accountEmail:(NSString*)accountEmail {
  self = [super initWithStyle:UITableViewStylePlain];

  if (self) {
    _targetDeviceList = std::move(targetDeviceList);
    _delegate = delegate;
    _accountEmail = [accountEmail copy];
    _accountAvatar = accountAvatar;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;

  // Configure the NavigationBar.
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(dismiss:)];
  cancelButton.accessibilityIdentifier = kSendTabToSelfModalCancelButton;

  self.navigationItem.leftBarButtonItem = cancelButton;
  self.navigationItem.title =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_TITLE);
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  [self loadModel];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierEnumZero];

  if (_targetDeviceList.empty()) {
    TableViewMultiDetailTextItem* noTargetDeviceItem =
        [[TableViewMultiDetailTextItem alloc]
            initWithType:ItemTypeNoTargetDevice];
    noTargetDeviceItem.leadingDetailText =
        l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL);
    [model addItem:noTargetDeviceItem
        toSectionWithIdentifier:kSectionIdentifierEnumZero];
  } else {
    [self addDeviceItems];
  }

  SendTabToSelfManageDevicesItem* manageDevicesItem =
      [[SendTabToSelfManageDevicesItem alloc]
          initWithType:ItemTypeManageDevices];
  manageDevicesItem.accountAvatar = self.accountAvatar;
  manageDevicesItem.accountEmail = self.accountEmail;
  manageDevicesItem.showManageDevicesLink = !_targetDeviceList.empty();
  manageDevicesItem.delegate = self.delegate;
  [model addItem:manageDevicesItem
      toSectionWithIdentifier:kSectionIdentifierEnumZero];

  if (_targetDeviceList.empty()) {
    // No need for the send button if there are no devices.
    return;
  }

  self.sendToDevice =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSend];
  self.sendToDevice.buttonText = l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF);
  self.sendToDevice.buttonTextColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  self.sendToDevice.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  self.sendToDevice.boldButtonText = NO;
  self.sendToDevice.accessibilityIdentifier = kSendTabToSelfModalSendButton;
  [model addItem:self.sendToDevice
      toSectionWithIdentifier:kSectionIdentifierEnumZero];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSend) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::apple::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(sendTabWhenPressed:)
                             forControlEvents:UIControlEventTouchUpInside];
  }

  // Hide the separator for the last row ("manage devices" item) by maxing out
  // the left margin. For other cells, use a standard value.
  CGFloat separatorLeftMargin = itemType == ItemTypeManageDevices
                                    ? self.tableView.bounds.size.width
                                    : kTableViewHorizontalSpacing;
  cell.separatorInset = UIEdgeInsetsMake(0.f, separatorLeftMargin, 0.f, 0.f);
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  CHECK(item, base::NotFatalUntil::M158);
  if (item.type == ItemTypeDevice) {
    SendTabToSelfImageDetailTextItem* imageDetailTextItem =
        base::apple::ObjCCastStrict<SendTabToSelfImageDetailTextItem>(item);
    if (imageDetailTextItem == self.selectedItem) {
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    self.selectedItem.accessoryType = UITableViewCellAccessoryNone;
    imageDetailTextItem.accessoryType = UITableViewCellAccessoryCheckmark;
    [self reconfigureCellsForItems:@[ self.selectedItem, imageDetailTextItem ]];
    self.selectedItem = imageDetailTextItem;
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  }
}

#pragma mark - Helpers

- (void)sendTabWhenPressed:(UIButton*)sender {
  [self.delegate sendTabToTargetDeviceCacheGUID:self.selectedItem.cacheGuid
                               targetDeviceName:self.selectedItem.text];
}

- (void)dismiss:(UIButton*)sender {
  [self.delegate dismissViewControllerAnimated];
}

- (void)addDeviceItems {
  BOOL isFirst = YES;
  for (const auto& targetDevice : _targetDeviceList) {
    SendTabToSelfImageDetailTextItem* deviceItem =
        [[SendTabToSelfImageDetailTextItem alloc] initWithType:ItemTypeDevice];
    deviceItem.text = base::SysUTF8ToNSString(targetDevice.device_name);
    deviceItem.detailText =
        base::SysUTF16ToNSString(targetDevice.GetLastActiveTimeForDisplay());
    switch (targetDevice.form_factor) {
      case syncer::DeviceInfo::FormFactor::kTablet:
        deviceItem.image =
            MakeSymbolMonochrome(SymbolWithPointSize(SymbolIPad, kSymbolSize));
        break;
      case syncer::DeviceInfo::FormFactor::kPhone:
        deviceItem.image = MakeSymbolMonochrome(
            SymbolWithPointSize(SymbolIPhone, kSymbolSize));
        break;
      case syncer::DeviceInfo::FormFactor::kDesktop:
        deviceItem.image = MakeSymbolMonochrome(
            SymbolWithPointSize(SymbolLaptop, kSymbolSize));
        break;
      default:
        deviceItem.image = MakeSymbolMonochrome(
            SymbolWithPointSize(SymbolLaptop, kSymbolSize));
        break;
    }

    deviceItem.imageViewTintColor = [UIColor colorNamed:kGrey400Color];

    if (isFirst) {
      deviceItem.accessoryType = UITableViewCellAccessoryCheckmark;
      self.selectedItem = deviceItem;
      isFirst = NO;
    }

    deviceItem.cacheGuid = base::SysUTF8ToNSString(targetDevice.cache_guid);

    [self.tableViewModel addItem:deviceItem
         toSectionWithIdentifier:kSectionIdentifierEnumZero];
  }
}

@end
