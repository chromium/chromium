// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/core/showcase_view_controller.h"

#include "base/logging.h"
#import "ios/showcase/common/coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace showcase {

NSString* const kClassForDisplayKey = @"classForDisplay";
NSString* const kClassForInstantiationKey = @"classForInstantiation";
NSString* const kUseCaseKey = @"useCase";

}  // namespace showcase

@interface ShowcaseViewController ()<UITableViewDataSource,
                                     UITableViewDelegate,
                                     UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Full data set displayed when tableView is not filtered.
@property(nonatomic, strong) NSArray<showcase::ModelRow*>* allRows;

// Displayed rows in tableView.
@property(nonatomic, strong) NSArray<showcase::ModelRow*>* displayedRows;

// Selected coordinator.
@property(nonatomic, strong) id<Coordinator> activeCoordinator;

@end

@implementation ShowcaseViewController
@synthesize searchController = _searchController;
@synthesize allRows = _allRows;
@synthesize displayedRows = _displayedRows;
@synthesize activeCoordinator = _activeCoordinator;

- (instancetype)initWithRows:(NSArray<showcase::ModelRow*>*)rows {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    self.allRows = [rows copy];
    // Default to displaying all rows.
    self.displayedRows = self.allRows;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = @"SC";
  self.tableView.tableFooterView = [[UIView alloc] init];
  self.tableView.rowHeight = 70.0;
  self.tableView.accessibilityIdentifier = @"showcase_home_collection";

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.tableView.tableHeaderView = self.searchController.searchBar;
  self.navigationController.navigationBar.translucent = NO;

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.navigationController.hidesBarsOnSwipe = NO;

  // Resets the current coordinator whenever the navigation controller pops
  // back to this view controller.
  self.activeCoordinator = nil;
}

#pragma mark - Table view data source

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return self.displayedRows.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:@"cell"];
    cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  }
  showcase::ModelRow* row = self.displayedRows[indexPath.row];
  cell.textLabel.text = row[showcase::kClassForDisplayKey];
  cell.detailTextLabel.text = row[showcase::kUseCaseKey];
  return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  showcase::ModelRow* row = self.displayedRows[indexPath.row];
  Class classForInstantiation =
      NSClassFromString(row[showcase::kClassForInstantiationKey]);
  if ([classForInstantiation isSubclassOfClass:[UIViewController class]]) {
    UIViewController* viewController = [[classForInstantiation alloc] init];
    viewController.title = row[showcase::kUseCaseKey];
    [self.navigationController pushViewController:viewController animated:YES];
  } else if ([classForInstantiation
                 conformsToProtocol:@protocol(Coordinator)]) {
    self.activeCoordinator = [[classForInstantiation alloc] init];
    self.activeCoordinator.baseViewController = self.navigationController;
    [self.activeCoordinator start];
  } else {
    NOTREACHED();
  }
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  [self filterContentForSearchText:searchController.searchBar.text];
}

#pragma mark - Private

// Filters |allRows| for |searchText| and reloads the tableView with
// animation. If |searchText| is empty, tableView will be loaded with |allRows|.
- (void)filterContentForSearchText:(NSString*)searchText {
  NSArray<showcase::ModelRow*>* newFilteredRows = self.allRows;

  if (![self.searchController.searchBar.text isEqualToString:@""]) {
    // The search is case-insensitive and searches both displayed texts.
    NSIndexSet* matchingRows =
        [self.allRows indexesOfObjectsPassingTest:^BOOL(
                          showcase::ModelRow* row, NSUInteger idx, BOOL* stop) {
          return [row[showcase::kClassForDisplayKey]
                     localizedCaseInsensitiveContainsString:searchText] ||
                 [row[showcase::kUseCaseKey]
                     localizedCaseInsensitiveContainsString:searchText];
        }];
    newFilteredRows = [self.allRows objectsAtIndexes:matchingRows];
  }

  NSArray<NSIndexPath*>* indexPathsToInsert =
      [self indexPathsToInsertFromArray:self.displayedRows
                                toArray:newFilteredRows
                       indexPathSection:0];
  NSArray<NSIndexPath*>* indexPathsToDelete =
      [self indexPathsToDeleteFromArray:self.displayedRows
                                toArray:newFilteredRows
                       indexPathSection:0];

  [self.tableView beginUpdates];
  [self.tableView insertRowsAtIndexPaths:indexPathsToInsert
                        withRowAnimation:UITableViewRowAnimationFade];
  [self.tableView deleteRowsAtIndexPaths:indexPathsToDelete
                        withRowAnimation:UITableViewRowAnimationFade];
  self.displayedRows = newFilteredRows;
  [self.tableView endUpdates];
}

// Returns indexPaths that need to be inserted into the tableView.
- (NSArray<NSIndexPath*>*)indexPathsToInsertFromArray:(NSArray*)fromArray
                                              toArray:(NSArray*)toArray
                                     indexPathSection:(NSUInteger)section {
  NSMutableArray<NSIndexPath*>* indexPathsToInsert =
      [[NSMutableArray alloc] init];
  for (NSUInteger row = 0; row < toArray.count; row++) {
    if (![fromArray containsObject:toArray[row]]) {
      [indexPathsToInsert
          addObject:[NSIndexPath indexPathForRow:row inSection:section]];
    }
  }
  return indexPathsToInsert;
}

// Returns indexPaths that need to be deleted from the tableView.
- (NSArray<NSIndexPath*>*)indexPathsToDeleteFromArray:(NSArray*)fromArray
                                              toArray:(NSArray*)toArray
                                     indexPathSection:(NSUInteger)section {
  NSMutableArray<NSIndexPath*>* indexPathsToDelete =
      [[NSMutableArray alloc] init];
  for (NSUInteger row = 0; row < fromArray.count; row++) {
    if (![toArray containsObject:fromArray[row]]) {
      [indexPathsToDelete
          addObject:[NSIndexPath indexPathForRow:row inSection:section]];
    }
  }
  return indexPathsToDelete;
}

@end
