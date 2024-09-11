// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_

#import <UIKit/UIKit.h>

#import <vector>

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_site_info.h"

@class TableViewCell;

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
    // A grey line to separate the header from the rest of the cell.
    kHeaderSeparator,
    // The view presenting the instructions on how to use virtual cards.
    kVirtualCardInstructions,
    // A grey line to separate the virtual card instruction view from the rest
    // of the cell.
    kVirtualCardInstructionsSeparator,
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

// Returns the horizontal spacing to use between the different chip buttons.
CGFloat GetHorizontalSpacingBetweenChips();

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

// Adds constraints like `AppendHorizontalConstraintsForViews` above,
// but with the given `trailing_view`, which is a view that is attached to the
// trailing side of the cell. The last view of `views` is therefore constraint
// to not overlap `trailing_view` when valid (i.e., when not `nil`).
// TODO(crbug.com/326398845): Remove the `margin` parameter once the Keyboard
// Accessory Upgrade feature has launched both on iPhone and iPad.
void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin,
    AppendConstraints options,
    UIView* trailing_view);

// Creates and adds constraints to `constraints` with the goal of laying as many
// `views` as possible horizontally. The available horizontal space is
// determined by the width of `layout_guide`. Whenever there's not enough
// horizontal space left to welcome the next view, a new row of views is
// generated below. The starting view of every row is then added to
// `vertical_lead_views`.
void LayViewsHorizontallyWhenPossible(
    NSArray<UIView*>* views,
    UILayoutGuide* guide,
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSMutableArray<UIView*>* vertical_lead_views);

// Creates and adds constraints like `LayViewsHorizontallyWhenPossible` above,
// but with a given `first_row_trailing_view` that should be taken into account
// when laying the first row of views. `first_row_trailing_view` being a view
// that's attached to the trailing side of the cell and limiting the space
// available horizontally.
void LayViewsHorizontallyWhenPossible(
    NSArray<UIView*>* views,
    UILayoutGuide* guide,
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSMutableArray<UIView*>* vertical_lead_views,
    UIView* first_row_trailing_view);

// Creates a blank label with autoresize mask off and adjustable font size.
UILabel* CreateLabel();

// Creates an attributed string composed of a title and subtitle. Used to
// generate the string for the manual fill cell's header label.
NSMutableAttributedString* CreateHeaderAttributedString(NSString* title,
                                                        NSString* subtitle);

// Creates an horizontal stack view containing an icon, a label and an overflow
// menu button. Used to create the different manual fill cells' header. `label`
// should never be `nil`.
UIStackView* CreateHeaderView(UIView* icon,
                              UILabel* label,
                              UIButton* overflow_menu_button);

// Creates and configures the overflow menu button that's displayed in the
// cell's header.
UIButton* CreateOverflowMenuButton();

// Creates a gray horizontal line separator. The gray line is added to the given
// `container` and proper constraints are enabled to keep the line in the
// desired location within the horizontal safe area.
UIView* CreateGraySeparatorForContainer(UIView* container);

// Creates the button used to fill the current form with the manual fill entity
// data.
UIButton* CreateAutofillFormButton();

// Creates a layout guide for the cell and adds it to the given 'content_view`.
// `cell_has_header` indicates whether or not the layout guide should take a
// header into account.
UILayoutGuide* AddLayoutGuideToContentView(UIView* content_view,
                                           BOOL cell_has_header);

// Creates the attributed string containing the site name and potentially a host
// subtitle for the site name label.
NSMutableAttributedString* CreateSiteNameLabelAttributedText(
    ManualFillSiteInfo* site_info,
    BOOL should_show_host);

// Sets the cell's container and its overflow menu button's accessibility label
// with the given `accessibility_context`. `accessibility_context` contains
// information on the position of the cell and its title (if any). Adding this
// information to the accessibility labels gives more context on the UI
// elements, and, therefore, allows accessibility users to better differentiate
// the different cells and their buttons.
void GiveAccessibilityContextToCellAndButton(UIView* cell_container,
                                             UIButton* overflow_menu_button,
                                             UIButton* autofill_form_button,
                                             NSString* accessibility_context);

// Set up the cell accessibility elements so that the cell itself is accessible
// as a container and the accessibility subviews are read in the right order.
// Without setting the cell's accessibility elements, VoiceOver would read the
// elements following the view's hierarchy, meaning that it would follow the
// back to front order instead of the top to bottom order.
void SetUpCellAccessibilityElements(TableViewCell* cell,
                                    NSArray<UIView*>* accessibilityElements);

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_MANUAL_FILL_CELL_UTILS_H_
