// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_

#import <UIKit/UIKit.h>

#import <vector>

// Margins of the cell content.
extern const CGFloat kCellMargin;

// Left and right margins for the chips.
extern const CGFloat kChipsHorizontalMargin;

// Options for `AppendHorizontalConstraintsForViews`.
typedef NS_OPTIONS(NSUInteger, AppendConstraints) {
  AppendConstraintsNone = 0,
  // Add an equal constraint to the baselines.
  AppendConstraintsHorizontalSyncBaselines = 1 << 0,
  // The views can be constraint smaller than the guide.
  AppendConstraintsHorizontalEqualOrSmallerThanGuide = 1 << 1,
};

// Struct used to bundle views that are part of the manual fill cells with an
// ElementType. This bundling is used by the
// `AppendVerticalConstraintsSpacingForViews` method to determine which spacing
// to add above the `view` when creating vertical constraints.
struct ManualFillCellView {
  // Enum which represents possible types of UI element that are added to a
  // manual fill cell.
  enum class ElementType {
    // The first chip button of a chip group.
    kFirstChipButtonOfGroup,
    // A labeled chip button that is not the first of its group.
    kLabeledChipButton,
    // A chip button that is not the first of its group and is unlabeled.
    kOtherChipButton,
    // Any other element not falling into one of the above types.
    kOther,
  };

  UIView* view;
  ElementType type;

  // Operator overloads.
  bool operator==(const ManualFillCellView& rhs) const {
    return [view isEqual:rhs.view] && type == rhs.type;
  }
  bool operator!=(const ManualFillCellView& rhs) const {
    return !(*this == rhs);
  }
};

// Creates a blank button in chip style, for the given `action` and `target`.
UIButton* CreateChipWithSelectorAndTarget(SEL action, id target);

// Adds vertical constraints to given list, laying `manual_fill_cell_views`
// vertically (based on firstBaselineAnchor for the buttons or labels) following
// the `layout_guide`. Constraints are not activated.
void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    const std::vector<ManualFillCellView>& manual_fill_cell_views,
    UILayoutGuide* layout_guide);

// Adds vertical constraints like `AppendVerticalConstraintsSpacingForViews`
// above but using an `offset` to shift the first view's top anchor upwards when
// displaying a password cell that is connected to the previous one.
// TODO(crbug.com/326398845): Remove the `offset` parameter once the Keyboard
// Accessory Upgrade feature has launched both on iPhone and iPad.
void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    const std::vector<ManualFillCellView>& manual_fill_cell_views,
    UILayoutGuide* layout_guide,
    CGFloat offset);

// Creates a ManualFillCellView with each chip button of `chip_groups`,
// and adds the ManualFillCellViews to `vertical_lead_views`.
void AddChipGroupsToVerticalLeadViews(
    NSArray<NSArray<UIView*>*>* chip_groups,
    std::vector<ManualFillCellView>& vertical_lead_views);

// Creates a ManualFillCellView with the `view` and adds the ManualFillCellView
// to `vertical_lead_views`.
void AddViewToVerticalLeadViews(
    UIView* view,
    ManualFillCellView::ElementType type,
    std::vector<ManualFillCellView>& vertical_lead_views);

// Adds constraints to the given list, for the given `views`, so as to lay them
// out horizontally, aligned with the `layout_guide`. Constraints are not
// activated.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide);

// Adds constraints like `AppendHorizontalConstraintsForViews` above but also
// applies the given constant `margin` at both ends of the whole row.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin);

// Adds constraints like `AppendHorizontalConstraintsForViews` above
// but with given `options`.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin,
    AppendConstraints options);

// Adds all baseline anchor constraints for the given `views` to match the first
// one. Constraints are not activated.
void AppendEqualBaselinesConstraints(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views);

// Creates a blank label with autoresize mask off and adjustable font size.
UILabel* CreateLabel();

// Creates an attributed string composed of a title and subtitle. Used to
// generate the string for the manual fill cell's header label.
NSMutableAttributedString* CreateHeaderAttributedString(NSString* title,
                                                        NSString* subtitle);

// Creates a gray horizontal line separator, with the same margin as the other
// components here. The gray line is added to the given `container` and proper
// constraints are enabled to keep the line at the bottom of the container and
// within the horizontal safe area.
UIView* CreateGraySeparatorForContainer(UIView* container);

// Creates a layout guide for the cell and adds it to the given 'content_view`.
UILayoutGuide* AddLayoutGuideToContentView(UIView* content_view);

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
