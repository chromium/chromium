// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_keyboard_accessory_view.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views.h"
#import "ios/chrome/browser/omnibox/ui/keyboard_assist/omnibox_assistive_keyboard_views_utils.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"

namespace {

// Delay between the time the view is shown, and the time the Lens button iph
// should be shown.
constexpr base::TimeDelta kLensButtonIPHDelay = base::Seconds(1);

// The glass effect view height.
constexpr CGFloat kGlassEffectViewHeight = 58;

// Corner radius of the keyboard accessory on iOS 26.0+.
constexpr CGFloat kCornerRadius = 24;

// Shadow parameters. Used when the liquid glass effect is enabled.
constexpr CGFloat kShadowRadius = 16.0;
constexpr CGFloat kShadowVerticalOffset = 4.0;
constexpr CGFloat kShadowOpacity = 0.12;

}  // namespace

@interface OmniboxKeyboardAccessoryView () <SearchEngineObserving>

@property(nonatomic, retain) NSArray<NSString*>* buttonTitles;
@property(nonatomic, weak) id<OmniboxAssistiveKeyboardDelegate> delegate;
@property(nonatomic, weak) id<UIPasteConfigurationSupporting> pasteTarget;

// The shortcut stack view that is displayed by this view.
@property(nonatomic, weak) UIStackView* shortcutStackView;

// The search stack view that is displayed by this view.
@property(nonatomic, weak) UIStackView* searchStackView;

// The text field that this view is an accessory to.
@property(nonatomic, weak) UIResponder* responder;

// Called when a keyboard shortcut button is pressed.
- (void)keyboardButtonPressed:(NSString*)title;
// Creates a button shortcut for `title`.
- (UIView*)shortcutButtonWithTitle:(NSString*)title;

@end

@implementation OmniboxKeyboardAccessoryView {
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  /// The visual effect view applied to the keyboard accessory view.
  UIVisualEffectView* _effectView;
}

@synthesize buttonTitles = _buttonTitles;
@synthesize delegate = _delegate;

- (instancetype)initWithButtons:(NSArray<NSString*>*)buttonTitles
                      showTools:(BOOL)showTools
                       delegate:(id<OmniboxAssistiveKeyboardDelegate>)delegate
                    pasteTarget:(id<UIPasteConfigurationSupporting>)pasteTarget
             templateURLService:(TemplateURLService*)templateURLService
                      responder:(UIResponder*)responder {
  self = [super initWithFrame:CGRectZero
               inputViewStyle:UIInputViewStyleKeyboard];
  if (self) {
    _buttonTitles = buttonTitles;
    _showTools = showTools;
    _delegate = delegate;
    _pasteTarget = pasteTarget;
    _responder = responder;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    // When the glass effect is enabled, the view's height is explicitly managed
    // by a height constraint, so self-sizing must be disabled. For the default
    // style, self-sizing is allowed.
    self.allowsSelfSizing = !GlassEffectEnabled();
    self.templateURLService = templateURLService;
    [self addSubviews];

    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitUserInterfaceStyle.class ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateLensAppearanceOnTraitChange)];
  }
  return self;
}

- (UIView*)contentView {
  if (@available(iOS 26, *)) {
    if (_effectView) {
      return _effectView.contentView;
    }

    // Create glass effect
    UIGlassEffect* glassEffect = [[UIGlassEffect alloc] init];
    glassEffect.interactive = YES;
    glassEffect.tintColor = [UIColor colorNamed:kSecondaryBackgroundColor];
    _effectView = [[UIVisualEffectView alloc] initWithEffect:glassEffect];
    self.layer.shadowRadius = kShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kShadowVerticalOffset);
    self.layer.shadowOpacity = kShadowOpacity;
    self.layer.shadowColor =
        [UIColor colorNamed:kBackgroundShadowColor].CGColor;
    self.layer.masksToBounds = NO;
    _effectView.cornerConfiguration = [UICornerConfiguration
        configurationWithRadius:
            [UICornerRadius
                containerConcentricRadiusWithMinimum:kCornerRadius]];
    _effectView.translatesAutoresizingMaskIntoConstraints = NO;
    [_effectView.heightAnchor constraintEqualToConstant:kGlassEffectViewHeight]
        .active = YES;

    [self addSubview:_effectView];

    // Insets around the effectView to keep it floating above keyboard.
    const NSDirectionalEdgeInsets effectViewInsets =
        NSDirectionalEdgeInsetsMake(0, 12., 12., 12.);
    NSLayoutConstraint* heightConstraint =
        [self.heightAnchor constraintEqualToConstant:kGlassEffectViewHeight +
                                                     effectViewInsets.bottom];
    heightConstraint.priority = UILayoutPriorityRequired;
    heightConstraint.active = YES;
    AddSameConstraintsToSidesWithInsets(
        _effectView, self,
        LayoutSides::kTrailing | LayoutSides::kLeading | LayoutSides::kBottom,
        effectViewInsets);
    AddSameConstraints(_effectView, _effectView.contentView);
    return _effectView.contentView;
  }

  // Pre-iOS 26, no glass effect is used, so views are added directly to this
  // view.
  return self;
}

