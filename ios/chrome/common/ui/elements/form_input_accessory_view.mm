// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/elements/form_input_accessory_view.h"

#import <QuartzCore/QuartzCore.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/form_input_accessory_view_text_data.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/background_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Symbol size for symbols on the keyboard accessory.
constexpr CGFloat kSymbolImagePointSize = 24;

// Checkmark symbol size.
constexpr CGFloat kCheckmarkSymbolPointSize = 17;

// Default height for the keyboard accessory.
constexpr CGFloat kDefaultAccessoryHeight = 44;

// Button target area for the large keyboard accessory.
constexpr CGFloat kLargeButtonTargetArea = 44;

// Button target area for the liquid glass keyboard accessory.
constexpr CGFloat kLiquidGlassButtonTargetArea = 48;

// Trailing horizontal padding.
constexpr CGFloat kKeyboardHorizontalPadding = 16;

// The padding between the image and the title on the manual fill button.
// Only applies to the iPad version of this button.
constexpr CGFloat kManualFillTitlePadding = 4;

// The font size used for the title of the manual fill button.
// Only applies to the iPad version of this button.
constexpr CGFloat kManualFillTitleFontSize = 18;

// The spacing between the items in the navigation view.
constexpr CGFloat ManualFillNavigationItemSpacing = 4;

// The leading content inset for the close button.
constexpr CGFloat ManualFillCloseButtonLeadingInset = 7;

// The trailing content inset for the close button.
constexpr CGFloat ManualFillCloseButtonTrailingInset = 15;

// The trailing content inset for the close button when using liquid glass.
constexpr CGFloat LiquidGlassCloseButtonTrailingInset = 28;

// The bottom content inset for the close button.
constexpr CGFloat ManualFillCloseButtonBottomInset = 4;

// The height for the top and bottom separator lines.
constexpr CGFloat ManualFillSeparatorHeight = 0.5;

// The width for the close button when split view is active.
constexpr CGFloat kManualFillCloseButtonWidth = 48;

// The height for the close button when split view is active.
constexpr CGFloat kManualFillCloseButtonHeight = 48;

// Symbols used by FormInputAccessoryView.
NSString* const kCheckmarkSymbol = @"checkmark";
NSString* const kChevronDownSymbol = @"chevron.down";
NSString* const kChevronUpSymbol = @"chevron.up";

// Returns the image of the symbol with the provided name.
UIImage* SymbolNamed(NSString* imageName) {
  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kSymbolImagePointSize
                          weight:UIImageSymbolWeightRegular
                           scale:UIImageSymbolScaleMedium];
  return [UIImage systemImageNamed:imageName withConfiguration:configuration];
}

// Padding around the keyboard accessory on iOS 26.0+.
constexpr CGFloat kSurroundingPadding = 12;

// Corner radius of the keyboard accessory on iOS 26.0+.
constexpr CGFloat kCornerRadius = 24;

// Width for the small keyboard accessory. Only when liquid glass effect is
// enabled.
constexpr CGFloat kSmallAccessoryWidth = 3 * kLiquidGlassButtonTargetArea + 8;

// Alpha of the tint color for the glass effect. A lower alpha will produce a
// more pronounced glass effect.
constexpr CGFloat kGlassTintAlpha = 1.0;

// Shadow parameters. Used when the liquid glass effect is enabled.
constexpr CGFloat kShadowRadius = 16.0;
constexpr CGFloat kShadowVerticalOffset = 4.0;
constexpr CGFloat kShadowOpacity = 0.12;

// Creates a `UIVisualEffectView` with a `UIGlassEffect`.
UIVisualEffectView* CreateGlassEffectView() {
  if (@available(iOS 26, *)) {
    // Create glass effect
    UIGlassEffect* glass_effect = [[UIGlassEffect alloc] init];
    glass_effect.interactive = YES;
    glass_effect.tintColor = [[UIColor colorNamed:kSecondaryBackgroundColor]
        colorWithAlphaComponent:kGlassTintAlpha];

    UIVisualEffectView* effect_view =
        [[UIVisualEffectView alloc] initWithEffect:nil];
    effect_view.effect = glass_effect;
    effect_view.cornerConfiguration = [UICornerConfiguration
        configurationWithRadius:
            [UICornerRadius
                containerConcentricRadiusWithMinimum:kCornerRadius]];
    effect_view.translatesAutoresizingMaskIntoConstraints = NO;
    return effect_view;
  }

  return nil;
}

}  // namespace

// Large height for the keyboard accessory.
const CGFloat kLargeKeyboardAccessoryHeight = 59;

NSString* const kFormInputAccessoryViewAccessibilityID =
    @"kFormInputAccessoryViewAccessibilityID";

NSString* const kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID =
    @"kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID";

@interface FormInputAccessoryView ()

