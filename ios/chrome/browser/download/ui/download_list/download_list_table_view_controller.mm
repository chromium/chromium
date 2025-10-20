// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/download/model/download_filter_util.h"
#import "ios/chrome/browser/download/model/external_app_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_action_delegate.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_consumer.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_group_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_grouping_util.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_item.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_mutator.h"
#import "ios/chrome/browser/download/ui/download_list/download_list_table_view_header.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_illustrated_empty_view.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
/// Size for the file icon image in the download list cells.
constexpr CGFloat kFileIconImageSize = 44.0;
/// Constants for cancel button styling.
static const CGFloat kCancelButtonIconSize = 30;

NSString* const kCancelButtonPrimaryActionIdentifier =
    @"kCancelButtonPrimaryActionIdentifier";

// Helper function to create the attributed string with a link.
NSAttributedString* GetAttributedString(NSString* message) {
  NSDictionary* textAttributes =
      [TableViewIllustratedEmptyView defaultTextAttributesForSubtitle];
  NSDictionary* linkAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleNone),
    NSLinkAttributeName : GetFilesAppUrl().absoluteString,
  };

  return AttributedStringFromStringWithLink(message, textAttributes,
                                            linkAttributes);
}

}  // namespace

// Diffable data source types using DownloadListGroupItem for section
// identifiers.
typedef UITableViewDiffableDataSource<DownloadListGroupItem*, DownloadListItem*>
    DownloadListDiffableDataSource;
typedef NSDiffableDataSourceSnapshot<DownloadListGroupItem*, DownloadListItem*>
    DownloadListSnapshot;

@interface DownloadListTableViewController () <
    TableViewIllustratedEmptyViewDelegate>
@property(nonatomic, strong) DownloadListTableViewHeader* filterHeaderView;
@end

@implementation DownloadListTableViewController {
  DownloadListDiffableDataSource* _diffableDataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_DOWNLOAD_LIST_TITLE);

  // Configure navigation bar.
  self.navigationController.navigationBar.prefersLargeTitles = YES;
  UIBarButtonItem* closeButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeButtonTapped)];
  self.navigationItem.rightBarButtonItem = closeButton;

  // Configure table view.
  [TableViewCellContentConfiguration registerCellForTableView:self.tableView];
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  // Setup filter header view
  [self setupFilterHeaderView];

  [self configureDiffableDataSource];

  // Load download records.
  [self.mutator loadDownloadRecords];
}

#pragma mark - Private

- (void)setupFilterHeaderView {
  self.filterHeaderView = [[DownloadListTableViewHeader alloc] init];
  self.filterHeaderView.mutator = self.mutator;
  [self updateTableHeaderViewFrame];
}

