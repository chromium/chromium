// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_all_tasks_view_controller.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/ui/level_up_task_collection_view_cell.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Spacing for layout margins and collection view cells.
const CGFloat kLayoutSpacing = 16.0;
// Section identifier for all tasks view categories.
NSString* const kCategoriesSectionIdentifier = @"CategoriesSection";

}  // namespace

@interface LevelUpAllTasksViewController () <
    LevelUpTaskCollectionViewCellDelegate>
@end

@implementation LevelUpAllTasksViewController {
  // The collection view.
  UICollectionView* _collectionView;
  // The list of categories.
  NSMutableArray<LevelUpCategory*>* _categories;
  // The diffable data source.
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _diffableDataSource;
  // Set of category titles whose completed tasks section is expanded.
  NSMutableSet<NSString*>* _expandedCategories;
}
- (instancetype)init {
  self = [super init];
  if (self) {
    _categories = [[NSMutableArray alloc] init];
    _expandedCategories = [[NSMutableSet alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_ALL_TASKS_TITLE);

  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];

  __weak __typeof(self) weakSelf = self;
  UICollectionViewCompositionalLayout* layout =
      [[UICollectionViewCompositionalLayout alloc]
          initWithSectionProvider:^NSCollectionLayoutSection*(
              NSInteger sectionIndex,
              id<NSCollectionLayoutEnvironment> layoutEnvironment) {
            return [weakSelf layoutSectionForIndex:sectionIndex];
          }
                    configuration:config];

  _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                       collectionViewLayout:layout];
  _collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  _collectionView.backgroundColor = [UIColor clearColor];
  _collectionView.allowsSelection = NO;

  [self.view addSubview:_collectionView];
  AddSameConstraints(_collectionView, self.view);

  UICollectionViewCellRegistration* categoryCardRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[LevelUpTaskCollectionViewCell class]
               configurationHandler:^(LevelUpTaskCollectionViewCell* cell,
                                      NSIndexPath* indexPath,
                                      NSString* itemIdentifier) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (!strongSelf) {
                   return;
                 }
                 [strongSelf configureCategoryCardCell:cell
                                        itemIdentifier:itemIdentifier];
               }];

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    NSString* itemIdentifier) {
                  return [collectionView
                      dequeueConfiguredReusableCellWithRegistration:
                          categoryCardRegistration
                                                       forIndexPath:indexPath
                                                               item:
                                                                   itemIdentifier];
                }];

  [self applyDataSnapshotAnimated:NO];
}

#pragma mark - LevelUpConsumer

- (void)addCategoryCard:(LevelUpCategory*)category {
  [_categories addObject:category];
}

#pragma mark - Private

// Configures a checklist card cell for a given category item identifier.
- (void)configureCategoryCardCell:(LevelUpTaskCollectionViewCell*)cell
                   itemIdentifier:(NSString*)itemIdentifier {
  for (LevelUpCategory* category in _categories) {
    if ([category.title isEqualToString:itemIdentifier]) {
      cell.showsSeeAllButton = NO;
      cell.headerTitle = category.title;
      cell.delegate = self;

      BOOL isExpanded = [_expandedCategories containsObject:category.title];
      [cell setTasks:category.activeTasks
             completedTasks:category.completedTasks
          completedExpanded:isExpanded];
      break;
    }
  }
}

- (LevelUpCategory*)categoryWithTitle:(NSString*)title {
  for (LevelUpCategory* category in _categories) {
    if ([category.title isEqualToString:title]) {
      return category;
    }
  }
  return nil;
}

#pragma mark - LevelUpTaskCollectionViewCellDelegate

- (void)didTapSeeAllTasks:(UICollectionViewCell*)cell {
  // Do nothing.
}

- (void)taskCollectionViewCell:(LevelUpTaskCollectionViewCell*)cell
                    didTapTask:(LevelUpTask*)task {
  [self.delegate levelUpAllTasksViewController:self didTapTask:task];
}

- (void)taskCollectionViewDidTapCompletedHeader:(UICollectionViewCell*)cell {
  LevelUpTaskCollectionViewCell* taskCell =
      (LevelUpTaskCollectionViewCell*)cell;
  NSString* title = taskCell.headerTitle;

  BOOL isExpanded = ![_expandedCategories containsObject:title];
  if (isExpanded) {
    [_expandedCategories addObject:title];
  } else {
    [_expandedCategories removeObject:title];
  }

  LevelUpCategory* category = [self categoryWithTitle:title];
  [taskCell setTasks:category.activeTasks
         completedTasks:category.completedTasks
      completedExpanded:isExpanded];

  [_collectionView performBatchUpdates:nil completion:nil];
}

// Rebuilds and applies the collection view diffable snapshot.
- (void)applyDataSnapshotAnimated:(BOOL)animated {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[ kCategoriesSectionIdentifier ]];

  NSMutableArray<NSString*>* categoryTitles = [[NSMutableArray alloc] init];
  for (LevelUpCategory* category in _categories) {
    [categoryTitles addObject:category.title];
  }
  [snapshot appendItemsWithIdentifiers:categoryTitles
             intoSectionWithIdentifier:kCategoriesSectionIdentifier];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:animated];
}

// Returns the collection layout section configuration.
- (NSCollectionLayoutSection*)layoutSectionForIndex:(NSInteger)sectionIndex {
  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:280.0]];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:[NSCollectionLayoutDimension
                                 estimatedDimension:280.0]];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                  subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.interGroupSpacing = kLayoutSpacing;
  section.contentInsets = NSDirectionalEdgeInsetsMake(
      kLayoutSpacing, kLayoutSpacing, kLayoutSpacing, kLayoutSpacing);

  return section;
}

@end
