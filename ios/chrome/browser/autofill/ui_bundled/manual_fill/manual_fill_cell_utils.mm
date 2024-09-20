// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/chip_button.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_labeled_chip.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Vertical insets of the "Autofill Form" button.
constexpr CGFloat kAutofillFormButtonVerticalInsets = 11;

// Minimum height of the "Autofill Form" button.
constexpr CGFloat kAutofillFormButtonMinHeight = 44;

// Bottom margin for the cell content. Used when the Keyboard Accessory Upgrade
// feature is disabled.
constexpr CGFloat kCellBottomMargin = 18;

// Line spacing for the cell's header title.
constexpr CGFloat kHeaderAttributedStringLineSpacing = 2;

// Font size for the cell's header title.
constexpr CGFloat kHeaderAttributedStringTitleFontSize = 15;

// Top and bottom padding for the header view label.
constexpr CGFloat kHeaderViewLabelVerticalPadding = 6;

// Minimum height for the header view.
constexpr CGFloat kHeaderViewMinHeight = 44;

// Height of the grey separator.
constexpr CGFloat kSeparatorHeight = 1;

// Horizontal spacing between views. Used when the Keyboard Accessory Upgrade
// feature is disabled.
constexpr CGFloat kHorizontalSpacing = 16;

// Vertical spacing between views. Used when the Keyboard Accessory Upgrade
// feature is disabled.
constexpr CGFloat kVerticalSpacing = 8;

// Generic vertical spacing between views. Delimits the different parts of the
// cell and the chip groups.
constexpr CGFloat kGenericVerticalSpacingBetweenViews = 16;

// Small vertical and horizontal spacing between views. Used to visually group
// chips together and as vertical padding for the cell's header.
constexpr CGFloat kSmallSpacingBetweenViews = 4;

// Vertical spacing between two labeled chip buttons.
constexpr CGFloat kVerticalSpacingBetweenLabeledChips = 8;

// Height and width of the overflow menu button displayed in the cell's header.
constexpr CGFloat kOverflowMenuButtonSize = 24;

// Minimum spacing that a cell's trailing view should have with the view on its
// left.
constexpr CGFloat kTrailingViewMinLeadingSpacing = 8;

// Top and bottom padding for the virtual card instruction view.
constexpr CGFloat kVirtualCardInstructionsVerticalPadding = 8;

// Adds all baseline anchor constraints for the given `views` to match the first
// one. Constraints are not activated.
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

    if (IsKeyboardAccessoryUpgradeEnabled()) {
      [constraints
          addObject:[view.centerYAnchor
                        constraintEqualToAnchor:leadingView.centerYAnchor]];
    } else {
      [constraints
          addObject:[view.lastBaselineAnchor
                        constraintEqualToAnchor:leadingView
                                                    .lastBaselineAnchor]];
    }
  }
}

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
    case ManualFillCellView::ElementType::kHeaderSeparator:
    case ManualFillCellView::ElementType::kOtherChipButton:
      return kSmallSpacingBetweenViews;
    case ManualFillCellView::ElementType::kVirtualCardInstructions:
    case ManualFillCellView::ElementType::kVirtualCardInstructionsSeparator:
      return kVirtualCardInstructionsVerticalPadding;
  }
}

// Returns the width of the given `layout_guide`.
CGFloat GetLayoutGuideWidth(UILayoutGuide* layout_guide) {
  return layout_guide.layoutFrame.size.width;
}

// Returns the width of the given `view`.
CGFloat GetViewWidth(UIView* view) {
  return view.intrinsicContentSize.width;
}

// Returns the font for the cell's header title.
UIFont* TitleFont() {
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    UIFont* font = [UIFont systemFontOfSize:kHeaderAttributedStringTitleFontSize
                                     weight:UIFontWeightMedium];
    return [[UIFontMetrics defaultMetrics] scaledFontForFont:font];
  } else {
    return [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  }
}

// Creates and adds constraints to `constraints`, so as to horizontally lay out
// the given `views`, while taking the `trailing_view` into account. Then, adds
// the first view to `vertical_lead_views` to mark the start of a new row of
// views.
void LayViewsHorizontally(NSArray<UIView*>* views,
                          UILayoutGuide* guide,
                          NSMutableArray<NSLayoutConstraint*>* constraints,
                          NSMutableArray<UIView*>* vertical_lead_views,
                          UIView* trailing_view) {
  AppendHorizontalConstraintsForViews(
      constraints, views, guide, 0,
      AppendConstraintsHorizontalSyncBaselines |
          AppendConstraintsHorizontalEqualOrSmallerThanGuide,
      trailing_view);
  [vertical_lead_views addObject:views.firstObject];
}

}  // namespace

