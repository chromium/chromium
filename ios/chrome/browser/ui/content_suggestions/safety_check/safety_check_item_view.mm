// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_view.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_icon.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"

namespace {

// The spacing between the title and description.
constexpr CGFloat kTitleDescriptionSpacing = 5;

// The spacing between elements within the item.
constexpr CGFloat kContentStackSpacing = 16;

// Constants related to the icon container view.
constexpr CGFloat kIconContainerSize = 56;
constexpr CGFloat kIconContainerCornerRadius = 12;

// The size of the checkmark icon.
constexpr CGFloat kCheckmarkSize = 16;

// The checkmark icon used for a hero-cell complete item.
UIImageView* CheckmarkIcon() {
  UIImageSymbolConfiguration* config = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightMedium];

  UIImageSymbolConfiguration* colorConfig =
      [UIImageSymbolConfiguration configurationWithPaletteColors:@[
        [UIColor whiteColor], [UIColor colorNamed:kGreen500Color]
      ]];

  config = [config configurationByApplyingConfiguration:colorConfig];

  UIImage* image =
      DefaultSymbolWithConfiguration(kCheckmarkCircleFillSymbol, config);

  UIImageView* icon = [[UIImageView alloc] initWithImage:image];

  icon.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [icon.widthAnchor constraintEqualToConstant:kCheckmarkSize],
    [icon.heightAnchor constraintEqualToAnchor:icon.widthAnchor],
  ]];

  return icon;
}

}  // namespace

@implementation SafetyCheckItemView {
  // The item type.
  SafetyCheckItemType _itemType;
  // The item layout type.
  SafetyCheckItemLayoutType _layoutType;
}

- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                   andLayoutType:(SafetyCheckItemLayoutType)layoutType {
  if (self = [super init]) {
    _itemType = itemType;
    _layoutType = layoutType;
  }

  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

#pragma mark - Private

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier =
      [self accessibilityIdentifierForItemType:_itemType];
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  // Add a horizontal stack to contain the icon, text stack, and (optional)
  // chevron.
  NSMutableArray* arrangedSubviews = [[NSMutableArray alloc] init];

  SafetyCheckItemIcon* icon = [self iconForItemType:_itemType
                                      andLayoutType:_layoutType];

  // When the item is displayed in a hero-style layout, the icon is more
  // prominently displayed via an icon container view.
  if (_layoutType == SafetyCheckItemLayoutType::kHero) {
    UIView* iconContainerView = [self iconInContainer:icon];

    // Display a green checkmark when the layout is hero-cell complete.
    if (_itemType == SafetyCheckItemType::kAllSafe) {
      UIImageView* checkmark = CheckmarkIcon();

      [iconContainerView addSubview:checkmark];

      [NSLayoutConstraint activateConstraints:@[
        [checkmark.topAnchor constraintEqualToAnchor:iconContainerView.topAnchor
                                            constant:-(0.3 * kCheckmarkSize)],
        [checkmark.trailingAnchor
            constraintEqualToAnchor:iconContainerView.trailingAnchor
                           constant:(0.4 * kCheckmarkSize)],
      ]];
    }

    [arrangedSubviews addObject:iconContainerView];
  } else {
    [arrangedSubviews addObject:icon];
  }

  UILabel* titleLabel = [self createTitleLabelForLayoutType:_layoutType];
  UILabel* descriptionLabel = [self createDescriptionLabel];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];

  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = kTitleDescriptionSpacing;
  [textStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                               forAxis:UILayoutConstraintAxisHorizontal];

  [arrangedSubviews addObject:textStack];

  // For compact layout, display a chevron at the end of the item.
  if (_layoutType == SafetyCheckItemLayoutType::kCompact) {
    UIImageView* chevron = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];

    [chevron setContentHuggingPriority:UILayoutPriorityDefaultHigh
                               forAxis:UILayoutConstraintAxisHorizontal];

    [arrangedSubviews addObject:chevron];
  }

  UIStackView* contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];

  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.axis = UILayoutConstraintAxisHorizontal;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.spacing = kContentStackSpacing;

  [self addSubview:contentStack];

  if (_layoutType == SafetyCheckItemLayoutType::kCompact) {
    AddSameConstraints(contentStack, self);
  }
}

