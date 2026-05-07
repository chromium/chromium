// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"

#import <UIKit/UIKit.h>

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/content_suggestions/public/ntp_home_constants.h"
#import "ios/chrome/browser/content_suggestions/ui/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_availability.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"
#import "ios/chrome/browser/omnibox/public/omnibox_constants.h"
#import "ios/chrome/browser/omnibox/public/omnibox_presentation_context.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/gradient/gradient_view.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/legacy_toolbar_button_factory.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/buttons/toolbar_configuration.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_constants.h"
#import "ios/chrome/browser/toolbar/legacy/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_constants.h"
#import "ios/chrome/browser/toolbar/tab_group/ui/tab_group_indicator_view.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_constants.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"

namespace {

// Element ID for Fakebox scribble.
NSString* const kScribbleFakeboxElementId = @"kScribbleFakeboxElementId";

// Fakebox highlight animation duration.
const CGFloat kFakeboxHighlightDuration = 0.4;

// Fakebox highlight background alpha.
const CGFloat kFakeboxHighlightAlpha = 0.06;

// Height margin of the fake location bar.
const CGFloat kFakeLocationBarHeightMargin = 2;

// When the placeholder text in the fakebox doesn't fit, the font shrinks to fit
// the string. This is the minimum allowed factor by which it shrinks.
const CGFloat kFakeboxMinimumFontScaleFactor = 0.57;

// The constants for the constraints affecting the end button; either Lens or
// Voice Search, depending on if Lens is enabled.
const CGFloat kEndButtonFakeboxTrailingSpace = 13.0;
const CGFloat kEndButtonNormalSizeFakeboxWithBadgeTrailingSpace = 7.0;
const CGFloat kEndButtonOmniboxTrailingSpace = 7.0;

// Distance between the trailing fakebox icon and the placeholder text.
const CGFloat kHintLabelFakeboxTrailingSpace = 12.0f;

// The constants for the constraints the leading-edge aligned UI elements.
const CGFloat kHintLabelFakeboxLeadingSpaceWithIcon = 42.0;
const CGFloat kHintLabelFakeboxLeadingSpaceWithPlus = 46.0;
const CGFloat kHintLabelOmniboxLeadingSpaceWithIcon = 42.0;
const CGFloat kHintLabelOmniboxLeadingSpaceWithWithPlus = 52.0;

// The constants for the search engine image.
const CGFloat kFakeboxImageLeadingSpace = 13.0;
const CGFloat kFakeboxPlusLeadingSpace = 18.0;
const CGFloat kOmniboxImageLeadingSpace = 22.0;
const CGFloat kOmniboxPlusLeadingSpace = 26.0;
const CGFloat kFakeboxImageSize = 20.0;

// The spacing between the items in the button stack.
const CGFloat kButtonSpacing = 9.0;

// The height of the divider between the mic and lens icons.
const CGFloat kIconDividerHeight = 13.0;

// The offset of the account error badge from the ADP center.
const CGFloat kAccountBadgeOffsetFromDiscCenter = 10.0;

// The size of the account error badge that is on top the ADP.
const CGFloat kErrorSymbolPointSize = 16.0;

// The offset from the center of the customization button for where to show the
// new feature badge.
const CGFloat kCustomizationNewBadgeOffset = 14.0;

// The name of the animation for the MIA button.
NSString* const kMIACircleAnimationLightMode = @"mia_circle_animation_no_glow";
NSString* const kMIACircleAnimationDarkMode = @"mia_glowing_circle_animation";

// Returns the top color of the Fakebox's gradient background.
UIColor* FakeboxTopColor() {
  return UIAccessibilityIsReduceTransparencyEnabled()
             ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
             : [UIColor colorNamed:@"fake_omnibox_top_gradient_color"];
}

// Returns the bottom color of the Fakebox's gradient background.
UIColor* FakeboxBottomColor() {
  return UIAccessibilityIsReduceTransparencyEnabled()
             ? [UIColor colorNamed:@"fake_omnibox_solid_background_color"]
             : [UIColor colorNamed:@"fake_omnibox_bottom_gradient_color"];
}

// Returns the background color for the NTP Header view. This is the color
// that shows when the fakebox is scrolled up.
UIColor* HeaderBackgroundColor(id<UITraitEnvironment> environment) {
  if (IsSplitToolbarMode(environment)) {
    return [UIColor colorNamed:kBackgroundColor];
  } else {
    return [UIColor colorNamed:@"ntp_background_color"];
  }
}

UIColor* AccountParticleDiscBadgeBackgroundColor(UIUserInterfaceStyle style) {
  if (style == UIUserInterfaceStyleDark) {
    return [UIColor colorNamed:kBackgroundColor];
  } else {
    return [UIColor colorNamed:@"ntp_background_color"];
  }
}

// Returns a value in the range of `from` up to `to`, depending on the given
// `percent`.
CGFloat Interpolate(CGFloat from, CGFloat to, CGFloat percent) {
  if (percent <= 0.0) {
    return from;
  } else if (percent >= 1.0) {
    return to;
  }
  return from + (to - from) * percent;
}

}  // namespace

// `UIStackView` that allows the extended tap area of it's arranged subviews to
// overflow it's touch area.
@interface TouchAreaOverflowStackView : UIStackView

@end

@implementation TouchAreaOverflowStackView

- (BOOL)pointInside:(CGPoint)point withEvent:(UIEvent*)event {
  for (UIView* subview in self.arrangedSubviews) {
    // We consider a touch valid and allow it to propagate if it falls within
    // the bounds of any subview.
    // This means that even if a touch visually appears outside the stack view,
    // the `pointInside:withEvent:` method can correctly register it within a
    // subview's touch area, especially where subviews might extend beyond and
    // overflow the stack view's visual limits.
    CGPoint convertedPoint = [self convertPoint:point toView:subview];
    if ([subview pointInside:convertedPoint withEvent:event]) {
      return YES;
    }
  }

  return NO;
}

@end

@interface NewTabPageHeaderView () <TabGroupIndicatorViewDelegate,
                                    UIIndirectScribbleInteractionDelegate,
                                    UIPointerInteractionDelegate>

// The Lens button. May be null if Lens is not available.
@property(nonatomic, strong, readwrite) ExtendedTouchTargetButton* lensButton;
// The button that opens multiodal actions in Composebox. May be nil if
// Composebox or multimodal actions are not enabled.
@property(nonatomic, strong, readwrite) ExtendedTouchTargetButton* plusButton;

@property(nonatomic, strong) UIView* voiceAndLensDivider;

@property(nonatomic, strong, readwrite)
    ExtendedTouchTargetButton* voiceSearchButton;

@property(nonatomic, strong) UIView* separator;

// Layout constraints for fake omnibox background image and blur.
@property(nonatomic, strong) NSLayoutConstraint* fakeLocationBarTopConstraint;
@property(nonatomic, strong)
    NSLayoutConstraint* fakeLocationBarHeightConstraint;

// Constraint between the search field's leading edge and the leading view.
@property(nonatomic, strong) NSLayoutConstraint* leadingViewConstraint;

@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelTrailingConstraint;
// View used to add on-touch highlight to the fake omnibox.
@property(nonatomic, strong) UIView* fakeLocationBarHighlightView;
// View used to simulate the top toolbar when the header is stuck to the top of
// the NTP.
@property(nonatomic, strong) UIView* fakeToolbar;

@end