// The navigation delegate if any.
@property(nonatomic, weak) id<FormInputAccessoryViewDelegate> delegate;

@property(nonatomic, weak) UIButton* previousButton;

@property(nonatomic, weak) UIButton* nextButton;

@property(nonatomic, weak) UIButton* manualFillButton;

@property(nonatomic, weak) UIButton* passwordManualFillButton;

@property(nonatomic, weak) UIButton* creditCardManualFillButton;

@property(nonatomic, weak) UIButton* addressManualFillButton;

@property(nonatomic, weak) UIView* leadingView;

@property(nonatomic, weak) UIView* trailingView;

@property(nonatomic, strong) UIImage* manualFillSymbol;

@property(nonatomic, strong) UIImage* passwordManualFillSymbol;

@property(nonatomic, strong) UIImage* creditCardManualFillSymbol;

@property(nonatomic, strong) UIImage* addressManualFillSymbol;

@property(nonatomic, strong) UIImage* closeButtonSymbol;

@end

@implementation FormInputAccessoryView {
  // Transparent view on the top edge to let the keyboard know about the
  // omnibox.
  UIButton* _omniboxTypingShield;
  // Height constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldHeightConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield`.
  NSLayoutConstraint* _omniboxTypingShieldBottomConstraint;
  // Bottom constraint used to show/hide the `omniboxTypingShield` when the view
  // is hidden.
  NSLayoutConstraint* _omniboxTypingShieldHiddenBottomConstraint;
  // View containing the leading and trailing buttons.
  UIView* _contentView;
  // The view used as the background for the content view.
  UIView* _backgroundView;
  // Whether we are using the large accessory view.
  BOOL _largeAccessoryViewEnabled;
  // Whether we are using the small width accessory view.
  BOOL _smallWidthAccessoryViewEnabled;
  // Whether the current form factor is a tablet.
  BOOL _isTabletFormFactor;
  // Whether the size of the accessory is compact.
  BOOL _isCompact;
  // Trailing constraint for `trailingView`.
  NSLayoutConstraint* _trailingConstraint;
  // Constraint for centering `trailingView` to the middle of the keyboard
  // accessory. On iPad, when split view is active, `trailingView`, instead of
  // occupying the whole width of the app window, is shrinked to fit its
  // content, and centered.
  NSLayoutConstraint* _trailingViewCenteringConstraint;
  // Leading constraint for the effect view, which is the background for the
  // suggestions and manual fill buttons. This constraint is deactivated on iPad
  // when there are no suggestions, so the manual fill buttons can be centered.
  NSLayoutConstraint* _effectViewLeadingConstraint;
  // Spacing constraint between the expand button and the close button.
  NSLayoutConstraint* _splitViewSpacingConstraint;
  // Trailing constraint in compact mode (tablet only).
  NSLayoutConstraint* _compactTrailingConstraint;
  // Whether split view is enabled.
  BOOL _splitViewEnabled;
  // The close button for closing the keyboard accessory.
  UIButton* _closeButton;
  // Current subitem group that is visible.
  FormInputAccessoryViewSubitemGroup _currentGroup;
}

#pragma mark - Public

// Override `intrinsicContentSize` so Auto Layout hugs the content of this view.
- (CGSize)intrinsicContentSize {
  return CGSizeZero;
}

- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate {
  [self setSmallWidthAccessoryViewEnabled:!leadingView];
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:nil
        passwordManualFillSymbol:nil
      creditCardManualFillSymbol:nil
         addressManualFillSymbol:nil
               closeButtonSymbol:nil];
}

- (void)setUpWithLeadingView:(UIView*)leadingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol
              splitViewEnabled:(BOOL)splitViewEnabled
            isTabletFormFactor:(BOOL)isTabletFormFactor {
  DCHECK(manualFillSymbol);
  _largeAccessoryViewEnabled = YES;
  _isTabletFormFactor = isTabletFormFactor;
  _splitViewEnabled = splitViewEnabled;
  [self setUpWithLeadingView:leadingView
              customTrailingView:nil
              navigationDelegate:delegate
                manualFillSymbol:manualFillSymbol
        passwordManualFillSymbol:passwordManualFillSymbol
      creditCardManualFillSymbol:creditCardManualFillSymbol
         addressManualFillSymbol:addressManualFillSymbol
               closeButtonSymbol:closeButtonSymbol];
}

- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight {
  _omniboxTypingShieldHeightConstraint.constant = typingShieldHeight;
  if (self.window) {
    [self layoutIfNeeded];
  }
}

- (void)setIsCompact:(BOOL)isCompact {
  if (_isCompact == isCompact) {
    return;
  }

  _isCompact = isCompact;
  [self adjustManualFillButtonTitle:self.manualFillButton];
  [self setHorizontalConstraints];
}

