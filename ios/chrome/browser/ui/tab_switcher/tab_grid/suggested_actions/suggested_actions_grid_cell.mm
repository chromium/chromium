// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_grid_cell.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation SuggestedActionsGridCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.layer.cornerRadius = kGridCellCornerRadius;
    self.layer.masksToBounds = YES;
    self.overrideUserInterfaceStyle = UIUserInterfaceStyleDark;
    self.backgroundView = [[UIView alloc] init];
    self.backgroundView.backgroundColor =
        [UIColor colorNamed:kGridBackgroundColor];
    self.accessibilityIdentifier = kSuggestedActionsGridCellIdentifier;
  }
  return self;
}

#pragma mark UICollectionViewCell Overrides

- (void)prepareForReuse {
  [super prepareForReuse];
  _suggestedActionsView = nil;
}

#pragma mark - Public

- (void)setSuggestedActionsView:(UIView*)view {
  if (view == _suggestedActionsView)
    return;
  if (_suggestedActionsView)
    [_suggestedActionsView removeFromSuperview];
  _suggestedActionsView = view;
  _suggestedActionsView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:_suggestedActionsView];
  NSArray* constraints = @[
    [_suggestedActionsView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor],
    [_suggestedActionsView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor],
    [_suggestedActionsView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor],
    [_suggestedActionsView.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ];
  [NSLayoutConstraint activateConstraints:constraints];
}

@end
