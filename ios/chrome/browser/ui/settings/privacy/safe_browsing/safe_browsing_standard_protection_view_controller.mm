// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/safe_browsing/safe_browsing_standard_protection_view_controller.h"

#import "ios/chrome/browser/ui/settings/cells/safe_browsing_header_item.h"
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

@interface SafeBrowsingStandardProtectionViewController ()

// All items for safe browsing standard protection view.
@property(nonatomic, strong) ItemArray safeBrowsingStandardProtectionItems;

// Header related to shield icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* shieldIconHeader;

// Header related to metric icon.
@property(nonatomic, strong) SafeBrowsingHeaderItem* metricIconHeader;

@end

@implementation SafeBrowsingStandardProtectionViewController

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
  [model addSectionWithIdentifier:SectionIdentifierHeaderShield];
  [model addSectionWithIdentifier:SectionIdentifierHeaderMetric];
  [model
      addSectionWithIdentifier:SectionIdentifierSafeBrowsingStandardProtection];

  [model setHeader:self.shieldIconHeader
      forSectionWithIdentifier:SectionIdentifierHeaderShield];
  [model setHeader:self.metricIconHeader
      forSectionWithIdentifier:SectionIdentifierHeaderMetric];

  for (TableViewItem* item in self.safeBrowsingStandardProtectionItems) {
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
