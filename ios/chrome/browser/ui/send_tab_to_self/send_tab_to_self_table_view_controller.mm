// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_table_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_image_detail_text_item.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_button_item.h"
#include "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Accessibility identifier of the Modal Cancel Button.
NSString* const kSendTabToSelfModalCancelButton =
    @"kSendTabToSelfModalCancelButton";
// Accessibility identifier of the Modal Cancel Button.
NSString* const kSendTabToSelfModalSendButton =
    @"kSendTabToSelfModalSendButton";

// Per histograms.xml this records whether the user has clicked the item when it
// is shown.
const char kClickResultHistogramName[] = "SendTabToSelf.ShareMenu.ClickResult";
// Per histograms.xml this records how many valid devices are shown when user
// trigger to see the device list.
const char kDeviceCountHistogramName[] = "SendTabToSelf.ShareMenu.DeviceCount";

// TODO(crbug.com/970886): Move to a directory accessible on all platforms.
// State of the send tab to self option in the context menu.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SendTabToSelfClickResult {
  kShowItem = 0,
  kClickItem = 1,
  kShowDeviceList = 2,
  kMaxValue = kShowDeviceList,
};

}  // namespace

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierDevicesToSend = kSectionIdentifierEnumZero,
  SectionIdentifierActionButton,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSend = kItemTypeEnumZero,
  ItemTypeDevice,
};

@interface SendTabToSelfTableViewController () {
  // The list of devices with thier names, cache_guids, device types,
  // and active times.
  std::vector<send_tab_to_self::TargetDeviceInfo> _target_device_list;
}
// Item that holds the currently selected device.
@property(nonatomic, strong) SendTabToSelfImageDetailTextItem* selectedItem;

// Delegate to handle dismisal and event actions.
@property(nonatomic, weak) id<SendTabToSelfModalDelegate> delegate;

// Item that holds the cancel Button for this modal dialog.
@property(nonatomic, strong) TableViewTextButtonItem* sendToDevice;
@end

@implementation SendTabToSelfTableViewController

- (instancetype)initWithModel:
                    (send_tab_to_self::SendTabToSelfModel*)sendTabToSelfModel
                     delegate:(id<SendTabToSelfModalDelegate>)delegate {
  self = [super initWithTableViewStyle:UITableViewStylePlain
                           appBarStyle:ChromeTableViewControllerStyleNoAppBar];

  if (self) {
    _target_device_list = sendTabToSelfModel->GetTargetDeviceInfoSortedList();
    _delegate = delegate;
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = UIColor.cr_systemBackgroundColor;
  self.styler.cellBackgroundColor = UIColor.cr_systemBackgroundColor;
  self.tableView.sectionHeaderHeight = 0;
  self.tableView.sectionFooterHeight = 0;
  [self.tableView
      setSeparatorInset:UIEdgeInsetsMake(0, kTableViewHorizontalSpacing, 0, 0)];

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
  [model addSectionWithIdentifier:SectionIdentifierDevicesToSend];
  for (auto iter = _target_device_list.begin();
       iter != _target_device_list.end(); ++iter) {
    int daysSinceLastUpdate =
        (base::Time::Now() - iter->last_updated_timestamp).InDays();

    SendTabToSelfImageDetailTextItem* deviceItem =
        [[SendTabToSelfImageDetailTextItem alloc] initWithType:ItemTypeDevice];
    deviceItem.text = base::SysUTF8ToNSString(iter->device_name);
    deviceItem.detailText =
        [self sendTabToSelfdaysSinceLastUpdate:daysSinceLastUpdate];
    switch (iter->device_type) {
      case sync_pb::SyncEnums::TYPE_TABLET:
        deviceItem.iconImageName = @"send_tab_to_self_tablet";
        break;
      case sync_pb::SyncEnums::TYPE_PHONE:
        deviceItem.iconImageName = @"send_tab_to_self_smartphone";
        break;
      case sync_pb::SyncEnums::TYPE_WIN:
      case sync_pb::SyncEnums::TYPE_MAC:
      case sync_pb::SyncEnums::TYPE_LINUX:
      case sync_pb::SyncEnums::TYPE_CROS:
        deviceItem.iconImageName = @"send_tab_to_self_laptop";
        break;
      default:
        deviceItem.iconImageName = @"send_tab_to_self_devices";
    }

    if (iter == _target_device_list.begin()) {
      deviceItem.selected = YES;
      self.selectedItem = deviceItem;
    }

    deviceItem.cacheGuid = base::SysUTF8ToNSString(iter->cache_guid);

    [model addItem:deviceItem
        toSectionWithIdentifier:SectionIdentifierDevicesToSend];
  }

  [model addSectionWithIdentifier:SectionIdentifierActionButton];
  self.sendToDevice =
      [[TableViewTextButtonItem alloc] initWithType:ItemTypeSend];
  self.sendToDevice.buttonText =
      l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ACTION);
  self.sendToDevice.buttonTextColor =
      [UIColor colorNamed:kSolidButtonTextColor];
  self.sendToDevice.buttonBackgroundColor = [UIColor colorNamed:kBlueColor];
  self.sendToDevice.boldButtonText = NO;
  self.sendToDevice.accessibilityIdentifier = kSendTabToSelfModalSendButton;
  [model addItem:self.sendToDevice
      toSectionWithIdentifier:SectionIdentifierActionButton];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  base::UmaHistogramEnumeration(kClickResultHistogramName,
                                SendTabToSelfClickResult::kShowDeviceList);
  base::UmaHistogramCounts100(kDeviceCountHistogramName,
                              _target_device_list.size());
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  if (itemType == ItemTypeSend) {
    TableViewTextButtonCell* tableViewTextButtonCell =
        base::mac::ObjCCastStrict<TableViewTextButtonCell>(cell);
    [tableViewTextButtonCell.button addTarget:self
                                       action:@selector(sendTabWhenPressed:)
                             forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  if (item.type == ItemTypeDevice) {
    SendTabToSelfImageDetailTextItem* imageDetailTextItem =
        base::mac::ObjCCastStrict<SendTabToSelfImageDetailTextItem>(item);
    if (imageDetailTextItem == self.selectedItem) {
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    self.selectedItem.selected = NO;
    imageDetailTextItem.selected = YES;
    [self reconfigureCellsForItems:@[ self.selectedItem, imageDetailTextItem ]];
    self.selectedItem = imageDetailTextItem;
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  }
}

#pragma mark - Helpers

- (void)sendTabWhenPressed:(UIButton*)sender {
  base::UmaHistogramEnumeration(kClickResultHistogramName,
                                SendTabToSelfClickResult::kClickItem);
  [self.delegate sendTabToTargetDeviceCacheGUID:self.selectedItem.cacheGuid
                               targetDeviceName:self.selectedItem.text];
  [self.delegate dismissViewControllerAnimated:YES completion:nil];
}

- (void)dismiss:(UIButton*)sender {
  [self.delegate dismissViewControllerAnimated:YES completion:nil];
}

- (NSString*)sendTabToSelfdaysSinceLastUpdate:(int)days {
  NSString* active_time;
  if (days == 0) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_TODAY);
  } else if (days == 1) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAY);
  } else {
    active_time = l10n_util::GetNSStringF(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAYS,
        base::NumberToString16(days));
  }
  return active_time;
}

@end
