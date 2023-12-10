// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view.h"

#import "base/feature_list.h"
#import "base/notreached.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/crossfade_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/constants.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_icon.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view+private.h"
#import "ios/chrome/browser/ui/content_suggestions/set_up_list/set_up_list_item_view_data.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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

// Returns an NSAttributedString with strikethrough.
NSAttributedString* Strikethrough(NSString* text) {
  NSDictionary<NSAttributedStringKey, id>* attrs =
      @{NSStrikethroughStyleAttributeName : @(NSUnderlineStyleSingle)};
  return [[NSAttributedString alloc] initWithString:text attributes:attrs];
}

// Holds all the configurable attributes of this view.
struct ViewConfig {
  BOOL compact_layout;
  BOOL hero_layout;
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
  UIView* _iconContainerView;
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
    if (data.compactLayout) {
      // ViewConfig for a compact layout.
      int syncString =
          base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)
              ? IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_SHORT_DESCRIPTION_NO_SYNC
              : IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_SHORT_DESCRIPTION;
      _config = {
          YES,
          NO,
          syncString,
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_SHORT_DESCRIPTION,
          IDS_IOS_SET_UP_LIST_AUTOFILL_SHORT_DESCRIPTION,
          UIFontTextStyleFootnote,
          UIFontTextStyleCaption2,
          kCompactTextSpacing,
      };
    } else if (data.heroCellMagicStackLayout) {
      int syncString =
          base::FeatureList::IsEnabled(
              syncer::kReplaceSyncPromosWithSignInPromos)
              ? IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL
              : IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_MAGIC_STACK_DESCRIPTION;
      _config = {
          NO,
          YES,
          syncString,
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_MAGIC_STACK_DESCRIPTION,
          IDS_IOS_SET_UP_LIST_AUTOFILL_MAGIC_STACK_DESCRIPTION,
          UIFontTextStyleSubheadline,
          UIFontTextStyleFootnote,
          kTextSpacing,
      };
    } else {
      // Normal ViewConfig.
      int syncString = base::FeatureList::IsEnabled(
                           syncer::kReplaceSyncPromosWithSignInPromos)
                           ? IDS_IOS_IDENTITY_DISC_SIGN_IN_PROMO_LABEL
                           : IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_DESCRIPTION;
      _config = {
          NO,
          NO,
          syncString,
          IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_DESCRIPTION,
          IDS_IOS_SET_UP_LIST_AUTOFILL_DESCRIPTION,
          UIFontTextStyleSubheadline,
          UIFontTextStyleFootnote,
          kTextSpacing,
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

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    // Force a layout since the size of text components may have changed.
    [self setNeedsLayout];
    [self layoutIfNeeded];
  }
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
  UIColor* newTextColor = [UIColor colorNamed:kTextSecondaryColor];
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

  BOOL putIconInSquareBackground =
      _config.hero_layout && _type != SetUpListItemType::kAllSet;
  _icon = [[SetUpListItemIcon alloc] initWithType:_type
                                         complete:_complete
                                    compactLayout:_config.compact_layout
                                         inSquare:putIconInSquareBackground];
  if (putIconInSquareBackground) {
    _icon.translatesAutoresizingMaskIntoConstraints = NO;
    _iconContainerView = [[UIView alloc] init];
    _iconContainerView.backgroundColor = [UIColor colorNamed:kGrey100Color];
    _iconContainerView.layer.cornerRadius = 12;
    _iconContainerView.layer.masksToBounds = NO;
    _iconContainerView.clipsToBounds = YES;
    [_iconContainerView addSubview:_icon];
    AddSameCenterConstraints(_icon, _iconContainerView);
    [NSLayoutConstraint activateConstraints:@[
      [_iconContainerView.widthAnchor constraintEqualToConstant:56],
      [_iconContainerView.widthAnchor
          constraintEqualToAnchor:_iconContainerView.heightAnchor],
    ]];
  }

  _title = [self createTitle];
  _description = [self createDescription];

  // Add a vertical stack for the title and description labels.
  UIStackView* textStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ _title, _description ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = _config.text_spacing;

  // Add a horizontal stack to contain the icon(s) and the text stack.
  NSArray* arrangedSubviews = putIconInSquareBackground
                                  ? @[ _iconContainerView, textStack ]
                                  : @[ _icon, textStack ];
  _contentStack =
      [[UIStackView alloc] initWithArrangedSubviews:arrangedSubviews];
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
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.font =
      _config.hero_layout
          ? CreateDynamicFont(UIFontTextStyleFootnote, UIFontWeightSemibold)
          : [UIFont preferredFontForTextStyle:_config.title_font];
  label.adjustsFontForContentSizeCategory = YES;
  if (_complete) {
    label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    label.attributedText = Strikethrough(label.text);
  } else {
    label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  }
  [label
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];

  return label;
}

// Creates the description label.
- (CrossfadeLabel*)createDescription {
  CrossfadeLabel* label = [[CrossfadeLabel alloc] init];
  label = [[CrossfadeLabel alloc] init];
  label.text = [self descriptionText];
  label.numberOfLines = IsMagicStackEnabled() ? 2 : 4;
  label.lineBreakMode = NSLineBreakByTruncatingTail;
  label.font = [UIFont preferredFontForTextStyle:_config.description_font];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];
  if (_complete) {
    label.attributedText = Strikethrough(label.text);
  }
  [label
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];

  return label;
}

// Returns the text for the title label.
- (NSString*)titleText {
  switch (_type) {
    case SetUpListItemType::kSignInSync:
      return base::FeatureList::IsEnabled(
                 syncer::kReplaceSyncPromosWithSignInPromos)
                 ? l10n_util::GetNSString(
                       IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_TITLE)
                 : l10n_util::GetNSString(
                       IDS_IOS_SET_UP_LIST_SIGN_IN_SYNC_TITLE);
    case SetUpListItemType::kDefaultBrowser:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_DEFAULT_BROWSER_TITLE);
    case SetUpListItemType::kAutofill:
      return l10n_util::GetNSString(IDS_IOS_SET_UP_LIST_AUTOFILL_TITLE);
    case SetUpListItemType::kContentNotification:
      // TODO(b/310713830): add strings for content notifications when they are
      // finalized.
      return @"Get Content Notifications";
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
    case SetUpListItemType::kContentNotification:
      // TODO(b/310713830): add strings for content notifications when they are
      // finalized.
      return @"Keep up with news, sports, and more based on your interest";
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
      return set_up_list::kSignInItemID;
    case SetUpListItemType::kDefaultBrowser:
      return set_up_list::kDefaultBrowserItemID;
    case SetUpListItemType::kAutofill:
      return set_up_list::kAutofillItemID;
    case SetUpListItemType::kContentNotification:
      return set_up_list::kContentNotificationItemID;
    case SetUpListItemType::kAllSet:
      return set_up_list::kAllSetItemID;
    case SetUpListItemType::kFollow:
      return set_up_list::kFollowItemID;
  }
}

#pragma mark - Private methods (animation helpers)

// Sets the various subview properties that should be animated.
- (void)setAnimations {
  [_icon markComplete];
  if (_iconContainerView) {
    _iconContainerView.backgroundColor = [UIColor clearColor];
  }
  [_title crossfade];
  [_description crossfade];
}

// Sets subview properties for animation completion, and cleans up.
- (void)animationCompletion:(BOOL)finished {
  [_title cleanupAfterCrossfade];
  [_description cleanupAfterCrossfade];
}

@end
