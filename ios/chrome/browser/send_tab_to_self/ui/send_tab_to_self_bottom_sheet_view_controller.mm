// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_bottom_sheet_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/send_tab_to_self/target_device_info.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync_device_info/device_info.h"
#import "ios/chrome/browser/send_tab_to_self/ui/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/browser/shared/ui/bottom_sheet/table_view_bottom_sheet_view_controller+subclassing.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
constexpr CGFloat kSymbolSize = 22;
NSString* const kSendTabToSelfModalSendButton =
    @"kSendTabToSelfModalSendButton";
NSString* const kSendTabToSelfModalCancelButton =
    @"kSendTabToSelfModalCancelButton";
NSString* const kSendTabToSelfModalMenuButton =
    @"kSendTabToSelfModalMenuButton";
}  // namespace

@interface SendTabToSelfBottomSheetViewController () <
    ConfirmationAlertActionHandler,
    UITableViewDataSource>
@end

@implementation SendTabToSelfBottomSheetViewController {
  std::vector<send_tab_to_self::TargetDeviceInfo> _targetDeviceList;
  NSString* _accountEmail;
  __weak id<SendTabToSelfModalDelegate> _delegate;
}

- (instancetype)initWithDeviceList:
                    (std::vector<send_tab_to_self::TargetDeviceInfo>)
                        targetDeviceList
                      accountEmail:(NSString*)accountEmail
                          delegate:(id<SendTabToSelfModalDelegate>)delegate {
  ButtonStackConfiguration* configuration =
      [[ButtonStackConfiguration alloc] init];
  self = [super initWithConfiguration:configuration];
  if (self) {
    _targetDeviceList = std::move(targetDeviceList);
    _accountEmail = accountEmail;
    _delegate = delegate;
    self.mainBackgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  // Set the properties read by the super when constructing the
  // views in `-[ConfirmationAlertViewController viewDidLoad]`.
  self.actionHandler = self;

  if (_targetDeviceList.empty()) {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_NO_DEVICES_FOUND_TITLE);
    self.subtitleString = l10n_util::GetNSStringF(
        IDS_IOS_SEND_TAB_TO_SELF_NO_TARGET_DEVICE_LABEL_WITH_EMAIL,
        base::SysNSStringToUTF16(_accountEmail));
    self.configuration.primaryActionString = l10n_util::GetNSString(IDS_CLOSE);
  } else {
    self.navigationItem.title =
        l10n_util::GetNSString(IDS_IOS_SEND_TAB_TO_SELF_BOTTOM_SHEET_TITLE);
    self.configuration.primaryActionString = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_BOTTOM_SHEET_SEND_BUTTON);
  }

  [super viewDidLoad];

  if (!_targetDeviceList.empty()) {
    // Note: The primaryActionButton is created in [super viewDidLoad], so this
    // can't be set earlier.
    self.primaryActionButton.accessibilityIdentifier =
        kSendTabToSelfModalSendButton;
  }

  // Set up the menu button ("...") and close button ("X") in the navigation
  // bar.
  __weak id<SendTabToSelfModalDelegate> weakDelegate = _delegate;
  UIAction* manageDevicesAction =
      [UIAction actionWithTitle:l10n_util::GetNSString(
                                    IDS_IOS_SEND_TAB_TO_SELF_MANAGE_DEVICES)
                          image:DefaultSymbolWithPointSize(kExternalLinkSymbol,
                                                           kSymbolSize)
                     identifier:nil
                        handler:^(UIAction* action) {
                          [weakDelegate openManageDevicesTab];
                        }];
  UIMenu* menu = [UIMenu menuWithTitle:@"" children:@[ manageDevicesAction ]];

  UIImage* menuImage = DefaultSymbolWithPointSize(kEllipsisSymbol, kSymbolSize);
  UIBarButtonItem* menuButton = [[UIBarButtonItem alloc] initWithImage:menuImage
                                                                  menu:menu];
  menuButton.accessibilityIdentifier = kSendTabToSelfModalMenuButton;
  menuButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  self.navigationItem.leftBarButtonItem = menuButton;

  // If there are no target devices, there's a big blue "Close" button, so no
  // need for the "x".
  if (!_targetDeviceList.empty()) {
    UIImage* closeImage = DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolSize);
    UIBarButtonItem* closeButton =
        [[UIBarButtonItem alloc] initWithImage:closeImage
                                         style:UIBarButtonItemStylePlain
                                        target:self
                                        action:@selector(closeButtonTapped)];
    closeButton.accessibilityIdentifier = kSendTabToSelfModalCancelButton;
    closeButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
    self.navigationItem.rightBarButtonItem = closeButton;
  }

  [self adjustTransactionsButtonHorizontalConstraints];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [self rowCount];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  // All the target devices are shown in a flat list i.e. a single section.
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
  return [self layoutCell:cell
        forTableViewWidth:tableView.frame.size.width
              atIndexPath:indexPath];
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // If there are no target devices, the UI shows a "no devices" message and the
  // primary button is "Close" - so nothing to be done except dismiss the UI.
  if (_targetDeviceList.empty()) {
    [_delegate dismissViewControllerAnimated];
    return;
  }

  NSInteger index = [self selectedRow];
  if (index >= 0 && index < static_cast<NSInteger>(_targetDeviceList.size())) {
    const send_tab_to_self::TargetDeviceInfo& device = _targetDeviceList[index];

    NSString* deviceName = base::SysUTF8ToNSString(device.device_name);
    [self showLoadingState:deviceName];

    [_delegate sendTabToTargetDeviceCacheGUID:base::SysUTF8ToNSString(
                                                  device.cache_guid)
                             targetDeviceName:deviceName];
  }
}

