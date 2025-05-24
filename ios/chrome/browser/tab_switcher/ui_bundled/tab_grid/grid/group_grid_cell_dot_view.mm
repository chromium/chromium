// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/group_grid_cell_dot_view.h"

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// The size of the group color dot under normal font size.
const CGFloat kColorDotSize = 16;
// The size of the group color dot under accessibility font size.
const CGFloat kColorDotLargeSize = 24;
}  // namespace

@implementation GroupGridCellDotView {
  NSArray<NSLayoutConstraint*>* _accessibilityConstraints;
  NSArray<NSLayoutConstraint*>* _normalConstraints;

  // Constraints to set the size of the view to the dot.
  NSArray<NSLayoutConstraint*>* _dotVisibleConstraints;

  // The view displaying the dot.
  UIView* _dotView;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _dotView = [[UIView alloc] init];
    _dotView.accessibilityIdentifier = kGroupGridCellColoredDotIdentifier;
    _dotView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_dotView];

    _normalConstraints = @[
      [_dotView.widthAnchor constraintEqualToConstant:kColorDotSize],
      [_dotView.heightAnchor constraintEqualToAnchor:_dotView.widthAnchor],
    ];

    _accessibilityConstraints = @[
      [_dotView.widthAnchor constraintEqualToConstant:kColorDotLargeSize],
      [_dotView.heightAnchor constraintEqualToAnchor:_dotView.widthAnchor],
    ];

    _dotVisibleConstraints = @[
      [self.leadingAnchor constraintEqualToAnchor:_dotView.leadingAnchor],
      [self.trailingAnchor constraintEqualToAnchor:_dotView.trailingAnchor],
      [self.topAnchor constraintEqualToAnchor:_dotView.topAnchor],
      [self.bottomAnchor constraintEqualToAnchor:_dotView.bottomAnchor],
    ];

    [self setContentCompressionResistancePriority:UILayoutPriorityRequired
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];

    [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withAction:@selector(updateAppearance)];

    [self updateAppearance];
  }
  return self;
}

- (void)setColor:(UIColor*)color {
  _color = color;
  _dotView.backgroundColor = color;
}

- (void)setFacePile:(UIView*)facePile {
  if ([_facePile isDescendantOfView:self]) {
    [_facePile removeFromSuperview];
  }
  _facePile = facePile;

  if (_facePile) {
    _facePile.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_facePile];
    AddSameConstraints(self, _facePile);
  }

  [self updateAppearance];

  [self invalidateIntrinsicContentSize];
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  if (_facePile) {
    return _facePile.intrinsicContentSize;
  }
  return [super intrinsicContentSize];
}

#pragma mark - Private

// Updates the appearance of the view (accessibility/normal) and its content
// (facepile/dot).
- (void)updateAppearance {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    _dotView.layer.cornerRadius = kColorDotLargeSize / 2;
    [NSLayoutConstraint deactivateConstraints:_normalConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    _dotView.layer.cornerRadius = kColorDotSize / 2;
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_normalConstraints];
  }

  if (_facePile) {
    _dotView.hidden = YES;
    [NSLayoutConstraint deactivateConstraints:_dotVisibleConstraints];
  } else {
    _dotView.hidden = NO;
    [NSLayoutConstraint activateConstraints:_dotVisibleConstraints];
  }
}

@end
