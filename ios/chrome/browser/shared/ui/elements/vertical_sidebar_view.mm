// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view.h"

#import "base/check.h"
#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/shared/ui/elements/passthrough_stack_view.h"
#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_cell.h"
#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view+Testing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Metrics matching the system vertical bar pills on the iPhone Duo cover
// display, measured on the iOS 27.2 simulator: 48pt capsules with 38pt items on
// a 52pt pitch, centered on the vertical status bar's axis.
// TODO(crbug.com/585252907): Use system metrics if an API for them ships.
constexpr CGFloat kPillWidth = 48;
constexpr CGFloat kItemSize = 38;
constexpr CGFloat kItemPitch = 52;
constexpr CGFloat kPillPadding = 5;
constexpr CGFloat kPillGap = 28;
// Minimum distance between the pills and either end of the column.
constexpr CGFloat kColumnEndMargin = 24;
// Distance the system pills extend below the bottom of the system's bar
// region: in folded portrait, the region ends at 644 and the pills at 654.
constexpr CGFloat kPillOverhangBelowRegion = 10;
// Width of the separator on the column's inner edge.
constexpr CGFloat kSeparatorWidth = 1;
// Opacity of the separator. The light value matches the spec, which has no
// dark value, so the dark one mirrors it.
// TODO(crbug.com/585252907): Use the spec's dark value once it has one.
constexpr CGFloat kSeparatorAlpha = 0.08;
// Priorities of the pill area's constraints. The pills' sizes and the rule that
// the top and bottom stacks don't overlap are required. Keeping the area in the
// column is just below required, so that an unsized column breaks it instead
// of logging unsatisfiable constraints. Following the system's bar region is
// weaker still: the margins win where the region reaches the column's ends,
// and pills that don't fit spill out of the region rather than overlap.
const UILayoutPriority kInsideColumnPriority = UILayoutPriorityRequired - 1;
const UILayoutPriority kBarRegionPriority = UILayoutPriorityDefaultHigh;

// Returns a vertical stack of `views`, `spacing` apart. Touches between the
// views pass through it.
UIStackView* CreateVerticalStack(NSArray<UIView*>* views, CGFloat spacing) {
  UIStackView* stack =
      [[PassthroughStackView alloc] initWithArrangedSubviews:views];
  stack.axis = UILayoutConstraintAxisVertical;
  stack.spacing = spacing;
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  return stack;
}

// Returns a capsule pill with the system glass material, holding `views` top
// to bottom in cells of the pill's width and a full item pitch. Since the
// pitch is taller than an item plus the pill's padding on each side, the end
// cells extend past the pill's ends, outside the pill's touch area.
UIVisualEffectView* CreatePill(NSArray<UIView*>* views) {
  UIVisualEffectView* pill = [[UIVisualEffectView alloc] initWithEffect:nil];
  pill.translatesAutoresizingMaskIntoConstraints = NO;
  if (@available(iOS 26, *)) {
    UIGlassEffect* glass =
        [UIGlassEffect effectWithStyle:UIGlassEffectStyleRegular];
    // An interactive glass reacts as a whole to a touch on any of its items,
    // which a pill holding several buttons must not do.
    glass.interactive = NO;
    pill.effect = glass;
    pill.cornerConfiguration = [UICornerConfiguration capsuleConfiguration];
  }

  NSMutableArray<UIView*>* cells = [NSMutableArray array];
  for (UIView* view in views) {
    UIView* cell = [[VerticalSidebarCell alloc] initWithHostedView:view];
    [cell.heightAnchor constraintEqualToConstant:kItemPitch].active = YES;
    [cells addObject:cell];
  }
  UIStackView* stack = CreateVerticalStack(cells, 0);
  [pill.contentView addSubview:stack];
  CGFloat cellOverhang = (kItemPitch - kItemSize) / 2 - kPillPadding;
  AddSameConstraintsWithInsets(
      stack, pill.contentView,
      NSDirectionalEdgeInsetsMake(-cellOverhang, 0, -cellOverhang, 0));
  return pill;
}

