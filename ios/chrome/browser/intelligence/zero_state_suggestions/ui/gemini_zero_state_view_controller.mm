// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_ui_utils.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/zero_state_suggestions_service.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// UI Constants for chip layout.
constexpr CGFloat kPlaceholderChipWidths[] = {200.0, 260.0, 140.0};
NSString* const kPlaceholderChipTitle = @" ";
constexpr NSTimeInterval kSuggestionChipsAnimationDuration = 0.3;
constexpr CGFloat kChipInterItemSpacing = 12.0;
constexpr CGFloat kChipsToInputSpacing = 16.0;
constexpr CGFloat kChipContentPaddingVertical = 12.0;
constexpr CGFloat kChipContentPaddingHorizontal = 16.0;
// Chip title font size at the default Dynamic Type size, and the maximum scale
// applied to it at larger Dynamic Type sizes.
constexpr CGFloat kChipFontSize = 15.0;
constexpr CGFloat kChipFontMaxScale = 1.6;
constexpr CGFloat kShadowRadius = 15.5;
constexpr float kShadowOpacity = 0.15;
// Distance the content mask extends beyond the leading, trailing and bottom
// edges so the chip shadows are not clipped. A `CALayer` shadow visibly extends
// up to about twice its `shadowRadius`.
constexpr CGFloat kShadowClipOutset = 2 * kShadowRadius;
constexpr CGFloat kContainerTopPadding = 48.0;
constexpr CGFloat kHeaderToSuggestionsSpacing = 24.0;
constexpr CGFloat kLogoToGreetingSpacing = 16.0;
constexpr CGFloat kCloseButtonSize = 44.0;
constexpr CGFloat kCloseButtonSymbolPointSize = 18.0;
constexpr NSTimeInterval kHeaderFadeAnimationDuration = 0.2;

// Applies the zero-state drop shadow to `view`.
void ApplyDropShadow(UIView* view) {
  view.layer.shadowColor = UIColor.blackColor.CGColor;
  view.layer.shadowOpacity = kShadowOpacity;
  view.layer.shadowRadius = kShadowRadius;
  view.layer.shadowOffset = CGSizeZero;
}

}  // namespace

