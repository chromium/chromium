// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_view_controller.h"

#import "ios/chrome/browser/level_up/coordinator/level_up_category.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_stat.h"
#import "ios/chrome/browser/level_up/coordinator/level_up_task.h"
#import "ios/chrome/browser/level_up/ui/level_up_progress_view.h"
#import "ios/chrome/browser/level_up/ui/level_up_stat_view.h"
#import "ios/chrome/browser/level_up/ui/level_up_task_collection_view.h"
#import "ios/chrome/browser/level_up/ui/level_up_welcome_header_view.h"
#import "ios/chrome/browser/shared/public/commands/level_up_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Identifier for the welcome profile section and cell.
NSString* const kWelcomeSectionIdentifier = @"WelcomeSection";
// Identifier for the tasks progress section and cell.
NSString* const kProgressSectionIdentifier = @"ProgressSection";
// Identifier for the stats card section and cell.
NSString* const kStatSectionIdentifier = @"StatSection";
// Identifier for the checklist tasks section and cell.
NSString* const kTasksSectionIdentifier = @"TasksSection";

// Item identifier for the welcome card cell.
NSString* const kWelcomeItemIdentifier = @"WelcomeHeaderItem";
// Item identifier for the progress card cell.
NSString* const kProgressItemIdentifier = @"ProgressCardItem";
// Item identifier for the tasks checklist table cell.
NSString* const kTasksItemIdentifier = @"TasksTableCardItem";

// Layout spacing.
const CGFloat kLayoutSpacing = 16.0;
// Height of the welcome card cell.
const CGFloat kWelcomeCellHeight = 56.0;
// Height of the progress card cell.
const CGFloat kProgressCellHeight = 150.0;
// Width ratio for stats card group.
const CGFloat kStatCardWidthRatio = 0.88;
// Height of the stats card cell.
const CGFloat kStatCardHeight = 96.0;
// Height of the tasks card cell.
const CGFloat kTasksCellHeight = 350.0;
}  // namespace

@interface LevelUpViewController () <LevelUpTaskCollectionViewDelegate,
                                     UICollectionViewDelegate>
@end

@implementation LevelUpViewController {
  // The collection view.
  UICollectionView* _collectionView;
  // The list of tasks.
  NSArray<LevelUpTask*>* _tasks;
  // The active level.
  NSInteger _level;
  // User's full name.
  NSString* _userFullName;
  // User's avatar image.
  UIImage* _userAvatar;

  // The list of stats.
  NSArray<LevelUpStat*>* _stats;
  // The diffable data source.
  UICollectionViewDiffableDataSource<NSString*, NSString*>* _diffableDataSource;
}

@synthesize delegate = _delegate;

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setupNavigationItems];
  [self setupContentView];
}

- (void)setDelegate:(id<LevelUpViewControllerDelegate>)delegate {
  _delegate = delegate;
}

#pragma mark - LevelUpConsumer

- (void)setLevel:(NSInteger)level tasksForLevel:(NSArray<LevelUpTask*>*)tasks {
  _level = level;
  _tasks = [tasks copy];
}

- (void)setStats:(NSArray<LevelUpStat*>*)stats {
  _stats = [stats copy];
}

#pragma mark - LevelUpProfileConsumer

- (void)setUserFullName:(NSString*)userFullName
             userAvatar:(UIImage*)userAvatar {
  _userFullName = userFullName;
  _userAvatar = userAvatar;
}

#pragma mark - LevelUpTaskCollectionViewDelegate

- (void)didTapSeeAllTasks:(UICollectionViewCell*)cell {
  [self.delegate didTapSeeAllTasks:self];
}

#pragma mark - Private

// Configures the welcome header cell.
- (void)configureWelcomeHeaderCell:(LevelUpWelcomeHeaderView*)cell {
  cell.userAvatar = _userAvatar;
  cell.userFullName = _userFullName;
}

// Configures the progress cell.
- (void)configureProgressCell:(LevelUpProgressView*)cell {
  [cell setLevel:_level tasksForLevel:_tasks];
}

// Configures the tasks checklist cell.
- (void)configureTasksCell:(LevelUpTaskCollectionView*)cell {
  cell.headerTitle = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_YOUR_TASKS);
  cell.showsSeeAllButton = YES;
  cell.delegate = self;
  [cell setLevel:_level tasksForLevel:_tasks];
}

// Configures the stat card cell for a given stat item identifier.
- (void)configureStatCell:(LevelUpStatView*)cell
           itemIdentifier:(NSString*)itemIdentifier {
  for (LevelUpStat* stat in _stats) {
    NSString* statIdentifier =
        [NSString stringWithFormat:@"StatCardItem_%d", stat.type];
    if ([statIdentifier isEqualToString:itemIdentifier]) {
      [cell setStatTitle:stat.title subtitle:stat.subtitle image:stat.image];
      break;
    }
  }
}