- (void)updateTableHeaderViewFrame {
  if (!self.filterHeaderView) {
    return;
  }

  [self.filterHeaderView setNeedsLayout];
  [self.filterHeaderView layoutIfNeeded];

  CGFloat width = self.tableView.bounds.size.width;
  CGSize fittingSize = [self.filterHeaderView
      systemLayoutSizeFittingSize:CGSizeMake(
                                      width,
                                      UILayoutFittingCompressedSize.height)];

  CGRect newFrame = CGRectMake(0, 0, width, fittingSize.height);
  if (!CGRectEqualToRect(self.filterHeaderView.frame, newFrame)) {
    self.filterHeaderView.frame = newFrame;
    // Reassign to trigger table view layout update
    if (self.tableView.tableHeaderView == self.filterHeaderView) {
      self.tableView.tableHeaderView = self.filterHeaderView;
    }
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  [self updateTableHeaderViewFrame];
}

#pragma mark - Mutator setter override

- (void)setMutator:(id<DownloadListMutator>)mutator {
  _mutator = mutator;
  // Update the filter view's mutator when it's set.
  if (self.filterHeaderView) {
    self.filterHeaderView.mutator = mutator;
  }
}

/// Dismisses the view controller when close button is tapped.
- (void)closeButtonTapped {
  [self.downloadListHandler hideDownloadList];
}

/// Configures the diffable data source for the table view.
- (void)configureDiffableDataSource {
  __weak __typeof(self) weakSelf = self;
  _diffableDataSource = [[DownloadListDiffableDataSource alloc]
      initWithTableView:self.tableView
           cellProvider:^UITableViewCell*(UITableView* tableView,
                                          NSIndexPath* indexPath,
                                          DownloadListItem* item) {
             return [weakSelf cellForItem:item atIndexPath:indexPath];
           }];
  self.tableView.dataSource = _diffableDataSource;
}

/// Creates and configures a cell for the given item at the specified index
/// path.
- (UITableViewCell*)cellForItem:(DownloadListItem*)item
                    atIndexPath:(NSIndexPath*)indexPath {
  ImageContentConfiguration* imageConfiguration =
      [[ImageContentConfiguration alloc] init];
  imageConfiguration.image = item.fileTypeIcon;
  imageConfiguration.imageSize =
      CGSizeMake(kFileIconImageSize, kFileIconImageSize);

  TableViewCellContentConfiguration* configuration =
      [[TableViewCellContentConfiguration alloc] init];
  configuration.title = item.fileName;
  configuration.subtitle = item.detailText;
  configuration.leadingConfiguration = imageConfiguration;

  UITableViewCell* cell =
      [TableViewCellContentConfiguration dequeueTableViewCell:self.tableView];
  cell.contentConfiguration = configuration;
  [self configureCancelButtonForCell:cell item:item];
  return cell;
}

/// Configures the cancel button accessory for the given cell and item.
- (void)configureCancelButtonForCell:(UITableViewCell*)cell
                                item:(DownloadListItem*)item {
  if (item.cancelable) {
    UIButton* cancelButton =
        base::apple::ObjCCast<UIButton>(cell.accessoryView);
    if (!cancelButton) {
      // Create and configure cancel button if it does not exist.
      cancelButton = SecondaryActionButton();
      cancelButton.translatesAutoresizingMaskIntoConstraints = YES;
      UIImage* cancelButtonImage =
          SymbolWithPalette(DefaultSymbolWithPointSize(kXMarkCircleFillSymbol,
                                                       kCancelButtonIconSize),
                            @[
                              [UIColor colorNamed:kGrey600Color],
                              [UIColor colorNamed:kGrey200Color],
                            ]);
      [cancelButton setImage:cancelButtonImage forState:UIControlStateNormal];
      cancelButton.frame =
          CGRectMake(0, 0, kCancelButtonIconSize, kCancelButtonIconSize);
      cancelButton.accessibilityLabel = l10n_util::GetNSString(
          IDS_IOS_DOWNLOAD_LIST_CANCEL_ACCESSIBILITY_LABEL);
      cell.accessoryView = cancelButton;
    }
    __weak __typeof(self) weakSelf = self;
    UIAction* primaryAction =
        [UIAction actionWithTitle:@""
                            image:nil
                       identifier:kCancelButtonPrimaryActionIdentifier
                          handler:^(__kindof UIAction* action) {
                            [weakSelf.mutator cancelDownloadItem:item];
                          }];
    [cancelButton addAction:primaryAction
           forControlEvents:UIControlEventTouchUpInside];
  } else {
    cell.accessoryView = nil;
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    performPrimaryActionForRowAtIndexPath:(NSIndexPath*)indexPath {
  // TODO(crbug.com/440222083): Implement download primary action handling.
}

- (UIContextMenuConfiguration*)tableView:(UITableView*)tableView
    contextMenuConfigurationForRowAtIndexPath:(NSIndexPath*)indexPath
                                        point:(CGPoint)point {
  DownloadListItem* item =
      [_diffableDataSource itemIdentifierForIndexPath:indexPath];

  // Downloads with no available actions do not support context menu.
  if (item.availableActions == DownloadListItemActionNone) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        return [weakSelf createMenuForDownloadItem:item];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

- (UIMenu*)createMenuForDownloadItem:(DownloadListItem*)item {
  NSMutableArray<UIMenuElement*>* actions = [[NSMutableArray alloc] init];
  __weak __typeof(self) weakSelf = self;
  DownloadListItemAction availableActions = item.availableActions;

  // Check if "Open in Files App" action is available.
  if (availableActions & DownloadListItemActionOpenInFiles) {
    UIAction* openInFilesAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(
                            IDS_IOS_OPEN_IN_FILES_APP_ACTION_TITLE)
                  image:DefaultSymbolWithPointSize(kOpenImageActionSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf.actionDelegate openDownloadInFiles:item];
                }];
    [actions addObject:openInFilesAction];
  }

  // Check if Delete action is available.
  if (availableActions & DownloadListItemActionDelete) {
    UIAction* deleteAction = [UIAction
        actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                  image:DefaultSymbolWithPointSize(kTrashSymbol,
                                                   kSymbolActionPointSize)
             identifier:nil
                handler:^(UIAction* action) {
                  [weakSelf.mutator deleteDownloadItem:item];
                }];
    deleteAction.attributes = UIMenuElementAttributesDestructive;
    [actions addObject:deleteAction];
  }

  return [UIMenu menuWithTitle:@"" children:actions];
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  // Get the section identifier (DownloadListGroupItem) from the data source
  // snapshot.
  DownloadListGroupItem* groupItem =
      [_diffableDataSource sectionIdentifierForIndex:section];

  TableViewTextHeaderFooterView* headerView =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
  [headerView setTitle:groupItem.title];

  return headerView;
}

#pragma mark - DownloadListConsumer

- (void)setDownloadListItems:(NSArray<DownloadListItem*>*)items {
  // Create a new snapshot with date-grouped sections.
  DownloadListSnapshot* snapshot = [[DownloadListSnapshot alloc] init];

  // Group items by date.
  NSArray<DownloadListGroupItem*>* groupItems =
      download_list_grouping_util::GroupDownloadItemsByDate(items);

  // Add sections and items.
  for (DownloadListGroupItem* groupItem in groupItems) {
    [snapshot appendSectionsWithIdentifiers:@[ groupItem ]];
    [snapshot appendItemsWithIdentifiers:groupItem.items
               intoSectionWithIdentifier:groupItem];
  }

  // Detect changes between old and new items.
  NSArray<DownloadListItem*>* oldItems =
      _diffableDataSource.snapshot.itemIdentifiers;
  if (oldItems.count > 0) {
    NSSet<DownloadListItem*>* oldItemsSet = [NSSet setWithArray:oldItems];
    NSMutableArray<DownloadListItem*>* changedItems =
        [[NSMutableArray alloc] init];

    for (DownloadListItem* newItem in items) {
      DownloadListItem* existingItem = [oldItemsSet member:newItem];
      if (existingItem && ![existingItem isEqualToItem:newItem]) {
        [changedItems addObject:newItem];
      }
    }

    if (changedItems.count > 0) {
      [snapshot reconfigureItemsWithIdentifiers:changedItems];
    }
  }

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:NO];
}

- (void)setLoadingState:(BOOL)loading {
}

- (void)setEmptyState:(BOOL)empty {
  if (empty) {
    // Empty downloads: show small title and empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeNever;
    if (!self.tableView.backgroundView) {
      UIImage* emptyImage = [UIImage imageNamed:@"download_list_empty"];
      TableViewIllustratedEmptyView* emptyView = [[TableViewIllustratedEmptyView
          alloc] initWithFrame:self.view.bounds
                         image:emptyImage
                         title:l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_TITLE)
            attributedSubtitle:GetAttributedString(l10n_util::GetNSString(
                                   IDS_IOS_DOWNLOAD_LIST_NO_ENTRIES_MESSAGE))];
      emptyView.delegate = self;
      self.tableView.backgroundView = emptyView;
    }
  } else {
    // Non-empty downloads: show large title initially and hide empty view.
    self.navigationItem.largeTitleDisplayMode =
        UINavigationItemLargeTitleDisplayModeAlways;
    self.tableView.backgroundView = nil;
  }
  if (self.filterHeaderView && self.filterHeaderView.isHidden == NO) {
    [self.filterHeaderView setAttributionTextShown:!empty];
  }
}

- (void)setDownloadListHeaderShown:(BOOL)shown {
  if (shown) {
    // Show the filter view if it's not already set.
    if (self.tableView.tableHeaderView != self.filterHeaderView) {
      self.tableView.tableHeaderView = self.filterHeaderView;
    }
  } else {
    // Hide the filter view by setting tableHeaderView to nil.
    self.tableView.tableHeaderView = nil;
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

/// Called before the presentation controller will dismiss.
- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  [self.downloadListHandler hideDownloadList];
}
#pragma mark - TableViewIllustratedEmptyViewDelegate

// Invoked when a link in `view`'s subtitle is tapped.
- (void)tableViewIllustratedEmptyView:(TableViewIllustratedEmptyView*)view
                   didTapSubtitleLink:(NSURL*)URL {
  if (!URL) {
    return;
  }
  [[UIApplication sharedApplication] openURL:URL
                                     options:@{}
                           completionHandler:nil];
}

@end
