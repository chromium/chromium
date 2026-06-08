// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_breadcrumbs_view_controller.h"

#import "ios/chrome/browser/cobrowse/debugger/aim_srp_debugger_event.h"

@interface AimSRPDebuggerBreadcrumbsViewController () <UITableViewDataSource>
@end

@implementation AimSRPDebuggerBreadcrumbsViewController {
  NSArray<AimSRPDebuggerEvent*>* _breadcrumbs;
}

- (instancetype)initWithEvents:(NSArray<AimSRPDebuggerEvent*>*)events {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    _breadcrumbs = [[events reverseObjectEnumerator] allObjects];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.dataSource = self;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.estimatedRowHeight = 50;
  self.title = @"AIM SRP Message Logs";

  UIBarButtonItem* closeButton =
      [[UIBarButtonItem alloc] initWithTitle:@"Close"
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(dismissModal)];
  self.navigationItem.rightBarButtonItem = closeButton;
}

- (void)dismissModal {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return _breadcrumbs.count;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"EventCell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:@"EventCell"];
  }

  AimSRPDebuggerEvent* event = _breadcrumbs[indexPath.row];

  static NSDateFormatter* formatter = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    formatter = [[NSDateFormatter alloc] init];
    [formatter setDateFormat:@"HH:mm:ss.SSS"];
  });
  NSString* timestampStr = [formatter stringFromDate:event.timestamp];

  NSString* directionIndicator =
      event.direction == kClientToAim ? @"->" : @"<-";

  UIListContentConfiguration* content = cell.defaultContentConfiguration;
  content.text =
      [NSString stringWithFormat:@"[%@] %@ %@", timestampStr,
                                 directionIndicator, event.messageName];
  cell.contentConfiguration = content;

  return cell;
}

@end
