// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/settings/cells/safe_browsing_header_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_view_controller_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ItemArray = NSArray<TableViewItem*>*;

namespace {
// List of sections. There are two section headers since one header is allowed
// per section, and two header rows needed to above the rest of the content in
// this view. There are two section headers instead of one extra section and
// attaching the second header to
// SectionIdentifierSafeBrowsingStandardProtection so that padding worked as
// intended and provided enough space between the header and the switches.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierHeaderShield = kSectionIdentifierEnumZero,
  SectionIdentifierHeaderMetric,
  SectionIdentifierSafeBrowsingStandardProtection,
};

const CGFloat kSafeBrowsingStandardProtectionContentInset = 16;
}  // namespace

@interface SafeBrowsingStandardProtectionViewController () <
    PopoverLabelViewControllerDelegate>

// All items for safe browsing standard protection view.
@property(nonatomic, strong) ItemArray safeBrowsingStandardProtectionItems;

// Header related to shield icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* shieldIconHeader;

// Header related to metric icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* metricIconHeader;

@end

@implementation SafeBrowsingStandardProtectionViewController

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if ((self = [super initWithStyle:style])) {
    // Wraps view controller to properly show navigation bar, otherwise "Done"
    // button won't show.
    self.navigationController =
        [[UINavigationController alloc] initWithRootViewController:self];
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(dismiss)];
    self.modalPresentationStyle = UIModalPresentationFormSheet;
    self.navigationItem.rightBarButtonItem = doneButton;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kSafeBrowsingStandardProtectionTableViewId;
  self.title = l10n_util::GetNSString(
      IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE);
  [self loadModel];
  // Moved position down to center the view better.
  [self.tableView
      setContentInset:UIEdgeInsetsMake(
                          kSafeBrowsingStandardProtectionContentInset, 0, 0,
                          0)];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate
      safeBrowsingStandardProtectionViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - Private

// Called when switch is toggled.
- (void)switchAction:(UISwitch*)sender {
  NSIndexPath* indexPath =
      [self.tableViewModel indexPathForItemType:sender.tag];
  DCHECK(indexPath);
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  [self.modelDelegate toggleSwitchItem:item withValue:sender.isOn];
}

// Removes the view as a result of pressing "Done" button.
- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView addTarget:self
                              action:@selector(switchAction:)
                    forControlEvents:UIControlEventValueChanged];
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    switchCell.switchView.tag = item.type;
  } else if ([cell isKindOfClass:[TableViewInfoButtonCell class]]) {
    TableViewInfoButtonCell* infoCell =
        base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
    [infoCell.trailingButton addTarget:self
                                action:@selector(didTapManagedUIInfoButton:)
                      forControlEvents:UIControlEventTouchUpInside];
  }

  return cell;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingStandardProtectionSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingStandardProtectionSettingsBack"));
}

#pragma mark - Actions

// Called when the user clicks on a managed information button.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];

  bubbleViewController.delegate = self;
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

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        safeBrowsingStandardProtectionViewControllerDidRemove:self];
  }
}

#pragma mark - SafeBrowsingStandardProtectionConsumer

- (void)reloadCellsForItems {
  if (!self.tableViewModel) {
    // No need to reconfigure since the model has not been loaded yet.
    return;
  }
  [self reloadCellsForItems:self.safeBrowsingStandardProtectionItems
           withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)setSafeBrowsingStandardProtectionItems:
    (ItemArray)safeBrowsingStandardProtectionItems {
  _safeBrowsingStandardProtectionItems = safeBrowsingStandardProtectionItems;
}

- (void)setShieldIconHeader:(SafeBrowsingHeaderItem*)shieldIconHeader {
  _shieldIconHeader = shieldIconHeader;
}

- (void)setMetricIconHeader:(SafeBrowsingHeaderItem*)metricIconHeader {
  _metricIconHeader = metricIconHeader;
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model
      addSectionWithIdentifier:SectionIdentifierSafeBrowsingStandardProtection];
  for (TableViewItem* item in self.safeBrowsingStandardProtectionItems) {
    [model addItem:item
        toSectionWithIdentifier:
            SectionIdentifierSafeBrowsingStandardProtection];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction(
      "IOSSafeBrowsingStandardProtectionSettingsCloseWithSwipe"));
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