@implementation NewTabPageHeaderView {
  CGFloat _lastAnimationPercent;
  BOOL _useNewBadgeForLensButton;
  BOOL _lensButtonWithNewBadgeTapped;
  // The current scale of the transform for the hint label. 1 if not currently
  //  scaled.
  CGFloat _currentHintLabelScale;
  // Stores the small font used for the pinned fakebox.
  UIFont* _hintLabelFontSmall;
  // Stores the big font used for the unpinned fakebox.
  UIFont* _hintLabelFontBig;
  // Image view of the account disc particle badge.
  UIImageView* _accountDiscParticleBadgeImageView;
  // The New Feature badge on the customization menu's entrypoint.
  UIView* _customizationNewFeatureBadge;
  // A view to contain all the buttons on the trailing side of the fakebox.
  UIStackView* _buttonStack;
  // Default search engine logo view.
  UIImageView* _logoView;

  // Constraints to update the `toolbarView`'s postion according to the
  // `tabGroupIndicatorView`'s visibility.
  NSLayoutConstraint* _toolbarNoTabGroupIndicartorConstraint;
  NSLayoutConstraint* _toolbarTabGroupIndicartorConstraint;

  // Whether AIM is allowed.
  BOOL _isAIMAllowed;

  // Whether the current session is eligible to fusebox.
  BOOL _fuseboxEligible;

  // Whether the omnibox is pinned to the bottom position.
  BOOL _isBottomOmnibox;

  // Location bar view for when it has a colored gradient.
  GradientView* _fakeLocationBarGradientView;
  // Location bar view to use for when it should have a blur effect.
  UIVisualEffectView* _fakeLocationBarBlurEffectView;
}

#pragma mark - Public

- (instancetype)initWithUseNewBadgeForLensButton:
    (BOOL)useNewBadgeForLensButton {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.clipsToBounds = YES;
    _useNewBadgeForLensButton = useNewBadgeForLensButton;
    _lastAnimationPercent = 0;
    _currentHintLabelScale = 1;

    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:
              @[UITraitPreferredContentSizeCategory.class, UITraitUserInterfaceStyle.class]
                      withHandler:handler];
    NSMutableArray<UITrait>* buttonTraits =
        [@[ UITraitUserInterfaceStyle.class ] mutableCopy];
    if (IsNTPBackgroundCustomizationEnabled()) {
      NSArray<UITrait>* customizationTraits =
          @[ NewTabPageTrait.class, NewTabPageImageBackgroundTrait.class ];
      [buttonTraits addObjectsFromArray:customizationTraits];
      [self registerForTraitChanges:customizationTraits
                         withAction:@selector(applyBackgroundTheme)];
    }
    [self registerForTraitChanges:buttonTraits
                       withAction:@selector
                       (updateButtonsForCurrentTraitCollection)];

    // Create the identity disc button.
    _identityDiscButton = [UIButton buttonWithType:UIButtonTypeCustom];
    _identityDiscButton.accessibilityIdentifier = kNTPFeedHeaderIdentityDisc;
    _identityDiscButton.pointerInteractionEnabled = YES;
    _identityDiscButton.pointerStyleProvider = ^UIPointerStyle*(
        UIButton* button, UIPointerEffect* proposedEffect,
        UIPointerShape* proposedShape) {
      CGFloat singleInset =
          (button.frame.size.width - ntp_home::kIdentityAvatarDimension) / 2;
      CGRect rect = CGRectInset(button.frame, singleInset, singleInset);
      UIPointerShape* shape =
          [UIPointerShape shapeWithRoundedRect:rect
                                  cornerRadius:rect.size.width / 2];
      return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
    };
  }
  return self;
}

- (void)addToolbarView:(UIView*)toolbarView {
  _toolBarView = toolbarView;
  [self addSubview:toolbarView];

  _toolbarNoTabGroupIndicartorConstraint =
      [toolbarView.topAnchor constraintEqualToAnchor:self.topAnchor];
  [NSLayoutConstraint activateConstraints:@[
    [toolbarView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [toolbarView.heightAnchor
        constraintEqualToConstant:content_suggestions::FakeToolbarHeight()],
    [toolbarView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    _toolbarNoTabGroupIndicartorConstraint,
  ]];

  // Add identity disc button to toolbar.
  [self.toolBarView addSubview:self.identityDiscButton];
  self.identityDiscButton.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.identityDiscButton.centerYAnchor
        constraintEqualToAnchor:self.toolBarView.centerYAnchor],
  ]];
}

- (void)setPlaceholderText:(NSString*)placeholderText {
  if (_placeholderText == placeholderText) {
    return;
  }
  _placeholderText = placeholderText;
  [self.omnibox.textInput setDefaultPlaceholderText:placeholderText];
  self.searchHintLabel.text = placeholderText;
}

- (void)setOmniboxPositionIsBottom:(BOOL)isBottomOmnibox {
  CHECK(IsChromeNextIaEnabled());
  _isBottomOmnibox = isBottomOmnibox;
}

- (void)setIsGoogleDefaultSearchEngine:(BOOL)isGoogleDefaultSearchEngine {
  if (_isGoogleDefaultSearchEngine == isGoogleDefaultSearchEngine) {
    return;
  }

  _isGoogleDefaultSearchEngine = isGoogleDefaultSearchEngine;

  [self removeAllFakeboxButtonsFromStack];
  [self addFakeboxButtonsToStack];
}

- (void)setupSubviews {
  [self setupFakeOmnibox];
  [self setupFakeTapView];
  [self setupIdentityDisc];
  [self addSeparatorToSearchField:self.fakeOmniboxContainer];
}

