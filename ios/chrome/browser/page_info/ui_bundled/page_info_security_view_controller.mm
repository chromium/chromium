// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/ui_bundled/page_info_security_view_controller.h"

#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_constants.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_presentation_commands.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/page_info_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/colorful_symbol_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

enum SectionIdentifier { SectionIdentifierSecurityContent };

enum ItemIdentifier {
  ItemIdentifierSecurityHeader,
  ItemIdentifierLearnMoreRow
};

}  // namespace

@implementation PageInfoSecurityViewController {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
  PageInfoSiteSecurityDescription* _pageInfoSecurityDescription;
}

#pragma mark - UIViewController

- (instancetype)initWithSiteSecurityDescription:
    (PageInfoSiteSecurityDescription*)siteSecurityDescription {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _pageInfoSecurityDescription = siteSecurityDescription;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  CHECK(!_pageInfoSecurityDescription.isEmpty);

  self.title = l10n_util::GetNSString(IDS_IOS_PAGE_INFO_SECURITY);
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.prompt = _pageInfoSecurityDescription.siteURL;
  self.tableView.accessibilityIdentifier =
      kPageInfoSecurityViewAccessibilityIdentifier;
  self.navigationController.navigationBar.accessibilityIdentifier =
      kPageInfoSecurityViewNavigationBarAccessibilityIdentifier;

  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self.pageInfoCommandsHandler
                           action:@selector(hidePageInfo)];
  self.navigationItem.rightBarButtonItem = dismissButton;

  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kPageInfoTableViewSeparatorInset, 0, 0);
  self.tableView.tableHeaderView = [[UIView alloc]
      initWithFrame:CGRectMake(0, 0, 0, kTableViewFirstHeaderHeight)];

  [self loadModel];
}

- (void)loadModel {
  __weak __typeof(self) weakSelf = self;
  _dataSource = [[UITableViewDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          NSNumber* itemIdentifier) {
             return
                 [weakSelf cellForTableView:tableView
                                  indexPath:indexPath
                             itemIdentifier:static_cast<ItemIdentifier>(
                                                itemIdentifier.integerValue)];
           }];

  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];

  NSDiffableDataSourceSnapshot* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot
      appendSectionsWithIdentifiers:@[ @(SectionIdentifierSecurityContent) ]];
  [snapshot appendItemsWithIdentifiers:@[
    @(ItemIdentifierSecurityHeader), @(ItemIdentifierLearnMoreRow)
  ]
             intoSectionWithIdentifier:@(SectionIdentifierSecurityContent)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (NSIndexPath*)tableView:(UITableView*)tableView
    willSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemIdentifier itemType = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);

  switch (itemType) {
    case ItemIdentifierSecurityHeader:
      return nil;
    default:
      return indexPath;
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  ItemIdentifier itemType = static_cast<ItemIdentifier>(
      [_dataSource itemIdentifierForIndexPath:indexPath].integerValue);
  switch (itemType) {
    case ItemIdentifierLearnMoreRow:
      [self.pageInfoPresentationHandler showSecurityHelpPage];
      break;
    default:
      break;
  }
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction(kMobileKeyCommandClose));
  [self.pageInfoCommandsHandler hidePageInfo];
}

#pragma mark - Private

// Returns a cell.
- (UITableViewCell*)cellForTableView:(UITableView*)tableView
                           indexPath:(NSIndexPath*)indexPath
                      itemIdentifier:(ItemIdentifier)itemIdentifier {
  switch (itemIdentifier) {
    case ItemIdentifierSecurityHeader: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title = _pageInfoSecurityDescription.securityStatus;
      configuration.subtitle = _pageInfoSecurityDescription.message;

      ColorfulSymbolContentConfiguration* symbolConfiguration =
          [[ColorfulSymbolContentConfiguration alloc] init];
      symbolConfiguration.symbolImage = _pageInfoSecurityDescription.iconImage;
      symbolConfiguration.symbolTintColor = UIColor.whiteColor;
      symbolConfiguration.symbolBackgroundColor =
          _pageInfoSecurityDescription.iconBackgroundColor;

      configuration.leadingConfiguration = symbolConfiguration;

      UITableViewCell* securityHeaderCell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      securityHeaderCell.contentConfiguration = configuration;

      return securityHeaderCell;
    }
    case ItemIdentifierLearnMoreRow: {
      TableViewCellContentConfiguration* configuration =
          [[TableViewCellContentConfiguration alloc] init];
      configuration.title = l10n_util::GetNSString(IDS_LEARN_MORE);
      configuration.titleColor = [UIColor colorNamed:kBlueColor];

      UITableViewCell* learnMoreCell =
          [TableViewCellContentConfiguration dequeueTableViewCell:tableView];
      learnMoreCell.contentConfiguration = configuration;
      learnMoreCell.isAccessibilityElement = YES;
      learnMoreCell.accessibilityTraits = UIAccessibilityTraitButton;

      return learnMoreCell;
    }
    default:
      break;
  }
}

@end
