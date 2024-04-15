// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/chip_button.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_labeled_chip.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Bottom margin for the cell content. Used when the Keyboard Accessory Upgrade
// feature is disabled.
constexpr CGFloat kCellBottomMargin = 18;

// Line spacing for the cell's header title.
constexpr CGFloat kHeaderAttributedStringLineSpacing = 2;

// Horizontal spacing between views used in
// `AppendHorizontalConstraintsForViews`.
constexpr CGFloat kHorizontalSpacing = 16;

// Vertical spacing between views. Used when the Keyboard Accessory Upgrade
// feature is disabled.
constexpr CGFloat kVerticalSpacing = 8;

// Generic vertical spacing between views. Delimits the different parts of the
// cell and the chip groups.
constexpr CGFloat kGenericVerticalSpacingBetweenViews = 16;

// Vertical spacing between two chip buttons.
constexpr CGFloat kVerticalSpacingBetweenChips = 4;

// Vertical spacing between two labeled chip buttons.
constexpr CGFloat kVerticalSpacingBetweenLabeledChips = 8;

// Returns the vertical spacing that should be added above a UI element of given
// `type`.
CGFloat GetVerticalSpacingForElementType(
    ManualFillCellView::ElementType element_type) {
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    return kVerticalSpacing;
  }

  switch (element_type) {
    case ManualFillCellView::ElementType::kFirstChipButtonOfGroup:
    case ManualFillCellView::ElementType::kOther:
      return kGenericVerticalSpacingBetweenViews;
    case ManualFillCellView::ElementType::kLabeledChipButton:
      return kVerticalSpacingBetweenLabeledChips;
    case ManualFillCellView::ElementType::kOtherChipButton:
      return kVerticalSpacingBetweenChips;
  }
}

}  // namespace

const CGFloat kCellMargin = 16;
const CGFloat kChipsHorizontalMargin = -1;

UIButton* CreateChipWithSelectorAndTarget(SEL action, id target) {
  UIButton* button = [ChipButton buttonWithType:UIButtonTypeCustom];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    const std::vector<ManualFillCellView>& manual_fill_cell_views,
    UILayoutGuide* layout_guide) {
  AppendVerticalConstraintsSpacingForViews(constraints, manual_fill_cell_views,
                                           layout_guide, 0);
}

void AppendVerticalConstraintsSpacingForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    const std::vector<ManualFillCellView>& manual_fill_cell_views,
    UILayoutGuide* layout_guide,
    CGFloat offset) {
  NSLayoutYAxisAnchor* previous_anchor = layout_guide.topAnchor;
  for (const ManualFillCellView& manual_fill_cell_view :
       manual_fill_cell_views) {
    CGFloat spacing =
        manual_fill_cell_view == manual_fill_cell_views.front()
            ? offset
            : GetVerticalSpacingForElementType(manual_fill_cell_view.type);

    UIView* view = manual_fill_cell_view.view;
    [constraints
        addObject:[view.topAnchor constraintEqualToAnchor:previous_anchor
                                                 constant:spacing]];
    previous_anchor = view.bottomAnchor;
  }

  [constraints
      addObject:[previous_anchor
                    constraintEqualToAnchor:layout_guide.bottomAnchor]];
}

void AddChipGroupsToVerticalLeadViews(
    NSArray<NSArray<UIView*>*>* chip_groups,
    std::vector<ManualFillCellView>& vertical_lead_views) {
  for (NSArray* chip_group in chip_groups) {
    for (UIView* chip in chip_group) {
      ManualFillCellView::ElementType element_type;
      if ([chip isEqual:[chip_group firstObject]]) {
        element_type = ManualFillCellView::ElementType::kFirstChipButtonOfGroup;
      } else if ([chip isKindOfClass:[ManualFillLabeledChip class]]) {
        element_type = ManualFillCellView::ElementType::kLabeledChipButton;
      } else {
        element_type = ManualFillCellView::ElementType::kOtherChipButton;
      }

      AddViewToVerticalLeadViews(chip, element_type, vertical_lead_views);
    }
  }
}