- (void)setupIdentityDisc {
  [self.identityDiscButton addTarget:self.commandHandler
                              action:@selector(identityDiscWasTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
}

- (void)setupFakeOmnibox {
  self.fakeOmniboxContainer = [[UIView alloc] init];
  self.fakeOmniboxContainer.translatesAutoresizingMaskIntoConstraints = NO;
  [self.fakeOmniboxContainer setIsAccessibilityElement:NO];
  self.fakeOmniboxContainer.accessibilityIdentifier =
      ntp_home::FakeOmniboxAccessibilityID();
  [self addSubview:self.fakeOmniboxContainer];

  self.accessibilityButton = [[UIButton alloc] init];
  [self.accessibilityButton addTarget:self.commandHandler
                               action:@selector(fakeboxTapped)
                     forControlEvents:UIControlEventTouchUpInside];
  [self.accessibilityButton addObserver:self
                             forKeyPath:@"highlighted"
                                options:NSKeyValueObservingOptionNew
                                context:NULL];

  CGFloat fakeOmniboxHeight = content_suggestions::FakeOmniboxHeight();
  self.accessibilityButton.layer.cornerRadius =
      (fakeOmniboxHeight - kFakeLocationBarHeightMargin) / 2;
  self.accessibilityButton.clipsToBounds = YES;
  self.accessibilityButton.isAccessibilityElement = YES;
  self.accessibilityButton.accessibilityLabel = self.placeholderText;
  self.accessibilityButton.accessibilityIdentifier =
      kNTPFakeOmniboxAccessibilityButton;
  [self.fakeOmniboxContainer addSubview:self.accessibilityButton];
  self.accessibilityButton.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.fakeOmniboxContainer, self.accessibilityButton);

  NSMutableArray<UIAccessibilityCustomAction*>* accessibilityCustomActions =
      [[NSMutableArray alloc] init];
  if (self.lensButton) {
    [accessibilityCustomActions
        addObject:[[UIAccessibilityCustomAction alloc]
                      initWithName:l10n_util::GetNSString(
                                       IDS_IOS_KEYBOARD_ACCESSORY_VIEW_LENS)
                             image:nil
                            target:self
                          selector:@selector(openLensViewFinder)]];
  }

  if (self.voiceSearchButton) {
    [accessibilityCustomActions
        addObject:
            [[UIAccessibilityCustomAction alloc]
                initWithName:l10n_util::GetNSString(
                                 IDS_IOS_KEYBOARD_ACCESSORY_VIEW_VOICE_SEARCH)
                       image:nil
                      target:self
                    selector:@selector(openVoiceSearch)]];
  }

  if ([self shouldShowPlusButton]) {
    [accessibilityCustomActions
        addObject:
            [[UIAccessibilityCustomAction alloc]
                initWithName:
                    l10n_util::GetNSString(
                        IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL)
                       image:nil
                      target:self
                    selector:@selector(openMultimodalActionsMenu)]];
  }

  self.accessibilityButton.accessibilityCustomActions =
      accessibilityCustomActions;

  [self.fakeOmniboxContainer
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

  [self addViewsToFakeOmnibox];

  UIIndirectScribbleInteraction* scribbleInteraction =
      [[UIIndirectScribbleInteraction alloc] initWithDelegate:self];
  [self.fakeOmniboxContainer addInteraction:scribbleInteraction];
}

- (void)openVoiceSearch {
  [self.NTPShortcutsHandler preloadVoiceSearch];
  [self.NTPShortcutsHandler loadVoiceSearchFromView:self.voiceSearchButton];
}

- (void)openMultimodalActionsMenu {
  [self.NTPShortcutsHandler openMultimodalActionsMenu];
}

- (void)setupFakeTapView {
  UIView* toolbar = [[UIView alloc] init];
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  self.fakeTapButton = [[UIButton alloc] init];
  if (!IsChromeNextIaEnabled()) {
    self.fakeTapButton.userInteractionEnabled = IsSplitToolbarMode(self);
  }
  self.fakeTapButton.isAccessibilityElement = NO;
  self.fakeTapButton.translatesAutoresizingMaskIntoConstraints = NO;
  [toolbar addSubview:self.fakeTapButton];
  [self addToolbarView:toolbar];
  [self.fakeTapButton addTarget:self
                         action:@selector(fakeTapViewTapped)
               forControlEvents:UIControlEventTouchUpInside];
  AddSameConstraints(self.fakeTapButton, toolbar);
}

- (void)fakeTapViewTapped {
  [self.commandHandler fakeTapViewTapped];
}

- (void)addViewsToFakeOmnibox {
  UIView* searchField = self.fakeOmniboxContainer;
  // Fake Toolbar.
  self.fakeToolbar = [[UIView alloc] init];
  [searchField insertSubview:self.fakeToolbar atIndex:0];
  self.fakeToolbar.translatesAutoresizingMaskIntoConstraints = NO;

  // Fake location bar.
  [self.fakeToolbar addSubview:self.fakeLocationBar];

  if (!IsComposeboxIOSEnabled()) {
    // Omnibox, used for animations.
    // TODO(crbug.com/40615993): See if it is possible to share some
    // initialization code with the real Omnibox.
    UIColor* color = [UIColor colorNamed:kTextfieldPlaceholderColor];
    OmniboxContainerView* omnibox = [[OmniboxContainerView alloc]
              initWithFrame:CGRectZero
                  textColor:color
              textInputTint:color
                   iconTint:color
        presentationContext:OmniboxPresentationContext::kNTPHeader];
    [omnibox.textInput setDefaultPlaceholderText:self.placeholderText];
    [omnibox.textInput setText:@""];
    omnibox.translatesAutoresizingMaskIntoConstraints = NO;
    [searchField addSubview:omnibox];
    AddSameConstraints(omnibox, self.fakeLocationBar);
    omnibox.textInput.view.userInteractionEnabled = NO;
    omnibox.hidden = YES;
    self.omnibox = omnibox;

    // Cancel button, used in animation.
    LegacyToolbarButtonFactory* factory = [[LegacyToolbarButtonFactory alloc]
        initWithStyle:ToolbarStyle::kNormal];
    self.cancelButton = [factory cancelButton];
    [searchField addSubview:self.cancelButton];
    self.cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
    [NSLayoutConstraint activateConstraints:@[
      [self.cancelButton.centerYAnchor
          constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
      [self.cancelButton.leadingAnchor
          constraintEqualToAnchor:self.fakeLocationBar.trailingAnchor],
    ]];
  }

  // Hint label.
  self.searchHintLabel = [[UILabel alloc] init];
  self.searchHintLabel.adjustsFontSizeToFitWidth = true;
  self.searchHintLabel.minimumScaleFactor = kFakeboxMinimumFontScaleFactor;
  content_suggestions::ConfigureSearchHintLabel(
      self.searchHintLabel, searchField, self.placeholderText);
  [self updateHintLabelFonts];

  self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
      constraintEqualToAnchor:self.fakeLocationBar.leadingAnchor
                     constant:self.hintLabelFakeboxLeadingSpace];
  if (IsNTPHeaderTransformsForAnimationsEnabled()) {
    // Keep constraints fixed at progress = 0 values.
    self.hintLabelLeadingConstraint.constant =
        self.hintLabelFakeboxLeadingSpace;
  }
  [NSLayoutConstraint activateConstraints:@[
    self.hintLabelLeadingConstraint,
    [self.searchHintLabel.heightAnchor
        constraintEqualToAnchor:self.fakeLocationBar.heightAnchor
                       constant:-ntp_header::kHintLabelHeightMargin],
    [self.searchHintLabel.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor
                       constant:-1.0],
  ]];
  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  self.searchHintLabel.isAccessibilityElement = NO;
  [self.searchHintLabel
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  // To ensure touch events are correctly forwarded to the buttons within the
  // stack view use a stack view implementation that propagates touches to its
  // subviews.
  // Otherwise the stack view would 'clip' the extended touch areas of its inner
  // buttons, preventing them from registering touches properly.
  _buttonStack = [[TouchAreaOverflowStackView alloc] init];
  _buttonStack.translatesAutoresizingMaskIntoConstraints = NO;
  _buttonStack.alignment = UIStackViewAlignmentCenter;
  _buttonStack.spacing = kButtonSpacing;
  _buttonStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, 0, 0, [self endButtonFakeboxTrailingSpace]);
  _buttonStack.layoutMarginsRelativeArrangement = true;
  [searchField addSubview:_buttonStack];
  [NSLayoutConstraint activateConstraints:@[
    [_buttonStack.trailingAnchor
        constraintEqualToAnchor:self.fakeLocationBar.trailingAnchor],
    [_buttonStack.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
  ]];

  [self addFakeboxButtonsToStack];

  // Constraints.
  AddSameConstraints(self.fakeToolbar, searchField);

  self.fakeLocationBarTopConstraint = [self.fakeLocationBar.topAnchor
      constraintEqualToAnchor:searchField.topAnchor];
  self.fakeLocationBarLeadingConstraint = [self.fakeLocationBar.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor];
  self.fakeLocationBarTrailingConstraint = [self.fakeLocationBar.trailingAnchor
      constraintEqualToAnchor:searchField.trailingAnchor];
  self.fakeLocationBarHeightConstraint = [self.fakeLocationBar.heightAnchor
      constraintEqualToConstant:content_suggestions::FakeOmniboxHeight()];
  self.fakeLocationBar.layer.cornerRadius =
      content_suggestions::FakeOmniboxHeight() / 2;
  [NSLayoutConstraint activateConstraints:@[
    self.fakeLocationBarTopConstraint,
    self.fakeLocationBarLeadingConstraint,
    self.fakeLocationBarTrailingConstraint,
    self.fakeLocationBarHeightConstraint,
  ]];

  [self addLeadingViewToSearchField:searchField];
}

// The leading padding to add in the search field when the fakebox is displayed
// on top.
- (CGFloat)omniboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kOmniboxPlusLeadingSpace;
  } else {
    return kOmniboxImageLeadingSpace;
  }
}

// The leading padding to add in the search field when the fakebox is displayed
// in the middle of the screen.
- (CGFloat)fakeboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kFakeboxPlusLeadingSpace;
  } else {
    return kFakeboxImageLeadingSpace;
  }
}

// Adds the appropriate leading view on the given search field.
- (void)addLeadingViewToSearchField:(UIView*)searchField {
  UIView* leadingView;
  CGFloat leadingViewYOffset = 0;
  if ([self shouldShowPlusButton]) {
    [self createPlusButton];
    leadingView = self.plusButton;
    leadingViewYOffset = -3;
  } else {
    _logoView = [[UIImageView alloc] init];
    _logoView.contentMode = UIViewContentModeScaleAspectFit;
    leadingView = _logoView;
    leadingViewYOffset = -2;
  }

  if (!leadingView) {
    return;
  }

  leadingView.translatesAutoresizingMaskIntoConstraints = NO;
  [searchField addSubview:leadingView];
  AddSquareConstraints(leadingView, kFakeboxImageSize);

  self.leadingViewConstraint = [leadingView.leadingAnchor
      constraintEqualToAnchor:searchField.leadingAnchor
                     constant:[self omniboxLeadingSpace]];

  [NSLayoutConstraint activateConstraints:@[
    self.leadingViewConstraint,
    [leadingView.centerYAnchor constraintEqualToAnchor:searchField.centerYAnchor
                                              constant:leadingViewYOffset],

  ]];
}

