// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_view.h"

#import "base/strings/string_number_conversions.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/safety_check_item_icon.h"
#import "ios/chrome/browser/ui/content_suggestions/safety_check/types.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The spacing between the title and description.
constexpr CGFloat kTitleDescriptionSpacing = 2;

// The spacing between elements within the item.
constexpr CGFloat kContentStackSpacing = 14;

// Constants related to the icon container view.
constexpr CGFloat kIconContainerSize = 56;
constexpr CGFloat kIconContainerCornerRadius = 12;

// The size of the checkmark icon.
constexpr CGFloat kCheckmarkSize = 19;
constexpr CGFloat kCheckmarkTopOffset = -6;
constexpr CGFloat kCheckmarkTrailingOffset = 6;

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

// Returns the number of password issue types found given
// `weak_passwords_count`, `reused_passwords_count`, and
// `compromised_passwords_count`.
int PasswordIssuesTypeCount(NSInteger weak_passwords_count,
                            NSInteger reused_passwords_count,
                            NSInteger compromised_passwords_count) {
  int count = 0;

  if (weak_passwords_count > 0) {
    count++;
  }

  if (reused_passwords_count > 0) {
    count++;
  }

  if (compromised_passwords_count > 0) {
    count++;
  }

  return count;
}

}  // namespace

@implementation SafetyCheckItemView {
  // The item layout type.
  SafetyCheckItemLayoutType _layoutType;
  // The number of weak passwords found by the Password check.
  NSInteger _weakPasswordsCount;
  // The number of reused passwords found by the Password check.
  NSInteger _reusedPasswordsCount;
  // The number of compromised passwords found by the Password check.
  NSInteger _compromisedPasswordsCount;
  // UI tap gesture recognizer.
  UITapGestureRecognizer* _tapGestureRecognizer;
}

- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                      layoutType:(SafetyCheckItemLayoutType)layoutType {
  self = [self initWithItemType:itemType
                     layoutType:layoutType
             weakPasswordsCount:0
           reusedPasswordsCount:0
      compromisedPasswordsCount:0];

  return self;
}

- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                      layoutType:(SafetyCheckItemLayoutType)layoutType
              weakPasswordsCount:(NSInteger)weakPasswordsCount
            reusedPasswordsCount:(NSInteger)reusedPasswordsCount
       compromisedPasswordsCount:(NSInteger)compromisedPasswordsCount {
  if (self = [super init]) {
    _itemType = itemType;
    _layoutType = layoutType;
    _weakPasswordsCount = weakPasswordsCount;
    _reusedPasswordsCount = reusedPasswordsCount;
    _compromisedPasswordsCount = compromisedPasswordsCount;
  }

  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

- (NSString*)accessibilityLabel {
  return
      [NSString stringWithFormat:@"%@, %@", [self titleText],
                                 _layoutType == SafetyCheckItemLayoutType::kHero
                                     ? [self descriptionText]
                                     : [self compactDescriptionText]];
}

#pragma mark - Private

- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.tapDelegate didTapSafetyCheckItemView:self];
  }
}

// Creates all views for an individual check row in the Safety Check (Magic
// Stack) module.
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
                                         layoutType:_layoutType];

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
                                            constant:kCheckmarkTopOffset],
        [checkmark.trailingAnchor
            constraintEqualToAnchor:iconContainerView.trailingAnchor
                           constant:kCheckmarkTrailingOffset],
      ]];
    }

    [arrangedSubviews addObject:iconContainerView];
  } else {
    [arrangedSubviews addObject:icon];
  }

  UILabel* titleLabel = [self createTitleLabelForLayoutType:_layoutType];

  [titleLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  UILabel* descriptionLabel = [self createDescriptionLabel];

  [descriptionLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  self.accessibilityLabel =
      [NSString stringWithFormat:@"%@,%@", titleLabel, descriptionLabel];

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

  AddSameConstraints(contentStack, self);

  // Set up the tap gesture recognizer.
  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];

  [self addGestureRecognizer:_tapGestureRecognizer];
}

