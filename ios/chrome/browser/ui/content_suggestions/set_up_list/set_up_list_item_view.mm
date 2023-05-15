// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"

#import "base/notreached.h"
#import "base/time/time.h"
#import "ios/chrome/browser/ntp/set_up_list_item_type.h"
#import "ios/chrome/browser/shared/ui/elements/crossfade_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_icon.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view+private.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The padding between the icon and the text.
constexpr CGFloat kPadding = 15;

// The spacing between the title and description labels.
constexpr CGFloat kTextSpacing = 5;

// The duration of the icon / label crossfade animation.
constexpr base::TimeDelta kAnimationDuration = base::Seconds(0.5);

// The duration of the "sparkle" animation.
constexpr base::TimeDelta kAnimationSparkleDuration = kAnimationDuration * 2;

// The delay between the crossfade animation and the start of the "sparkle".
constexpr base::TimeDelta kAnimationSparkleDelay = kAnimationDuration * 0.5;

// Accessibility IDs used for various UI items.
constexpr NSString* const kSetUpListItemSignInID = @"kSetUpListItemSignInID";
constexpr NSString* const kSetUpListItemDefaultBrowserID =
    @"kSetUpListItemDefaultBrowserID";
constexpr NSString* const kSetUpListItemAutofillID =
    @"kSetUpListItemAutofillID";
constexpr NSString* const kSetUpListItemFollowID = @"kSetUpListItemFollowID";

// Returns an NSAttributedString with strikethrough.
NSAttributedString* Strikethrough(NSString* text) {
  NSDictionary<NSAttributedStringKey, id>* attrs =
      @{NSStrikethroughStyleAttributeName : @(NSUnderlineStyleSingle)};
  return [[NSAttributedString alloc] initWithString:text attributes:attrs];
}

}  // namespace

@implementation SetUpListItemView {
  SetUpListItemIcon* _icon;
  CrossfadeLabel* _title;
  CrossfadeLabel* _description;
  UIStackView* _contentStack;
  UITapGestureRecognizer* _tapGestureRecognizer;
}

- (instancetype)initWithData:(SetUpListItemViewData*)data {
  self = [super init];
  if (self) {
    _type = data.type;
    _complete = data.complete;
  }
  return self;
}

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];

  [self createSubviews];
}

- (NSString*)accessibilityLabel {
  return [NSString
      stringWithFormat:@"%@, %@", [self titleText], [self descriptionText]];
}

#pragma mark - Public methods

- (void)handleTap:(UITapGestureRecognizer*)sender {
  if (sender.state == UIGestureRecognizerStateEnded) {
    [self.tapDelegate didTapSetUpListItemView:self];
  }
}

- (void)markComplete {
  if (_complete) {
    return;
  }
  _complete = YES;
  self.accessibilityTraits += UIAccessibilityTraitNotEnabled;

  // Set up the label crossfades.
  UIColor* newTextColor = [UIColor colorNamed:kTextQuaternaryColor];
  [_title setUpCrossfadeWithTextColor:newTextColor
                       attributedText:Strikethrough(_title.text)];
  [_description setUpCrossfadeWithTextColor:newTextColor
                             attributedText:Strikethrough(_description.text)];

  [_icon playSparkleWithDuration:kAnimationSparkleDuration
                           delay:kAnimationSparkleDelay];

  // Set up the main animation.
  __weak __typeof(self) weakSelf = self;
  [UIView animateWithDuration:kAnimationDuration.InSecondsF()
      animations:^{
        [weakSelf setAnimations];
      }
      completion:^(BOOL finished) {
        [weakSelf animationCompletion:finished];
      }];
}

#pragma mark - Private methods

- (void)createSubviews {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0)) {
    return;
  }

  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.accessibilityIdentifier = [self itemAccessibilityIdentifier];
  self.isAccessibilityElement = YES;
  self.accessibilityTraits = UIAccessibilityTraitButton;

  if (_complete) {
    self.accessibilityTraits += UIAccessibilityTraitNotEnabled;
  }

  _icon = [[SetUpListItemIcon alloc] initWithType:_type complete:_complete];
  _title = [self createTitle];
  _description = [self createDescription];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _title, _description ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = kTextSpacing;

  // Add a horizontal stack to contain the icon(s) and the text stack.
  _contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _icon, textStack ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.spacing = kPadding;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);

  // Set up the tap gesture recognizer.
  _tapGestureRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handleTap:)];
  [self addGestureRecognizer:_tapGestureRecognizer];
}

// Creates the title label.
- (CrossfadeLabel*)createTitle {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label.text = [self titleText];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  if (_complete) {
    label.textColor = [UIColor colorNamed:kTextQuaternaryColor];
    label.attributedText = Strikethrough(label.text);
  } else {
    label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  return label;
}

// Creates the description label.
- (CrossfadeLabel*)createDescription {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label = [[CrossfadeLabel alloc] init];
  label.text = [self descriptionText];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  if (_complete) {
    label.textColor = [UIColor colorNamed:kTextQuaternaryColor];
    label.attributedText = Strikethrough(label.text);
  } else {
    label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  }
  return label;
}

// Returns the text for the title label.
- (NSString*)titleText {
  switch (_type) {
    case SetUpListItemType::kSignInSync:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_TITLE);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_AUTOFILL_TITLE);
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
      NOTREACHED();
      return @"";
  }
}

// Returns the text for the description label.
- (NSString*)descriptionText {
  switch (_type) {
    case SetUpListItemType::kSignInSync:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_DESCRIPTION);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_DESCRIPTION);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_AUTOFILL_DESCRIPTION);
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
      NOTREACHED();
      return @"";
  }
}

- (NSString*)itemAccessibilityIdentifier {
  switch (_type) {
    case SetUpListItemType::kSignInSync:
      return kSetUpListItemSignInID;
    case SetUpListItemType::kDefaultBrowser:
      return kSetUpListItemDefaultBrowserID;
    case SetUpListItemType::kAutofill:
      return kSetUpListItemAutofillID;
    case SetUpListItemType::kFollow:
      return kSetUpListItemFollowID;
  }
}

#pragma mark - Private methods (animation helpers)

// Sets the various subview properties that should be animated.
- (void)setAnimations {
  [_icon markComplete];
  [_title crossfade];
  [_description crossfade];
}

// Sets subview properties for animation completion, and cleans up.
- (void)animationCompletion:(BOOL)finished {
  [_title cleanupAfterCrossfade];
  [_description cleanupAfterCrossfade];
}

@end
