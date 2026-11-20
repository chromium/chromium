// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view.h"

#import <algorithm>

#import "ios/chrome/browser/shared/ui/elements/elements_constants.h"
#import "ios/chrome/browser/shared/ui/elements/vertical_sidebar_view+Testing.h"
#import "ios/chrome/test/app/uikit_test_util.h"
#import "testing/platform_test.h"

namespace {

// Size of the system vertical bar column on the iPhone Duo cover display, in
// portrait and landscape.
constexpr CGFloat kColumnWidth = 84;
constexpr CGFloat kColumnHeight = 678;
constexpr CGFloat kLandscapeColumnHeight = 466;
// System bar region of the column on the right in folded portrait, measured on
// the simulator.
constexpr CGRect kPortraitBarRegion = {{12, 174}, {48, 470}};

// Size of the placeholder buttons, larger than a sidebar item.
constexpr CGFloat kButtonSize = 44;
// System pill metrics measured on the iPhone Duo cover display, which the
// sidebar matches.
// Width of a pill.
constexpr CGFloat kPillWidth = 48;
// Padding between a pill's edge and its first or last cell.
constexpr CGFloat kPillPadding = 5;
// Height of a cell.
constexpr CGFloat kCellHeight = 38;
// Distance between the centers of consecutive cells in a pill.
constexpr CGFloat kCellPitch = 52;
// Minimum gap between the pills and the column's ends.
constexpr CGFloat kColumnEndMargin = 24;
// Distance the last pill extends below the bottom of the system's bar region.
constexpr CGFloat kPillOverhangBelowRegion = 10;
// Gap between consecutive pills.
constexpr CGFloat kPillGap = 28;
// Center of the first cell of a pill, below the pill's top.
constexpr CGFloat kFirstCellCenter = kPillPadding + kCellHeight / 2;
// Height of a pill with two cells.
constexpr CGFloat kTwoCellPillHeight =
    2 * kPillPadding + kCellPitch + kCellHeight;

// Returns `count` Auto Layout buttons with fixed `size` constraints, like
// toolbar buttons.
NSArray<UIView*>* Buttons(NSUInteger count, CGFloat size = kButtonSize) {
  NSMutableArray<UIView*>* buttons = [NSMutableArray array];
  for (NSUInteger i = 0; i < count; ++i) {
    UIButton* button = [[UIButton alloc] init];
    [NSLayoutConstraint activateConstraints:@[
      [button.widthAnchor constraintEqualToConstant:size],
      [button.heightAnchor constraintEqualToConstant:size],
    ]];
    [buttons addObject:button];
  }
  return buttons;
}

// Returns `view`'s center in `sidebar`'s coordinates.
CGPoint CenterInSidebar(UIView* view, UIView* sidebar) {
  CGRect frame = [view convertRect:view.bounds toView:sidebar];
  return CGPointMake(CGRectGetMidX(frame), CGRectGetMidY(frame));
}

// Returns `sidebar`'s separator.
UIView* Separator(UIView* sidebar) {
  return chrome_test_util::FindViewById(sidebar,
                                        kVerticalSidebarSeparatorIdentifier);
}

// Returns the number of pills in `sidebar`, which are glass effect views.
NSUInteger PillCount(UIView* sidebar) {
  return chrome_test_util::FindViewsByClass<UIVisualEffectView>(sidebar).count;
}

// Constrains `sidebar`'s pills to a fake system bar region at `frame`, in
// `sidebar`'s coordinates.
void SetBarRegion(VerticalSidebarView* sidebar, CGRect frame) {
  UILayoutGuide* guide = [[UILayoutGuide alloc] init];
  [sidebar addLayoutGuide:guide];
  [NSLayoutConstraint activateConstraints:@[
    [guide.leftAnchor constraintEqualToAnchor:sidebar.leftAnchor
                                     constant:frame.origin.x],
    [guide.topAnchor constraintEqualToAnchor:sidebar.topAnchor
                                    constant:frame.origin.y],
    [guide.widthAnchor constraintEqualToConstant:frame.size.width],
    [guide.heightAnchor constraintEqualToConstant:frame.size.height],
  ]];
  [sidebar constrainPillsToBarRegionGuide:guide];
}

}  // namespace

