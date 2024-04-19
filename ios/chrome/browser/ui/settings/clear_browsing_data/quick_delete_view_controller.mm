// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/quick_delete_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"

@implementation QuickDeleteViewController {
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
}

#pragma mark - UIViewController

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/335387869): Add contents to the page.
}

@end
