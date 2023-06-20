// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/password_manager/core/common/password_manager_features.h"
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
constexpr CGFloat kCompactTextSpacing = 0;

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
constexpr NSString* const kSetUpListItemAllSetID = @"kSetUpListItemAllSetID";
constexpr NSString* const kSetUpListItemFollowID = @"kSetUpListItemFollowID";

// Returns an NSAttributedString with strikethrough.
NSAttributedString* Strikethrough(NSString* text) {
  NSDictionary<NSAttributedStringKey, id>* attrs =
      @{NSStrikethroughStyleAttributeName : @(NSUnderlineStyleSingle)};
  return [[NSAttributedString alloc] initWithString:text attributes:attrs];
}

// Holds all the configurable attributes of this view.
struct ViewConfig {
  BOOL compact_layout;
  int signin_sync_description;
  int default_browser_description;
  int autofill_description;
  NSString* title_font;
  NSString* description_font;
  CGFloat text_spacing;
};

}  // namespace

@implementation SetUpListItemView {
  SetUpListItemIcon* _icon;
  CrossfadeLabel* _title;
  CrossfadeLabel* _description;
  UIStackView* _contentStack;
  UITapGestureRecognizer* _tapGestureRecognizer;
  ViewConfig _config;
}

- (instancetype)initWithData:(SetUpListItemViewData*)data {
  self = [super init];
  if (self) {
    _type = data.type;
    _complete = data.complete;
    if (!data.compactLayout) {
      // Normal ViewConfig.
      const int syncString =
          base::FeatureList::IsEnabled(
              password_manager::features::kEnablePasswordsAccountStorage)
              ? IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_DESCRIPTION_NO_PASSWORDS
              : IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_DESCRIPTION;
      _config = {
          NO,
          syncString,
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_DESCRIPTION,
          IDS_IOS_SET_UP_LIST_AUTOFILL_DESCRIPTION,
          UIFontTextStyleSubheadline,
          UIFontTextStyleFootnote,
          kTextSpacing,
      };
    } else {
      // ViewConfig for a compact layout.
      const int syncString =
          base::FeatureList::IsEnabled(
              password_manager::features::kEnablePasswordsAccountStorage)
              ? IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_SHORT_DESCRIPTION_NO_PASSWORDS
              : IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_SHORT_DESCRIPTION;
      _config = {
          YES,
          syncString,
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SHORT_DESCRIPTION,
          IDS_IOS_SET_UP_LIST_AUTOFILL_SHORT_DESCRIPTION,
          UIFontTextStyleFootnote,
          UIFontTextStyleCaption2,
          kCompactTextSpacing,
      };
    }
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
  if (sender.state == UIGestureRecognizerStateEnded && !self.complete) {
    [self.tapDelegate didTapSetUpListItemView:self];
  }
}

- (void)markCompleteWithCompletion:(ProceduralBlock)completion {
  if (_complete) {
    return;
  }
  _complete = YES;

  // If this is called before the view is moved to superview, then we don't
  // need to update / animate here.
  if (self.subviews.count == 0) {
    return;
  }
  self.accessibilityTraits += UIAccessibilityTraitNotEnabled;

  // Set up the label crossfades.
  UIColor* newTextColor = [UIColor colorNamed:kTextQuaternaryColor];
  [_title setUpCrossfadeWithTextColor:newTextColor
                       attributedText:Strikethrough(_title.text)];
  [_description setUpCrossfadeWithTextColor:newTextColor
                             attributedText:Strikethrough(_description.text)];

  [_icon playSparkleWithDuration:kAnimationSparkleDuration
                           delay:kAnimationSparkleDelay];

  if (completion) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(completion),
        kAnimationSparkleDuration + kAnimationSparkleDelay);
  }

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

  _icon = [[SetUpListItemIcon alloc] initWithType:_type
                                         complete:_complete
                                    compactLayout:_config.compact_layout];
  _title = [self createTitle];
  _description = [self createDescription];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _title, _description ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = _config.text_spacing;

  // Add a horizontal stack to contain the icon(s) and the text stack.
  _contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _icon, textStack ]];
  _contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  _contentStack.alignment = UIStackViewAlignmentCenter;
  _contentStack.spacing = kPadding;
  [self addSubview:_contentStack];
  AddSameConstraints(_contentStack, self);

  if (_type != SetUpListItemType::kAllSet) {
    // Set up the tap gesture recognizer.
    _tapGestureRecognizer =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(handleTap:)];
    [self addGestureRecognizer:_tapGestureRecognizer];
  }
}

// Creates the title label.
- (CrossfadeLabel*)createTitle {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label.text = [self titleText];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:_config.title_font];
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
  label.font = [UIFont preferredFontForTextStyle:_config.description_font];
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
    case SetUpListItemType::kAllSet:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_ALL_SET_TITLE);
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
      NOTREACHED_NORETURN();
  }
}

// Returns the text for the description label.
- (NSString*)descriptionText {
  switch (_type) {
    case SetUpListItemType::kSignInSync:
      return l10n_util::GetNSString(_config.signin_sync_description);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(_config.default_browser_description);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(_config.autofill_description);
    case SetUpListItemType::kAllSet:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_ALL_SET_DESCRIPTION);
    case SetUpListItemType::kFollow:
      // TODO(crbug.com/1428070): Add a Follow item to the Set Up List.
      NOTREACHED_NORETURN();
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
    case SetUpListItemType::kAllSet:
      return kSetUpListItemAllSetID;
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