// Tests the sidebar as the column on the right in folded portrait, with the
// system's bar region for it.
class VerticalSidebarViewTest : public PlatformTest {
 protected:
  VerticalSidebarViewTest() {
    sidebar_ = [[VerticalSidebarView alloc]
        initWithFrame:CGRectMake(0, 0, kColumnWidth, kColumnHeight)];
    SetBarRegion(sidebar_, kPortraitBarRegion);
  }

  VerticalSidebarView* sidebar_;
};

// Tests that in folded portrait, the pills follow the system's bar region with
// the system pill metrics: centered on its axis, top groups stacking down from
// its top (below the vertical status bar), and bottom groups stacking up from
// 10pt below its bottom, 24pt above the bottom of the column.
TEST_F(VerticalSidebarViewTest, FollowsPortraitBarRegion) {
  NSArray<UIView*>* top = Buttons(2);
  NSArray<UIView*>* tabs = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
  [sidebar_ layoutIfNeeded];

  // Buttons keep their own size, centered in their cells on the region's axis.
  EXPECT_DOUBLE_EQ(kButtonSize, top[0].bounds.size.width);
  CGPoint first = CenterInSidebar(top[0], sidebar_);
  EXPECT_DOUBLE_EQ(36, first.x);
  EXPECT_DOUBLE_EQ(174 + kFirstCellCenter, first.y);
  // Cells within a pill are on a 52pt pitch.
  EXPECT_DOUBLE_EQ(first.y + kCellPitch, CenterInSidebar(top[1], sidebar_).y);

  CGPoint last = CenterInSidebar(overflow[0], sidebar_);
  EXPECT_DOUBLE_EQ(36, last.x);
  EXPECT_DOUBLE_EQ(644 + kPillOverhangBelowRegion - kFirstCellCenter, last.y);
  EXPECT_DOUBLE_EQ(kColumnHeight - kColumnEndMargin - kFirstCellCenter, last.y);
  // Bottom pills are 28pt apart: the cell centers are separated by half a
  // cell, 5pt padding, the 28pt gap, 5pt padding and half a cell.
  EXPECT_DOUBLE_EQ(last.y - kFirstCellCenter - kPillGap - kFirstCellCenter,
                   CenterInSidebar(tabs[1], sidebar_).y);
}

// Tests that the pills are centered on the system's bar region.
TEST_F(VerticalSidebarViewTest, FollowsBarRegionAxis) {
  SetBarRegion(sidebar_, CGRectMake(20, 174, 40, 470));
  NSArray<UIView*>* top = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[]];
  [sidebar_ layoutIfNeeded];

  EXPECT_DOUBLE_EQ(40, CenterInSidebar(top[0], sidebar_).x);
}

// Tests that the separator is a 1pt line running the full height on the inner
// (left) edge.
TEST_F(VerticalSidebarViewTest, SeparatorOnInnerEdge) {
  [sidebar_ layoutIfNeeded];
  UIView* separator = Separator(sidebar_);
  ASSERT_TRUE(separator);
  EXPECT_DOUBLE_EQ(0, separator.frame.origin.x);
  EXPECT_DOUBLE_EQ(0, separator.frame.origin.y);
  EXPECT_DOUBLE_EQ(kColumnHeight, separator.frame.size.height);
  EXPECT_DOUBLE_EQ(1, separator.frame.size.width);
}