// Returns the corresponding `SafetyCheckItemIcon*` given an `itemType` and
// `layoutType`.
- (SafetyCheckItemIcon*)iconForItemType:(SafetyCheckItemType)itemType
                             layoutType:(SafetyCheckItemLayoutType)layoutType {
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

  label.text = [self titleText];
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

- (NSString*)titleText {
  switch (_itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_RUNNING);
    case SafetyCheckItemType::kUpdateChrome:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_UPDATE_CHROME);
    case SafetyCheckItemType::kPassword:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_PASSWORD);
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

// Creates the description label.
- (UILabel*)createDescriptionLabel {
  UILabel* label = [[UILabel alloc] init];

  label.text = _layoutType == SafetyCheckItemLayoutType::kHero
                   ? [self descriptionText]
                   : [self compactDescriptionText];
  label.numberOfLines = 2;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

- (NSString*)descriptionText {
  switch (_itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_DEFAULT);
  }
}

- (NSString*)updateChromeItemDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_DESCRIPTION_UPDATE_CHROME);
  }
}

- (NSString*)passwordItemDescriptionText {
  int passwordIssueTypeCount = PasswordIssuesTypeCount(
      _weakPasswordsCount, _reusedPasswordsCount, _compromisedPasswordsCount);

  if (passwordIssueTypeCount > 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
  }

  if (_weakPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_WEAK_PASSWORDS,
        base::NumberToString16(_weakPasswordsCount));
  }

  if (_weakPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_WEAK_PASSWORD);
  }

  if (_reusedPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_REUSED_PASSWORDS,
        base::NumberToString16(_reusedPasswordsCount));
  }

  if (_reusedPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_REUSED_PASSWORD);
  }

  if (_compromisedPasswordsCount > 1) {
    return l10n_util::GetNSStringF(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_MULTIPLE_COMPROMISED_PASSWORDS,
        base::NumberToString16(_compromisedPasswordsCount));
  }

  if (_compromisedPasswordsCount == 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
}

- (NSString*)compactDescriptionText {
  switch (_itemType) {
    case SafetyCheckItemType::kAllSafe:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_DESCRIPTION_ALL_SAFE);
    case SafetyCheckItemType::kRunning:
      // The running state has no description text.
      return @"";
    case SafetyCheckItemType::kUpdateChrome:
      return [self updateChromeItemCompactDescriptionText];
    case SafetyCheckItemType::kPassword:
      return [self passwordItemCompactDescriptionText];
    case SafetyCheckItemType::kSafeBrowsing:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_SAFE_BROWSING);
    case SafetyCheckItemType::kDefault:
      return l10n_util::GetNSString(IDS_IOS_SAFETY_CHECK_TITLE_DEFAULT);
  }
}

- (NSString*)updateChromeItemCompactDescriptionText {
  switch (::GetChannel()) {
    case version_info::Channel::STABLE:
    case version_info::Channel::DEV:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
    case version_info::Channel::BETA:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
    case version_info::Channel::CANARY:
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
    default:
      return l10n_util::GetNSString(
          IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_UPDATE_CHROME);
  }
}

- (NSString*)passwordItemCompactDescriptionText {
  int passwordIssueTypeCount = PasswordIssuesTypeCount(
      _weakPasswordsCount, _reusedPasswordsCount, _compromisedPasswordsCount);

  if (passwordIssueTypeCount > 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
  }

  if (_weakPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_WEAK_PASSWORD);
  }

  if (_reusedPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_REUSED_PASSWORD);
  }

  if (_compromisedPasswordsCount >= 1) {
    return l10n_util::GetNSString(
        IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_COMPROMISED_PASSWORD);
  }

  return l10n_util::GetNSString(
      IDS_IOS_SAFETY_CHECK_COMPACT_DESCRIPTION_MULTIPLE_PASSWORD_ISSUES);
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
