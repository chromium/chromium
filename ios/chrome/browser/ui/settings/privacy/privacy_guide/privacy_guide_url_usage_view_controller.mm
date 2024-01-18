// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/elements/self_sizing_table_view.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_constants.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_guide/privacy_guide_url_usage_view_controller_presentation_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

NSString* const kURLUsageBannerName = @"url_usage_illustration";
const CGFloat kSwitchCellCornerRadius = 12;

enum SectionIdentifier {
  kSectionIdentifierSwitch,
};

enum ItemIdentifier {
  kItemIdentifierSwitch,
};

}  // namespace

@interface PrivacyGuideURLUsageViewController () <UITableViewDelegate>
@end

@implementation PrivacyGuideURLUsageViewController {
  SelfSizingTableView* _tableView;
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier = kPrivacyGuideURLUsageViewID;

  self.bannerName = kURLUsageBannerName;
  self.bannerSize = BannerImageSizeType::kShort;

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GUIDE_NEXT_BUTTON);

  self.subtitleBottomMargin = 0;

  [super viewDidLoad];

  [self setupTableView];
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    [self.presentationDelegate
        privacyGuideURLUsageViewControllerDidRemove:self];
  }
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return section == kSectionIdentifierSwitch ? 0
                                             : UITableViewAutomaticDimension;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  SectionIdentifier sectionIdentifier = static_cast<SectionIdentifier>(
      [_dataSource sectionIdentifierForIndex:section].integerValue);
  switch (sectionIdentifier) {
    case kSectionIdentifierSwitch:
      return nil;
  }
}

#pragma mark - Private

// Initializes a SelfSizingTableView, adds it to the view hierarchy and
// specifies its constraints.
- (void)setupTableView {
  _tableView =
      [[SelfSizingTableView alloc] initWithFrame:CGRectZero
                                           style:ChromeTableViewStyle()];
  _tableView.delegate = self;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.backgroundColor = [UIColor clearColor];
  _tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
  _tableView.separatorInset = UIEdgeInsetsZero;
  [_tableView setLayoutMargins:UIEdgeInsetsZero];
  _tableView.sectionHeaderTopPadding = 0;

  [self.specificContentView addSubview:_tableView];
  [NSLayoutConstraint activateConstraints:@[
    [_tableView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [_tableView.leadingAnchor
        constraintEqualToAnchor:self.specificContentView.leadingAnchor],
    [_tableView.trailingAnchor
        constraintEqualToAnchor:self.specificContentView.trailingAnchor],
    [_tableView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];
}

// Initializes the data source and populates the initial snapshot.
- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:_tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  RegisterTableViewCell<TableViewSwitchCell>(_tableView);

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(kSectionIdentifierSwitch) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kItemIdentifierSwitch) ]
             intoSectionWithIdentifier:@(kSectionIdentifierSwitch)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

// Returns the appropriate cell for the table view.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case kItemIdentifierSwitch: {
      TableViewSwitchCell* cell =
          DequeueTableViewCell<TableViewSwitchCell>(tableView);
      NSString* title = l10n_util::GetNSString(
          IDS_IOS_GOOGLE_SERVICES_SETTINGS_BETTER_SEARCH_AND_BROWSING_TEXT);
      // TODO(crbug.com/1509830): Initial state of the switch should be queried
      // from the mediator.
      [cell configureCellWithTitle:title subtitle:nil switchEnabled:YES on:YES];
      [cell setUseCustomSeparator:NO];
      cell.textLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
      cell.textLabel.numberOfLines = 0;
      cell.textLabel.adjustsFontForContentSizeCategory = YES;
      cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
      cell.layer.cornerRadius = kSwitchCellCornerRadius;
      cell.accessibilityIdentifier = kPrivacyGuideURLUsageSwitchID;
      return cell;
    }
  }
}

@end