- (void)showGroup:(FormInputAccessoryViewSubitemGroup)group {
  if (_currentGroup == group) {
    return;
  }
  _currentGroup = group;

  BOOL hideNavigationButtons =
      group != FormInputAccessoryViewSubitemGroup::kNavigationButtons;
  self.previousButton.hidden = hideNavigationButtons;
  self.nextButton.hidden = hideNavigationButtons;

  BOOL hideManualFillByCategoryButtons =
      (group != FormInputAccessoryViewSubitemGroup::kManualFillButtons);
  self.passwordManualFillButton.hidden = hideManualFillByCategoryButtons;
  self.creditCardManualFillButton.hidden = hideManualFillByCategoryButtons;
  self.addressManualFillButton.hidden = hideManualFillByCategoryButtons;

  BOOL hideManualFillButton =
      (group != FormInputAccessoryViewSubitemGroup::kExpandButton);
  self.manualFillButton.hidden = hideManualFillButton;

  if ([self isSplitViewActive]) {
    BOOL fixedSpacing = !hideManualFillButton;
    if (_isTabletFormFactor) {
      // iPad:
      // The close button is hidden for iPad. The spacing constraint isn't
      // needed.
      _splitViewSpacingConstraint.active = NO;

      // In `kDetailedButtons` mode, the effect view's constraint that aligns to
      // the leading edge of the keyboard accessary has to be disabled. The
      // `_trailingViewCenteringConstraint` then centers the manual fill buttons
      // to the center of the keyboard accessory.
      _effectViewLeadingConstraint.active = fixedSpacing;
      _trailingViewCenteringConstraint.active = !fixedSpacing;
      _trailingConstraint.active = fixedSpacing;
    } else {
      // iPhone:
      // The effect view is always aligned to the leading anchor of the keyboard
      // accessory. `trailingView`, does not need to be centered or aligned to
      // the trailing anchor of the keyboard accessory.
      _effectViewLeadingConstraint.active = YES;
      _trailingViewCenteringConstraint.active = NO;
      _trailingConstraint.active = NO;

      // In `kExpandButtonOnly` mode:
      // A fixed space between `trailingView` and the close button is needed.
      // In `kDetailedButtons` mode:
      // The space between `trailingView` and the close button is flexible.
      _splitViewSpacingConstraint.active = fixedSpacing;
    }
  }
}

#pragma mark - UIInputViewAudioFeedback

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

#pragma mark - Private Methods

// Whether the liquid glass effect is enabled. Restricted to iOS 26+.
- (BOOL)isLiquidGlassEffectEnabled {
  if (@available(iOS 26, *)) {
    if (_largeAccessoryViewEnabled || _smallWidthAccessoryViewEnabled) {
      return YES;
    }
  }

  return NO;
}

// Whether split view is in use.
- (BOOL)isSplitViewActive {
  return [self isLiquidGlassEffectEnabled] && _splitViewEnabled;
}

// Sets up split view.
- (void)setupSplitView:(UIStackView*)trailingStackView {
  // On iPad, close button is hidden. No setup is needed.
  if (_isTabletFormFactor) {
    return;
  }

  CHECK(_closeButton);
  [trailingStackView removeArrangedSubview:_closeButton];

  UIVisualEffectView* effectView = CreateGlassEffectView();
  [effectView.contentView addSubview:_closeButton];

  AddSameConstraints(effectView, _closeButton);

  [self addSubview:effectView];
  [NSLayoutConstraint activateConstraints:@[
    [_closeButton.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                constant:-kSurroundingPadding],
    [_closeButton.widthAnchor
        constraintEqualToConstant:kManualFillCloseButtonWidth],
    [_closeButton.heightAnchor
        constraintEqualToConstant:kManualFillCloseButtonHeight]
  ]];
}

// Sets the small width mode. This mode is always disabled on iOS < 26.
- (void)setSmallWidthAccessoryViewEnabled:(BOOL)enabled {
  if (@available(iOS 26, *)) {
    _smallWidthAccessoryViewEnabled = enabled;
    return;
  }

  _smallWidthAccessoryViewEnabled = NO;
}

- (void)closeButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCloseButton:self];
}

- (void)nextButtonTapped {
  [self.delegate formInputAccessoryViewDidTapNextButton:self];
}

- (void)previousButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPreviousButton:self];
}

- (void)manualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapManualFillButton:self];
}

- (void)passwordManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapPasswordManualFillButton:self];
}

- (void)creditCardManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapCreditCardManualFillButton:self];
}

- (void)addressManualFillButtonTapped {
  [self.delegate formInputAccessoryViewDidTapAddressManualFillButton:self];
}

- (void)omniboxTypingShieldTapped {
  [self.delegate fromInputAccessoryViewDidTapOmniboxTypingShield:self];
}