// Tests that the separator is black at 8% opacity in light mode, and white at
// 8% opacity in dark mode.
TEST_F(VerticalSidebarViewTest, SeparatorColor) {
  UIColor* color = Separator(sidebar_).backgroundColor;
  ASSERT_TRUE(color);
  const struct {
    UIUserInterfaceStyle style;
    CGFloat component;
  } kCases[] = {
      {UIUserInterfaceStyleLight, 0},
      {UIUserInterfaceStyleDark, 1},
  };
  for (const auto& test_case : kCases) {
    UIColor* resolved =
        [color resolvedColorWithTraitCollection:
                   [UITraitCollection
                       traitCollectionWithUserInterfaceStyle:test_case.style]];
    CGFloat red, green, blue, alpha;
    ASSERT_TRUE([resolved getRed:&red green:&green blue:&blue alpha:&alpha]);
    EXPECT_NEAR(test_case.component, red, 1e-6);
    EXPECT_NEAR(test_case.component, green, 1e-6);
    EXPECT_NEAR(test_case.component, blue, 1e-6);
    EXPECT_NEAR(0.08, alpha, 1e-6);
  }
}

// Tests that the separator moves to the inner edge when the column's outer edge
// changes, and that the pills follow the region on the new outer edge.
TEST_F(VerticalSidebarViewTest, FollowsOuterEdge) {
  NSArray<UIView*>* top = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[]];

  // Measured on the simulator with the column on the left.
  sidebar_.outerEdge = UIRectEdgeLeft;
  SetBarRegion(sidebar_, CGRectMake(24, 174, 48, 470));
  [sidebar_ layoutIfNeeded];
  EXPECT_DOUBLE_EQ(48, CenterInSidebar(top[0], sidebar_).x);
  EXPECT_DOUBLE_EQ(kColumnWidth, CGRectGetMaxX(Separator(sidebar_).frame));

  sidebar_.outerEdge = UIRectEdgeRight;
  SetBarRegion(sidebar_, kPortraitBarRegion);
  [sidebar_ layoutIfNeeded];
  EXPECT_DOUBLE_EQ(36, CenterInSidebar(top[0], sidebar_).x);
  EXPECT_DOUBLE_EQ(0, Separator(sidebar_).frame.origin.x);
}

// Tests that switching the column's side back and forth doesn't add a system
// bar region guide each time.
TEST_F(VerticalSidebarViewTest, ReusesBarRegionGuidesWhenSwitchingSides) {
  sidebar_.outerEdge = UIRectEdgeLeft;
  sidebar_.outerEdge = UIRectEdgeRight;
  NSUInteger guide_count = sidebar_.layoutGuides.count;

  for (int i = 0; i < 3; ++i) {
    sidebar_.outerEdge = UIRectEdgeLeft;
    sidebar_.outerEdge = UIRectEdgeRight;
  }
  EXPECT_EQ(guide_count, sidebar_.layoutGuides.count);
}

// Tests that empty groups get no pill.
TEST_F(VerticalSidebarViewTest, SkipsEmptyGroups) {
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ @[] ] bottomGroups:@[ overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_EQ(1u, PillCount(sidebar_));
  EXPECT_DOUBLE_EQ(kColumnHeight - kColumnEndMargin - kFirstCellCenter,
                   CenterInSidebar(overflow[0], sidebar_).y);
}

// Tests that only the pills receive touches.
TEST_F(VerticalSidebarViewTest, PassesThroughTouchesOutsidePills) {
  NSArray<UIView*>* top = Buttons(1);
  NSArray<UIView*>* tabs = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_EQ(top[0], [sidebar_ hitTest:CenterInSidebar(top[0], sidebar_)
                            withEvent:nil]);
  // On the separator, and in the pills' column below the pill.
  EXPECT_FALSE([sidebar_ hitTest:CGPointMake(0.5, kColumnHeight / 2)
                       withEvent:nil]);
  EXPECT_FALSE([sidebar_ hitTest:CGPointMake(36, kColumnHeight / 2)
                       withEvent:nil]);
  // In the gap between two pills of a group.
  CGFloat gap_center = CenterInSidebar(overflow[0], sidebar_).y -
                       kFirstCellCenter - kPillGap / 2;
  EXPECT_FALSE([sidebar_ hitTest:CGPointMake(36, gap_center) withEvent:nil]);
}