@implementation GeminiZeroStateViewController {
  // Stack view containing chip button items.
  UIStackView* _suggestionsStack;
  // Stack view containing the Gemini logo and the greeting label.
  UIStackView* _headerStack;
  // Image view displaying the Gemini logo.
  UIImageView* _logoImageView;
  // Label containing the greeting text.
  UILabel* _greetingLabel;
  // Fixed list of suggestion chip buttons (initially placeholders).
  NSArray<UIButton*>* _suggestionButtons;
  // Fixed width constraints corresponding to each button in
  // `_suggestionButtons` when in the placeholder state.
  NSArray<NSLayoutConstraint*>* _placeholderWidthConstraints;
  // Container for the header and suggestion chips, clipped by
  // `_contentMaskView`. The close button lives outside of it so that its shadow
  // is not clipped at the top edge.
  UIView* _contentView;
  // Mask clipping `_contentView` at its top edge only.
  UIView* _contentMaskView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // Rather than `clipsToBounds`, a mask (see `viewDidLayoutSubviews`) clips the
  // content at the top edge only, so content overflowing when the sheet is
  // shrunk still disappears under the top of the view, while the suggestion
  // chip shadows can extend into the parent container's padding. A mask view
  // (rather than a mask layer) is used so that its frame changes animate along
  // with the content during sheet resize animations.
  _contentView = [[UIView alloc] init];
  _contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_contentView];
  AddSameConstraints(_contentView, self.view);

  _contentMaskView = [[UIView alloc] init];
  _contentMaskView.backgroundColor = UIColor.blackColor;
  _contentView.maskView = _contentMaskView;

  UIStackView* headerStack = [[UIStackView alloc] init];
  headerStack.translatesAutoresizingMaskIntoConstraints = NO;
  headerStack.axis = UILayoutConstraintAxisVertical;
  headerStack.spacing = kLogoToGreetingSpacing;
  _headerStack = headerStack;
  [_contentView addSubview:headerStack];

  _logoImageView = [[UIImageView alloc] init];
  _logoImageView.translatesAutoresizingMaskIntoConstraints = NO;
  _logoImageView.contentMode = UIViewContentModeCenter;
  _logoImageView.image = [GeminiUIUtils multicolorGeminiSpark];
  [_logoImageView setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
  [_logoImageView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [headerStack addArrangedSubview:_logoImageView];

  _greetingLabel = [[UILabel alloc] init];
  _greetingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _greetingLabel.numberOfLines = 0;
  _greetingLabel.textAlignment = NSTextAlignmentCenter;
  _greetingLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle1];
  _greetingLabel.adjustsFontForContentSizeCategory = YES;
  _greetingLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  _greetingLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  [_greetingLabel setContentHuggingPriority:UILayoutPriorityRequired
                                    forAxis:UILayoutConstraintAxisVertical];
  [_greetingLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [self updateGreetingText];
  [headerStack addArrangedSubview:_greetingLabel];

  _suggestionsStack = [[UIStackView alloc] init];
  _suggestionsStack.translatesAutoresizingMaskIntoConstraints = NO;
  _suggestionsStack.axis = UILayoutConstraintAxisVertical;
  _suggestionsStack.spacing = kChipInterItemSpacing;
  _suggestionsStack.alignment = UIStackViewAlignmentLeading;
  _suggestionsStack.layoutMarginsRelativeArrangement = YES;
  _suggestionsStack.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, 0, kChipsToInputSpacing, 0);
  [_suggestionsStack setContentHuggingPriority:UILayoutPriorityRequired
                                       forAxis:UILayoutConstraintAxisVertical];
  [_suggestionsStack
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  [_contentView addSubview:_suggestionsStack];

  [self setupSuggestionButtons];

  UIButtonConfiguration* closeButtonConfig =
      [UIButtonConfiguration filledButtonConfiguration];
  closeButtonConfig.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  closeButtonConfig.image =
      SymbolWithPointSize(SymbolXMark, kCloseButtonSymbolPointSize);
  closeButtonConfig.baseBackgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  closeButtonConfig.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];

  __weak __typeof(self) weakSelf = self;
  UIAction* closeAction = [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.mutator didTapZeroStateCloseButton];
  }];
  UIButton* closeButton = [UIButton buttonWithConfiguration:closeButtonConfig
                                              primaryAction:closeAction];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_IOS_ICON_CLOSE);
  ApplyDropShadow(closeButton);
  [self.view addSubview:closeButton];
  AddSquareConstraints(closeButton, kCloseButtonSize);

  UILayoutGuide* headerLayoutGuide = [[UILayoutGuide alloc] init];
  [self.view addLayoutGuide:headerLayoutGuide];

  AddSameConstraintsToSides(headerStack, self.view,
                            LayoutSides::kLeading | LayoutSides::kTrailing);
  AddSameConstraintsToSides(
      _suggestionsStack, self.view,
      LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

  NSLayoutConstraint* guideTopConstraint = [headerLayoutGuide.topAnchor
      constraintEqualToAnchor:self.view.topAnchor
                     constant:kContainerTopPadding];
  guideTopConstraint.priority = UILayoutPriorityDefaultLow;

  NSLayoutConstraint* centerHeaderConstraint = [headerStack.centerYAnchor
      constraintEqualToAnchor:headerLayoutGuide.centerYAnchor];
  centerHeaderConstraint.priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [closeButton.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    guideTopConstraint,
    [headerLayoutGuide.bottomAnchor
        constraintEqualToAnchor:_suggestionsStack.topAnchor
                       constant:-kHeaderToSuggestionsSpacing],
    [headerStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:headerLayoutGuide.topAnchor],
    [headerStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:headerLayoutGuide.bottomAnchor],
    centerHeaderConstraint,
  ]];

  [self registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                     withAction:@selector(contentSizeCategoryDidChange)];
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  // At this point only the direct subviews of `self.view` have their updated
  // frames. `updateHeaderVisibility` reads the frames of `_contentView`'s
  // subviews, so they need to be laid out first.
  [_contentView layoutIfNeeded];
  [self updateHeaderVisibility];
  CGRect bounds = _contentView.bounds;
  _contentMaskView.frame = CGRectMake(
      CGRectGetMinX(bounds) - kShadowClipOutset, CGRectGetMinY(bounds),
      CGRectGetWidth(bounds) + 2 * kShadowClipOutset,
      CGRectGetHeight(bounds) + kShadowClipOutset);
}

#pragma mark - GeminiZeroStateConsumer

