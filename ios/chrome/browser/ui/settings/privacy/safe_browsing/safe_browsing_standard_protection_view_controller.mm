// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_view_controller.h"

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ItemArray = NSArray<TableViewItem*>*;

namespace {
// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSafeBrowsingStandardProtection = kSectionIdentifierEnumZero,
};
}  // namespace

@interface SafeBrowsingStandardProtectionViewController ()

@property(nonatomic, strong) ItemArray safeBrowsingStandardProtectionItems;

@end

@implementation SafeBrowsingStandardProtectionViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier =
      kSafeBrowsingStandardProtectionTableViewId;
  self.title = l10n_util::GetNSString(
      IDS_IOS_PRIVACY_SAFE_BROWSING_STANDARD_PROTECTION_TITLE);
  [self loadModel];
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  // TODO(crbug.com/1307428): Add UMA recording.
}

- (void)reportBackUserAction {
  // TODO(crbug.com/1307428): Add UMA recording.
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

- (void)reloadSection {
  if (!self.tableViewModel) {
    // No need to reload since the model has not been loaded yet.
    return;
  }
  TableViewModel* model = self.tableViewModel;
  NSUInteger sectionIndex =
      [model sectionForSectionIdentifier:
                 SectionIdentifierSafeBrowsingStandardProtection];
  NSIndexSet* sections = [NSIndexSet indexSetWithIndex:sectionIndex];
  [self.tableView reloadSections:sections
                withRowAnimation:UITableViewRowAnimationNone];
}

- (void)setSafeBrowsingStandardProtectionItems:
    (ItemArray)safeBrowsingStandardProtectionItems {
  _safeBrowsingStandardProtectionItems = safeBrowsingStandardProtectionItems;
}

#pragma mark - CollectionViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;
  [model
      addSectionWithIdentifier:SectionIdentifierSafeBrowsingStandardProtection];
  for (TableViewItem* item in self.safeBrowsingStandardProtectionItems) {
    self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
    self.styler.cellBackgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    [model addItem:item
        toSectionWithIdentifier:
            SectionIdentifierSafeBrowsingStandardProtection];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  // TODO(crbug.com/1307428): Add UMA recording.
}

@end