// Sets up the view with the given `leadingView`. If `delegate` is not nil,
// navigation controls are shown on the right and use `delegate` for actions.
// Else navigation controls are replaced with `customTrailingView`. If none of
// `delegate` and `customTrailingView` is set, leadingView will take all the
// space.
- (void)setUpWithLeadingView:(UIView*)leadingView
            customTrailingView:(UIView*)customTrailingView
            navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate
              manualFillSymbol:(UIImage*)manualFillSymbol
      passwordManualFillSymbol:(UIImage*)passwordManualFillSymbol
    creditCardManualFillSymbol:(UIImage*)creditCardManualFillSymbol
       addressManualFillSymbol:(UIImage*)addressManualFillSymbol
             closeButtonSymbol:(UIImage*)closeButtonSymbol {
  DCHECK(!self.subviews.count);  // This should only be called once.

  self.accessibilityIdentifier = kFormInputAccessoryViewAccessibilityID;
  self.translatesAutoresizingMaskIntoConstraints = NO;
  self.backgroundColor = UIColor.clearColor;
  self.opaque = NO;
  self.manualFillSymbol = manualFillSymbol;
  self.passwordManualFillSymbol = passwordManualFillSymbol;
  self.creditCardManualFillSymbol = creditCardManualFillSymbol;
  self.addressManualFillSymbol = addressManualFillSymbol;
  self.closeButtonSymbol = closeButtonSymbol;

  // Attempt to set up the liquid glass effect, otherwise, use the non liquid
  // glass accessory.
  if (![self setupLiquidGlassEffect]) {
    _contentView = [[UIView alloc] init];
    _contentView.translatesAutoresizingMaskIntoConstraints = NO;
    if (_largeAccessoryViewEnabled) {
      _backgroundView = PrimaryBackgroundBlurView();
      _backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
      [_contentView addSubview:_backgroundView];
      AddSameConstraints(_backgroundView, _contentView);
    } else {
      _contentView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    }
    [self addSubview:_contentView];
    AddSameConstraintsToSides(
        self, _contentView,
        LayoutSides::kBottom | LayoutSides::kLeading | LayoutSides::kTrailing);

    [self setOmniboxSafeTopConstraint:_contentView];
  }

  leadingView = leadingView ?: [[UIView alloc] init];
  self.leadingView = leadingView;
  leadingView.translatesAutoresizingMaskIntoConstraints = NO;

  UIView* trailingView;
  if (delegate) {
    self.delegate = delegate;
    UIStackView* trailingStackView = [self viewForNavigationButtons];
    if ([self isSplitViewActive]) {
      [self setupSplitView:trailingStackView];
    }
    trailingView = trailingStackView;
  } else {
    trailingView = customTrailingView;
  }
  self.trailingView = trailingView;

  // If there is no trailing view, set the leading view as the only view and
  // return early.
  if (!trailingView) {
    [_contentView addSubview:leadingView];
    AddSameConstraints(_contentView, leadingView);
    return;
  }

  UIView* leadingViewContainer = [[UIView alloc] init];
  leadingViewContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self clipToLiquidGlassEffectBounds:leadingViewContainer];
  [_contentView addSubview:leadingViewContainer];
  [leadingViewContainer addSubview:leadingView];
  if ([self isLiquidGlassEffectEnabled]) {
    AddSameConstraintsToSides(
        leadingViewContainer, leadingView,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    [self setBottomAnchorForView:leadingView];
  } else {
    AddSameConstraints(leadingViewContainer, leadingView);
  }

  trailingView.translatesAutoresizingMaskIntoConstraints = NO;
  [self addTrailingView:trailingView];

  [self setDefaultHeightConstraint:_contentView];

  id<LayoutGuideProvider> layoutGuide = self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [leadingViewContainer.topAnchor
        constraintEqualToAnchor:_contentView.topAnchor],
    [leadingViewContainer.leadingAnchor
        constraintEqualToAnchor:[self isLiquidGlassEffectEnabled]
                                    ? _contentView.leadingAnchor
                                    : layoutGuide.leadingAnchor],
    [trailingView.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
  ]];

  [self setBottomAnchorForView:leadingViewContainer];
  [self setBottomAnchorForView:trailingView];

  if ([self isSplitViewActive]) {
    [_closeButton.centerYAnchor
        constraintEqualToAnchor:trailingView.centerYAnchor]
        .active = YES;

    _trailingViewCenteringConstraint = [trailingView.centerXAnchor
        constraintEqualToAnchor:layoutGuide.centerXAnchor];

    NSLayoutConstraint* trailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:_contentView.trailingAnchor];
    trailingConstraint.priority = UILayoutPriorityDefaultHigh;
    trailingConstraint.active = YES;

    if (!_isTabletFormFactor) {
      _splitViewSpacingConstraint = [_contentView.trailingAnchor
          constraintEqualToAnchor:_closeButton.leadingAnchor
                         constant:-kSurroundingPadding];
    }
  }

  if (_isTabletFormFactor && _largeAccessoryViewEnabled) {
    // On tablets, when using the large keyboard accessory, add padding at both
    // ends of the content view to match the keyboard's padding.

    // Trailing constraint in non compact mode.
    _trailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor
                       constant:-kKeyboardHorizontalPadding];
    // Trailing constraint in compact mode.
    _compactTrailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor];

    // When using multiple windows, ensure that the out of focus window keeps a
    // minimum top anchor preventing the keyboard accessory from being reduced
    // in height. This is only relevant on tablets.
    [self.topAnchor
        constraintLessThanOrEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor
                                 constant:-[self accessoryHeight]]
        .active = YES;

    [self setHorizontalConstraints];
  } else {
    _trailingConstraint = [trailingView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor];
    _trailingConstraint.active = YES;
  }

  // When using the blur effect background, do not add top and bottom lines.
  if (!_largeAccessoryViewEnabled && ![self isLiquidGlassEffectEnabled]) {
    UIView* topGrayLine = [[UIView alloc] init];
    topGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
    topGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:topGrayLine];

    UIView* bottomGrayLine = [[UIView alloc] init];
    bottomGrayLine.backgroundColor = [UIColor colorNamed:kGrey50Color];
    bottomGrayLine.translatesAutoresizingMaskIntoConstraints = NO;
    [_contentView addSubview:bottomGrayLine];

    [NSLayoutConstraint activateConstraints:@[
      [topGrayLine.topAnchor constraintEqualToAnchor:_contentView.topAnchor],
      [topGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [topGrayLine.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
      [topGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],

      [bottomGrayLine.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [bottomGrayLine.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [bottomGrayLine.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [bottomGrayLine.heightAnchor
          constraintEqualToConstant:ManualFillSeparatorHeight],
    ]];
  }

  [leadingViewContainer.trailingAnchor
      constraintEqualToAnchor:trailingView.leadingAnchor]
      .active = YES;

  [self createOmniboxTypingShield];
}

// If the liquid glass effect is enabled, clip the contents of the leading view
// to the liquid glass effect's bounds.
- (void)clipToLiquidGlassEffectBounds:(UIView*)leadingViewContainer {
  if (@available(iOS 26, *)) {
    if ([self isLiquidGlassEffectEnabled]) {
      // Set leading view container bounds to match the glass effect.
      leadingViewContainer.clipsToBounds = YES;
      leadingViewContainer.cornerConfiguration = [UICornerConfiguration
          configurationWithRadius:
              [UICornerRadius
                  containerConcentricRadiusWithMinimum:kCornerRadius]];
      leadingViewContainer.layer.maskedCorners =
          kCALayerMinXMaxYCorner | kCALayerMinXMinYCorner;
    }
  }
}

// Adds the trailing view in the accessory's view hierarchy.
- (void)addTrailingView:(UIView*)trailingView {
  // When the liquid glass effect is enabled, all views must be subviews of the
  // content view so that the glass effect can apply properly to all views.
  if ([self isLiquidGlassEffectEnabled]) {
    [_contentView addSubview:trailingView];
  } else {
    [self addSubview:trailingView];
  }
}

// Sets a top constraint which is bottom omnibox safe.
- (void)setOmniboxSafeTopConstraint:(UIView*)view {
  // Lower the top constraint as the omniboxTypingShield can be above it.
  NSLayoutConstraint* topConstraint =
      [self.topAnchor constraintEqualToAnchor:view.topAnchor];
  topConstraint.priority = UILayoutPriorityRequired - 1;
  topConstraint.active = YES;
}

// Sets the height constraint of the entire keyboard accessory.
- (void)setDefaultHeightConstraint:(UIView*)view {
  NSLayoutConstraint* defaultHeightConstraint =
      [view.heightAnchor constraintEqualToConstant:[self accessoryHeight]];
  defaultHeightConstraint.priority = UILayoutPriorityDefaultHigh;
  defaultHeightConstraint.active = YES;
}

// Returns a view that shows navigation buttons.
- (UIStackView*)viewForNavigationButtons {
  FormInputAccessoryViewTextData* textData =
      [self.delegate textDataforFormInputAccessoryView:self];

  _closeButton = [self createCloseButtonWithText:textData];

  UIStackView* navigationView = nil;
  if (_largeAccessoryViewEnabled) {
    UIButton* manualFillButton = [self createManualFillButtonWithText:textData];
    manualFillButton.hidden = YES;
    self.manualFillButton = manualFillButton;

    UIButton* passwordManualFillButton =
        [self createPasswordManualFillButtonWithText:textData];
    passwordManualFillButton.hidden = YES;
    self.passwordManualFillButton = passwordManualFillButton;

    UIButton* creditCardManualFillButton =
        [self createCreditCardManualFillButtonWithText:textData];
    creditCardManualFillButton.hidden = YES;
    self.creditCardManualFillButton = creditCardManualFillButton;

    UIButton* addressManualFillButton =
        [self createAddressManualFillButtonWithText:textData];
    addressManualFillButton.hidden = YES;
    self.addressManualFillButton = addressManualFillButton;

    if (_isTabletFormFactor) {
      _closeButton.hidden = YES;
    }

    UIButton* previousButton = [self createPreviousButtonWithText:textData];
    previousButton.hidden = YES;
    self.previousButton = previousButton;

    UIButton* nextButton = [self createNextButtonWithText:textData];
    nextButton.hidden = YES;
    self.nextButton = nextButton;

    navigationView = [[UIStackView alloc] initWithArrangedSubviews:@[
      previousButton, nextButton, passwordManualFillButton,
      creditCardManualFillButton, addressManualFillButton, manualFillButton,
      _closeButton
    ]];
  } else {
    UIButton* previousButton = [self createPreviousButtonWithText:textData];
    self.previousButton = previousButton;

    UIButton* nextButton = [self createNextButtonWithText:textData];
    self.nextButton = nextButton;

    navigationView = [[UIStackView alloc]
        initWithArrangedSubviews:@[ previousButton, nextButton, _closeButton ]];
  }
  navigationView.spacing = ManualFillNavigationItemSpacing;
  return navigationView;
}

// Sets the minimum size constraints for an image button.
- (void)setMinimumSizeForButton:(UIButton*)button {
  CGFloat targetArea = [self isLiquidGlassEffectEnabled]
                           ? kLiquidGlassButtonTargetArea
                           : kLargeButtonTargetArea;
  [button.widthAnchor constraintGreaterThanOrEqualToConstant:targetArea]
      .active = YES;
  [button.heightAnchor constraintGreaterThanOrEqualToConstant:targetArea]
      .active = YES;
}

// Create a button with the desired image, action and accessibility label.
- (UIButton*)createImageButton:(UIImage*)image
                        action:(SEL)action
            accessibilityLabel:(NSString*)accessibilityLabel {
  UIButton* imageButton = [self createButton:image];
  [imageButton setImage:[self applySymbolTint:image]
               forState:UIControlStateNormal];
  [self setMinimumSizeForButton:imageButton];
  [imageButton addTarget:self
                  action:action
        forControlEvents:UIControlEventTouchUpInside];
  [imageButton setAccessibilityLabel:accessibilityLabel];
  return imageButton;
}

// Sets or removes the title for the manual fill button based on whether the UI
// is currently in compact mode (tablets only).
- (void)adjustManualFillButtonTitle:(UIButton*)manualFillButton {
  // The manual fill button can only have a title when using the large accessory
  // view on a tablet.
  if (!_isTabletFormFactor || !_largeAccessoryViewEnabled) {
    return;
  }

  // The default configuration has no title or padding.
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];

  // The image should always be set, whether or not there's a title.
  buttonConfiguration.image = self.manualFillSymbol;

  if ([self isLiquidGlassEffectEnabled]) {
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        0, 0, 0, LiquidGlassCloseButtonTrailingInset);
  }

  FormInputAccessoryViewTextData* textData =
      [self.delegate textDataforFormInputAccessoryView:self];

  if (!_isCompact && textData.manualFillButtonTitle) {
    // Set the button title with a custom sized font.
    UIFont* font = [UIFont systemFontOfSize:kManualFillTitleFontSize
                                     weight:UIFontWeightMedium];
    NSDictionary* attributes;
    if ([self isLiquidGlassEffectEnabled]) {
      attributes = @{
        NSFontAttributeName : font,
        NSForegroundColorAttributeName : [UIColor colorNamed:kTextPrimaryColor]
      };
    } else {
      attributes = @{NSFontAttributeName : font};
    }

    NSAttributedString* attributedTitle = [[NSAttributedString alloc]
        initWithString:textData.manualFillButtonTitle
            attributes:attributes];
    buttonConfiguration.attributedTitle = attributedTitle;

    // If the button has both a title and an image, add padding around the
    // title so that it's not directly next to the image.
    if (self.manualFillSymbol) {
      buttonConfiguration.imagePadding = kManualFillTitlePadding;
    }
  }

  manualFillButton.configuration = buttonConfiguration;
}

// Creates a button of the correct type, depending on whether it is an image
// button or not.
- (UIButton*)createButton:(BOOL)isImageButton {
  // With liquid glass, image buttons apply a tint to the symbols, so the type
  // needs to be set to UIButtonTypeCustom.
  return [UIButton
      buttonWithType:(isImageButton && [self isLiquidGlassEffectEnabled])
                         ? UIButtonTypeCustom
                         : UIButtonTypeSystem];
}

// With liquid glass, we apply a symbol tint to make the symbols visible with
// the glass background.
- (UIImage*)applySymbolTint:(UIImage*)image {
  if ([self isLiquidGlassEffectEnabled]) {
    return [image imageWithTintColor:[UIColor colorNamed:kTextPrimaryColor]
                       renderingMode:UIImageRenderingModeAlwaysOriginal];
  }

  return image;
}

// Returns an image that is tinted with blue color when split view is
// active.
- (UIImage*)applySymbolTintForCloseButton:(UIImage*)image {
  if ([self isSplitViewActive]) {
    return [image imageWithTintColor:[UIColor colorNamed:kStaticBlueColor]
                       renderingMode:UIImageRenderingModeAlwaysOriginal];
  } else {
    return [self applySymbolTint:image];
  }
}

// Create the manual fill button.
- (UIButton*)createManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  UIButton* manualFillButton =
      [self createImageButton:self.manualFillSymbol
                       action:@selector(manualFillButtonTapped)
           accessibilityLabel:textData.manualFillButtonAccessibilityLabel];

  [self adjustManualFillButtonTitle:manualFillButton];

  return manualFillButton;
}

