// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/uikit_table_view_cell/uikit_table_view_cell_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIKitTableViewCellViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.separatorStyle = UITableViewCellSeparatorStyleNone;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 4;
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 1;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  switch ([self styleForSection:section]) {
    case UITableViewCellStyleDefault:
      return @"Default Style";
    case UITableViewCellStyleValue1:
      return @"Value 1 Style";
    case UITableViewCellStyleValue2:
      return @"Value 2 Style";
    case UITableViewCellStyleSubtitle:
      return @"Subtitle Style";
  }
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCellStyle style = [self styleForSection:indexPath.section];
  NSString* reuseIdentifier = @(style).stringValue;
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:reuseIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:style
                                  reuseIdentifier:reuseIdentifier];
  }
  cell.textLabel.text = @"Text";
  cell.detailTextLabel.text = @"Detail Text";
  return cell;
}

#pragma mark - Private

- (UITableViewCellStyle)styleForSection:(NSInteger)section {
  NSAssert(section >= 0, @"");
  NSAssert(section < 4, @"");
  return (UITableViewCellStyle)section;
}

@end