// Returns the corresponding `SafetyCheckItemIcon*` given an `itemType` and
// `layoutType`.
- (SafetyCheckItemIcon*)iconForItemType:(SafetyCheckItemType)itemType
                          andLayoutType:(SafetyCheckItemLayoutType)layoutType {
  BOOL compactLayout = layoutType == SafetyCheckItemLayoutType::kCompact;
  BOOL inSquare = YES;

  switch (itemType) {
    case SafetyCheckItemType::kUpdateChrome:
      return
          [[SafetyCheckItemIcon alloc] initWithDefaultSymbol:kInfoCircleSymbol
                                               compactLayout:compactLayout
                                                    inSquare:inSquare];
    case SafetyCheckItemType::kPassword:
      return [[SafetyCheckItemIcon alloc] initWithCustomSymbol:kPasswordSymbol
                                                 compactLayout:compactLayout
                                                      inSquare:inSquare];
    case SafetyCheckItemType::kSafeBrowsing:
      return [[SafetyCheckItemIcon alloc] initWithCustomSymbol:kPrivacySymbol
                                                 compactLayout:compactLayout
                                                      inSquare:inSquare];
    case SafetyCheckItemType::kAllSafe:
    case SafetyCheckItemType::kRunning:
    case SafetyCheckItemType::kDefault:
      return
          [[SafetyCheckItemIcon alloc] initWithCustomSymbol:kSafetyCheckSymbol
                                              compactLayout:compactLayout
                                                   inSquare:inSquare];
  }
}

// Returns `icon` wrapped in a container view.
- (UIView*)iconInContainer:(SafetyCheckItemIcon*)icon {
  icon.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* iconContainer = [[UIView alloc] init];

  iconContainer.backgroundColor = [UIColor colorNamed:kGrey100Color];
  iconContainer.layer.cornerRadius = kIconContainerCornerRadius;

  [iconContainer addSubview:icon];

  AddSameCenterConstraints(icon, iconContainer);

  [NSLayoutConstraint activateConstraints:@[
    [iconContainer.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [iconContainer.widthAnchor
        constraintEqualToAnchor:iconContainer.heightAnchor],
  ]];

  return iconContainer;
}

// Creates the title label using `layoutType`.
- (UILabel*)createTitleLabelForLayoutType:
    (SafetyCheckItemLayoutType)layoutType {
  UILabel* label = [[UILabel alloc] init];

  // NOTE: In subsequent CL, `label.text` will come from `ios_strings`.
  label.text = @"Title";
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font =
      layoutType == SafetyCheckItemLayoutType::kHero
          ? CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold)
          : [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];

  return label;
}

// Creates the description label.
- (UILabel*)createDescriptionLabel {
  UILabel* label = [[UILabel alloc] init];

  // NOTE: In subsequent CL, `label.text` will come from `ios_strings`.
  label.text = @"Description";
  label.numberOfLines = 2;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

- (NSString*)accessibilityIdentifierForItemType:(SafetyCheckItemType)itemType {
  switch (itemType) {
    case SafetyCheckItemType::kAllSafe:
      return safety_check::kAllSafeItemID;
    case SafetyCheckItemType::kRunning:
      return safety_check::kRunningItemID;
    case SafetyCheckItemType::kUpdateChrome:
      return safety_check::kUpdateChromeItemID;
    case SafetyCheckItemType::kPassword:
      return safety_check::kPasswordItemID;
    case SafetyCheckItemType::kSafeBrowsing:
      return safety_check::kSafeBrowsingItemID;
    case SafetyCheckItemType::kDefault:
      return safety_check::kDefaultItemID;
  }
}

@end