- (void)setZeroStateSuggestions:(NSArray<ZeroStateSuggestion*>*)suggestions {
  if (!suggestions) {
    [self resetToPlaceholderSuggestionChips];
    return;
  }

  // This ensures the view hierarchy is ready for the subsequent button updates
  [self.view layoutIfNeeded];

  __weak __typeof(self) weakSelf = self;
  for (NSUInteger i = 0; i < _suggestionButtons.count; ++i) {
    UIButton* chipButton = _suggestionButtons[i];
    [self removeAllActionsFromButton:chipButton];

    // If we receive less than 3 suggestions, hide the remaining buttons
    if (i >= suggestions.count) {
      chipButton.hidden = YES;
      chipButton.userInteractionEnabled = NO;
      continue;
    }

    ZeroStateSuggestion* suggestion = suggestions[i];
    _placeholderWidthConstraints[i].active = NO;

    UIButtonConfiguration* config = chipButton.configuration;
    config.title = suggestion.text;
    chipButton.configuration = config;
    chipButton.hidden = NO;
    chipButton.userInteractionEnabled = YES;
    chipButton.titleLabel.alpha = 0.0;

    UIAction* tapAction = [UIAction actionWithHandler:^(UIAction* action) {
      [weakSelf.mutator geminiZeroStateViewController:weakSelf
                                  didSelectSuggestion:suggestion];
    }];
    [chipButton addAction:tapAction
         forControlEvents:UIControlEventTouchUpInside];
  }

  [UIView animateWithDuration:kSuggestionChipsAnimationDuration
                   animations:^{
                     [weakSelf layoutSuggestionChips];
                   }];
}

#pragma mark - Private

// Fades out `_headerStack` when the view is too short to show it unclipped
// above the suggestion chips (e.g. in landscape or when the keyboard is shown).
// `alpha` is used instead of `hidden` so the header keeps contributing to the
// content height, which drives the container's medium detent height. Otherwise
// the detent would shrink and the header would never fit again.
- (void)updateHeaderVisibility {
  CGFloat requiredHeight =
      kContainerTopPadding + _headerStack.frame.size.height +
      kHeaderToSuggestionsSpacing + _suggestionsStack.frame.size.height;
  BOOL headerFits = self.view.bounds.size.height >= requiredHeight;
  CGFloat targetAlpha = headerFits ? 1.0 : 0.0;
  if (_headerStack.alpha == targetAlpha) {
    return;
  }

  _headerStack.accessibilityElementsHidden = !headerFits;
  if (!self.view.window) {
    _headerStack.alpha = targetAlpha;
    return;
  }
  __weak UIView* headerStack = _headerStack;
  [UIView animateWithDuration:kHeaderFadeAnimationDuration
                   animations:^{
                     headerStack.alpha = targetAlpha;
                   }];
}

// Re-resolves the chip title fonts for the new Dynamic Type size, since the
// font is computed in the configuration's `titleTextAttributesTransformer`, and
// notifies the mutator that the content size changed.
- (void)contentSizeCategoryDidChange {
  for (UIButton* chipButton in _suggestionButtons) {
    [chipButton setNeedsUpdateConfiguration];
  }
  // The notification is deferred so that the trait change has propagated to
  // all subviews (e.g. the greeting label and the Gemini input plate) before
  // the content height is measured.
  __weak __typeof(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf.mutator zeroStateContentSizeDidChange];
  });
}