// Replaces the pills of `stack` with one pill per non-empty group.
void SetPillGroups(UIStackView* stack, NSArray<NSArray<UIView*>*>* groups) {
  for (UIView* pill in stack.arrangedSubviews) {
    [pill removeFromSuperview];
  }
  for (NSArray<UIView*>* group in groups) {
    if (group.count) {
      [stack addArrangedSubview:CreatePill(group)];
    }
  }
}

}  // namespace

@implementation VerticalSidebarView {
  // 1pt line on the edge facing the content, and its constraint to that edge.
  UIView* _separator;
  NSLayoutConstraint* _separatorEdgeConstraint;
  // Area the pills are laid out in: the pills' column horizontally, and the
  // column's span free of system obstructions vertically.
  UILayoutGuide* _pillArea;
  // Pills of the top groups, stacking down from the top of `_pillArea`, and of
  // the bottom groups, stacking up from its bottom.
  UIStackView* _topStack;
  UIStackView* _bottomStack;
  // Keeps `_bottomStack` below `_topStack`.
  NSLayoutConstraint* _noOverlapConstraint;
  // Constraints of `_pillArea` to the system's bar region on `outerEdge`.
  NSArray<NSLayoutConstraint*>* _barRegionConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    _outerEdge = UIRectEdgeRight;

    _separator = [[UIView alloc] init];
    _separator.translatesAutoresizingMaskIntoConstraints = NO;
    // `kSolidBlackColor` is black in light mode and white in dark mode, and the
    // alpha keeps it dynamic.
    _separator.backgroundColor = [[UIColor colorNamed:kSolidBlackColor]
        colorWithAlphaComponent:kSeparatorAlpha];
    _separator.accessibilityIdentifier = kVerticalSidebarSeparatorIdentifier;
    [self addSubview:_separator];
    AddSameConstraintsToSides(_separator, self,
                              LayoutSides::kTop | LayoutSides::kBottom);
    [_separator.widthAnchor constraintEqualToConstant:kSeparatorWidth].active =
        YES;

    _pillArea = [[UILayoutGuide alloc] init];
    [self addLayoutGuide:_pillArea];
    [_pillArea.widthAnchor constraintEqualToConstant:kPillWidth].active = YES;
    NSArray<NSLayoutConstraint*>* insideColumn = @[
      [_pillArea.leftAnchor
          constraintGreaterThanOrEqualToAnchor:self.leftAnchor],
      [_pillArea.rightAnchor
          constraintLessThanOrEqualToAnchor:self.rightAnchor],
      [_pillArea.topAnchor
          constraintGreaterThanOrEqualToAnchor:self.topAnchor
                                      constant:kColumnEndMargin],
      [_pillArea.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.bottomAnchor
                                   constant:-kColumnEndMargin],
    ];
    for (NSLayoutConstraint* constraint in insideColumn) {
      constraint.priority = kInsideColumnPriority;
    }
    [NSLayoutConstraint activateConstraints:insideColumn];

    _topStack = CreateVerticalStack(@[], kPillGap);
    _bottomStack = CreateVerticalStack(@[], kPillGap);
    for (UIStackView* stack in @[ _topStack, _bottomStack ]) {
      [self addSubview:stack];
      AddSameConstraintsToSides(stack, _pillArea, LayoutSides::kHorizontal);
    }
    [NSLayoutConstraint activateConstraints:@[
      [_topStack.topAnchor constraintEqualToAnchor:_pillArea.topAnchor],
      [_bottomStack.bottomAnchor
          constraintEqualToAnchor:_pillArea.bottomAnchor],
    ]];
    [self updateOuterEdgeConstraints];
  }
  return self;
}

#pragma mark - Properties