// Create the password manual fill button.
- (UIButton*)createPasswordManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.passwordManualFillSymbol
                  action:@selector(passwordManualFillButtonTapped)
      accessibilityLabel:textData.passwordManualFillButtonAccessibilityLabel];
}

// Create the credit card manual fill button.
- (UIButton*)createCreditCardManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.creditCardManualFillSymbol
                  action:@selector(creditCardManualFillButtonTapped)
      accessibilityLabel:textData.creditCardManualFillButtonAccessibilityLabel];
}

// Create the address manual fill button.
- (UIButton*)createAddressManualFillButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self
       createImageButton:self.addressManualFillSymbol
                  action:@selector(addressManualFillButtonTapped)
      accessibilityLabel:textData.addressManualFillButtonAccessibilityLabel];
}

// Create the previous button.
- (UIButton*)createPreviousButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:SymbolNamed(kChevronUpSymbol)
                          action:@selector(previousButtonTapped)
              accessibilityLabel:textData.previousButtonAccessibilityLabel];
}

// Create the next button.
- (UIButton*)createNextButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  return [self createImageButton:SymbolNamed(kChevronDownSymbol)
                          action:@selector(nextButtonTapped)
              accessibilityLabel:textData.nextButtonAccessibilityLabel];
}

