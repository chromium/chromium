// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_view_controller_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

using ItemArray = NSArray<TableViewItem*>*;

namespace {
// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierPrivacySafeBrowsing = kSectionIdentifierEnumZero,
};
}  // namespace

@interface PrivacySafeBrowsingViewController () <
    PopoverLabelViewControllerDelegate>

// All the items for the safe browsing section.
@property(nonatomic, strong) ItemArray safeBrowsingItems;

// Boolean to detect if enterprise is enabled.
@property(nonatomic, assign, readonly) BOOL enterpriseEnabled;

@end

@implementation PrivacySafeBrowsingViewController

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:YES];
  [self.modelDelegate selectItem];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPrivacySafeBrowsingTableViewId;
  self.title = l10n_util::GetNSString(IDS_IOS_PRIVACY_SAFE_BROWSING_TITLE);
  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePrivacySafeBrowsingSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobilePrivacySafeBrowsingSettingsBack"));
}

#pragma mark - PrivacySafeBrowsingConsumer

- (void)reconfigureCellsForItems {
  if (!self.tableViewModel) {
    // No need to reconfigure since the model has not been loaded yet.
    return;
  }

  NSMutableArray<NSIndexPath*>* indexPaths = [[NSMutableArray alloc] init];
  for (TableViewItem* item in self.safeBrowsingItems) {
    [indexPaths addObject:[self.tableViewModel indexPathForItem:item]];
  }

  [self.tableView reconfigureRowsAtIndexPaths:indexPaths];
}

- (void)setSafeBrowsingItems:(ItemArray)safeBrowsingItems {
  _safeBrowsingItems = safeBrowsingItems;
}

- (void)setEnterpriseEnabled:(BOOL)enterpriseEnabled {
  _enterpriseEnabled = enterpriseEnabled;
}

- (void)selectItem:(TableViewItem*)item {
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:item];
  [self.tableView selectRowAtIndexPath:indexPath
                              animated:YES
                        scrollPosition:UITableViewScrollPositionNone];
}

- (void)showEnterprisePopUp:(UIButton*)buttonView {
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

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:SectionIdentifierPrivacySafeBrowsing];
  for (TableViewItem* item in self.safeBrowsingItems) {
    [model addItem:item
        toSectionWithIdentifier:SectionIdentifierPrivacySafeBrowsing];
  }
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate privacySafeBrowsingViewControllerDidRemove:self];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSPrivacySafeBrowsingSettingsCloseWithSwipe"));
}

#pragma mark - Actions

// Called when the user clicks on a information button.
- (void)didTapUIInfoButton:(UIButton*)buttonView {
  CGPoint hitPoint = [buttonView convertPoint:CGPointZero
                                       toView:self.tableView];
  NSIndexPath* indexPath = [self.tableView indexPathForRowAtPoint:hitPoint];
  TableViewModel* model = self.tableViewModel;
  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];

  [self.modelDelegate didTapInfoButton:buttonView onItem:selectedItem];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  cell.selectionStyle = UITableViewCellSelectionStyleBlue;

  TableViewInfoButtonCell* infoCell =
      base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
  [infoCell.trailingButton addTarget:self
                              action:@selector(didTapUIInfoButton:)
                    forControlEvents:UIControlEventTouchUpInside];

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  TableViewModel* model = self.tableViewModel;
  TableViewItem* selectedItem = [model itemAtIndexPath:indexPath];
  [self.modelDelegate didSelectItem:selectedItem];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.enterpriseEnabled) {
    return NO;
  }
  return YES;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

@end