- (void)setOuterEdge:(UIRectEdge)outerEdge {
  CHECK(outerEdge == UIRectEdgeLeft || outerEdge == UIRectEdgeRight);
  if (_outerEdge == outerEdge) {
    return;
  }
  _outerEdge = outerEdge;
  [self updateOuterEdgeConstraints];
  // Changing constraints doesn't schedule a layout of a view without a
  // superview, so request one.
  [self setNeedsLayout];
}

#pragma mark - Public

- (void)setTopGroups:(NSArray<NSArray<UIView*>*>*)topGroups
        bottomGroups:(NSArray<NSArray<UIView*>*>*)bottomGroups {
  SetPillGroups(_topStack, topGroups);
  SetPillGroups(_bottomStack, bottomGroups);

  // The bottom stack starts below the top one, or within the area when either
  // is empty.
  BOOL hasTopPills = _topStack.arrangedSubviews.count > 0;
  BOOL hasBottomPills = _bottomStack.arrangedSubviews.count > 0;
  NSLayoutYAxisAnchor* topStackEnd =
      hasTopPills ? _topStack.bottomAnchor : _pillArea.topAnchor;
  NSLayoutYAxisAnchor* bottomStackStart =
      hasBottomPills ? _bottomStack.topAnchor : _pillArea.bottomAnchor;
  _noOverlapConstraint.active = NO;
  _noOverlapConstraint = [bottomStackStart
      constraintGreaterThanOrEqualToAnchor:topStackEnd
                                  constant:hasTopPills && hasBottomPills
                                               ? kPillGap
                                               : 0];
  _noOverlapConstraint.active = YES;
}

#pragma mark - UIView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hit = [super hitTest:point withEvent:event];
  // Only the pills (and the views inside them) take touches, so that the views
  // under the column (e.g. the tab grid's scroll view) get the others.
  return hit == self || hit == _separator ? nil : hit;
}

#pragma mark - Private

// Positions the separator and the pills relative to `outerEdge`.
- (void)updateOuterEdgeConstraints {
  // The separator faces the content, on the inner edge of the column.
  _separatorEdgeConstraint.active = NO;
  _separatorEdgeConstraint =
      self.outerEdge == UIRectEdgeRight
          ? [_separator.leftAnchor constraintEqualToAnchor:self.leftAnchor]
          : [_separator.rightAnchor constraintEqualToAnchor:self.rightAnchor];
  _separatorEdgeConstraint.active = YES;

#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    // The region must be on the outer edge: on the inner one, it's clamped to
    // the column and its center isn't the system's axis. The extent only sets
    // the region's width, which isn't used: its center and vertical span were
    // the same for extents 0, 48 and 84 on the iOS 27.2 simulator, so a bar as
    // wide as the pills is the natural value. UIKit keeps one guide per edge
    // and extent, so switching sides reuses the two guides.
    [self constrainPillsToBarRegionGuide:
              [self layoutGuideForLayoutRegion:
                        [UIViewLayoutRegion
                            layoutRegionForBarOnEdge:self.outerEdge
                                              extent:kPillWidth]]];
  }
#endif
}

- (void)constrainPillsToBarRegionGuide:(UILayoutGuide*)guide {
  [NSLayoutConstraint deactivateConstraints:_barRegionConstraints];
  // Like the system pills, the top pill starts at the region's top, and the
  // bottom pill overhangs the region's bottom a little.
  _barRegionConstraints = @[
    [_pillArea.centerXAnchor constraintEqualToAnchor:guide.centerXAnchor],
    [_pillArea.topAnchor constraintEqualToAnchor:guide.topAnchor],
    [_pillArea.bottomAnchor constraintEqualToAnchor:guide.bottomAnchor
                                           constant:kPillOverhangBelowRegion],
  ];
  for (NSLayoutConstraint* constraint in _barRegionConstraints) {
    constraint.priority = kBarRegionPriority;
  }
  [NSLayoutConstraint activateConstraints:_barRegionConstraints];
  // As in `-setOuterEdge:`, request a layout for the new constraints.
  [self setNeedsLayout];
}

@end
