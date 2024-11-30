// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_cell.h"

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"

namespace {

// Offset of the new activity label.
const CGFloat kNewActivityLabelBottomOffset = 10;

}  // namespace

@implementation TabCell {
  ActivityLabelView* _activityLabelView;
}

- (void)showActivityLabel {
  [self setupActivityLabel];
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.itemIdentifier = nil;
}

#pragma mark - Private

- (void)setupActivityLabel {
  _activityLabelView = [[ActivityLabelView alloc] init];
  UIView* contentView = self.contentView;
  [contentView addSubview:_activityLabelView];

  NSArray* constraints = @[
    [_activityLabelView.centerXAnchor
        constraintEqualToAnchor:contentView.centerXAnchor],
    [_activityLabelView.bottomAnchor
        constraintEqualToAnchor:contentView.bottomAnchor
                       constant:-kNewActivityLabelBottomOffset],
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

@end
