// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/lockdown_mode/lockdown_mode_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/ui/settings/elements/info_popover_view_controller.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierLockdownMode = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeLockdownModeSwitch = kItemTypeEnumZero,
  ItemTypeLockdownModeFooter,
};

NSString* const kLockdownModeTableViewId = @"kLockdownModeTableViewId";
NSString* const kLockdownModeCellId = @"kLockdownModeCellId";

}  // namespace

@interface LockdownModeViewController ()

// The item related to the switch for the lockdown mode setting.
@property(nonatomic, strong) TableViewItem* lockdownModeItem;

// Boolean related to dismissing settings.
@property(nonatomic) BOOL settingsAreDismissed;

// Boolean that is set through the model in order to tell the view if OS
// Lockdown Mode is enabled.
@property(nonatomic) BOOL osLockdownModeEnabled;

@end

@implementation LockdownModeViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kLockdownModeTableViewId;
  self.title = l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_TITLE);
  [self loadModel];
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierLockdownMode];
  [model addItem:self.lockdownModeItem
      toSectionWithIdentifier:SectionIdentifierLockdownMode];
  [model setFooter:[self showLockdownModeFooter]
      forSectionWithIdentifier:SectionIdentifierLockdownMode];
}

#pragma mark - LockdownModeConsumer

- (void)reloadCellsForItems {
  if (!self.tableViewModel) {
    // No need to reconfigure since the model has not been loaded yet.
    return;
  }
  NSArray<TableViewItem*>* items = @[ self.lockdownModeItem ];
  [self reloadCellsForItems:items withRowAnimation:UITableViewRowAnimationNone];
}

- (void)setBrowserLockdownModeEnabled:(BOOL)enabled {
  if (!_osLockdownModeEnabled) {
    TableViewSwitchItem* lockdownModeSwitchItem =
        base::apple::ObjCCastStrict<TableViewSwitchItem>(self.lockdownModeItem);
    lockdownModeSwitchItem.on = enabled;
  }
  [self reloadCellsForItems];
}

- (void)setOSLockdownModeEnabled:(BOOL)enabled {
  // Cell isn't reloaded because the variable is only set during the initial
  // view load. This boolean doesn't get changed unless the OS setting is
  // changed which requires a device restart.
  _osLockdownModeEnabled = enabled;
}

#pragma mark - Private

// Called when switch is toggled.
- (void)lockdownModeSwitchChanged:(UISwitch*)sender {
  [self.modelDelegate didEnableBrowserLockdownMode:sender.isOn];
}

// Removes the view as a result of pressing "Done" button.
- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

// Returns the primary attributed string for the InfoPopOverViewController.
- (NSAttributedString*)createPrimaryMessage {
  NSDictionary* primaryAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]
  };

  return [[NSAttributedString alloc]
      initWithString:l10n_util::GetNSString(
                         IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_TITLE)
          attributes:primaryAttributes];
}

// Returns the secondary attributed string for the InfoPopOverViewController.
- (NSAttributedString*)createSecondaryMessage {
  // Create and format the text.
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]
  };

  NSString* message;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    message = l10n_util::GetNSString(
        IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_SUMMARY_FOR_IPAD);
  } else {
    message = l10n_util::GetNSString(
        IDS_IOS_LOCKDOWN_MODE_INFO_BUTTON_SUMMARY_FOR_IPHONE);
  }
  NSAttributedString* attributedString =
      [[NSAttributedString alloc] initWithString:message
                                      attributes:textAttributes];

  return attributedString;
}

#pragma mark - Actions

// Called when the user clicks on a managed information button.
- (void)didTapUIInfoButton:(UIButton*)buttonView {
  NSAttributedString* primaryString = [self createPrimaryMessage];
  NSAttributedString* secondaryString = [self createSecondaryMessage];

  InfoPopoverViewController* bubbleViewController =
      [[InfoPopoverViewController alloc]
          initWithPrimaryAttributedString:primaryString
                secondaryAttributedString:secondaryString
                                     icon:nil
                   isPresentingFromButton:YES];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;

  [self presentViewController:bubbleViewController animated:YES completion:nil];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
      case ItemTypeLockdownModeSwitch: {
        TableViewSwitchCell* switchCell =
            base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
        [switchCell.switchView addTarget:self
                                  action:@selector(lockdownModeSwitchChanged:)
                        forControlEvents:UIControlEventValueChanged];
        break;
      }
    }
  } else if ([cell isKindOfClass:[TableViewInfoButtonCell class]]) {
    TableViewInfoButtonCell* infoCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [infoCell.trailingButton addTarget:self
                                action:@selector(didTapUIInfoButton:)
                      forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate lockdownModeViewControllerDidRemove:self];
  }
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileLockdownModeSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileLockdownModeSettingsBack"));
}

#pragma mark - Properties

- (TableViewItem*)lockdownModeItem {
  if (!_lockdownModeItem) {
    if (_osLockdownModeEnabled) {
      TableViewInfoButtonItem* lockdownModeItem =
          [[TableViewInfoButtonItem alloc]
              initWithType:ItemTypeLockdownModeSwitch];
      lockdownModeItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_ON);
      lockdownModeItem.text =
          l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_TITLE);
      lockdownModeItem.detailText =
          l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_SWITCH_BUTTON_SUMMARY);
      lockdownModeItem.accessibilityIdentifier = kLockdownModeCellId;
      _lockdownModeItem = lockdownModeItem;
    } else {
      TableViewSwitchItem* lockdownModeItem =
          [[TableViewSwitchItem alloc] initWithType:ItemTypeLockdownModeSwitch];
      lockdownModeItem.text =
          l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_TITLE);
      lockdownModeItem.detailText =
          l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_SWITCH_BUTTON_SUMMARY);
      lockdownModeItem.accessibilityIdentifier = kLockdownModeCellId;
      _lockdownModeItem = lockdownModeItem;
    }
  }
  return _lockdownModeItem;
}

- (TableViewHeaderFooterItem*)showLockdownModeFooter {
  TableViewLinkHeaderFooterItem* lockdownModeFooterItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeLockdownModeFooter];
  lockdownModeFooterItem.text =
      l10n_util::GetNSString(IDS_IOS_LOCKDOWN_MODE_FOOTER);
  return lockdownModeFooterItem;
}

@end