// Sets up the navigation bar buttons and titles.
- (void)setupNavigationItems {
  self.view.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  self.title = l10n_util::GetNSString(IDS_IOS_TOOLS_MENU_LEVEL_UP);

  UIButton* menuButton = [UIButton buttonWithType:UIButtonTypeSystem];
  menuButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  menuButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [menuButton setImage:DefaultSymbolTemplateWithPointSize(
                           kEllipsisSymbol, kSymbolAccessoryPointSize)
              forState:UIControlStateNormal];
  menuButton.menu = [UIMenu menuWithTitle:@"" children:@[]];
  menuButton.showsMenuAsPrimaryAction = YES;
  self.navigationItem.leftBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:menuButton];

  UIButton* dismissButton = [UIButton buttonWithType:UIButtonTypeSystem];
  dismissButton.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  dismissButton.tintColor = [UIColor colorNamed:kTextPrimaryColor];
  [dismissButton setImage:DefaultSymbolTemplateWithPointSize(
                              kXMarkSymbol, kSymbolAccessoryPointSize)
                 forState:UIControlStateNormal];
  [dismissButton addTarget:self
                    action:@selector(dismiss)
          forControlEvents:UIControlEventTouchUpInside];
  self.navigationItem.rightBarButtonItem =
      [[UIBarButtonItem alloc] initWithCustomView:dismissButton];
}

// Sets up the collection view layout.
- (void)setupContentView {
  UICollectionViewCompositionalLayoutConfiguration* config =
      [[UICollectionViewCompositionalLayoutConfiguration alloc] init];
  config.interSectionSpacing = kLayoutSpacing;

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
  _collectionView.alwaysBounceVertical = YES;
  _collectionView.delegate = self;
  _collectionView.allowsSelection = NO;

  [self.view addSubview:_collectionView];
  AddSameConstraints(_collectionView, self.view);

  UICollectionViewCellRegistration* welcomeRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[LevelUpWelcomeHeaderView class]
               configurationHandler:^(LevelUpWelcomeHeaderView* cell,
                                      NSIndexPath* indexPath,
                                      NSString* itemIdentifier) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (!strongSelf) {
                   return;
                 }
                 [strongSelf configureWelcomeHeaderCell:cell];
               }];

  UICollectionViewCellRegistration* progressRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[LevelUpProgressView class]
               configurationHandler:^(LevelUpProgressView* cell,
                                      NSIndexPath* indexPath,
                                      NSString* itemIdentifier) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (!strongSelf) {
                   return;
                 }
                 [strongSelf configureProgressCell:cell];
               }];

  UICollectionViewCellRegistration* tasksRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[LevelUpTaskCollectionView class]
               configurationHandler:^(LevelUpTaskCollectionView* cell,
                                      NSIndexPath* indexPath,
                                      NSString* itemIdentifier) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (!strongSelf) {
                   return;
                 }
                 [strongSelf configureTasksCell:cell];
               }];

  UICollectionViewCellRegistration* statRegistration =
      [UICollectionViewCellRegistration
          registrationWithCellClass:[LevelUpStatView class]
               configurationHandler:^(LevelUpStatView* cell,
                                      NSIndexPath* indexPath,
                                      NSString* itemIdentifier) {
                 __strong __typeof(weakSelf) strongSelf = weakSelf;
                 if (!strongSelf) {
                   return;
                 }
                 [strongSelf configureStatCell:cell
                                itemIdentifier:itemIdentifier];
               }];

  _diffableDataSource = [[UICollectionViewDiffableDataSource alloc]
      initWithCollectionView:_collectionView
                cellProvider:^UICollectionViewCell*(
                    UICollectionView* collectionView, NSIndexPath* indexPath,
                    NSString* itemIdentifier) {
                  __strong __typeof(weakSelf) strongSelf = weakSelf;
                  if (!strongSelf) {
                    return nil;
                  }
                  NSString* sectionIdentifier =
                      [strongSelf->_diffableDataSource.snapshot
                          sectionIdentifierForSectionContainingItemIdentifier:
                              itemIdentifier];

                  if ([sectionIdentifier
                          isEqualToString:kWelcomeSectionIdentifier]) {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            welcomeRegistration
                                                         forIndexPath:indexPath
                                                                 item:
                                                                     itemIdentifier];
                  }
                  if ([sectionIdentifier
                          isEqualToString:kProgressSectionIdentifier]) {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            progressRegistration
                                                         forIndexPath:indexPath
                                                                 item:
                                                                     itemIdentifier];
                  }
                  if ([sectionIdentifier
                          isEqualToString:kStatSectionIdentifier]) {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            statRegistration
                                                         forIndexPath:indexPath
                                                                 item:
                                                                     itemIdentifier];
                  }
                  if ([sectionIdentifier
                          isEqualToString:kTasksSectionIdentifier]) {
                    return [collectionView
                        dequeueConfiguredReusableCellWithRegistration:
                            tasksRegistration
                                                         forIndexPath:indexPath
                                                                 item:
                                                                     itemIdentifier];
                  }
                  return [[UICollectionViewCell alloc] init];
                }];

  [self applyDataSnapshotAnimated:NO];
}