- (void)showLoadingState:(NSString*)deviceName {
  self.primaryActionButton.title = @"";
  self.primaryActionButton.tunedDownStyle = YES;
  self.primaryActionButton.primaryButtonImage = PrimaryButtonImageSpinner;
  self.primaryActionButton.enabled = NO;

  // Lock down interactions on the entire navigation controller to prevent taps
  // on navigation bar items (like Close) or swiping the page sheet away.
  if (self.navigationController) {
    self.navigationController.view.userInteractionEnabled = NO;
    self.navigationController.modalInPresentation = YES;
  } else {
    self.view.userInteractionEnabled = NO;
  }

  self.primaryActionButton.accessibilityLabel =
      l10n_util::GetNSStringF(IDS_IOS_SEND_TAB_TO_SELF_SNACKBAR_MESSAGE,
                              base::SysNSStringToUTF16(deviceName));
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.primaryActionButton.accessibilityLabel);
}

- (void)showSuccessState:(NSString*)deviceName {
  self.primaryActionButton.primaryButtonImage = PrimaryButtonImageCheckmark;
  self.primaryActionButton.accessibilityLabel =
      l10n_util::GetNSStringF(IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                              base::SysNSStringToUTF16(deviceName));
  UIAccessibilityPostNotification(UIAccessibilityAnnouncementNotification,
                                  self.primaryActionButton.accessibilityLabel);
}

#pragma mark - TableViewBottomSheetViewController

- (UITableView*)createTableView {
  UITableView* tableView = [super createTableView];
  tableView.dataSource = self;
  [TableViewCellContentConfiguration registerCellForTableView:tableView];
  return tableView;
}

- (NSUInteger)rowCount {
  return _targetDeviceList.size();
}

- (CGFloat)computeTableViewCellHeightAtIndex:(NSUInteger)index {
  static UITableViewCell* cell = [[UITableViewCell alloc] init];
  CGFloat tableWidth = [self tableViewWidth];
  cell = [self layoutCell:cell
        forTableViewWidth:tableWidth
              atIndexPath:[NSIndexPath indexPathForRow:index inSection:0]];
  return [cell systemLayoutSizeFittingSize:CGSizeMake(tableWidth, 1)
             withHorizontalFittingPriority:UILayoutPriorityRequired
                   verticalFittingPriority:UILayoutPriorityFittingSizeLevel]
      .height;
}

#pragma mark - Private

- (void)closeButtonTapped {
  [_delegate dismissViewControllerAnimated];
}

- (UITableViewCell*)layoutCell:(UITableViewCell*)cell
             forTableViewWidth:(CGFloat)tableViewWidth
                   atIndexPath:(NSIndexPath*)indexPath {
  const send_tab_to_self::TargetDeviceInfo& device =
      _targetDeviceList[indexPath.row];

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = base::SysUTF8ToNSString(device.device_name);
  configuration.titleNumberOfLines = 1;
  configuration.titleLineBreakMode = NSLineBreakByTruncatingMiddle;
  configuration.subtitle =
      base::SysUTF16ToNSString(device.GetLastActiveTimeForDisplay());
  configuration.subtitleNumberOfLines = 1;
  configuration.subtitleLineBreakMode = NSLineBreakByTruncatingMiddle;

  UIImage* deviceImage;
  switch (device.form_factor) {
    case syncer::DeviceInfo::FormFactor::kDesktop:
      deviceImage = MakeSymbolMonochrome(
          DefaultSymbolWithPointSize(kLaptopSymbol, kSymbolSize));
      break;
    case syncer::DeviceInfo::FormFactor::kPhone:
      deviceImage = MakeSymbolMonochrome(
          DefaultSymbolWithPointSize(kIPhoneSymbol, kSymbolSize));
      break;
    case syncer::DeviceInfo::FormFactor::kTablet:
      deviceImage = MakeSymbolMonochrome(
          DefaultSymbolWithPointSize(kIPadSymbol, kSymbolSize));
      break;
    case syncer::DeviceInfo::FormFactor::kUnknown:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
      // These form factors don't have a dedicated icon (but very likely these
      // devices don't support SendTabToSelf anyway). Fall back to the generic
      // laptop icon.
      deviceImage = MakeSymbolMonochrome(
          DefaultSymbolWithPointSize(kLaptopSymbol, kSymbolSize));
      break;
  }

  ImageContentConfiguration* imageConfiguration =
      [[ImageContentConfiguration alloc] init];
  imageConfiguration.image = deviceImage;
  imageConfiguration.imageTintColor = [UIColor colorNamed:kTextPrimaryColor];
  imageConfiguration.imageSize = CGSizeMake(kSymbolSize, kSymbolSize);

  configuration.leadingConfiguration = imageConfiguration;

  cell.contentConfiguration = configuration;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;

  cell.accessoryType = [self accessoryType:indexPath];
  return cell;
}

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.isAccessibilityElement = YES;
  subtitle.accessibilityLabel = subtitle.text;
  subtitle.userInteractionEnabled = YES;
}

@end