- (void)setDefaultSearchEngineLogo:(UIImage*)logo {
  _logoView.image = logo;
}

// Updates button styling for the current trait collection.
- (void)updateButtonsForCurrentTraitCollection {
  // Variations containing MIA entry point force disable colors in the icons.
  const BOOL forceDisableColors = IsAimEnabledInNtp();
  const BOOL darkUIStyle =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark;
  const BOOL ntpHasCustomBackground =
      IsNTPBackgroundCustomizationEnabled() &&
      ([self.traitCollection boolForNewTabPageImageBackgroundTrait] ||
       [self.traitCollection objectForNewTabPageTrait]);
  const BOOL useColorIcon =
      !darkUIStyle && !forceDisableColors && !ntpHasCustomBackground;

  content_suggestions::ConfigureVoiceSearchButton(self.voiceSearchButton,
                                                  useColorIcon);
  if (self.lensButton) {
    // Only color the badge if there's no image background.
    UIColor* newBadgeColor =
        [self.traitCollection boolForNewTabPageImageBackgroundTrait]
            ? nil
            : [self.traitCollection objectForNewTabPageTrait].tintColor;
    content_suggestions::ConfigureLensButtonAppearance(
        self.lensButton, _useNewBadgeForLensButton, useColorIcon,
        newBadgeColor);
    if (_useNewBadgeForLensButton) {
      content_suggestions::ConfigureLensButtonWithNewBadgeAlpha(
          self.lensButton, 1 - _lastAnimationPercent);
    }
  }
}

- (void)addSeparatorToSearchField:(UIView*)searchField {
  DCHECK(searchField.superview == self);

  self.separator = [[UIView alloc] init];
  self.separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  self.separator.alpha = 0;
  self.separator.translatesAutoresizingMaskIntoConstraints = NO;
  [searchField addSubview:self.separator];
  [NSLayoutConstraint activateConstraints:@[
    [self.separator.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [self.separator.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
    [self.separator.topAnchor constraintEqualToAnchor:searchField.bottomAnchor],
    [self.separator.heightAnchor
        constraintEqualToConstant:content_suggestions::HeaderSeparatorHeight()],
  ]];
}

- (CGFloat)searchFieldProgressForOffset:(CGFloat)offset {
  // The scroll offset at which point searchField's frame should stop growing.
  CGFloat maxScaleOffset = [self offsetToBeginFakeOmniboxExpansion];

  // The scroll offset at which point searchField's frame should start
  // growing.
  CGFloat startScaleOffset = maxScaleOffset - ntp_header::kAnimationDistance;
  CGFloat percent = 0;
  if (offset && offset > startScaleOffset) {
    CGFloat animatingOffset = offset - startScaleOffset;
    percent = std::clamp<CGFloat>(
        animatingOffset / ntp_header::kAnimationDistance, 0, 1);
  }
  if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
    // For ipad and landscape iphone, this makes the animation start slowly
    // and accelerate especially towards the end.
    percent = percent * percent;
  }
  return percent;
}

// Animate the leading view from its fakebox position to its scrolled omnibox
// position linearly. When percent is 0, the fakebox is displayed in the
// middle of the screen; when it's 1, the fakebox is fully scrolled up.
- (void)updateLogoAnimationWithProgress:(CGFloat)progress {
  if (IsNTPHeaderTransformsForAnimationsEnabled()) {
    self.leadingViewConstraint.constant = kFakeboxImageLeadingSpace;
    CGFloat translationX =
        (kOmniboxImageLeadingSpace - kFakeboxImageLeadingSpace) * progress;
    _logoView.transform = CGAffineTransformMakeTranslation(translationX, 0);
  } else {
    self.leadingViewConstraint.constant = Interpolate(
        [self fakeboxLeadingSpace], [self omniboxLeadingSpace], progress);
  }
}

// Updates the background color and opacity of the fakebox based on progress.
- (void)updateFakeboxBackgroundWithProgress:(CGFloat)progress {
  // Update the opacity of the header background color as the user scrolls so
  // that content does not appear beneath it. Since the NTP background might be
  // a gradient, the opacity must be 0 by default.
  self.backgroundColor =
      [HeaderBackgroundColor(self) colorWithAlphaComponent:progress];

  [self setFakeboxColorsWithProgress:progress];
}

// Animates the hint label position and scale between fakebox and omnibox based
// on progress.
- (void)updateHintLabelAnimationWithProgress:(CGFloat)progress {
  [self scaleHintLabelForPercent:progress];
  CGFloat hintLabelScalingExtraOffset =
      (_currentHintLabelScale - 1) *
      self.searchHintLabel.intrinsicContentSize.width * 0.5;

  if (IsNTPHeaderTransformsForAnimationsEnabled()) {
    self.hintLabelTrailingConstraint.constant = -kHintLabelFakeboxTrailingSpace;
    CGFloat tx = 0;
    if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
      tx = hintLabelScalingExtraOffset;
    } else {
      tx = hintLabelScalingExtraOffset + (self.hintLabelOmniboxLeadingSpace -
                                          self.hintLabelFakeboxLeadingSpace) *
                                             progress;
    }

    // Combine scale (already set in scaleHintLabelForPercent:) and translation.
    self.searchHintLabel.transform =
        CGAffineTransformScale(CGAffineTransformMakeTranslation(tx, 0),
                               _currentHintLabelScale, _currentHintLabelScale);
  } else {
    // If MIA animation view is shown then add an aditional spacing to avoid any
    // overlap with the label.
    self.hintLabelTrailingConstraint.constant =
        -hintLabelScalingExtraOffset - kHintLabelFakeboxTrailingSpace;

    if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
      self.hintLabelLeadingConstraint.constant =
          self.hintLabelFakeboxLeadingSpace + hintLabelScalingExtraOffset;
    } else {
      self.hintLabelLeadingConstraint.constant =
          hintLabelScalingExtraOffset +
          Interpolate(self.hintLabelFakeboxLeadingSpace,
                      self.hintLabelOmniboxLeadingSpace, progress);
    }
  }
}

// Updates constraints for the pinned layout where the search field is
// collapsed.
- (void)updatePinnedLayoutWithProgress:(CGFloat)progress
                searchFieldNormalWidth:(CGFloat)searchFieldNormalWidth
                       widthConstraint:(NSLayoutConstraint*)widthConstraint {
  CGFloat fakeOmniboxHeight = content_suggestions::FakeOmniboxHeight();

  // When Voiceover is running, if the header's alpha is set to 0, voiceover
  // can't scroll back to it, and it will never come back into view. To
  // prevent that, set the alpha to non-zero when the header is fully
  // offscreen. It will still not be seen, but it will be accessible to
  // Voiceover.
  self.alpha = std::max(1 - progress, 0.01);

  widthConstraint.constant = searchFieldNormalWidth;
  self.fakeLocationBarHeightConstraint.constant =
      fakeOmniboxHeight - kFakeLocationBarHeightMargin;
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  self.fakeLocationBarLeadingConstraint.constant = 0;
  self.fakeLocationBarTrailingConstraint.constant = 0;
  self.fakeLocationBarTopConstraint.constant = 0;

  self.separator.alpha = 0;

  _buttonStack.directionalLayoutMargins = NSDirectionalEdgeInsetsMake(
      0, 0, 0, [self endButtonFakeboxTrailingSpace]);
}

