// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_context_menu/tab_cell.h"

#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_data.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/activity_label_view.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_grid_constants.h"

namespace {

// Offset of the new activity label.
const CGFloat kNewActivityLabelBottomOffset = 10;

}  // namespace

@implementation TabCell {
  ActivityLabelData* _activityLabelData;
  ActivityLabelView* _activityLabelView;
}

- (void)setActivityLabelData:(ActivityLabelData*)data {
  if (!data) {
    _activityLabelData = nil;
    [self resetAndHideActivityLabel];
    return;
  }

  [self showActivityLabel];

  // Set the text and the user icon to the activity label.
  [_activityLabelView setLabelText:data.labelString];
  // `avatarPrimitive` is nil on the activity label for a group cell.
  if (data.avatarPrimitive) {
    [_activityLabelView setUserIcon:[data.avatarPrimitive view]];
    [data.avatarPrimitive resolve];
  }

  _activityLabelData = data;
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  [self resetAndHideActivityLabel];
  self.itemIdentifier = nil;
}

#pragma mark - Private

// Initializes the activity label if it doesn't exist yet. Otherwise, updates
// the visibility to show it.
- (void)showActivityLabel {
  if (_activityLabelView) {
    _activityLabelView.hidden = NO;
  } else {
    [self setupActivityLabel];
  }
}

// Resets and hides the activity label if it exists.
- (void)resetAndHideActivityLabel {
  if (!_activityLabelView) {
    return;
  }
  _activityLabelView.hidden = YES;
  [_activityLabelView setLabelText:@""];
  [_activityLabelView setUserIcon:nil];
}

// Initializes the activity label and adds it as a subview.
- (void)setupActivityLabel {
  _activityLabelView = [[ActivityLabelView alloc] init];
  _activityLabelView.accessibilityIdentifier = kTabCellActivityLabelIdentifier;
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
