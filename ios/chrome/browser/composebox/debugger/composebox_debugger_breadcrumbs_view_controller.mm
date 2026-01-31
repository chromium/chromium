// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/debugger/composebox_debugger_breadcrumbs_view_controller.h"

@interface ComposeboxDebuggerBreadcrumbsViewController () <
    UITableViewDataSource>

@end

@implementation ComposeboxDebuggerBreadcrumbsViewController {
  NSArray<ComposeboxDebuggerEvent*>* _breadcrumbs;
}

- (instancetype)initWithEvents:(NSArray<ComposeboxDebuggerEvent*>*)breadcrumbs {
  self = [super init];
  if (self) {
    _breadcrumbs = [[breadcrumbs reverseObjectEnumerator] allObjects];
  }

  return self;
}

- (void)viewDidLoad {
  self.tableView.dataSource = self;

  UILabel* headerLabel = [[UILabel alloc]
      initWithFrame:CGRectMake(0, 0, self.view.frame.size.width, 50)];
  headerLabel.text = @"Breadcrumbs logs";
  headerLabel.textAlignment = NSTextAlignmentCenter;
  headerLabel.font = [UIFont boldSystemFontOfSize:18];
  self.tableView.tableHeaderView = headerLabel;
}

- (void)dismissModal {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _breadcrumbs.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"Cell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:@"Cell"];
  }

  cell.textLabel.text = _breadcrumbs[indexPath.item].eventDescription;
  cell.detailTextLabel.text = _breadcrumbs[indexPath.item].eventMetadata;

  return cell;
}

@end