// Updates the greeting label text based on the first name provided by the
// mutator.
- (void)updateGreetingText {
  NSString* trimmedName = [[self.mutator userFirstName]
      stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
  _greetingLabel.text =
      trimmedName.length > 0
          ? l10n_util::GetNSStringF(
                IDS_GEMINI_IOS_ZERO_STATE_GREETING_WITH_FIRST_NAME,
                base::SysNSStringToUTF16(trimmedName))
          : l10n_util::GetNSString(IDS_GEMINI_IOS_ZERO_STATE_GREETING);
}

// Initializes the 3 suggestion chip buttons and their placeholder width
// constraints, and adds them to `_suggestionsStack`.
- (void)setupSuggestionButtons {
  NSMutableArray<UIButton*>* buttons = [[NSMutableArray alloc] init];
  NSMutableArray<NSLayoutConstraint*>* widthConstraints =
      [[NSMutableArray alloc] init];

  for (CGFloat width : kPlaceholderChipWidths) {
    UIButton* chipButton = [self createPlaceholderChipButton];
    [_suggestionsStack addArrangedSubview:chipButton];

    // Chips never exceed the available width, so long titles (e.g. with large
    // Dynamic Type sizes) wrap onto multiple lines instead of being cropped.
    [chipButton.widthAnchor
        constraintLessThanOrEqualToAnchor:_suggestionsStack.widthAnchor]
        .active = YES;
    NSLayoutConstraint* widthConstraint =
        [chipButton.widthAnchor constraintEqualToConstant:width];
    widthConstraint.priority = UILayoutPriorityRequired - 1;
    widthConstraint.active = YES;

    [buttons addObject:chipButton];
    [widthConstraints addObject:widthConstraint];
  }

  _suggestionButtons = [buttons copy];
  _placeholderWidthConstraints = [widthConstraints copy];
}

// Resets all suggestion chip buttons to their empty placeholder state.
- (void)resetToPlaceholderSuggestionChips {
  for (NSUInteger i = 0; i < _suggestionButtons.count; ++i) {
    UIButton* chipButton = _suggestionButtons[i];
    [self removeAllActionsFromButton:chipButton];

    UIButtonConfiguration* config = chipButton.configuration;
    config.title = kPlaceholderChipTitle;
    chipButton.configuration = config;
    chipButton.hidden = NO;
    chipButton.userInteractionEnabled = NO;
    chipButton.titleLabel.alpha = 0.0;
    _placeholderWidthConstraints[i].active = YES;
  }
}

// Removes all registered `UIAction`s from `button`.
- (void)removeAllActionsFromButton:(UIButton*)button {
  [button enumerateEventHandlers:^(UIAction* action, id target, SEL selector,
                                   UIControlEvents controlEvents, BOOL* stop) {
    if (action) {
      [button removeAction:action forControlEvents:controlEvents];
    }
  }];
}

// Creates an empty suggestion chip button in its initial placeholder state.
- (UIButton*)createPlaceholderChipButton {
  UIButtonConfiguration* config =
      [UIButtonConfiguration filledButtonConfiguration];
  config.title = kPlaceholderChipTitle;
  config.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
  config.contentInsets = NSDirectionalEdgeInsetsMake(
      kChipContentPaddingVertical, kChipContentPaddingHorizontal,
      kChipContentPaddingVertical, kChipContentPaddingHorizontal);
  config.titleAlignment = UIButtonConfigurationTitleAlignmentCenter;
  config.titleLineBreakMode = NSLineBreakByWordWrapping;
  // Scales the title with Dynamic Type like `UIFontTextStyleSubheadline`, but
  // caps it at `kChipFontMaxScale` of its default size.
  config.titleTextAttributesTransformer =
      ^NSDictionary<NSAttributedStringKey, id>*(
          NSDictionary<NSAttributedStringKey, id>* incoming) {
    NSMutableDictionary<NSAttributedStringKey, id>* outgoing =
        [incoming mutableCopy];
    outgoing[NSFontAttributeName] =
        [[UIFontMetrics metricsForTextStyle:UIFontTextStyleSubheadline]
            scaledFontForFont:[UIFont systemFontOfSize:kChipFontSize]
             maximumPointSize:kChipFontSize * kChipFontMaxScale];
    return outgoing;
  };
  config.baseBackgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  config.baseForegroundColor = [UIColor colorNamed:kTextPrimaryColor];

  UIButton* chipButton = [UIButton buttonWithConfiguration:config
                                             primaryAction:nil];
  chipButton.translatesAutoresizingMaskIntoConstraints = NO;
  chipButton.userInteractionEnabled = NO;
  chipButton.titleLabel.alpha = 0.0;
  ApplyDropShadow(chipButton);
  [chipButton setContentHuggingPriority:UILayoutPriorityRequired
                                forAxis:UILayoutConstraintAxisVertical];
  [chipButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  return chipButton;
}

// Flushes pending layout for the suggestion chip buttons and the root view,
// and fades in the chip button titles.
- (void)layoutSuggestionChips {
  [self.view layoutIfNeeded];
  for (UIButton* chipButton in _suggestionButtons) {
    if (chipButton.hidden) {
      continue;
    }
    [chipButton layoutIfNeeded];
    [UIView performWithoutAnimation:^{
      [chipButton.titleLabel.layer removeAllAnimations];
      chipButton.titleLabel.alpha = 0.0;
    }];
    chipButton.titleLabel.alpha = 1.0;
  }
}

@end