- (void)addSubviews {
  if (!self.subviews.count) {
    return;
  }

  // Remove any existing stack views from this view.
  [self.shortcutStackView removeFromSuperview];
  [self.searchStackView removeFromSuperview];

  const CGFloat kButtonMinWidth = 36.0;
  const CGFloat kButtonHeight = 36.0;
  const CGFloat kBetweenShortcutButtonSpacing = 5.0;
  const CGFloat kBetweenSearchButtonSpacing = 12.0;
  const CGFloat kSearchStackViewLeadingMargin = 10.0;
  const CGFloat kShortcutStackViewTrailingMargin = 10.0;
  const CGFloat kSearchStackViewTopMargin = 4.0;
  const CGFloat kSearchStackViewBottomMargin = 4.0;

  // Create and add stackview filled with the shortcut buttons.
  UIStackView* shortcutStackView = [[UIStackView alloc] init];
  shortcutStackView.translatesAutoresizingMaskIntoConstraints = NO;
  shortcutStackView.spacing = kBetweenShortcutButtonSpacing;
  shortcutStackView.alignment = UIStackViewAlignmentCenter;
  for (NSString* title in self.buttonTitles) {
    UIView* button = [self shortcutButtonWithTitle:title];
    [button setTranslatesAutoresizingMaskIntoConstraints:NO];
    [button.widthAnchor constraintGreaterThanOrEqualToConstant:kButtonMinWidth]
        .active = YES;
    [button.heightAnchor constraintEqualToConstant:kButtonHeight].active = YES;
    [shortcutStackView addArrangedSubview:button];
  }
  UIView* container = self.contentView;
  [container addSubview:shortcutStackView];
  self.shortcutStackView = shortcutStackView;

  // Create and add a stackview containing the leading assistive buttons, i.e.
  // Voice search, camera/Lens search and paste search.
  BOOL useLens = ios::provider::IsLensSupported() &&
                 !base::FeatureList::IsEnabled(kDisableLensCamera) &&
                 [self isGoogleSearchEngine:self.templateURLService];
  NSArray<UIControl*>* leadingControls =
      _showTools ? OmniboxAssistiveKeyboardLeadingControls(
                       _delegate, self.pasteTarget, useLens)
                 : @[];
  UIStackView* searchStackView = [[UIStackView alloc] init];
  searchStackView.translatesAutoresizingMaskIntoConstraints = NO;
  searchStackView.spacing = kBetweenSearchButtonSpacing;
  for (UIControl* button in leadingControls) {
    [searchStackView addArrangedSubview:button];
  }
  [container addSubview:searchStackView];
  self.searchStackView = searchStackView;

  // Position the stack views.
  id<LayoutGuideProvider> layoutGuide =
      _effectView ? _effectView.contentView.safeAreaLayoutGuide
                  : self.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [searchStackView.leadingAnchor
        constraintEqualToAnchor:layoutGuide.leadingAnchor
                       constant:kSearchStackViewLeadingMargin],
    [shortcutStackView.trailingAnchor
        constraintEqualToAnchor:layoutGuide.trailingAnchor
                       constant:-kShortcutStackViewTrailingMargin],
    [searchStackView.trailingAnchor
        constraintLessThanOrEqualToAnchor:shortcutStackView.leadingAnchor],
    [searchStackView.topAnchor
        constraintEqualToAnchor:_effectView ? _effectView.contentView.topAnchor
                                            : layoutGuide.topAnchor
                       constant:kSearchStackViewTopMargin],
    [searchStackView.bottomAnchor
        constraintEqualToAnchor:_effectView
                                    ? _effectView.contentView.bottomAnchor
                                    : layoutGuide.bottomAnchor
                       constant:-kSearchStackViewBottomMargin],
    [shortcutStackView.topAnchor
        constraintEqualToAnchor:searchStackView.topAnchor],
    [shortcutStackView.bottomAnchor
        constraintEqualToAnchor:searchStackView.bottomAnchor],
  ]];
}