// Tests that touches anywhere in a view's share of the pill go to that view,
// even outside the view's own bounds.
TEST_F(VerticalSidebarViewTest, ExtendsTouchTargetsToTheirCell) {
  NSArray<UIView*>* buttons = Buttons(2);
  UIView* small = Buttons(1, kButtonSize / 2)[0];
  [sidebar_ setTopGroups:@[ @[ buttons[0], buttons[1], small ] ]
            bottomGroups:@[]];
  [sidebar_ layoutIfNeeded];

  // Near the pill's outer side, outside the 44pt button but inside the 48pt
  // pill, and halfway between two items.
  CGPoint first = CenterInSidebar(buttons[0], sidebar_);
  CGPoint second = CenterInSidebar(buttons[1], sidebar_);
  CGPoint pill_side = CGPointMake(first.x + kPillWidth / 2 - 1, first.y);
  EXPECT_EQ(buttons[0], [sidebar_ hitTest:pill_side withEvent:nil]);
  CGPoint above_midpoint = CGPointMake(first.x, (first.y + second.y) / 2 - 1);
  EXPECT_EQ(buttons[0], [sidebar_ hitTest:above_midpoint withEvent:nil]);
  CGPoint below_midpoint = CGPointMake(first.x, (first.y + second.y) / 2 + 1);
  EXPECT_EQ(buttons[1], [sidebar_ hitTest:below_midpoint withEvent:nil]);
  // A view smaller than the button gets the whole cell too.
  EXPECT_DOUBLE_EQ(kButtonSize / 2, small.bounds.size.width);
  CGPoint small_center = CenterInSidebar(small, sidebar_);
  EXPECT_EQ(small,
            [sidebar_ hitTest:CGPointMake(small_center.x - kCellHeight / 2,
                                          small_center.y + kCellHeight / 2)
                    withEvent:nil]);

  // Hidden views don't take touches, but their pill still absorbs them.
  buttons[0].hidden = YES;
  UIView* hidden_hit = [sidebar_ hitTest:first withEvent:nil];
  EXPECT_TRUE(hidden_hit);
  EXPECT_NE(buttons[0], hidden_hit);
}