// Updates constraints for the expanding layout where the search field grows to
// fill the width.
- (void)updateExpandingLayoutWithProgress:(CGFloat)progress
                           safeAreaInsets:(UIEdgeInsets)safeAreaInsets
                   searchFieldNormalWidth:(CGFloat)searchFieldNormalWidth
                          widthConstraint:(NSLayoutConstraint*)widthConstraint
                         heightConstraint:(NSLayoutConstraint*)heightConstraint
                      topMarginConstraint:
                          (NSLayoutConstraint*)topMarginConstraint {
  CGFloat fakeOmniboxHeight = content_suggestions::FakeOmniboxHeight();
  CGFloat locationBarHeight = content_suggestions::PinnedFakeOmniboxHeight();

  self.alpha = 1;
  self.separator.alpha = progress;

  CGFloat maxWidth = self.bounds.size.width;
  widthConstraint.constant =
      Interpolate(searchFieldNormalWidth, maxWidth, progress);
  CGFloat maxTopMarginDiff = fakeOmniboxHeight - locationBarHeight -
                             kAdaptiveLocationBarVerticalMargin;
  topMarginConstraint.constant =
      -content_suggestions::SearchFieldTopMargin(self.logoState) -
      maxTopMarginDiff * progress;
  heightConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint -
      content_suggestions::HeaderSeparatorHeight() +
      Interpolate(fakeOmniboxHeight,
                  locationBarHeight + kAdaptiveLocationBarVerticalMargin,
                  progress);

  // Calculate the amount to shrink the width and height of background so that
  // it's where the focused adapative toolbar focuses.
  self.fakeLocationBarLeadingConstraint.constant = Interpolate(
      0, safeAreaInsets.left + kExpandedLocationBarHorizontalMargin, progress);
  self.fakeLocationBarTrailingConstraint.constant = -Interpolate(
      0, safeAreaInsets.right + kExpandedLocationBarHorizontalMargin, progress);

  self.fakeLocationBarTopConstraint.constant =
      ntp_header::kFakeLocationBarTopConstraint * progress;
  self.fakeLocationBarHeightConstraint.constant =
      Interpolate(fakeOmniboxHeight, locationBarHeight, progress);
  self.fakeLocationBar.layer.cornerRadius =
      self.fakeLocationBarHeightConstraint.constant / 2;

  // Adjust the position of the search field's subviews.
  CGFloat endButtonInset =
      Interpolate([self endButtonFakeboxTrailingSpace],
                  kEndButtonOmniboxTrailingSpace, progress);
  _buttonStack.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, 0, 0, endButtonInset);

  // Fade in badge treatment when scrolled.
  if (_useNewBadgeForLensButton && !_lensButtonWithNewBadgeTapped &&
      self.lensButton) {
    content_suggestions::ConfigureLensButtonWithNewBadgeAlpha(self.lensButton,
                                                              1 - progress);
    // Hide divider when N badge is shown.
    self.voiceAndLensDivider.alpha = progress;
  }
}

// Calculates progress and calls appropriate helper methods to update the header
// layout based on scroll offset.
- (void)updateSearchFieldWidth:(NSLayoutConstraint*)widthConstraint
                        height:(NSLayoutConstraint*)heightConstraint
                     topMargin:(NSLayoutConstraint*)topMarginConstraint
                     forOffset:(CGFloat)offset
                   screenWidth:(CGFloat)screenWidth
                safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  CGFloat contentWidth = std::max<CGFloat>(
      0, screenWidth - safeAreaInsets.left - safeAreaInsets.right);
  if (screenWidth == 0 || contentWidth == 0) {
    return;
  }

  CGFloat searchFieldNormalWidth =
      content_suggestions::SearchFieldWidth(contentWidth, self.traitCollection);

  CGFloat percent = [self searchFieldProgressForOffset:offset];

  [self updateFakeboxBackgroundWithProgress:percent];

  [self updateTabGroupIndicatorAvailabilityWithOffset:offset];

  [self updateHintLabelAnimationWithProgress:percent];

  [self updateLogoAnimationWithProgress:percent];

  if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
    [self updatePinnedLayoutWithProgress:percent
                  searchFieldNormalWidth:searchFieldNormalWidth
                         widthConstraint:widthConstraint];
  } else {
    [self updateExpandingLayoutWithProgress:percent
                             safeAreaInsets:safeAreaInsets
                     searchFieldNormalWidth:searchFieldNormalWidth
                            widthConstraint:widthConstraint
                           heightConstraint:heightConstraint
                        topMarginConstraint:topMarginConstraint];
  }

  _lastAnimationPercent = percent;
}

- (void)setFakeboxHighlighted:(BOOL)highlighted {
  [UIView animateWithDuration:kFakeboxHighlightDuration
                        delay:0
                      options:UIViewAnimationOptionCurveEaseOut
                   animations:^{
                     CGFloat alpha = highlighted ? kFakeboxHighlightAlpha : 0;
                     self.fakeLocationBarHighlightView.backgroundColor =
                         [UIColor colorWithWhite:0 alpha:alpha];
                   }
                   completion:nil];
}

- (void)setIdentityDiscErrorBadge {
  if (!_identityDiscButton) {
    return;
  }
  _accountDiscParticleBadgeImageView = [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kErrorCircleFillSymbol,
                                               kErrorSymbolPointSize)];
  _accountDiscParticleBadgeImageView.translatesAutoresizingMaskIntoConstraints =
      NO;
  _accountDiscParticleBadgeImageView.tintColor =
      [UIColor colorNamed:kRed500Color];
  _accountDiscParticleBadgeImageView.backgroundColor =
      AccountParticleDiscBadgeBackgroundColor(
          self.traitCollection.userInterfaceStyle);
  _accountDiscParticleBadgeImageView.layer.cornerRadius =
      _accountDiscParticleBadgeImageView.frame.size.width / 2;
  _accountDiscParticleBadgeImageView.clipsToBounds = YES;
  _accountDiscParticleBadgeImageView.accessibilityIdentifier =
      kNTPFeedHeaderIdentityDiscBadge;

  [_identityDiscButton addSubview:_accountDiscParticleBadgeImageView];

  [NSLayoutConstraint activateConstraints:@[
    [_accountDiscParticleBadgeImageView.centerXAnchor
        constraintEqualToAnchor:_identityDiscButton.centerXAnchor
                       constant:kAccountBadgeOffsetFromDiscCenter],
    [_accountDiscParticleBadgeImageView.centerYAnchor
        constraintEqualToAnchor:_identityDiscButton.centerYAnchor
                       constant:kAccountBadgeOffsetFromDiscCenter],
  ]];
}

- (void)removeIdentityDiscErrorBadge {
  [_accountDiscParticleBadgeImageView removeFromSuperview];
  _accountDiscParticleBadgeImageView = nil;
}

- (void)setCustomizationMenuButton:(UIButton*)customizationMenuButton
                      withNewBadge:(BOOL)hasNewBadge {
  if (_customizationMenuButton) {
    [_customizationMenuButton removeFromSuperview];
  }

  if (IsNTPBackgroundCustomizationEnabled()) {
    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];

    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kPencilSymbol, ntp_home::kNTPMenuButtonIconSize);
    configuration.image = icon;
    configuration.background.cornerRadius =
        ntp_home::kNTPMenuButtonCornerRadius;
    customizationMenuButton.configuration = configuration;

    UIColor* unthemedTintColor = [UIColor colorNamed:kBlue600Color];
    customizationMenuButton.configurationUpdateHandler =
        CreateThemedButtonConfigurationUpdateHandler(
            unthemedTintColor, ^UIColor*(NewTabPageColorPalette* palette) {
              if (palette) {
                return palette.headerButtonColor;
              }

              return [UIColor colorWithDynamicProvider:^UIColor*(
                                  UITraitCollection* traits) {
                return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                           ? [UIColor
                                 colorNamed:kTabGroupFaviconBackgroundColor]
                           : [[UIColor colorNamed:kSolidWhiteColor]
                                 colorWithAlphaComponent:
                                     ntp_home::
                                         kNTPMenuButtonLightUnthemedAlpha];
              }];
            });
  }

  customizationMenuButton.translatesAutoresizingMaskIntoConstraints = NO;
  customizationMenuButton.pointerInteractionEnabled = YES;
  customizationMenuButton.clipsToBounds = YES;

  NewFeatureBadgeView* newBadgeView =
      [[NewFeatureBadgeView alloc] initWithBadgeSize:20 fontSize:10];
  newBadgeView.translatesAutoresizingMaskIntoConstraints = NO;
  newBadgeView.userInteractionEnabled = NO;
  newBadgeView.layer.opacity = hasNewBadge ? 1 : 0;

  [self.toolBarView addSubview:customizationMenuButton];
  [self.toolBarView addSubview:newBadgeView];

  [NSLayoutConstraint activateConstraints:@[
    [customizationMenuButton.centerYAnchor
        constraintEqualToAnchor:self.toolBarView.centerYAnchor],
    [customizationMenuButton.heightAnchor
        constraintEqualToConstant:ntp_home::kNTPMenuButtonDimension],
    [customizationMenuButton.widthAnchor
        constraintEqualToAnchor:customizationMenuButton.heightAnchor],
    [customizationMenuButton.leadingAnchor
        constraintEqualToAnchor:self.safeAreaLayoutGuide.leadingAnchor
                       constant:(ntp_home::kIdentityAvatarPadding +
                                 ntp_home::kHeaderIconMargin)],
    [newBadgeView.centerXAnchor
        constraintEqualToAnchor:customizationMenuButton.centerXAnchor
                       constant:kCustomizationNewBadgeOffset],
    [newBadgeView.centerYAnchor
        constraintEqualToAnchor:customizationMenuButton.centerYAnchor
                       constant:-kCustomizationNewBadgeOffset],
  ]];

  _customizationMenuButton = customizationMenuButton;
  _customizationNewFeatureBadge = newBadgeView;

  if (IsNTPBackgroundCustomizationEnabled()) {
    [self applyBackgroundTheme];
  }
}