// Create the close button.
- (UIButton*)createCloseButtonWithText:
    (FormInputAccessoryViewTextData*)textData {
  UIButton* closeButton = [self createButton:self.closeButtonSymbol];
  [closeButton addTarget:self
                  action:@selector(closeButtonTapped)
        forControlEvents:UIControlEventTouchUpInside];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  if (self.closeButtonSymbol) {
    buttonConfiguration.image =
        [self applySymbolTintForCloseButton:self.closeButtonSymbol];
  } else {
    UIImage* checkmarkSymbol;
    if ([self isLiquidGlassEffectEnabled]) {
      UIImageConfiguration* configuration = [UIImageSymbolConfiguration
          configurationWithPointSize:kCheckmarkSymbolPointSize
                              weight:UIImageSymbolWeightSemibold
                               scale:UIImageSymbolScaleMedium];
      checkmarkSymbol = [UIImage systemImageNamed:kCheckmarkSymbol
                                withConfiguration:configuration];
      [self setMinimumSizeForButton:closeButton];
    }

    if (checkmarkSymbol) {
      buttonConfiguration.image =
          [self applySymbolTintForCloseButton:checkmarkSymbol];
    } else {
      buttonConfiguration.title = textData.closeButtonTitle;
    }
  }

  if ([self isSplitViewActive]) {
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsZero;
    closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  } else {
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        0, ManualFillCloseButtonLeadingInset,
        self.closeButtonSymbol ? ManualFillCloseButtonBottomInset : 0,
        [self isLiquidGlassEffectEnabled] ? LiquidGlassCloseButtonTrailingInset
                                          : ManualFillCloseButtonTrailingInset);
  }
  closeButton.configuration = buttonConfiguration;

  [closeButton setAccessibilityLabel:textData.closeButtonAccessibilityLabel];

  return closeButton;
}

