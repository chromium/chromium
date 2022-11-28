// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_enhanced_protection_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/settings/cells/settings_image_detail_text_item.h"
#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ItemArray = NSArray<TableViewItem*>*;

namespace {
// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSafeBrowsingEnhancedProtection = kSectionIdentifierEnumZero,
};
}  // namespace

@interface SafeBrowsingEnhancedProtectionViewController ()

// All the items for the enhanced safe browsing section.
@property(nonatomic, strong) ItemArray safeBrowsingEnhancedProtectionItems;

@end

@implementation SafeBrowsingEnhancedProtectionViewController

- (instancetype)initWithStyle:(UITableViewStyle)style {
  if (self = [super initWithStyle:style]) {
    // Wraps view controller to properly show navigation bar, otherwise "Done"
    // button won't show.
    self.navigationController =
        [[UINavigationController alloc] initWithRootViewController:self];
    UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
        initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                             target:self
                             action:@selector(dismiss)];
    self.navigationController.modalPresentationStyle =
        UIModalPresentationFormSheet;
    self.navigationItem.rightBarButtonItem = doneButton;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kSafeBrowsingEnhancedProtectionTableViewId;
  self.tableView.separatorColor = UIColor.clearColor;
  self.title =
      l10n_util::GetNSString(IDS_IOS_SAFE_BROWSING_ENHANCED_PROTECTION_TITLE);
  self.styler.cellBackgroundColor = UIColor.clearColor;
  [self loadModel];
}

- (void)viewDidDisappear:(BOOL)animated {
  [self.presentationDelegate
      safeBrowsingEnhancedProtectionViewControllerDidRemove:self];
  [super viewDidDisappear:animated];
}

#pragma mark - Private

// Removes the view as a result of pressing "Done" button.
- (void)dismiss {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingEnhancedProtectionSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction(
      "MobileSafeBrowsingEnhancedProtectionSettingsBack"));
}

#pragma mark - SafeBrowsingEnhancedProtectionConsumer

- (void)setSafeBrowsingEnhancedProtectionItems:
    (ItemArray)safeBrowsingEnhancedProtectionItems {
  _safeBrowsingEnhancedProtectionItems = safeBrowsingEnhancedProtectionItems;
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model
      addSectionWithIdentifier:SectionIdentifierSafeBrowsingEnhancedProtection];
  for (TableViewItem* item in self.safeBrowsingEnhancedProtectionItems) {
    [model addItem:item
        toSectionWithIdentifier:
            SectionIdentifierSafeBrowsingEnhancedProtection];
  }
}

#pragma mark - UIViewController

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.presentationDelegate
        safeBrowsingEnhancedProtectionViewControllerDidRemove:self];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(base::UserMetricsAction(
      "IOSSafeBrowsingEnhancedProtectionSettingsCloseWithSwipe"));
}

#pragma mark - UITableViewDelegate

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  // None of the items in this page should be allowed to be highlighted. This
  // also removes the ability to select a row since highlighting comes before
  // selecting a row.
  return NO;
}

@end