void AddViewToVerticalLeadViews(
    UIView* view,
    ManualFillCellView::ElementType type,
    std::vector<ManualFillCellView>& vertical_lead_views) {
  ManualFillCellView manual_fill_cell_view = {view, type};
  vertical_lead_views.push_back(manual_fill_cell_view);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide) {
  AppendHorizontalConstraintsForViews(constraints, views, layout_guide, 0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin) {
  AppendHorizontalConstraintsForViews(constraints, views, layout_guide, margin,
                                      0);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin,
    AppendConstraints options) {
  if (views.count == 0) {
    return;
  }

  NSLayoutXAxisAnchor* previous_anchor = layout_guide.leadingAnchor;

  BOOL is_first_view = YES;
  for (UIView* view in views) {
    CGFloat spacing = is_first_view ? margin : kHorizontalSpacing;
    [constraints
        addObject:[view.leadingAnchor constraintEqualToAnchor:previous_anchor
                                                     constant:spacing]];
    [view setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
    [view setContentHuggingPriority:UILayoutPriorityDefaultHigh
                            forAxis:UILayoutConstraintAxisHorizontal];
    previous_anchor = view.trailingAnchor;
    is_first_view = NO;
  }

  if (options & AppendConstraintsHorizontalEqualOrSmallerThanGuide) {
    [constraints
        addObject:[views.lastObject.trailingAnchor
                      constraintLessThanOrEqualToAnchor:layout_guide
                                                            .trailingAnchor
                                               constant:-margin]];

  } else {
    [constraints
        addObject:[views.lastObject.trailingAnchor
                      constraintEqualToAnchor:layout_guide.trailingAnchor
                                     constant:-margin]];
    // Give all remaining space to the last button, minus margin, as per UX.
    [views.lastObject
        setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [views.lastObject
        setContentHuggingPriority:UILayoutPriorityDefaultLow
                          forAxis:UILayoutConstraintAxisHorizontal];
  }

  if (options & AppendConstraintsHorizontalSyncBaselines) {
    AppendEqualBaselinesConstraints(constraints, views);
  }
}

void AppendEqualBaselinesConstraints(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views) {
  UIView* leadingView = views.firstObject;
  for (UIView* view in views) {
    DCHECK([view isKindOfClass:[UIButton class]] ||
           [view isKindOfClass:[UILabel class]]);
    if (view == leadingView) {
      continue;
    }
    [constraints
        addObject:[view.lastBaselineAnchor
                      constraintEqualToAnchor:leadingView.lastBaselineAnchor]];
  }
}

UILabel* CreateLabel() {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.adjustsFontForContentSizeCategory = YES;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  return label;
}

NSMutableAttributedString* CreateHeaderAttributedString(NSString* title,
                                                        NSString* subtitle) {
  NSMutableAttributedString* attributed_title =
      [[NSMutableAttributedString alloc]
          initWithString:title
              attributes:@{
                NSForegroundColorAttributeName :
                    [UIColor colorNamed:kTextPrimaryColor],
                NSFontAttributeName : IsKeyboardAccessoryUpgradeEnabled()
                    ? CreateDynamicFont(UIFontTextStyleSubheadline,
                                        UIFontWeightSemibold)
                    : [UIFont
                          preferredFontForTextStyle:UIFontTextStyleHeadline],
              }];

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    NSMutableParagraphStyle* title_paragraph_style =
        [[NSMutableParagraphStyle alloc] init];
    title_paragraph_style.lineSpacing = kHeaderAttributedStringLineSpacing;
    title_paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
    [attributed_title
        addAttributes:@{NSParagraphStyleAttributeName : title_paragraph_style}
                range:NSMakeRange(0, attributed_title.string.length)];
  }

  if (subtitle && subtitle.length) {
    NSMutableAttributedString* attributed_subtitle = [[NSMutableAttributedString
        alloc]
        initWithString:[NSString stringWithFormat:@"\n%@", subtitle]
            attributes:@{
              NSForegroundColorAttributeName :
                  [UIColor colorNamed:kTextSecondaryColor],
              NSFontAttributeName : [UIFont
                  preferredFontForTextStyle:IsKeyboardAccessoryUpgradeEnabled()
                                                ? UIFontTextStyleCaption2
                                                : UIFontTextStyleFootnote],
            }];

    if (IsKeyboardAccessoryUpgradeEnabled()) {
      NSMutableParagraphStyle* subtitle_paragraph_style =
          [[NSMutableParagraphStyle alloc] init];
      subtitle_paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
      [attributed_subtitle
          addAttributes:@{
            NSParagraphStyleAttributeName : subtitle_paragraph_style
          }
                  range:NSMakeRange(0, attributed_subtitle.string.length)];
    }

    [attributed_title appendAttributedString:attributed_subtitle];
  }

  return attributed_title;
}

UIView* CreateGraySeparatorForContainer(UIView* container) {
  UIView* grayLine = [[UIView alloc] init];
  grayLine.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  grayLine.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:grayLine];

  id<LayoutGuideProvider> safeArea = container.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    // Vertical constraints.
    [grayLine.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    [grayLine.heightAnchor constraintEqualToConstant:1],
    // Horizontal constraints.
    [grayLine.leadingAnchor constraintEqualToAnchor:safeArea.leadingAnchor
                                           constant:kCellMargin],
    [safeArea.trailingAnchor constraintEqualToAnchor:grayLine.trailingAnchor
                                            constant:kCellMargin],
  ]];

  return grayLine;
}

UILayoutGuide* AddLayoutGuideToContentView(UIView* content_view) {
  UILayoutGuide* layout_guide = [[UILayoutGuide alloc] init];
  [content_view addLayoutGuide:layout_guide];

  id<LayoutGuideProvider> safe_area = content_view.safeAreaLayoutGuide;
  CGFloat bottom_margin =
      IsKeyboardAccessoryUpgradeEnabled() ? kCellMargin : kCellBottomMargin;
  [NSLayoutConstraint activateConstraints:@[
    [layout_guide.topAnchor constraintEqualToAnchor:content_view.topAnchor
                                           constant:kCellMargin],
    [layout_guide.bottomAnchor constraintEqualToAnchor:content_view.bottomAnchor
                                              constant:-bottom_margin],
    [layout_guide.leadingAnchor constraintEqualToAnchor:safe_area.leadingAnchor
                                               constant:kCellMargin],
    [layout_guide.trailingAnchor
        constraintEqualToAnchor:safe_area.trailingAnchor
                       constant:-kCellMargin],
  ]];

  return layout_guide;
}
