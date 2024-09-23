// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_show_more_item_view.h"

#import "base/notreached.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/segmentation_platform/model/segmented_default_browser_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/crossfade_label.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_icon.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_tap_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kExtraSpacingForSmallerCompleteIcon = 16.0f;

const CGFloat kContentStackViewSpacing = 16.0f;
const CGFloat kContentStackVerticalSpacing = 15.0f;
const CGFloat kIconImageContainerWidth = 64.0f;
const CGFloat kTryButtonWidth = 64.0f;
const CGFloat kTitleDescriptionSpacing = 5.0f;

// Returns an NSAttributedString with strikethrough.
NSAttributedString* Strikethrough(NSString* text) {
  NSDictionary<NSAttributedStringKey, id>* attrs =
      @{NSStrikethroughStyleAttributeName : @(NSUnderlineStyleSingle)};
  return [[NSAttributedString alloc] initWithString:text attributes:attrs];
}

}  // namespace

@implementation SetUpListShowMoreItemView {
  SetUpListItemViewData* _data;
}

- (instancetype)initWithData:(SetUpListItemViewData*)data {
  self = [super init];
  if (self) {
    _data = data;
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

  // Add a horizontal stack to contain the icon(s) and the text stack.
  UIStackView* contentStack = [[UIStackView alloc] init];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.distribution = UIStackViewDistributionFill;
  CGFloat extraSpacingForSmallerCompleteIcon =
      _data.complete ? kExtraSpacingForSmallerCompleteIcon : 0;
  contentStack.spacing =
      kContentStackViewSpacing + extraSpacingForSmallerCompleteIcon;
  [self addSubview:contentStack];
  AddSameConstraintsWithInsets(
      contentStack, self,
      NSDirectionalEdgeInsetsMake(kContentStackVerticalSpacing,
                                  extraSpacingForSmallerCompleteIcon,
                                  kContentStackVerticalSpacing, 0));

  SetUpListItemIcon* icon =
      [[SetUpListItemIcon alloc] initWithType:_data.type
                                     complete:_data.complete
                                compactLayout:NO
                                     inSquare:YES];
  if (!_data.complete) {
    icon.translatesAutoresizingMaskIntoConstraints = NO;
    UIView* imageContainerView = [[UIView alloc] init];
    imageContainerView.backgroundColor = [UIColor colorNamed:kGrey100Color];
    imageContainerView.layer.cornerRadius = 12;
    imageContainerView.layer.masksToBounds = NO;
    imageContainerView.clipsToBounds = YES;
    [imageContainerView addSubview:icon];
    AddSameCenterConstraints(icon, imageContainerView);
    [NSLayoutConstraint activateConstraints:@[
      [imageContainerView.widthAnchor
          constraintEqualToConstant:kIconImageContainerWidth],
      [imageContainerView.widthAnchor
          constraintEqualToAnchor:imageContainerView.heightAnchor],
    ]];
    [contentStack addArrangedSubview:imageContainerView];
  } else {
    [contentStack addArrangedSubview:icon];
  }

  UILabel* title = [self createTitle];
  UILabel* description = [self createDescription];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ title, description ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.spacing = kTitleDescriptionSpacing;
  [contentStack addArrangedSubview:textStack];

  if (!_data.complete) {
    UIButton* tryButton = [[UIButton alloc] init];
    tryButton.backgroundColor =
        [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
    tryButton.titleLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
    NSString* tryButtonTitle =
        l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_TRY_BUTTON_TEXT);
    [tryButton setTitle:tryButtonTitle forState:UIControlStateNormal];
    [tryButton setTitleColor:[UIColor colorNamed:kBlueColor]
                    forState:UIControlStateNormal];
    [tryButton addTarget:self
                  action:@selector(tryTapped)
        forControlEvents:UIControlEventTouchUpInside];
    NSString* itemTitle = [self titleText];
    tryButton.accessibilityIdentifier =
        [NSString stringWithFormat:@"%@ Try Button", itemTitle];
    tryButton.accessibilityLabel =
        [NSString stringWithFormat:@"%@, %@", tryButtonTitle, itemTitle];
    tryButton.layer.cornerRadius = 15;
    tryButton.pointerInteractionEnabled = YES;
    [NSLayoutConstraint activateConstraints:@[
      [tryButton.widthAnchor constraintEqualToConstant:kTryButtonWidth],
    ]];
    [contentStack addArrangedSubview:tryButton];
    self.accessibilityHint = l10n_util::GetNSString(
        IDS_IOS_SET_UP_LIST_TRY_BUTTON_ACCESSIBILITY_HINT);
  } else {
    self.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }

  self.isAccessibilityElement = YES;
  self.accessibilityLabel =
      [NSString stringWithFormat:@"%@, %@", title.text, description.text];
}

// Creates the title label.
- (CrossfadeLabel*)createTitle {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label.text = [self titleText];
  label.accessibilityIdentifier = set_up_list::kAccessibilityID;
  label.font =
      CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
  if (_data.complete) {
    label.textColor = [UIColor colorNamed:kTextQuaternaryColor];
    label.attributedText = Strikethrough(label.text);
  } else {
    label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return label;
}

// Returns the text for the title label.
- (NSString*)titleText {
  switch (_data.type) {
    case SetUpListItemType::kSignInSync:
      return l10n_util::GetNSString(
          IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_TITLE);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_AUTOFILL_TITLE);
    case SetUpListItemType::kNotifications:
      return IsIOSTipsNotificationsEnabled()
                 ? l10n_util::GetNSString(
                       IDS_IOS_SET_UP_LIST_NOTIFICATIONS_TITLE)
                 : l10n_util::GetNSString(
                       IDS_IOS_SET_UP_LIST_CONTENT_NOTIFICATION_TITLE);
    case SetUpListItemType::kAllSet:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_ALL_SET_TITLE);
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/40262090): Add a Follow item to the Set Up List.
      NOTREACHED();
  }
}

// Creates the description label.
- (CrossfadeLabel*)createDescription {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label = [[CrossfadeLabel alloc] init];
  label.text = [self descriptionText];
  label.accessibilityIdentifier = set_up_list::kAccessibilityID;
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  //  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  if (_data.complete) {
    label.textColor = [UIColor colorNamed:kTextQuaternaryColor];
    label.attributedText = Strikethrough(label.text);
  } else {
    label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return label;
}

// Returns the text for the description label.
- (NSString*)descriptionText {
  switch (_data.type) {
    case SetUpListItemType::kSignInSync:
      return l10n_util::GetNSString(IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(
          IsSegmentedDefaultBrowserPromoEnabled()
              ? GetSetUpListDefaultBrowserDescriptionStringID(_data.userSegment)
              : IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SEE_MORE_DESCRIPTION);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_AUTOFILL_SEE_MORE_DESCRIPTION);
    case SetUpListItemType::kNotifications:
      return IsIOSTipsNotificationsEnabled()
                 ? l10n_util::GetNSString(
                       IDS_IOS_SET_UP_LIST_NOTIFICATIONS_DESCRIPTION)
                 : l10n_util::GetNSString(
                       IDS_IOS_SET_UP_LIST_CONTENT_NOTIFICATION_DESCRIPTION);
    case SetUpListItemType::kAllSet:
    case SetUpListItemType::kFollow:
      NOTREACHED();
  }
}

// Handles button tap.
- (void)tryTapped {
  [self.tapDelegate didSelectSetUpListItem:_data.type];
}

#pragma mark - UIAccessibility

- (BOOL)accessibilityActivate {
  if (_data.complete) {
    return NO;
  }
  [self tryTapped];
  return YES;
}

@end