- (UIView*)shortcutButtonWithTitle:(NSString*)title {
  const CGFloat kHorizontalEdgeInset = 8;
  const CGFloat kButtonTitleFontSize = 16.0;
  UIColor* kTitleColorStateHighlighted = [UIColor colorWithWhite:0.0 alpha:0.3];

  UIButton* button =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeCustom];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
      0, kHorizontalEdgeInset, 0, kHorizontalEdgeInset);
  UIFont* font = [UIFont systemFontOfSize:kButtonTitleFontSize
                                   weight:UIFontWeightMedium];
  NSAttributedString* attributedTitle =
      [[NSAttributedString alloc] initWithString:title
                                      attributes:@{NSFontAttributeName : font}];
  buttonConfiguration.attributedTitle = attributedTitle;
  buttonConfiguration.baseForegroundColor =
      [UIColor colorNamed:kTextPrimaryColor];
  button.configuration = buttonConfiguration;
  button.configurationUpdateHandler = ^(UIButton* incomingButton) {
    UIButtonConfiguration* updatedConfig = incomingButton.configuration;
    switch (incomingButton.state) {
      case UIControlStateHighlighted:
        updatedConfig.baseForegroundColor = kTitleColorStateHighlighted;
        break;
      case UIControlStateNormal:
        updatedConfig.baseForegroundColor =
            [UIColor colorNamed:kTextPrimaryColor];
        break;
      default:
        break;
    }
    incomingButton.configuration = updatedConfig;
  };

  button.clipsToBounds = YES;
  [button addTarget:self
                action:@selector(keyboardButtonPressed:)
      forControlEvents:UIControlEventTouchUpInside];
  button.isAccessibilityElement = YES;
  [button setAccessibilityLabel:title];
  return button;
}

- (BOOL)enableInputClicksWhenVisible {
  return YES;
}

- (void)keyboardButtonPressed:(id)sender {
  UIButton* button = base::apple::ObjCCastStrict<UIButton>(sender);
  [[UIDevice currentDevice] playInputClick];
  [_delegate keyPressed:button.configuration.title];
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  if (!self.window || ![self.responder isFirstResponder]) {
    return;
  }
  if (self.templateURLService) {
    // Log the Lens support status when the keyboard is opened.
    lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
        LensEntrypoint::Keyboard,
        [self isGoogleSearchEngine:self.templateURLService]);
  }

  UIButton* lensButton = _delegate.lensButton;
  if (lensButton) {
    __weak __typeof__(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(^{
          [weakSelf.delegate presentLensKeyboardInProductHelper];
        }),
        kLensButtonIPHDelay);
  }
}

#pragma mark - Setters

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
  } else {
    _searchEngineObserver.reset();
  }
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  // Regenerate the shortcut buttons depending on the new search engine.
  [self addSubviews];
}
- (void)templateURLServiceShuttingDown:(TemplateURLService*)urlService {
  self.templateURLService = nil;
}

#pragma mark - Private

- (BOOL)isGoogleSearchEngine:(TemplateURLService*)service {
  DCHECK(service);
  const TemplateURL* defaultURL = service->GetDefaultSearchProvider();
  return defaultURL &&
         defaultURL->GetEngineType(service->search_terms_data()) ==
             SEARCH_ENGINE_GOOGLE;
}

// Updates the Lens Button's appearance when the view's UITraits are modified.
- (void)updateLensAppearanceOnTraitChange {
  // The Lens button needs to be updated when the device goes from light to dark
  // mode or vice versa.
  UIButton* lensButton = _delegate.lensButton;
  if (lensButton) {
    UpdateLensButtonAppearance(lensButton);
  }
}

@end