- (void)createOmniboxTypingShield {
  if (!_omniboxTypingShield) {
    CHECK(_contentView);
    _omniboxTypingShield = [[UIButton alloc] init];
    _omniboxTypingShield.translatesAutoresizingMaskIntoConstraints = NO;
    _omniboxTypingShield.backgroundColor = UIColor.clearColor;
    _omniboxTypingShield.isAccessibilityElement = NO;
    _omniboxTypingShield.opaque = NO;
    _omniboxTypingShield.accessibilityIdentifier =
        kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID;
    [self addSubview:_omniboxTypingShield];

    AddSameConstraintsToSides(
        self, _omniboxTypingShield,
        LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);
    _omniboxTypingShieldHeightConstraint =
        [_omniboxTypingShield.heightAnchor constraintEqualToConstant:0];
    _omniboxTypingShieldBottomConstraint = [_omniboxTypingShield.bottomAnchor
        constraintEqualToAnchor:_contentView.topAnchor];
    _omniboxTypingShieldHiddenBottomConstraint =
        [_omniboxTypingShield.bottomAnchor
            constraintEqualToAnchor:self.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      _omniboxTypingShieldHeightConstraint, _omniboxTypingShieldBottomConstraint
    ]];
    [_omniboxTypingShield addTarget:self
                             action:@selector(omniboxTypingShieldTapped)
                   forControlEvents:UIControlEventTouchUpInside];
  }
}