// Rebuilds and applies the collection view diffable snapshot.
- (void)applyDataSnapshotAnimated:(BOOL)animated {
  NSDiffableDataSourceSnapshot<NSString*, NSString*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[
    kWelcomeSectionIdentifier, kProgressSectionIdentifier,
    kStatSectionIdentifier, kTasksSectionIdentifier
  ]];

  [snapshot appendItemsWithIdentifiers:@[ kWelcomeItemIdentifier ]
             intoSectionWithIdentifier:kWelcomeSectionIdentifier];

  [snapshot appendItemsWithIdentifiers:@[ kProgressItemIdentifier ]
             intoSectionWithIdentifier:kProgressSectionIdentifier];

  NSMutableArray<NSString*>* statIdentifiers = [[NSMutableArray alloc] init];
  for (LevelUpStat* stat in _stats) {
    NSString* statIdentifier =
        [NSString stringWithFormat:@"StatCardItem_%d", stat.type];
    [statIdentifiers addObject:statIdentifier];
  }
  [snapshot appendItemsWithIdentifiers:statIdentifiers
             intoSectionWithIdentifier:kStatSectionIdentifier];

  [snapshot appendItemsWithIdentifiers:@[ kTasksItemIdentifier ]
             intoSectionWithIdentifier:kTasksSectionIdentifier];

  [_diffableDataSource applySnapshot:snapshot animatingDifferences:animated];
}

// Returns the collection layout section configuration for a given index path.
- (NSCollectionLayoutSection*)layoutSectionForIndex:(NSInteger)sectionIndex {
  NSString* sectionIdentifier = [_diffableDataSource.snapshot.sectionIdentifiers
      objectAtIndex:sectionIndex];

  NSDirectionalEdgeInsets standardInsets = NSDirectionalEdgeInsetsMake(
      kLayoutSpacing, kLayoutSpacing, 0, kLayoutSpacing);

  if ([sectionIdentifier isEqualToString:kWelcomeSectionIdentifier]) {
    return [self
        flatSectionWithHeightDimension:
            [NSCollectionLayoutDimension estimatedDimension:kWelcomeCellHeight]
                         contentInsets:standardInsets];
  }
  if ([sectionIdentifier isEqualToString:kProgressSectionIdentifier]) {
    return [self
        flatSectionWithHeightDimension:
            [NSCollectionLayoutDimension estimatedDimension:kProgressCellHeight]
                         contentInsets:standardInsets];
  }
  if ([sectionIdentifier isEqualToString:kStatSectionIdentifier]) {
    NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:1.0]
               heightDimension:[NSCollectionLayoutDimension
                                   fractionalHeightDimension:1.0]];
    NSCollectionLayoutItem* item =
        [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

    NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
        sizeWithWidthDimension:[NSCollectionLayoutDimension
                                   fractionalWidthDimension:kStatCardWidthRatio]
               heightDimension:[NSCollectionLayoutDimension
                                   absoluteDimension:kStatCardHeight]];
    NSCollectionLayoutGroup* group =
        [NSCollectionLayoutGroup horizontalGroupWithLayoutSize:groupSize
                                                      subitems:@[ item ]];

    NSCollectionLayoutSection* section =
        [NSCollectionLayoutSection sectionWithGroup:group];
    section.orthogonalScrollingBehavior =
        UICollectionLayoutSectionOrthogonalScrollingBehaviorGroupPaging;
    section.interGroupSpacing = kLayoutSpacing;
    section.contentInsets = standardInsets;
    return section;
  }
  if ([sectionIdentifier isEqualToString:kTasksSectionIdentifier]) {
    NSDirectionalEdgeInsets listInsets = NSDirectionalEdgeInsetsMake(
        kLayoutSpacing, kLayoutSpacing, kLayoutSpacing, kLayoutSpacing);
    return [self
        flatSectionWithHeightDimension:[NSCollectionLayoutDimension
                                           estimatedDimension:kTasksCellHeight]
                         contentInsets:listInsets];
  }
  return nil;
}

// Returns a vertical section with the given height.
- (NSCollectionLayoutSection*)
    flatSectionWithHeightDimension:(NSCollectionLayoutDimension*)heightDimension
                     contentInsets:(NSDirectionalEdgeInsets)insets {
  NSCollectionLayoutSize* itemSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:heightDimension];
  NSCollectionLayoutItem* item =
      [NSCollectionLayoutItem itemWithLayoutSize:itemSize];

  NSCollectionLayoutSize* groupSize = [NSCollectionLayoutSize
      sizeWithWidthDimension:[NSCollectionLayoutDimension
                                 fractionalWidthDimension:1.0]
             heightDimension:heightDimension];
  NSCollectionLayoutGroup* group =
      [NSCollectionLayoutGroup verticalGroupWithLayoutSize:groupSize
                                                  subitems:@[ item ]];

  NSCollectionLayoutSection* section =
      [NSCollectionLayoutSection sectionWithGroup:group];
  section.contentInsets = insets;
  return section;
}

// Handles dismissing the view.
- (void)dismiss {
  [self.handler dismissLevelUp];
}

@end
