// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"


@implementation AtMemoryGranularFillViewController {
  // The data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, AtMemoryGranularFillItem*>*
      _dataSource;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsSelection = NO;
  [self loadModel];
}

- (void)setItems:(NSArray<AtMemoryGranularFillItem*>*)items {
  _items = [items copy];
  if (self.isViewLoaded) {
    [self loadModel];
  }
}

- (void)loadModel {
}

@end