// Returns the height of the accessory. Returns a larger height when using the
// large accessory view.
- (CGFloat)accessoryHeight {
  return (_largeAccessoryViewEnabled || [self isLiquidGlassEffectEnabled])
             ? kLargeKeyboardAccessoryHeight
             : kDefaultAccessoryHeight;
}

// Sets up the liquid glass effect for the accessory. Returns whether liquid
// glass is enabled.
- (BOOL)setupLiquidGlassEffect {
  if ([self isLiquidGlassEffectEnabled]) {
    if (@available(iOS 26, *)) {
      UIVisualEffectView* effectView = CreateGlassEffectView();
      CHECK(effectView);

      [self addSubview:effectView];

      [self setOmniboxSafeTopConstraint:effectView];

      // Add padding under and on the sides of the keyboard accessory.
      [self.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:effectView.bottomAnchor
                                      constant:kSurroundingPadding]
          .active = YES;
      if (![self isSplitViewActive]) {
        [self.trailingAnchor constraintEqualToAnchor:effectView.trailingAnchor
                                            constant:kSurroundingPadding]
            .active = YES;
      }

      // For showing a smaller accessory, the width anchor is set instead of the
      // leading anchor.
      if (_smallWidthAccessoryViewEnabled) {
        [effectView.widthAnchor constraintEqualToConstant:kSmallAccessoryWidth]
            .active = YES;
      } else {
        _effectViewLeadingConstraint =
            [self.leadingAnchor constraintEqualToAnchor:effectView.leadingAnchor
                                               constant:-kSurroundingPadding];
        _effectViewLeadingConstraint.active = YES;
      }

      _contentView = effectView.contentView;
      AddSameConstraints(effectView, _contentView);
      [self setDefaultHeightConstraint:effectView];

      // Add shadow around the glass effect.
      self.layer.shadowRadius = kShadowRadius;
      self.layer.shadowOffset = CGSizeMake(0, kShadowVerticalOffset);
      self.layer.shadowOpacity = kShadowOpacity;
      self.layer.shadowColor =
          [UIColor colorNamed:kBackgroundShadowColor].CGColor;
      self.layer.masksToBounds = NO;

      return YES;
    }
  }

  return NO;
}

// Sets the bottom anchor depending on whether the liquid glass effect is
// enabled.
- (void)setBottomAnchorForView:(UIView*)view {
  if ([self isLiquidGlassEffectEnabled]) {
    // When using liquid glass, to ensure a constant height, we use the height
    // constraints, instead of the bottom constraint.
    [self setDefaultHeightConstraint:view];
  } else {
    [view.bottomAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.bottomAnchor]
        .active = YES;
  }
}

// Applies the proper horizontal padding, depending on whether the keyboard
// accessory is in compact mode (tablet only).
- (void)setHorizontalConstraints {
  if (!_isTabletFormFactor || !_largeAccessoryViewEnabled) {
    return;
  }

  _trailingConstraint.active = !_isCompact;
  _compactTrailingConstraint.active = _isCompact;
}

#pragma mark - UIView

- (void)setHidden:(BOOL)hidden {
  [super setHidden:hidden];

  // The bottom omnibox is anchored to the typing shield. If we don't change the
  // shield's anchor, when hiding the accessory view, there is a blank space the
  // size of the keyboard accessory view between the top of the view below the
  // accessory view and the omnibox. By changing the anchor here, the omnibox
  // appears directly above the view below the accessory view, without any gaps.
  _omniboxTypingShieldBottomConstraint.active = !hidden;
  _omniboxTypingShieldHiddenBottomConstraint.active = hidden;

  [self layoutIfNeeded];
}

@end