- (void)setToolsMenuButton:(UIButton*)toolsMenuButton {
  CHECK(IsChromeNextIaEnabled());
  if (_toolsMenuButton) {
    [_toolsMenuButton removeFromSuperview];
  }

  if (IsNTPBackgroundCustomizationEnabled()) {
    UIButtonConfiguration* configuration =
        [UIButtonConfiguration plainButtonConfiguration];
    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kMenuSymbol, ntp_home::kNTPMenuButtonIconSize);
    configuration.image = icon;
    configuration.background.cornerRadius =
        ntp_home::kNTPMenuButtonCornerRadius;
    toolsMenuButton.configuration = configuration;

    UIColor* unthemedTintColor = [UIColor colorNamed:kBlue600Color];
    toolsMenuButton.configurationUpdateHandler =
        CreateThemedButtonConfigurationUpdateHandler(
            unthemedTintColor, ^UIColor*(NewTabPageColorPalette* palette) {
              if (palette) {
                return palette.headerButtonColor;
              }

              return [UIColor colorWithDynamicProvider:^UIColor*(
                                  UITraitCollection* traits) {
                return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                           ? [UIColor
                                 colorNamed:kTabGroupFaviconBackgroundColor]
                           : [[UIColor colorNamed:kSolidWhiteColor]
                                 colorWithAlphaComponent:
                                     ntp_home::
                                         kNTPMenuButtonLightUnthemedAlpha];
              }];
            });
  }

  toolsMenuButton.translatesAutoresizingMaskIntoConstraints = NO;
  toolsMenuButton.pointerInteractionEnabled = YES;
  toolsMenuButton.clipsToBounds = YES;

  [self.toolBarView addSubview:toolsMenuButton];

  [NSLayoutConstraint activateConstraints:@[
    [toolsMenuButton.centerYAnchor
        constraintEqualToAnchor:self.toolBarView.centerYAnchor],
    [toolsMenuButton.heightAnchor
        constraintEqualToConstant:ntp_home::kNTPMenuButtonDimension],
    [toolsMenuButton.widthAnchor
        constraintEqualToAnchor:toolsMenuButton.heightAnchor],
    [toolsMenuButton.leadingAnchor
        constraintEqualToAnchor:self.customizationMenuButton.trailingAnchor
                       constant:ntp_home::kHeaderIconMargin]
  ]];

  _toolsMenuButton = toolsMenuButton;

  if (IsNTPBackgroundCustomizationEnabled()) {
    [self applyBackgroundTheme];
  }
}

- (void)hideBadgeOnCustomizationMenu {
  _customizationNewFeatureBadge.alpha = 0;
}

- (void)updateTabGroupIndicatorAvailabilityWithOffset:(CGFloat)offset {
  BOOL canShowTabStrip = CanShowTabStrip(self);
  BOOL isAvailable = !IsCompactHeight(self) && !canShowTabStrip;
  _tabGroupIndicatorView.available = isAvailable;

  // Make the view disappear when the indicator is scrolled out of the screen.
  // The absolute value of the offset is used to make the view disappear when:
  // - Scrolling down to reveal the Discover section.
  // - Scrolling up to reveal the overscroll actions.
  _tabGroupIndicatorView.alpha =
      1 - fmax(0, (abs(offset) / kTabGroupIndicatorHeight));

  _toolbarTabGroupIndicartorConstraint.constant =
      kTabGroupIndicatorNTPToolbarMargin - fmax(offset, 0);
  if (_tabGroupIndicatorView.hidden) {
    _toolbarTabGroupIndicartorConstraint.active = NO;
    _toolbarNoTabGroupIndicartorConstraint.active = YES;
  } else {
    _toolbarNoTabGroupIndicartorConstraint.active = NO;
    _toolbarTabGroupIndicartorConstraint.active = YES;
  }
}

- (UIView*)fakeboxButtonsSnapshot {
  return [_buttonStack snapshotViewAfterScreenUpdates:NO];
}

- (void)setAIMAllowed:(BOOL)allowed {
  _isAIMAllowed = allowed;
}

- (void)setFuseboxEligible:(BOOL)eligible {
  _fuseboxEligible = eligible;
}

- (BOOL)shouldShowPlusButton {
  return IsPlusButtonInFakeboxEnabled() && _isAIMAllowed && _fuseboxEligible;
}

#pragma mark - Property accessors

- (UIView*)fakeLocationBar {
  if (!_fakeLocationBar) {
    _fakeLocationBar = [[UIView alloc] init];
    _fakeLocationBar.userInteractionEnabled = NO;
    _fakeLocationBar.clipsToBounds = YES;
    _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;

    _fakeLocationBarGradientView =
        [[GradientView alloc] initWithTopColor:FakeboxTopColor()
                                   bottomColor:FakeboxBottomColor()];
    _fakeLocationBarGradientView.userInteractionEnabled = NO;
    _fakeLocationBarGradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [_fakeLocationBar addSubview:_fakeLocationBarGradientView];
    AddSameConstraints(_fakeLocationBar, _fakeLocationBarGradientView);

    if (IsNTPBackgroundCustomizationEnabled()) {
      UIVisualEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemThickMaterial];
      _fakeLocationBarBlurEffectView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      _fakeLocationBarBlurEffectView.userInteractionEnabled = NO;
      _fakeLocationBarBlurEffectView.translatesAutoresizingMaskIntoConstraints =
          NO;
      [_fakeLocationBar addSubview:_fakeLocationBarBlurEffectView];
      AddSameConstraints(_fakeLocationBar, _fakeLocationBarBlurEffectView);
    }

    _fakeLocationBarHighlightView = [[UIView alloc] init];
    _fakeLocationBarHighlightView.userInteractionEnabled = NO;
    _fakeLocationBarHighlightView.backgroundColor = UIColor.clearColor;
    _fakeLocationBarHighlightView.translatesAutoresizingMaskIntoConstraints =
        NO;
    [_fakeLocationBar addSubview:_fakeLocationBarHighlightView];
    AddSameConstraints(_fakeLocationBar, _fakeLocationBarHighlightView);

    // Make sure the correct background is visible.
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self applyBackgroundTheme];
    } else {
      _fakeLocationBarGradientView.hidden = NO;
      _fakeLocationBarBlurEffectView.hidden = YES;
    }
  }
  return _fakeLocationBar;
}

#pragma mark - Setters

