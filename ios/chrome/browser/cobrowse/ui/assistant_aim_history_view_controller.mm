// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// The top inset for the history collection view.
const CGFloat kCollectionViewTopInset = 8.0;

// The number of lines for the title label in the history cell.
const NSInteger kTitleNumberOfLines = 2;

// Identifier for the section in diffable data source.
NSString* const kHistorySectionIdentifier = @"kHistorySectionIdentifier";

}  // namespace

@interface AssistantAIMHistoryViewController () <UICollectionViewDelegate>
@end

@implementation AssistantAIMHistoryViewController {
  UICollectionView* _collectionView;
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _dataSource;
  std::vector<AssistantAIMHistoryItem> _items;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor clearColor];

  [self setUpCollectionView];
}

- (void)setUpCollectionView {
  UICollectionLayoutListConfiguration* config =
      [[UICollectionLayoutListConfiguration alloc]
          initWithAppearance:UICollectionLayoutListAppearanceInsetGrouped];
  config.backgroundColor = [UIColor clearColor];

  UICollectionViewCompositionalLayout* layout =
      [UICollectionViewCompositionalLayout layoutWithListConfiguration:config];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.delegate = self;

  [self.view addSubview:_collectionView];

  __weak __typeof(self) weakSelf = self;
  UICollectionViewCellRegistration* cellRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[UICollectionViewListCell class]
               configurationHandler:^(UICollectionViewListCell* cell,
                                      NSIndexPath* indexPath,
                                      NSString* taskId) {
                 [weakSelf configureListCell:cell atIndexPath:indexPath];
               }];

  _dataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    NSString* taskId) {
                  return [collectionView
                      dequeueConfiguredReusableCellWithRegistration:
                          cellRegistration
                                                       forIndexPath:indexPath
                                                               item:taskId];
                }];

  NSDirectionalEdgeInsets insets =
      NSDirectionalEdgeInsetsMake(kCollectionViewTopInset, 0, 0, 0);
  AddSameConstraintsWithInsets(_collectionView, self.view, insets);
}

- (void)configureListCell:(UICollectionViewListCell*)cell
              atIndexPath:(NSIndexPath*)indexPath {
  if (static_cast<size_t>(indexPath.row) >= _items.size()) {
    return;
  }
  const AssistantAIMHistoryItem& item = _items[indexPath.row];

  UIListContentConfiguration* content = [cell defaultContentConfiguration];
  content.text = base::SysUTF8ToNSString(item.title);
  content.textProperties.numberOfLines = kTitleNumberOfLines;
  content.textProperties.lineBreakMode = NSLineBreakByTruncatingTail;
  content.textProperties.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  content.textProperties.color = [UIColor colorNamed:kTextPrimaryColor];
  cell.contentConfiguration = content;

  UICellAccessoryDisclosureIndicator* disclosureIndicator =
      [[UICellAccessoryDisclosureIndicator alloc] init];
  cell.accessories = @[ disclosureIndicator ];
}

- (void)updateHistoryItems:(const std::vector<AssistantAIMHistoryItem>&)items {
  _items = items;
  [self applySnapshot];
}

- (void)applySnapshot {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kHistorySectionIdentifier ]];
  NSMutableArray<NSString*>* taskIds = [[NSMutableArray alloc] init];
  for (const AssistantAIMHistoryItem& item : _items) {
    [taskIds addObject:base::SysUTF8ToNSString(item.task_id)];
  }
  [snapshot appendItemsWithIdentifiers:taskIds];
  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - Actions

- (void)didTapDismiss {
  [self.delegate assistantAIMHistoryViewControllerDidTapDismiss:self];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [collectionView deselectItemAtIndexPath:indexPath animated:YES];
  if (static_cast<size_t>(indexPath.row) >= _items.size()) {
    return;
  }
  const AssistantAIMHistoryItem& item = _items[indexPath.row];
  [self.delegate
      assistantAIMHistoryViewController:self
                    didSelectTaskWithId:base::SysUTF8ToNSString(item.task_id)];
}

- (UIContextMenuConfiguration*)collectionView:(UICollectionView*)collectionView
    contextMenuConfigurationForItemAtIndexPath:(NSIndexPath*)indexPath
                                         point:(CGPoint)point {
  if (!IsAimHistoryThreadsManagementEnabled() ||
      static_cast<size_t>(indexPath.row) >= _items.size()) {
    return nil;
  }

  __weak __typeof(self) weakSelf = self;
  return [UIContextMenuConfiguration
      configurationWithIdentifier:nil
                  previewProvider:nil
                   actionProvider:^UIMenu*(
                       NSArray<UIMenuElement*>* suggestedActions) {
                     return [weakSelf contextMenuForIndexPath:indexPath];
                   }];
}

#pragma mark - Private

// Returns the context menu for the item at `indexPath`.
- (UIMenu*)contextMenuForIndexPath:(NSIndexPath*)indexPath {
  if (static_cast<size_t>(indexPath.row) >= _items.size()) {
    return nil;
  }

  UIAction* shareAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_SHARE_BUTTON_LABEL)
                image:SymbolWithPointSize(SymbolShare, kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action){
                  // TODO(crbug.com/545943169): Implement the action handler.
              }];

  UIAction* deleteAction = [UIAction
      actionWithTitle:l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE)
                image:SymbolWithPointSize(SymbolDeleteAction,
                                          kSymbolActionPointSize)
           identifier:nil
              handler:^(UIAction* action){
                  // TODO(crbug.com/545943169): Implement the action handler.
              }];
  deleteAction.attributes = UIMenuElementAttributesDestructive;

  return [UIMenu menuWithTitle:@"" children:@[ shareAction, deleteAction ]];
}

@end
