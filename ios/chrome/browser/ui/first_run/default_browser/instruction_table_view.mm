// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/default_browser/instruction_table_view.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

constexpr CGFloat kTableViewSeparatorInsetWithLabel = 60;
constexpr CGFloat kTableViewCornerRadius = 12;

}  // namespace

#pragma mark - UITableView

@implementation InstructionTableView

- (instancetype)initWithFrame:(CGRect)frame style:(UITableViewStyle)style {
  self = [super initWithFrame:frame style:style];
  if (self) {
    self.layer.cornerRadius = kTableViewCornerRadius;
    self.tableFooterView = [[UIView alloc]
        initWithFrame:CGRectMake(0, 0, self.frame.size.width, 1)];

    [self setBackgroundColor:[UIColor colorNamed:kSecondaryBackgroundColor]];
    [self setSeparatorInset:UIEdgeInsetsMake(
                                0, kTableViewSeparatorInsetWithLabel, 0, 0)];
  }
  return self;
}
@end