const CGFloat kCellMargin = 16;
const CGFloat kChipsHorizontalMargin = -1;

CGFloat GetHorizontalSpacingBetweenChips() {
  return IsKeyboardAccessoryUpgradeEnabled() ? kSmallSpacingBetweenViews
                                             : kHorizontalSpacing;
}

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
  AppendHorizontalConstraintsForViews(constraints, views, layout_guide, margin,
                                      options, nil);
}

void AppendHorizontalConstraintsForViews(
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSArray<UIView*>* views,
    UILayoutGuide* layout_guide,
    CGFloat margin,
    AppendConstraints options,
    UIView* trailing_view) {
  if (views.count == 0) {
    return;
  }

  NSLayoutXAxisAnchor* previous_anchor = layout_guide.leadingAnchor;

  BOOL is_first_view = YES;
  for (UIView* view in views) {
    CGFloat spacing =
        is_first_view ? margin : GetHorizontalSpacingBetweenChips();

    NSLayoutConstraint* constraint =
        [view.leadingAnchor constraintEqualToAnchor:previous_anchor
                                           constant:spacing];

    if (!is_first_view && IsKeyboardAccessoryUpgradeEnabled()) {
      // Set the in-between view constraints to a low priority so that the views
      // can be easily reorganized when the width of the `layout_guide` changes.
      constraint.priority = UILayoutPriorityDefaultLow;
    }
    [constraints addObject:constraint];

    if (!IsKeyboardAccessoryUpgradeEnabled()) {
      [view
          setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      [view setContentHuggingPriority:UILayoutPriorityDefaultHigh
                              forAxis:UILayoutConstraintAxisHorizontal];
    }

    previous_anchor = view.trailingAnchor;
    is_first_view = NO;
  }

  // If there's a `trailing_view`, constraint the trailing anchor of the last
  // view to `trailing_view`'s leading anchor. Otherwise constraint the last
  // view's trailing anchor to the `layout_guide`'s trailing anchor.
  UIView* last_view = views.lastObject;
  if (trailing_view) {
    [constraints
        addObject:
            [last_view.trailingAnchor
                constraintLessThanOrEqualToAnchor:trailing_view.leadingAnchor
                                         constant:
                                             -kTrailingViewMinLeadingSpacing]];
    [constraints
        addObject:[trailing_view.trailingAnchor
                      constraintEqualToAnchor:layout_guide.trailingAnchor]];

  } else if (options & AppendConstraintsHorizontalEqualOrSmallerThanGuide) {
    [constraints
        addObject:[last_view.trailingAnchor
                      constraintLessThanOrEqualToAnchor:layout_guide
                                                            .trailingAnchor
                                               constant:-margin]];

  } else {
    [constraints
        addObject:[last_view.trailingAnchor
                      constraintEqualToAnchor:layout_guide.trailingAnchor
                                     constant:-margin]];
    if (!IsKeyboardAccessoryUpgradeEnabled()) {
      // Give all remaining space to the last button, minus margin, as per UX.
      [last_view
          setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                          forAxis:
                                              UILayoutConstraintAxisHorizontal];
      [last_view setContentHuggingPriority:UILayoutPriorityDefaultLow
                                   forAxis:UILayoutConstraintAxisHorizontal];
    }
  }

  if (options & AppendConstraintsHorizontalSyncBaselines) {
    AppendEqualBaselinesConstraints(constraints, views);
  }
}

void LayViewsHorizontallyWhenPossible(
    NSArray<UIView*>* views,
    UILayoutGuide* guide,
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSMutableArray<UIView*>* vertical_lead_views) {
  LayViewsHorizontallyWhenPossible(views, guide, constraints,
                                   vertical_lead_views, nil);
}

void LayViewsHorizontallyWhenPossible(
    NSArray<UIView*>* views,
    UILayoutGuide* guide,
    NSMutableArray<NSLayoutConstraint*>* constraints,
    NSMutableArray<UIView*>* vertical_lead_views,
    UIView* first_row_trailing_view) {
  if (!views || !views.count) {
    return;
  }

  // `first_row_trailing_view` should only be considered for the first row.
  BOOL should_consider_trailing_view = first_row_trailing_view;
  CGFloat available_width = GetLayoutGuideWidth(guide);
  if (should_consider_trailing_view) {
    // Remove the width of the `first_row_trailing_view` and its leading spacing
    // from the `available_width`.
    available_width -= (GetViewWidth(first_row_trailing_view) +
                        kTrailingViewMinLeadingSpacing);
  }
  NSMutableArray<UIView*>* horizontal_views = [[NSMutableArray alloc] init];

  for (UIView* view in views) {
    CGFloat view_width = GetViewWidth(view);
    BOOL fits_horizontally =
        !horizontal_views.count || available_width - view_width >= 0;

    if (fits_horizontally) {
      [horizontal_views addObject:view];
      available_width -= (view_width + GetHorizontalSpacingBetweenChips());
    } else {
      LayViewsHorizontally(
          horizontal_views, guide, constraints, vertical_lead_views,
          should_consider_trailing_view ? first_row_trailing_view : nil);
      should_consider_trailing_view = NO;

      // Start new row of views.
      [horizontal_views removeAllObjects];
      [horizontal_views addObject:view];
      available_width = GetLayoutGuideWidth(guide) - view_width -
                        GetHorizontalSpacingBetweenChips();
    }
  }

  LayViewsHorizontally(
      horizontal_views, guide, constraints, vertical_lead_views,
      should_consider_trailing_view ? first_row_trailing_view : nil);
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
                NSFontAttributeName : TitleFont(),
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
                                                ? UIFontTextStyleCaption1
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

UIStackView* CreateHeaderView(UIView* icon,
                              UILabel* label,
                              UIButton* overflow_menu_button) {
  NSMutableArray<UIView*>* subviews = [[NSMutableArray alloc] init];
  if (icon) {
    [subviews addObject:icon];
  }
  CHECK(label);
  [subviews addObject:label];
  if (overflow_menu_button) {
    [subviews addObject:overflow_menu_button];
  }

  UIStackView* header_view =
      [[UIStackView alloc] initWithArrangedSubviews:subviews];
  header_view.translatesAutoresizingMaskIntoConstraints = NO;
  header_view.spacing = UIStackViewSpacingUseSystem;  // Spacing of 8px.
  header_view.alignment = UIStackViewAlignmentCenter;

  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [NSLayoutConstraint activateConstraints:@[
      [header_view.heightAnchor
          constraintGreaterThanOrEqualToConstant:kHeaderViewMinHeight],
      [label.topAnchor constraintEqualToAnchor:header_view.topAnchor
                                      constant:kHeaderViewLabelVerticalPadding],
      [label.bottomAnchor
          constraintEqualToAnchor:header_view.bottomAnchor
                         constant:-kHeaderViewLabelVerticalPadding],
    ]];
  }

  return header_view;
}

UIButton* CreateOverflowMenuButton() {
  ExtendedTouchTargetButton* menu_button =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  menu_button.translatesAutoresizingMaskIntoConstraints = NO;
  menu_button.contentMode = UIViewContentModeCenter;
  menu_button.accessibilityIdentifier =
      manual_fill::kExpandedManualFillOverflowMenuID;

  UIImage* menu_image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kEllipsisCircleFillSymbol,
                                 kOverflowMenuButtonSize),
      @[
        [UIColor colorNamed:kBlue600Color], [UIColor tertiarySystemFillColor]
      ]);
  [menu_button setImage:menu_image forState:UIControlStateNormal];

  [menu_button setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
  [menu_button
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  menu_button.showsMenuAsPrimaryAction = YES;

  return menu_button;
}

UIView* CreateGraySeparatorForContainer(UIView* container) {
  UIView* gray_line = [[UIView alloc] init];
  gray_line.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  gray_line.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:gray_line];

  id<LayoutGuideProvider> safe_area = container.safeAreaLayoutGuide;
  if (IsKeyboardAccessoryUpgradeEnabled()) {
    [NSLayoutConstraint activateConstraints:@[
      // Vertical constraints.
      [gray_line.heightAnchor constraintEqualToConstant:kSeparatorHeight],
      // Horizontal constraints.
      [gray_line.leadingAnchor constraintEqualToAnchor:safe_area.leadingAnchor
                                              constant:kCellMargin],
      [safe_area.trailingAnchor
          constraintEqualToAnchor:gray_line.trailingAnchor],
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      // Vertical constraints.
      [gray_line.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
      [gray_line.heightAnchor constraintEqualToConstant:kSeparatorHeight],
      // Horizontal constraints.
      [gray_line.leadingAnchor constraintEqualToAnchor:safe_area.leadingAnchor
                                              constant:kCellMargin],
      [safe_area.trailingAnchor constraintEqualToAnchor:gray_line.trailingAnchor
                                               constant:kCellMargin],
    ]];
  }

  return gray_line;
}

UIButton* CreateAutofillFormButton() {
  UIButton* button = PrimaryActionButton(/*pointer_interaction_enabled=*/YES);
  button.accessibilityIdentifier =
      manual_fill::kExpandedManualFillAutofillFormButtonID;
  UIButtonConfiguration* buttonConfiguration = button.configuration;
  buttonConfiguration.contentInsets =
      NSDirectionalEdgeInsetsMake(kAutofillFormButtonVerticalInsets, 0,
                                  kAutofillFormButtonVerticalInsets, 0);
  button.configuration = buttonConfiguration;

  [button.heightAnchor
      constraintGreaterThanOrEqualToConstant:kAutofillFormButtonMinHeight]
      .active = YES;

  SetConfigurationTitle(
      button, l10n_util::GetNSString(
                  IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_TITLE));

  return button;
}

UILayoutGuide* AddLayoutGuideToContentView(UIView* content_view,
                                           BOOL cell_has_header) {
  UILayoutGuide* layout_guide = [[UILayoutGuide alloc] init];
  [content_view addLayoutGuide:layout_guide];

  id<LayoutGuideProvider> safe_area = content_view.safeAreaLayoutGuide;
  CGFloat top_margin = cell_has_header && IsKeyboardAccessoryUpgradeEnabled()
                           ? kSmallSpacingBetweenViews
                           : kCellMargin;
  CGFloat bottom_margin =
      IsKeyboardAccessoryUpgradeEnabled() ? kCellMargin : kCellBottomMargin;
  [NSLayoutConstraint activateConstraints:@[
    [layout_guide.topAnchor constraintEqualToAnchor:content_view.topAnchor
                                           constant:top_margin],
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

NSMutableAttributedString* CreateSiteNameLabelAttributedText(
    ManualFillSiteInfo* site_info,
    BOOL should_show_host) {
  NSString* siteName = site_info.siteName ? site_info.siteName : @"";
  NSString* host;
  NSMutableAttributedString* attributedString;

  if (should_show_host) {
    if (IsKeyboardAccessoryUpgradeEnabled()) {
      host = site_info.host;
    }
    // If the Keyboard Accessory Upgrade feature is disabled, `host` will be
    // `nil` here, and so it won't be added to `attributedString` right away.
    attributedString = CreateHeaderAttributedString(siteName, host);

    if (!IsKeyboardAccessoryUpgradeEnabled()) {
      host = [NSString stringWithFormat:@" –– %@", site_info.host];
      NSDictionary* attributes = @{
        NSForegroundColorAttributeName :
            [UIColor colorNamed:kTextSecondaryColor],
        NSFontAttributeName :
            [UIFont preferredFontForTextStyle:UIFontTextStyleBody]
      };
      NSAttributedString* hostAttributedString =
          [[NSAttributedString alloc] initWithString:host
                                          attributes:attributes];
      [attributedString appendAttributedString:hostAttributedString];
    }
  } else {
    attributedString = CreateHeaderAttributedString(siteName, nil);
  }

  return attributedString;
}

void GiveAccessibilityContextToCellAndButton(UIView* cell_container,
                                             UIButton* overflow_menu_button,
                                             UIButton* autofill_form_button,
                                             NSString* accessibility_context) {
  CHECK(cell_container);
  CHECK(overflow_menu_button);
  CHECK(autofill_form_button);

  cell_container.accessibilityLabel = accessibility_context;
  overflow_menu_button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_THREE_DOT_MENU_BUTTON_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(accessibility_context));
  autofill_form_button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_AUTOFILL_FORM_BUTTON_ACCESSIBILITY_LABEL,
      base::SysNSStringToUTF16(accessibility_context));
}

void SetUpCellAccessibilityElements(TableViewCell* cell,
                                    NSArray<UIView*>* accessibilityElements) {
  // If the Keyboard Accessory Upgrade feature is disabled, keep the default
  // accessibility behaviour.
  if (!IsKeyboardAccessoryUpgradeEnabled()) {
    return;
  }

  // The following two lines are needed to make the cell as a container, as well
  // as its content, accessible.
  cell.isAccessibilityElement = NO;
  cell.contentView.isAccessibilityElement = YES;

  cell.accessibilityElements = accessibilityElements;
}