// Sets tabgroupIndicatorView.
- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  _tabGroupIndicatorView = view;
  _tabGroupIndicatorView.hidden = YES;
  _tabGroupIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  _tabGroupIndicatorView.showSeparator = YES;
  _tabGroupIndicatorView.delegate = self;
  [self addSubview:_tabGroupIndicatorView];

  _toolbarTabGroupIndicartorConstraint = [_toolBarView.topAnchor
      constraintEqualToAnchor:_tabGroupIndicatorView.bottomAnchor
                     constant:kTabGroupIndicatorNTPToolbarMargin];
  [NSLayoutConstraint activateConstraints:@[
    [self.tabGroupIndicatorView.leadingAnchor
        constraintEqualToAnchor:self.leadingAnchor],
    [self.tabGroupIndicatorView.trailingAnchor
        constraintEqualToAnchor:self.trailingAnchor],
    [self.tabGroupIndicatorView.topAnchor
        constraintEqualToAnchor:self.topAnchor
                       constant:kTabGroupIndicatorNTPTopMargin],
    [_tabGroupIndicatorView.heightAnchor
        constraintEqualToConstant:kTabGroupIndicatorHeight],
  ]];
  [self updateTabGroupIndicatorAvailabilityWithOffset:0];
}

#pragma mark - Private

// Handles the creation of the plus button.
- (void)createPlusButton {
  self.plusButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  self.plusButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_IOS_COMPOSEBOX_ADD_ATTACHMENT_BUTTON_ACCESSIBILITY_LABEL);
  [self.plusButton
      setImage:DefaultSymbolWithPointSize(kPlusSymbol, kSymbolActionPointSize)
      forState:UIControlStateNormal];
  [self.plusButton addTarget:self.NTPShortcutsHandler
                      action:@selector(openMultimodalActionsMenu)
            forControlEvents:UIControlEventTouchUpInside];
}

// Sets the background based on the current NTP background, current color
// palette, or defaults if neither are set.
- (void)applyBackgroundTheme {
  // Fakebox coloring looks at image/color/default to determine correct colors.
  [self setFakeboxColorsWithProgress:_lastAnimationPercent];

  BOOL hasBlurredBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];
  if (hasBlurredBackground) {
    _fakeLocationBarGradientView.hidden = YES;
    _fakeLocationBarBlurEffectView.hidden = NO;
    return;
  }

  _fakeLocationBarGradientView.hidden = NO;
  _fakeLocationBarBlurEffectView.hidden = YES;
}

// Empties the fakebox buttons stack.
- (void)removeAllFakeboxButtonsFromStack {
  for (UIView* arrangedSubview in _buttonStack.arrangedSubviews) {
    [arrangedSubview removeFromSuperview];
  }
}

// Adds the necessary buttons to the fakebox stack.
- (void)addFakeboxButtonsToStack {
  // Voice search.
  self.voiceSearchButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  [_buttonStack addArrangedSubview:self.voiceSearchButton];

  // Lens.
  const BOOL useLens =
      lens_availability::CheckAndLogAvailabilityForLensEntryPoint(
          LensEntrypoint::NewTabPage, self.isGoogleDefaultSearchEngine);
  if (useLens) {
    [self addVoiceAndLensDivider];
    self.lensButton =
        [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
    [_buttonStack addArrangedSubview:self.lensButton];
    if (_useNewBadgeForLensButton) {
      [self.lensButton addTarget:self
                          action:@selector(lensButtonWithNewBadgeTapped:)
                forControlEvents:UIControlEventTouchUpInside];
    }
  }

  [self updateButtonsForCurrentTraitCollection];

  [self addActionsToFakeboxButtons];
  [self updateHintLabelTrailingConstraint];
}

// Registers the actions for the fakebox buttons.
- (void)addActionsToFakeboxButtons {
  [self.voiceSearchButton addTarget:self
                             action:@selector(loadVoiceSearch:)
                   forControlEvents:UIControlEventTouchUpInside];
  [self.voiceSearchButton addTarget:self
                             action:@selector(preloadVoiceSearch:)
                   forControlEvents:UIControlEventTouchDown];
  [self.lensButton addTarget:self
                      action:@selector(openLensViewFinder)
            forControlEvents:UIControlEventTouchUpInside];
}

// Updates the trailing constraint of the label to the nearest button stack
// element.
- (void)updateHintLabelTrailingConstraint {
  UIView* referenceView = _buttonStack.arrangedSubviews.firstObject;
  if (!referenceView) {
    return;
  }

  self.hintLabelTrailingConstraint = [self.searchHintLabel.trailingAnchor
      constraintLessThanOrEqualToAnchor:referenceView.leadingAnchor
                               constant:-kHintLabelFakeboxTrailingSpace];
  self.hintLabelTrailingConstraint.priority = UILayoutPriorityDefaultHigh;
  [NSLayoutConstraint activateConstraints:@[
    [referenceView.centerYAnchor
        constraintEqualToAnchor:self.fakeLocationBar.centerYAnchor],
    self.hintLabelTrailingConstraint,
  ]];
}

// Gets the fonts for the pinned and unpinned fakebox hint label, and sets
// the correct one.
- (void)updateHintLabelFonts {
  UIContentSizeCategory maxCategory =
      IsChromeNextIaEnabled() ? LocationBarSteadyViewMaxSizeCategory()
                              : LegacyLocationBarSteadyViewMaxSizeCategory();
  _hintLabelFontSmall = PreferredFontForTextStyleWithMaxCategory(
      LocationBarFontTextStyle(),
      self.traitCollection.preferredContentSizeCategory, maxCategory);
  CGFloat bigFontSize = _hintLabelFontSmall.pointSize /
                        (1.0 - content_suggestions::kHintTextScale);
  _hintLabelFontBig = [_hintLabelFontSmall fontWithSize:bigFontSize];
  self.searchHintLabel.font =
      [self hintLabelFontForPercent:_lastAnimationPercent];
}

// Returns the font for the hint label at the given animation percent.
- (UIFont*)hintLabelFontForPercent:(CGFloat)percent {
  if (percent == 1 && !self.allowFontScaleAnimation) {
    return _hintLabelFontSmall;
  }
  return _hintLabelFontBig;
}

// Scale the the hint label down to at most content_suggestions::kHintTextScale.
- (void)scaleHintLabelForPercent:(CGFloat)percent {
  DCHECK(self.searchHintLabel);
  if (percent == _lastAnimationPercent) {
    return;
  }

  if (percent > 0.90) {
    // When percent is very close to 1, the big font will be scaled down to be
    // almost the same size as the small font. But due to rendering differences
    // the big font scaled down can actually look slightly smaller than the
    // small font. By switching to the small font 10% early, a glitchy jump in
    // size is avoided.
    percent = 1;
  }

  UILabel* searchHintLabel = self.searchHintLabel;
  UIFont* font = [self hintLabelFontForPercent:percent];
  if (searchHintLabel.font != font) {
    searchHintLabel.font = font;
  }

  if (percent == 1 && !self.allowFontScaleAnimation) {
    // When pinned, the small font is used without scaling down.
    _currentHintLabelScale = 1;
    searchHintLabel.transform = CGAffineTransformIdentity;
    return;
  }

  // When unpinned, the bigger font is used and scaling is applied depending on
  // the animation percent.
  _currentHintLabelScale = 1 - (content_suggestions::kHintTextScale * percent);
  searchHintLabel.transform = CGAffineTransformMakeScale(
      _currentHintLabelScale, _currentHintLabelScale);
}

// The positive offset value to begin the fake omnibox expansion animation.
- (CGFloat)offsetToBeginFakeOmniboxExpansion {
  CGFloat offset =
      self.frame.size.height - content_suggestions::FakeToolbarHeight();

  // For non-split toolbar, the fake omnibox goes beneath the toolbar.
  if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
    // The animation should start when the primary toolbar is met.
    offset += content_suggestions::FakeOmniboxHeight();

    // iPads pin slightly earlier than landscape iPhones.
    if (CanShowTabStrip(self)) {
      offset -= content_suggestions::SearchFieldTopMargin(self.logoState);
    } else if (IsChromeNextIaEnabled() && !_isBottomOmnibox) {
      offset -= kToolbarHeight + self.safeAreaInsets.top;
    }
  }
  return offset;
}