// Tests that setting new groups replaces the previous pills and their views.
TEST_F(VerticalSidebarViewTest, ReplacesGroups) {
  NSArray<UIView*>* first = Buttons(1);
  NSArray<UIView*>* kept = Buttons(1);
  [sidebar_ setTopGroups:@[ first, kept ] bottomGroups:@[]];

  NSArray<UIView*>* second = Buttons(1);
  [sidebar_ setTopGroups:@[] bottomGroups:@[ kept, second ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_FALSE([first[0] isDescendantOfView:sidebar_]);
  EXPECT_EQ(2u, PillCount(sidebar_));
  EXPECT_TRUE([kept[0] isDescendantOfView:sidebar_]);
  EXPECT_TRUE([second[0] isDescendantOfView:sidebar_]);
  EXPECT_DOUBLE_EQ(kColumnHeight - kColumnEndMargin - kFirstCellCenter,
                   CenterInSidebar(second[0], sidebar_).y);
  EXPECT_DOUBLE_EQ(kColumnHeight - kColumnEndMargin - kFirstCellCenter -
                       kFirstCellCenter - kPillGap - kFirstCellCenter,
                   CenterInSidebar(kept[0], sidebar_).y);
}

// Tests that in folded landscape with the column on the right (camera at the
// bottom of the column), the bottom pills stay above the camera and the top
// pill keeps a margin from the top of the column, which the region reaches.
TEST_F(VerticalSidebarViewTest, FollowsLandscapeLeftBarRegion) {
  sidebar_.frame = CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
  // Measured on the simulator, {12, 2.33, 48, 381.67}, rounded: the region
  // ends at 384, above the camera, which starts at about 400.
  SetBarRegion(sidebar_, CGRectMake(12, 2, 48, 382));
  NSArray<UIView*>* top = Buttons(2);
  NSArray<UIView*>* tabs = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_DOUBLE_EQ(kColumnEndMargin + kFirstCellCenter,
                   CenterInSidebar(top[0], sidebar_).y);
  // The menu pill ends at 394, 10pt below the region, and the tabs pill spans
  // 218 to 318, clear of the top pill, which ends at 124.
  EXPECT_DOUBLE_EQ(394 - kFirstCellCenter,
                   CenterInSidebar(overflow[0], sidebar_).y);
  EXPECT_DOUBLE_EQ(218 + kFirstCellCenter,
                   CenterInSidebar(tabs[0], sidebar_).y);
}

// Tests that in folded landscape with the column on the left (vertical status
// bar area at the top of the column), the top pill starts at the region's top.
TEST_F(VerticalSidebarViewTest, FollowsLandscapeRightBarRegion) {
  sidebar_.frame = CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
  sidebar_.outerEdge = UIRectEdgeLeft;
  // The region's top was measured on the simulator; it ends at the bottom safe
  // area inset.
  SetBarRegion(sidebar_, CGRectMake(24, 82, 48, 350));
  NSArray<UIView*>* top = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ Buttons(2), overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_DOUBLE_EQ(48, CenterInSidebar(top[0], sidebar_).x);
  EXPECT_DOUBLE_EQ(82 + kFirstCellCenter, CenterInSidebar(top[0], sidebar_).y);
  EXPECT_DOUBLE_EQ(kLandscapeColumnHeight - kColumnEndMargin - kFirstCellCenter,
                   CenterInSidebar(overflow[0], sidebar_).y);
}

// Tests that the browser's current 5 views (back and forward; new tab and tab
// grid; menu) fit in both folded landscape regions: the bottom stack stays a
// pill gap below the top one, and both stay in the region (less the margins).
TEST_F(VerticalSidebarViewTest, FitsCurrentViewsInLandscape) {
  const struct {
    UIRectEdge outer_edge;
    CGRect region;
  } kCases[] = {
      {UIRectEdgeRight, CGRectMake(12, 2, 48, 382)},
      {UIRectEdgeLeft, CGRectMake(24, 82, 48, 350)},
  };
  for (const auto& test_case : kCases) {
    sidebar_.frame = CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
    sidebar_.outerEdge = test_case.outer_edge;
    SetBarRegion(sidebar_, test_case.region);
    NSArray<UIView*>* top = Buttons(2);
    NSArray<UIView*>* tabs = Buttons(2);
    NSArray<UIView*>* overflow = Buttons(1);
    [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
    [sidebar_ layoutIfNeeded];

    CGFloat top_pill_top =
        CenterInSidebar(top[0], sidebar_).y - kFirstCellCenter;
    CGFloat top_pill_bottom =
        CenterInSidebar(top[1], sidebar_).y + kFirstCellCenter;
    CGFloat tabs_pill_top =
        CenterInSidebar(tabs[0], sidebar_).y - kFirstCellCenter;
    CGFloat overflow_pill_bottom =
        CenterInSidebar(overflow[0], sidebar_).y + kFirstCellCenter;
    EXPECT_GE(top_pill_top,
              std::max(kColumnEndMargin, CGRectGetMinY(test_case.region)));
    EXPECT_GE(tabs_pill_top - top_pill_bottom, kPillGap);
    EXPECT_DOUBLE_EQ(
        std::min(kLandscapeColumnHeight - kColumnEndMargin,
                 CGRectGetMaxY(test_case.region) + kPillOverhangBelowRegion),
        overflow_pill_bottom);
  }
}

// Tests that when the system bar region is too short for the pills (60pt
// shorter than in folded landscape), the pills extend past it but stay in the
// column, and the top and bottom pills don't overlap.
TEST_F(VerticalSidebarViewTest, KeepsPillsInColumnWithShortBarRegion) {
  sidebar_.frame = CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
  CGRect region = CGRectMake(12, 82, 48, 290);
  SetBarRegion(sidebar_, region);
  NSArray<UIView*>* top = Buttons(2);
  NSArray<UIView*>* tabs = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
  [sidebar_ layoutIfNeeded];

  CGFloat top_pill_top = CenterInSidebar(top[0], sidebar_).y - kFirstCellCenter;
  CGFloat top_pill_bottom =
      CenterInSidebar(top[1], sidebar_).y + kFirstCellCenter;
  CGFloat tabs_pill_top =
      CenterInSidebar(tabs[0], sidebar_).y - kFirstCellCenter;
  CGFloat overflow_pill_bottom =
      CenterInSidebar(overflow[0], sidebar_).y + kFirstCellCenter;
  EXPECT_GT(overflow_pill_bottom - top_pill_top,
            CGRectGetHeight(region) + kPillOverhangBelowRegion);
  EXPECT_GE(top_pill_top, kColumnEndMargin);
  EXPECT_LE(overflow_pill_bottom, kLandscapeColumnHeight - kColumnEndMargin);
  EXPECT_GE(tabs_pill_top - top_pill_bottom, kPillGap);
}

// Tests that a system bar region that's still empty (a zero frame, before the
// system populates it) keeps the pills in the column and in order: the pills
// rest against the column's left edge, the top pill keeps the top margin, and
// the bottom pills stack right below it rather than above the column's top.
TEST_F(VerticalSidebarViewTest, KeepsPillsInColumnWithEmptyBarRegion) {
  sidebar_.frame = CGRectMake(0, 0, kColumnWidth, kLandscapeColumnHeight);
  SetBarRegion(sidebar_, CGRectZero);
  NSArray<UIView*>* top = Buttons(2);
  NSArray<UIView*>* tabs = Buttons(2);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[ top ] bottomGroups:@[ tabs, overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_DOUBLE_EQ(kPillWidth / 2, CenterInSidebar(top[0], sidebar_).x);
  EXPECT_DOUBLE_EQ(kColumnEndMargin + kFirstCellCenter,
                   CenterInSidebar(top[0], sidebar_).y);
  CGFloat tabs_top = kColumnEndMargin + kTwoCellPillHeight + kPillGap;
  EXPECT_DOUBLE_EQ(tabs_top + kFirstCellCenter,
                   CenterInSidebar(tabs[0], sidebar_).y);
  EXPECT_DOUBLE_EQ(tabs_top + kTwoCellPillHeight + kPillGap + kFirstCellCenter,
                   CenterInSidebar(overflow[0], sidebar_).y);
}

// Tests that with only bottom groups, an empty system bar region doesn't pull
// the pills above the column's top margin.
TEST_F(VerticalSidebarViewTest, KeepsBottomPillsInColumnWithEmptyBarRegion) {
  SetBarRegion(sidebar_, CGRectZero);
  NSArray<UIView*>* overflow = Buttons(1);
  [sidebar_ setTopGroups:@[] bottomGroups:@[ overflow ]];
  [sidebar_ layoutIfNeeded];

  EXPECT_DOUBLE_EQ(kColumnEndMargin + kFirstCellCenter,
                   CenterInSidebar(overflow[0], sidebar_).y);
}

// Tests that an unsized column with an empty or missing system bar region
// (outside a window, or with SDKs before iOS 27.1) lays out without crashing,
// and the views keep their size.
TEST_F(VerticalSidebarViewTest, LaysOutUnsizedColumn) {
  VerticalSidebarView* sidebar =
      [[VerticalSidebarView alloc] initWithFrame:CGRectZero];
  NSArray<UIView*>* top = Buttons(2);
  [sidebar setTopGroups:@[ top ] bottomGroups:@[ Buttons(2), Buttons(1) ]];
  [sidebar layoutIfNeeded];

  EXPECT_DOUBLE_EQ(kButtonSize, top[0].bounds.size.width);
  EXPECT_DOUBLE_EQ(kButtonSize, top[0].bounds.size.height);
}
