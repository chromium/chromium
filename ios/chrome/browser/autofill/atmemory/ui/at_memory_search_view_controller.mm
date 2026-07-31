// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

@interface AtMemorySearchViewController () <UISearchResultsUpdating>
@end

@implementation AtMemorySearchViewController {
  // Search controller for users to type a query for performing an AtMemory
  // search and filtering items.
  UISearchController* _searchController;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  _searchController.obscuresBackgroundDuringPresentation = NO;
  _searchController.searchResultsUpdater = self;

  self.definesPresentationContext = YES;
  self.navigationItem.searchController = _searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  // TODO(crbug.com/522338028): Update search cell
}

@end