// Sets the fakebox's colors, based on the current customization settings and
// the progress towards being pinned at the top.
- (void)setFakeboxColorsWithProgress:(CGFloat)progress {
  UIColor* pinnedColor = [UIColor colorNamed:kTextfieldBackgroundColor];
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];

  // Use a quadratic curve interpolation.
  progress = progress * progress;
  [_fakeLocationBarGradientView
      setStartColor:BlendColors(colorPalette ? colorPalette.omniboxColor
                                             : FakeboxTopColor(),
                                pinnedColor, progress)
           endColor:BlendColors(colorPalette ? colorPalette.omniboxColor
                                             : FakeboxBottomColor(),
                                pinnedColor, progress)];

  UIColor* defaultTintColor =
      content_suggestions::DefaultIconTintColorWithAIMAllowed(_isAIMAllowed);
  UIColor* defaultDividerColor = [UIColor colorNamed:kGrey600Color];
  UIColor* tintColor = colorPalette ? BlendColors(colorPalette.tintColor,
                                                  defaultTintColor, progress)
                                    : defaultTintColor;
  UIColor* dividerColor =
      colorPalette ? BlendColors(colorPalette.omniboxIconDividerColor,
                                 defaultDividerColor, progress)
                   : defaultDividerColor;
  _voiceSearchButton.tintColor = tintColor;
  _lensButton.tintColor = tintColor;
  _plusButton.tintColor = tintColor;
  _voiceAndLensDivider.backgroundColor = dividerColor;
}

// Creates a thin grey divider that acts as a visual separator.
- (UIView*)createDivider {
  UIView* divider = [[UIView alloc] init];
  if (!IsNTPBackgroundCustomizationEnabled()) {
    divider.backgroundColor = [UIColor colorNamed:kGrey600Color];
  }
  divider.translatesAutoresizingMaskIntoConstraints = NO;
  CGFloat dividerWidth = 1.0 / [[UIScreen mainScreen] scale];

  [NSLayoutConstraint activateConstraints:@[
    [divider.heightAnchor constraintEqualToConstant:kIconDividerHeight],
    [divider.widthAnchor constraintEqualToConstant:dividerWidth],
  ]];

  return divider;
}

// Adds a short vertical line between the mic and lens icons in the fakebox.
- (void)addVoiceAndLensDivider {
  UIView* divider = [self createDivider];
  self.voiceAndLensDivider = divider;
  [_buttonStack addArrangedSubview:divider];
}

// Handles a lens button with new badge tap. Registers that the tap has occurred
// and animates out the new badge portion of the button.
- (void)lensButtonWithNewBadgeTapped:(id)sender {
  if (!_lensButtonWithNewBadgeTapped) {
    _lensButtonWithNewBadgeTapped = YES;
    [UIView
        animateWithDuration:kMaterialDuration1
                 animations:^{
                   content_suggestions::ConfigureLensButtonWithNewBadgeAlpha(
                       self.lensButton, 0);
                 }];
  }
}

// Returns end button fakebox trailing space depending on fakebox size and
// whether the new badge is displayed.
- (CGFloat)endButtonFakeboxTrailingSpace {
  // If normal sized fakebox and new bade is showing, reduce trailing space.
  if (_useNewBadgeForLensButton && !IsAimEnabledInNtp()) {
    return kEndButtonNormalSizeFakeboxWithBadgeTrailingSpace;
  }
  // Common trailing space.
  return kEndButtonFakeboxTrailingSpace;
}

// Updates facets of the UI to reflect the change in the collection of UITraits.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateHintLabelFonts];
  }

  if (previousTraitCollection.userInterfaceStyle !=
      self.traitCollection.userInterfaceStyle) {
    // The fakebox background can be a blended color, which will not
    // automatically update when dark/light mode is changed. It needs to be
    // manually updated here.
    [self setFakeboxColorsWithProgress:_lastAnimationPercent];

    if (_accountDiscParticleBadgeImageView) {
      _accountDiscParticleBadgeImageView.backgroundColor =
          AccountParticleDiscBadgeBackgroundColor(
              self.traitCollection.userInterfaceStyle);
    }
  }
}

#pragma mark - helpers

- (CGFloat)hintLabelFakeboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kHintLabelFakeboxLeadingSpaceWithPlus;
  } else {
    return kHintLabelFakeboxLeadingSpaceWithIcon;
  }
}

- (CGFloat)hintLabelOmniboxLeadingSpace {
  if ([self shouldShowPlusButton]) {
    return kHintLabelOmniboxLeadingSpaceWithWithPlus;
  } else {
    return kHintLabelOmniboxLeadingSpaceWithIcon;
  }
}

#pragma mark - Action handling

- (void)openAIM {
  [self.NTPShortcutsHandler openAIM];
}

- (void)openLensViewFinder {
  [self.NTPShortcutsHandler openLensViewFinder];
}

- (void)loadVoiceSearch:(id)sender {
  UIView* voiceSearchButton = base::apple::ObjCCastStrict<UIView>(sender);
  [self.NTPShortcutsHandler loadVoiceSearchFromView:voiceSearchButton];
}

- (void)preloadVoiceSearch:(id)sender {
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  [self.NTPShortcutsHandler preloadVoiceSearch];
}

#pragma mark - TabGroupIndicatorViewDelegate

- (void)tabGroupIndicatorViewVisibilityUpdated:(BOOL)visible {
  CHECK_EQ(_tabGroupIndicatorView.hidden, !visible);
  if (visible) {
    _toolbarTabGroupIndicartorConstraint.active = YES;
    _toolbarNoTabGroupIndicartorConstraint.active = NO;
  } else {
    _toolbarTabGroupIndicartorConstraint.active = NO;
    _toolbarNoTabGroupIndicartorConstraint.active = YES;
  }
}

#pragma mark - UIIndirectScribbleInteractionDelegate

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
              requestElementsInRect:(CGRect)rect
                         completion:
                             (void (^)(NSArray<UIScribbleElementIdentifier>*
                                           elements))completion {
  completion(@[ kScribbleFakeboxElementId ]);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                   isElementFocused:
                       (UIScribbleElementIdentifier)elementIdentifier {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);
  return
      [self.toolbarDelegate fakeboxScribbleForwardingTarget].isFirstResponder;
}

- (CGRect)
    indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                frameForElement:(UIScribbleElementIdentifier)elementIdentifier {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);

  // Imitate the entire location bar being scribblable.
  return interaction.view.bounds;
}

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
               focusElementIfNeeded:
                   (UIScribbleElementIdentifier)elementIdentifier
                     referencePoint:(CGPoint)focusReferencePoint
                         completion:
                             (void (^)(UIResponder<UITextInput>* focusedInput))
                                 completion {
  if (!
      [self.toolbarDelegate fakeboxScribbleForwardingTarget].isFirstResponder) {
    [[self.toolbarDelegate fakeboxScribbleForwardingTarget]
        becomeFirstResponder];
  }

  completion([self.toolbarDelegate fakeboxScribbleForwardingTarget]);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
         shouldDelayFocusForElement:
             (UIScribbleElementIdentifier)elementIdentifier {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);
  return YES;
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  // If the view is no longer in a window due to a race condition, no
  // pointer style is needed.
  if (!interaction.view.window) {
    return nil;
  }
  // Without this, the hover effect looks slightly oversized.
  CGRect rect = CGRectInset(interaction.view.bounds, 1, 1);
  UIBezierPath* path =
      [UIBezierPath bezierPathWithRoundedRect:rect
                                 cornerRadius:rect.size.height];
  UIPreviewParameters* parameters = [[UIPreviewParameters alloc] init];
  parameters.visiblePath = path;
  UITargetedPreview* preview =
      [[UITargetedPreview alloc] initWithView:interaction.view
                                   parameters:parameters];
  UIPointerHoverEffect* effect =
      [UIPointerHoverEffect effectWithPreview:preview];
  effect.prefersScaledContent = NO;
  effect.prefersShadow = NO;
  UIPointerShape* shape = [UIPointerShape
      beamWithPreferredLength:interaction.view.bounds.size.height / 2
                         axis:UIAxisVertical];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([@"highlighted" isEqualToString:keyPath]) {
    [self setFakeboxHighlighted:[object isHighlighted]];
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}

- (void)dealloc {
  [self.accessibilityButton removeObserver:self forKeyPath:@"highlighted"];
}

@end
