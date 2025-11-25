// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/bubble/public/in_product_help_type.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/ntp_home_constant.h"
#import "ios/chrome/browser/home_customization/coordinator/home_customization_delegate.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_consumer.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "ios/chrome/browser/ntp/shared/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller_delegate.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_mutator.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_shortcuts_handler.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_container_view.h"
#import "ios/chrome/browser/shared/model/profile/features.h"
#import "ios/chrome/browser/shared/public/commands/help_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/elements/new_feature_badge_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/start_surface/ui_bundled/start_surface_features.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/fakebox_focuser.h"
#import "ios/chrome/browser/toolbar/ui_bundled/public/toolbar_utils.h"
#import "ios/chrome/browser/toolbar/ui_bundled/tab_groups/ui/tab_group_indicator_view.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {

NSString* const kScribbleFakeboxElementId = @"fakebox";

// Height margin of the fake location bar.
const CGFloat kFakeLocationBarHeightMargin = 2;

// Horizontal padding between the edge of the pill and its label.
const CGFloat kPillHorizontalPadding = 13;

// Vertical padding between the edge of the pill and its label.
const CGFloat kPillVerticalPadding = 11;

// Multiplier for applying margins on multiple sides
const CGFloat kMarginMultiplier = 2;

// The maximum point size of the font for the Identity Disc button.
const CGFloat kIdentityDiscMaxFontSize = 24;

// The extra space between the avatar icon and the background for the sign in
// button. This is used when IsNTPBackgroundCustomizationEnabled() == YES and
// IsSignInButtonNoAvatarEnabled() == NO;
const CGFloat kIdentityDiscAvatarBackgroundSpacing = 5;

}  // namespace

@interface NewTabPageHeaderViewController () <
    SearchEngineLogoConsumer,
    UIIndirectScribbleInteractionDelegate,
    UIPointerInteractionDelegate>

// `YES` if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

@property(nonatomic, strong) NewTabPageHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) UIButton* accessibilityButton;
@property(nonatomic, copy) NSString* identityDiscAccessibilityLabel;
@property(nonatomic, strong, readwrite) UIButton* identityDiscButton;
@property(nonatomic, strong) UIImage* identityDiscImage;
@property(nonatomic, strong) UIButton* fakeTapButton;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* headerViewHeightConstraint;

// Whether or not the user is signed in.
@property(nonatomic, assign) BOOL isSignedIn;

// Name of the default search engine. Used for the omnibox placeholder text.
@property(nonatomic, copy) NSString* defaultSearchEngineName;

@end

@implementation NewTabPageHeaderViewController {
  BOOL _useNewBadgeForLensButton;
  BOOL _useNewBadgeForCustomizationMenu;
  // Tracks if the mutator has already been notified of the Lens entrypoint
  // "new" badge display.
  BOOL _didNotifyLensBadgeDisplay;
  // Tracks if the mutator has already been notified of the Homepage
  // Customization "new" badge display.
  BOOL _didNotifyCustomizationBadgeDisplay;
  BOOL _hasAccountError;
  // Constraint for the identity disc button width.
  NSLayoutConstraint* _identityDiscWidthConstraint;
  // Constraint for the identity disc button height.
  NSLayoutConstraint* _identityDiscHeightConstraint;
  // Trailing Anchor for the identity disc button.
  NSLayoutConstraint* _identityDiscTrailingConstraint;
  // Constraint for the identity disc button's capsule-style width.
  NSLayoutConstraint* _identityDiscCapsuleWidthConstraint;
  // Whether AIM is allowed.
  BOOL _isAIMAllowed;
  // The logo for the default search engine. This is owned by the caching system
  // backing this logo.
  __weak UIImage* _dseLogo;
  SearchEngineLogoMediator* _searchEngineLogoMediator;
  SearchEngineLogoState _searchEngineLogoState;
}

- (instancetype)initWithUseNewBadgeForLensButton:(BOOL)useNewBadgeForLensButton
                 useNewBadgeForCustomizationMenu:
                     (BOOL)useNewBadgeForCustomizationMenu {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _useNewBadgeForLensButton = useNewBadgeForLensButton;
    _useNewBadgeForCustomizationMenu = useNewBadgeForCustomizationMenu;

    NSArray<UITrait>* traits = TraitCollectionSetForTraits(@[
      UITraitHorizontalSizeClass.class,
      UITraitPreferredContentSizeCategory.class, UITraitUserInterfaceStyle.class
    ]);
    __weak __typeof(self) weakSelf = self;
    UITraitChangeHandler handler = ^(id<UITraitEnvironment> traitEnvironment,
                                     UITraitCollection* previousCollection) {
      [weakSelf updateUIOnTraitChange:previousCollection];
    };
    [self registerForTraitChanges:traits withHandler:handler];
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self registerForTraitChanges:
                @[ NewTabPageTrait.class, NewTabPageImageBackgroundTrait.class ]
                         withAction:@selector(applyBackgroundTheme)];
    }
  }
  return self;
}

#pragma mark - Public

- (UIView*)toolBarView {
  return self.headerView.toolBarView;
}

- (UIView*)fakeOmniboxView {
  return self.headerView.omnibox;
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        // Ensure omnibox is reset when not a regular tablet.
        if (IsSplitToolbarMode(newCollection)) {
          [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
        }
        // Fake Tap button only needs to work in portrait. Disable the button
        // in landscape because in landscape the button covers logoView (which
        // need to handle taps).
        self.fakeTapButton.userInteractionEnabled = IsSplitToolbarMode(self);
      };

  [coordinator animateAlongsideTransition:transition completion:nil];
}

- (void)dealloc {
  [self.accessibilityButton removeObserver:self forKeyPath:@"highlighted"];
}

- (void)expandHeaderForFocus {
  // Make sure that the offset is after the pinned offset to have the fake
  // omnibox taking the full width.
  CGFloat offset = 9000;
  [self updateLogoForOffset:offset];
  [self.headerView updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                                   height:self.fakeOmniboxHeightConstraint
                                topMargin:self.fakeOmniboxTopMarginConstraint
                                forOffset:offset
                              screenWidth:self.headerView.bounds.size.width
                           safeAreaInsets:self.view.safeAreaInsets];

  self.fakeOmniboxWidthConstraint.constant = self.headerView.bounds.size.width;
  [self.headerView layoutIfNeeded];
  UIView* topOmnibox =
      [self.layoutGuideCenter referencedViewUnderName:kTopOmniboxGuide];
  CGRect omniboxFrameInFakebox = [topOmnibox convertRect:topOmnibox.bounds
                                                  toView:self.fakeOmnibox];
  self.headerView.fakeLocationBarLeadingConstraint.constant =
      omniboxFrameInFakebox.origin.x;
  self.headerView.fakeLocationBarTrailingConstraint.constant =
      -(self.fakeOmnibox.bounds.size.width -
        (omniboxFrameInFakebox.origin.x + omniboxFrameInFakebox.size.width));
  self.headerView.voiceSearchButton.alpha = 0;
  self.headerView.cancelButton.alpha = 0.7;
  self.headerView.omnibox.alpha = 1;
  self.headerView.searchHintLabel.alpha = 0;
  [self.headerView layoutIfNeeded];
}

- (void)completeHeaderFakeOmniboxFocusAnimationWithFinalPosition:
    (UIViewAnimatingPosition)finalPosition {
  self.headerView.omnibox.hidden = YES;
  self.headerView.cancelButton.hidden = YES;
  self.headerView.searchHintLabel.alpha = 1;
  self.headerView.voiceSearchButton.alpha = 1;
  if (finalPosition == UIViewAnimatingPositionEnd &&
      self.delegate.scrolledToMinimumHeight) {
    // Check to see if the collection are still scrolled to the top --
    // it's possible (and difficult) to unfocus the omnibox and initiate a
    // -shiftTilesDownForOmniboxDefocus before the animation here completes.
    if (IsSplitToolbarMode(self)) {
      [self.fakeboxFocuserHandler onFakeboxAnimationComplete];
    } else {
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
    }
  }
}

// TODO(crbug.com/40251610): Name animateScrollAnimation something more aligned
// to its true state indication. Why update the constraints only sometimes?
- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets
            animateScrollAnimation:(BOOL)animateScrollAnimation {
  if (self.isShowing) {
    [self.headerView updateTabGroupIndicatorAvailabilityWithOffset:offset];
    CGFloat progress =
        (_searchEngineLogoState != SearchEngineLogoState::kNone) ||
                !CanShowTabStrip(self)
            ? [self.headerView searchFieldProgressForOffset:offset]
            // RxR with no logo hides the fakebox, so always show the omnibox.
            : 1;
    [self updateLogoForOffset:offset];
    if (CanShowTabStrip(self) || !IsSplitToolbarMode(self)) {
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:progress];
    } else {
      // Ensure omnibox is reset when not a regular tablet.
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
    }
  }

  if (animateScrollAnimation) {
    [self.headerView updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                                     height:self.fakeOmniboxHeightConstraint
                                  topMargin:self.fakeOmniboxTopMarginConstraint
                                  forOffset:offset
                                screenWidth:screenWidth
                             safeAreaInsets:safeAreaInsets];
  }
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  self.fakeOmniboxWidthConstraint.constant =
      content_suggestions::SearchFieldWidth(width, self.traitCollection);
}

- (void)layoutHeader {
  [self.headerView layoutIfNeeded];
}

- (CGFloat)pinnedOffsetY {
  CGFloat offsetY = [self headerHeight];
  if (IsSplitToolbarMode(self) && !CanShowTabStrip(self)) {
    offsetY -= content_suggestions::FakeToolbarHeight();
  }

  return AlignValueToPixel(offsetY);
}

- (CGFloat)headerHeight {
  return content_suggestions::HeightForLogoHeader(_searchEngineLogoState,
                                                  self.traitCollection);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (!self.headerView) {
    self.view.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat width = self.view.frame.size.width;

    self.headerView = [[NewTabPageHeaderView alloc]
        initWithUseNewBadgeForLensButton:_useNewBadgeForLensButton];
    [self.headerView setAIMAllowed:_isAIMAllowed];
    self.headerView.NTPShortcutsHandler = self.NTPShortcutsHandler;
    self.headerView.isGoogleDefaultSearchEngine =
        self.isGoogleDefaultSearchEngine;
    self.headerView.placeholderText = self.placeholderText;
    self.headerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.headerView];
    AddSameConstraints(self.headerView, self.view);

    [self addFakeOmnibox];

    [self.headerView addSubview:_searchEngineLogoMediator.view];
    // Fake Tap View has identity disc, which should render above the doodle.
    [self addFakeTapView];
    [self.headerView addSubview:self.fakeOmnibox];
    _searchEngineLogoMediator.view.translatesAutoresizingMaskIntoConstraints =
        NO;
    _searchEngineLogoMediator.view.accessibilityIdentifier =
        ntp_home::NTPLogoAccessibilityID();
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

    [self.headerView addSeparatorToSearchField:self.fakeOmnibox];

    // Identity disc needs to be added after the Google logo/doodle since it
    // needs to respond to user taps first.
    [self addIdentityDisc];

    [self addCustomizationMenu];

    UIEdgeInsets safeAreaInsets = self.baseViewController.view.safeAreaInsets;
    width = std::max<CGFloat>(
        0, width - safeAreaInsets.left - safeAreaInsets.right);

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::SearchFieldWidth(
                                      width, self.traitCollection)];
    [self addConstraintsForLogoView:_searchEngineLogoMediator.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.headerView];

    self.headerView.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    if (IsNTPBackgroundCustomizationEnabled()) {
      [self applyBackgroundTheme];
    }
  }
}

- (void)setIsGoogleDefaultSearchEngine:(BOOL)isGoogleDefaultSearchEngine {
  _isGoogleDefaultSearchEngine = isGoogleDefaultSearchEngine;
  self.headerView.isGoogleDefaultSearchEngine = isGoogleDefaultSearchEngine;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if (_useNewBadgeForLensButton && !_didNotifyLensBadgeDisplay) {
    [self.mutator notifyLensBadgeDisplayed];
    _didNotifyLensBadgeDisplay = YES;
  }

  if (_useNewBadgeForCustomizationMenu &&
      !_didNotifyCustomizationBadgeDisplay) {
    [self.mutator notifyCustomizationBadgeDisplayed];
    _didNotifyCustomizationBadgeDisplay = YES;
  }

  // Check if the identity disc button was properly set before the view appears.
  DCHECK(self.identityDiscButton);
  DCHECK(self.identityDiscImage);
  DCHECK(self.identityDiscButton.accessibilityLabel);
  if (!IsSignInButtonNoAvatarEnabled()) {
    DCHECK([self.identityDiscButton imageForState:UIControlStateNormal] ||
           self.identityDiscButton.configuration.image);
  }

  [self maybeShowSwitchAccountsIPH];
}

- (void)setAllowFontScaleAnimation:(BOOL)allowFontScaleAnimation {
  _allowFontScaleAnimation = allowFontScaleAnimation;
  self.headerView.allowFontScaleAnimation = allowFontScaleAnimation;
}

- (void)omniboxDidResignFirstResponder {
  // Return early if the view is already showing.
  if (self.view.alpha == 1) {
    return;
  }
  self.view.alpha = 1;
}

- (void)hideBadgeOnCustomizationMenu {
  [self.headerView hideBadgeOnCustomizationMenu];
}

- (void)setTabGroupIndicatorView:(TabGroupIndicatorView*)view {
  self.headerView.tabGroupIndicatorView = view;
}

- (void)setNTPShortcutsHandler:
    (id<NewTabPageShortcutsHandler>)NTPShortcutsHandler {
  _NTPShortcutsHandler = NTPShortcutsHandler;
  self.headerView.NTPShortcutsHandler = NTPShortcutsHandler;
}

#pragma mark - FakeboxButtonsSnapshotProvider

- (UIView*)fakeboxButtonsSnapshot {
  return [self.headerView fakeboxButtonsSnapshot];
}

#pragma mark - Private

// Initialize and add a search field tap target and a voice search button.
- (void)addFakeOmnibox {
  self.fakeOmnibox = [[UIButton alloc] init];
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  self.fakeOmnibox.configuration = buttonConfiguration;
  self.fakeOmnibox.configurationUpdateHandler = ^(UIButton* incomingButton) {
    UIButtonConfiguration* updatedConfig = incomingButton.configuration;
    switch (incomingButton.state) {
      case UIControlStateHighlighted:
      case UIControlStateNormal:
        // This overrides default logic which would highlight the image. This
        // effectively disables highlighting.
        break;
      default:
        break;
    }
    incomingButton.configuration = updatedConfig;
  };

  // Set isAccessibilityElement to NO so that Voice Search button is accessible.
  [self.fakeOmnibox setIsAccessibilityElement:NO];
  self.fakeOmnibox.accessibilityIdentifier =
      ntp_home::FakeOmniboxAccessibilityID();

  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  self.accessibilityButton = [[UIButton alloc] init];
  [self.accessibilityButton addTarget:self
                               action:@selector(fakeboxTapped)
                     forControlEvents:UIControlEventTouchUpInside];
  // Because the visual fakebox background is implemented within
  // NewTabPageHeaderView, KVO the highlight events of
  // `accessibilityButton` and pass them along.
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
  [self.fakeOmnibox addSubview:self.accessibilityButton];
  self.accessibilityButton.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.fakeOmnibox, self.accessibilityButton);

  [self.fakeOmnibox
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

  [self.headerView addViewsToSearchField:self.fakeOmnibox];
  if (_dseLogo) {
    [self.headerView setDefaultSearchEngineLogo:_dseLogo];
  }

  UIIndirectScribbleInteraction* scribbleInteraction =
      [[UIIndirectScribbleInteraction alloc] initWithDelegate:self];
  [self.fakeOmnibox addInteraction:scribbleInteraction];

  if (self.headerView.lensButton) {
    [self.layoutGuideCenter referenceView:self.headerView.lensButton
                                underName:kFakeboxLensIconGuide];
  }

  [self updateVoiceSearchDisplay];
}

// On NTP in split toolbar mode the omnibox has different location (in the
// middle of the screen), but the users have muscle memory and still tap on area
// where omnibox is normally placed (the top area of NTP). Fake Tap Button is
// located in the same position where omnibox is normally placed and focuses the
// omnibox when tapped. Fake Tap Button user interactions are only enabled in
// split toolbar mode.
- (void)addFakeTapView {
  UIView* toolbar = [[UIView alloc] init];
  toolbar.translatesAutoresizingMaskIntoConstraints = NO;
  self.fakeTapButton = [[UIButton alloc] init];
  self.fakeTapButton.userInteractionEnabled = IsSplitToolbarMode(self);
  self.fakeTapButton.isAccessibilityElement = NO;
  self.fakeTapButton.translatesAutoresizingMaskIntoConstraints = NO;
  [toolbar addSubview:self.fakeTapButton];
  [self.headerView addToolbarView:toolbar];
  [self.fakeTapButton addTarget:self
                         action:@selector(fakeTapViewTapped)
               forControlEvents:UIControlEventTouchUpInside];
  AddSameConstraints(self.fakeTapButton, toolbar);
}

- (void)addIdentityDisc {
  // Set up a button. Details for the button will be set through delegate
  // implementation of UserAccountImageUpdateDelegate.
  self.identityDiscButton = [UIButton buttonWithType:UIButtonTypeCustom];
  self.identityDiscButton.accessibilityIdentifier = kNTPFeedHeaderIdentityDisc;
  [self.identityDiscButton addTarget:self.commandHandler
                              action:@selector(identityDiscWasTapped:)
                    forControlEvents:UIControlEventTouchUpInside];
  self.identityDiscButton.pointerInteractionEnabled = YES;
  self.identityDiscButton.pointerStyleProvider =
      ^UIPointerStyle*(UIButton* button, UIPointerEffect* proposedEffect,
                       UIPointerShape* proposedShape) {
        // The identity disc button is oversized to the avatar image to meet the
        // minimum touch target dimensions. The hover pointer effect should
        // match the avatar image dimensions, not the button dimensions.
        CGFloat singleInset =
            (button.frame.size.width - ntp_home::kIdentityAvatarDimension) / 2;
        CGRect rect = CGRectInset(button.frame, singleInset, singleInset);
        UIPointerShape* shape =
            [UIPointerShape shapeWithRoundedRect:rect
                                    cornerRadius:rect.size.width / 2];
        return [UIPointerStyle styleWithEffect:proposedEffect shape:shape];
      };

  // `self.identityDiscButton` should not be updated if `self.identityDiscImage`
  // is not available yet.
  if (self.identityDiscImage) {
    [self updateIdentityDiscState];
  }
  [self.headerView setIdentityDiscView:self.identityDiscButton];

  [self.layoutGuideCenter referenceView:self.identityDiscButton
                              underName:kNTPIdentityDiscButtonGuide];

  _identityDiscWidthConstraint =
      [self.identityDiscButton.widthAnchor constraintEqualToConstant:0];
  _identityDiscHeightConstraint =
      [self.identityDiscButton.heightAnchor constraintEqualToConstant:0];
  _identityDiscTrailingConstraint = [self.identityDiscButton.trailingAnchor
      constraintEqualToAnchor:self.headerView.safeAreaLayoutGuide.trailingAnchor
                     constant:0];
  _identityDiscTrailingConstraint.active = YES;
  _identityDiscCapsuleWidthConstraint = [self.identityDiscButton.widthAnchor
      constraintGreaterThanOrEqualToAnchor:self.identityDiscButton.heightAnchor
                                multiplier:2.0];

  // Initially set the constraints of the identity disc.
  [self updateIdentityDiscConstraints];
}

// Creates the Home customization menu and adds it to the header view.
- (void)addCustomizationMenu {
  UIButton* customizationMenuButton =
      [[ExtendedTouchTargetButton alloc] initWithFrame:CGRectZero];

  if (!IsNTPBackgroundCustomizationEnabled()) {
    UIImage* icon = DefaultSymbolTemplateWithPointSize(
        kPencilSymbol,
        IsSignInButtonNoAvatarEnabled()
            ? ntp_home::kCustomizationMenuIconSizeWhenSignInButtonHasNoAvatar
            : ntp_home::kCustomizationMenuIconSize);
    [customizationMenuButton setImage:icon forState:UIControlStateNormal];
    customizationMenuButton.backgroundColor =
        [self defaultButtonBackgroundColor];

    UIColor* tintColor = [UIColor
        colorNamed:(IsSignInButtonNoAvatarEnabled() ? kBlue600Color
                                                    : kTextSecondaryColor)];
    customizationMenuButton.tintColor = tintColor;

    customizationMenuButton.layer.cornerRadius =
        ntp_home::kCustomizationMenuButtonCornerRadius;
    customizationMenuButton.clipsToBounds = YES;
  }

  customizationMenuButton.accessibilityIdentifier =
      kNTPCustomizationMenuButtonIdentifier;
  customizationMenuButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_IOS_HOME_CUSTOMIZATION_ACCESSIBILITY_LABEL);

  [customizationMenuButton addTarget:self.commandHandler
                              action:@selector(customizationMenuWasTapped:)
                    forControlEvents:UIControlEventTouchUpInside];

  [self.headerView setCustomizationMenuButton:customizationMenuButton
                                 withNewBadge:_useNewBadgeForCustomizationMenu];
}

// Configures `identityDiscButton` with the current state of
// `identityDiscImage`.
- (void)updateIdentityDiscState {
  DCHECK(self.identityDiscImage);
  DCHECK(self.identityDiscAccessibilityLabel);

  UIButton* button = self.identityDiscButton;

  button.accessibilityLabel = self.identityDiscAccessibilityLabel;
  button.clipsToBounds = YES;

  BOOL useOldExperience = !IsNTPBackgroundCustomizationEnabled() &&
                          !IsSignInButtonNoAvatarEnabled();
  if (self.isSignedIn || useOldExperience) {
    UIImage* image = self.identityDiscImage;
    button.configuration = nil;
    [button setImage:image forState:UIControlStateNormal];
    button.backgroundColor = nil;
    button.imageView.layer.cornerRadius = image.size.width / 2;
    button.imageView.layer.masksToBounds = YES;
    button.layer.cornerRadius = image.size.width;
    return;
  }

  // Other configuration uses UIButtonConfiguration, not this property.
  button.layer.cornerRadius = 0;

  if (!IsNTPBackgroundCustomizationEnabled()) {
    CHECK(IsSignInButtonNoAvatarEnabled());
    [button setImage:nil forState:UIControlStateNormal];
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.background.backgroundColor =
        [self defaultButtonBackgroundColor];
    NSDictionary* attributes = @{
      NSFontAttributeName : PreferredFontForTextStyle(
          UIFontTextStyleSubheadline, UIFontWeightSemibold,
          kIdentityDiscMaxFontSize),
      NSForegroundColorAttributeName : [UIColor colorNamed:kBlue600Color],
    };
    buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_IOS_SIGNIN_BUTTON_TEXT)
            attributes:attributes];
    buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kPillVerticalPadding, kPillHorizontalPadding, kPillVerticalPadding,
        kPillHorizontalPadding);
    button.configuration = buttonConfiguration;
    return;
  }

  [button setImage:nil forState:UIControlStateNormal];

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  UIColor* foregroundColor;
  if ([self.traitCollection boolForNewTabPageImageBackgroundTrait]) {
    UIVisualEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    buttonConfiguration.background.customView = blurBackgroundView;

    foregroundColor = [UIColor colorNamed:kTextPrimaryColor];
  } else {
    NewTabPageColorPalette* colorPalette =
        [self.traitCollection objectForNewTabPageTrait];
    foregroundColor = colorPalette ? colorPalette.tintColor
                                   : [UIColor colorNamed:kBlue600Color];

    UIColor* backgroundColor = colorPalette
                                   ? colorPalette.headerButtonColor
                                   : [self defaultButtonBackgroundColor];
    // The default avatar icon does not have a background.
    if (colorPalette || IsSignInButtonNoAvatarEnabled()) {
      buttonConfiguration.background.backgroundColor = backgroundColor;
    }
  }

  if (IsSignInButtonNoAvatarEnabled()) {
    buttonConfiguration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        kPillVerticalPadding, kPillHorizontalPadding, kPillVerticalPadding,
        kPillHorizontalPadding);

    NSDictionary* attributes = @{
      NSFontAttributeName : PreferredFontForTextStyle(
          UIFontTextStyleSubheadline, UIFontWeightSemibold,
          kIdentityDiscMaxFontSize),
      NSForegroundColorAttributeName : foregroundColor,
    };
    buttonConfiguration.attributedTitle = [[NSAttributedString alloc]
        initWithString:l10n_util::GetNSString(IDS_IOS_SIGNIN_BUTTON_TEXT)
            attributes:attributes];
  } else {
    buttonConfiguration.image = self.identityDiscImage;
    buttonConfiguration.baseForegroundColor = foregroundColor;

    // UIBackgroundConfiguration requires a background inset, but it's easier to
    // think about the amount of space desired between the avatar and the
    // background, an "outset" from the avatar. So use that amount to calculate
    // the inset from the button edges.
    CGFloat rawInsetAmount = (_identityDiscWidthConstraint.constant -
                              buttonConfiguration.image.size.width) /
                                 2 -
                             kIdentityDiscAvatarBackgroundSpacing;
    // Make sure that the background isn't larger than the view.
    CGFloat insetAmount = MAX(rawInsetAmount, 0);
    buttonConfiguration.background.backgroundInsets =
        NSDirectionalEdgeInsetsMake(insetAmount, insetAmount, insetAmount,
                                    insetAmount);
    buttonConfiguration.background.cornerRadius =
        buttonConfiguration.image.size.width;
  }

  button.configuration = buttonConfiguration;
}

- (void)fakeTapViewTapped {
  [self.NTPMetricsRecorder recordFakeTapViewTapped];
  [self.commandHandler fakeboxTapped];
}

- (void)fakeboxTapped {
  [self.NTPMetricsRecorder recordFakeOmniboxTapped];
  TriggerHapticFeedbackForSelectionChange();
  [self.commandHandler fakeboxTapped];
}

- (void)focusAccessibilityOnOmnibox {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.fakeOmnibox);
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([@"highlighted" isEqualToString:keyPath]) {
    [self.headerView setFakeboxHighlighted:[object isHighlighted]];
  }
}

// If display is compact size, shows fakebox. If display is regular size,
// shows fakebox if the logo is visible and hides otherwise
- (void)updateFakeboxDisplay {
  self.doodleTopMarginConstraint.constant =
      content_suggestions::DoodleTopMargin(_searchEngineLogoState,
                                           self.traitCollection);
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(_searchEngineLogoState,
                                                    self.traitCollection)];
  self.fakeOmnibox.hidden =
      CanShowTabStrip(self) &&
      (_searchEngineLogoState == SearchEngineLogoState::kNone);
  [self.headerView layoutIfNeeded];
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(_searchEngineLogoState,
                                               self.traitCollection);
}

// Ensures the state of the Voice Search button matches whether or not it's
// enabled. If it's not, disables the button and removes it from the a11y loop
// for VoiceOver.
- (void)updateVoiceSearchDisplay {
  self.headerView.voiceSearchButton.enabled = self.voiceSearchIsEnabled;
  self.headerView.voiceSearchButton.isAccessibilityElement =
      self.voiceSearchIsEnabled;
}

// Adds the constraints for the `logoView`, the `fakeomnibox` related to the
// `headerView`. It also sets the properties constraints related to those views.
- (void)addConstraintsForLogoView:(UIView*)logoView
                      fakeOmnibox:(UIView*)fakeOmnibox
                    andHeaderView:(UIView*)headerView {
  self.doodleTopMarginConstraint = [logoView.topAnchor
      constraintEqualToAnchor:headerView.topAnchor
                     constant:content_suggestions::DoodleTopMargin(
                                  _searchEngineLogoState,
                                  self.traitCollection)];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::DoodleHeight(
                                    _searchEngineLogoState,
                                    self.traitCollection)];
  self.fakeOmniboxHeightConstraint = [fakeOmnibox.heightAnchor
      constraintEqualToConstant:content_suggestions::FakeOmniboxHeight()];
  self.fakeOmniboxTopMarginConstraint = [logoView.bottomAnchor
      constraintEqualToAnchor:fakeOmnibox.topAnchor
                     constant:-content_suggestions::SearchFieldTopMargin()];
  self.headerViewHeightConstraint =
      [headerView.heightAnchor constraintEqualToConstant:[self headerHeight]];
  self.headerViewHeightConstraint.active = YES;
  self.doodleTopMarginConstraint.active = YES;
  self.doodleHeightConstraint.active = YES;
  self.fakeOmniboxWidthConstraint.active = YES;
  self.fakeOmniboxHeightConstraint.active = YES;
  self.fakeOmniboxTopMarginConstraint.active = YES;
  [logoView.widthAnchor constraintEqualToAnchor:headerView.widthAnchor].active =
      YES;
  [logoView.leadingAnchor constraintEqualToAnchor:headerView.leadingAnchor]
      .active = YES;
  [fakeOmnibox.centerXAnchor constraintEqualToAnchor:headerView.centerXAnchor]
      .active = YES;
}

// Updates opacity of doodle for scroll position, preventing it from showing
// within the safe area insets.
- (void)updateLogoForOffset:(CGFloat)offset {
  _searchEngineLogoMediator.view.alpha =
      std::max(1 - [self.headerView searchFieldProgressForOffset:offset], 0.0);
}

- (void)maybeShowSwitchAccountsIPH {
  if (!_isSignedIn) {
    return;
  }

  // Note: BubblePresenter will take care to only show the bubble if the page is
  // scrolled to the top.
  [self.helpHandler
      presentInProductHelpWithType:
          InProductHelpType::kSwitchAccountsWithNTPAccountParticleDisc];
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
  return self.toolbarDelegate.fakeboxScribbleForwardingTarget.isFirstResponder;
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
  if (!self.toolbarDelegate.fakeboxScribbleForwardingTarget.isFirstResponder) {
    [self.toolbarDelegate.fakeboxScribbleForwardingTarget becomeFirstResponder];
  }

  completion(self.toolbarDelegate.fakeboxScribbleForwardingTarget);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
         shouldDelayFocusForElement:
             (UIScribbleElementIdentifier)elementIdentifier {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);
  return YES;
}

#pragma mark - SearchEngineLogoConsumer

- (void)searchEngineLogoStateDidChange:(SearchEngineLogoState)logoState {
  _searchEngineLogoState = logoState;
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(_searchEngineLogoState,
                                                    self.traitCollection)];
  self.doodleTopMarginConstraint.constant =
      content_suggestions::DoodleTopMargin(_searchEngineLogoState,
                                           self.traitCollection);
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(_searchEngineLogoState,
                                               self.traitCollection);
  // Trigger relayout so that it immediately returns the updated content height
  // for the NTP to update content inset.
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
  [self.commandHandler updateForHeaderSizeChange];
  [self updateFakeboxDisplay];
}

#pragma mark - NewTabPageHeaderConsumer

- (void)setSearchEngineLogoState:(SearchEngineLogoState)logoState {
  _searchEngineLogoState = logoState;
  [self updateFakeboxDisplay];
}

- (void)setSearchEngineLogoMediator:
    (SearchEngineLogoMediator*)searchEngineLogoMediator {
  _searchEngineLogoMediator = searchEngineLogoMediator;
  _searchEngineLogoMediator.consumer = self;
}

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled {
  if (_voiceSearchIsEnabled == voiceSearchIsEnabled) {
    return;
  }
  _voiceSearchIsEnabled = voiceSearchIsEnabled;
  [self updateVoiceSearchDisplay];
}

- (void)setDefaultSearchEngineName:(NSString*)defaultSearchEngineName {
  if (_defaultSearchEngineName == defaultSearchEngineName) {
    return;
  }
  _defaultSearchEngineName = defaultSearchEngineName;
  self.headerView.placeholderText = self.placeholderText;
  self.accessibilityButton.accessibilityLabel = self.placeholderText;
}

- (void)setDefaultSearchEngineImage:(UIImage*)image {
  if (!base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdateV2)) {
    return;
  }
  // The header view might not be created yet. Store the logo image until it is
  // consumed.
  if (!self.headerView) {
    _dseLogo = image;
    return;
  }

  [self.headerView setDefaultSearchEngineLogo:image];
}

- (void)updateADPBadgeWithErrorFound:(BOOL)hasAccountError
                                name:(NSString*)name
                               email:(NSString*)email {
  CHECK(
      base::FeatureList::IsEnabled(switches::kEnableErrorBadgeOnIdentityDisc) ||
      base::FeatureList::IsEnabled(switches::kEnableIdentityInAuthError));

  if (hasAccountError == _hasAccountError) {
    return;
  }

  _hasAccountError = hasAccountError;
  if (_hasAccountError) {
    [self.headerView setIdentityDiscErrorBadge];
  } else {
    [self.headerView removeIdentityDiscErrorBadge];
  }
  [self updateIdentityDiscAccessibilityLabelWithName:name email:email];
}

- (void)setAIMAllowed:(BOOL)allowed {
  [_headerView setAIMAllowed:allowed];
  _isAIMAllowed = allowed;
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)setSignedOutAccountImage {
  self.identityDiscImage = DefaultSymbolTemplateWithPointSize(
      kPersonCropCircleSymbol, ntp_home::kSignedOutIdentityIconSize);

  self.isSignedIn = NO;

  self.identityDiscAccessibilityLabel =
      IsSignInButtonNoAvatarEnabled()
          ? l10n_util::GetNSString(IDS_IOS_SIGN_IN_BUTTON_ACCESSIBILITY_LABEL)
          : l10n_util::GetNSString(
                IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL);

  // `self.identityDiscButton` should not be updated if the view has not been
  // created yet.
  if (self.identityDiscButton) {
    [self updateIdentityDiscState];
  }
}

- (void)updateAccountImage:(UIImage*)image
                      name:(NSString*)name
                     email:(NSString*)email {
  DCHECK(image && image.size.width == ntp_home::kIdentityAvatarDimension &&
         image.size.height == ntp_home::kIdentityAvatarDimension)
      << base::SysNSStringToUTF8([image description]);
  DCHECK(email);

  self.identityDiscImage = image;

  self.isSignedIn = YES;

  [self updateIdentityDiscAccessibilityLabelWithName:name email:email];
}

#pragma mark UIPointerInteractionDelegate

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

#pragma mark - Getters

- (UIButton*)customizationMenuButton {
  return [self.headerView customizationMenuButton];
}

#pragma mark - Private

// Returns the default background color for buttons based on the current
// appearance.
- (UIColor*)defaultButtonBackgroundColor {
  return
      [UIColor colorWithDynamicProvider:^UIColor*(UITraitCollection* traits) {
        if (!IsSignInButtonNoAvatarEnabled()) {
          return [[UIColor colorNamed:@"fake_omnibox_solid_background_color"]
              colorWithAlphaComponent:0.8];
        }
        return traits.userInterfaceStyle == UIUserInterfaceStyleDark
                   ? [UIColor colorNamed:kTabGroupFaviconBackgroundColor]
                   : [[UIColor colorNamed:kSolidWhiteColor]
                         colorWithAlphaComponent:0.75];
      }];
}
// Sets the background using the current color palette, or defaults if none is
// set.
- (void)applyBackgroundTheme {
  [self updateIdentityDiscState];

  BOOL hasBlurredBackground =
      [self.traitCollection boolForNewTabPageImageBackgroundTrait];
  if (hasBlurredBackground) {
    _searchEngineLogoMediator.usesMonochromeLogo = YES;
    _searchEngineLogoMediator.view.tintColor = UIColor.whiteColor;
    return;
  }

  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  if (colorPalette) {
    _searchEngineLogoMediator.usesMonochromeLogo = YES;
    _searchEngineLogoMediator.view.tintColor = colorPalette.tintColor;
    return;
  }

  _searchEngineLogoMediator.usesMonochromeLogo = NO;
  _searchEngineLogoMediator.view.tintColor = nil;
}

- (void)setIsSignedIn:(BOOL)isSignedIn {
  BOOL wasSignedIn = _isSignedIn;
  _isSignedIn = isSignedIn;
  if (wasSignedIn != _isSignedIn) {
    [self updateIdentityDiscConstraints];
    // `self.identityDiscButton` should not be updated if the view has not been
    // created or initialized yet.
    if (self.identityDiscButton && self.identityDiscImage &&
        self.identityDiscAccessibilityLabel) {
      [self updateIdentityDiscState];
    }
  }
}

// Activates or deactivates the identity disc constraints based on sign-in
// state.
- (void)updateIdentityDiscConstraints {
  BOOL showSignInButtonWithoutAvatar =
      IsSignInButtonNoAvatarEnabled() && !self.isSignedIn;

  CGFloat dimension = ntp_home::kIdentityAvatarDimension +
                      kMarginMultiplier * ntp_home::kHeaderIconMargin;

  CGFloat identityAvatarPadding = ntp_home::kIdentityAvatarPadding;

  if (showSignInButtonWithoutAvatar) {
    identityAvatarPadding *= kMarginMultiplier;
  } else {
    dimension += ntp_home::kHeaderIconMargin;
    identityAvatarPadding -= ntp_home::kHeaderIconMargin / 2;
  }

  _identityDiscWidthConstraint.constant = dimension;
  _identityDiscHeightConstraint.constant = dimension;
  if (showSignInButtonWithoutAvatar) {
    _identityDiscWidthConstraint.active = NO;
    _identityDiscHeightConstraint.active = NO;
    _identityDiscCapsuleWidthConstraint.active = YES;
  } else {
    _identityDiscCapsuleWidthConstraint.active = NO;
    _identityDiscWidthConstraint.active = YES;
    _identityDiscHeightConstraint.active = YES;
  }
  _identityDiscTrailingConstraint.constant = -identityAvatarPadding;
}

- (void)updateIdentityDiscAccessibilityLabelWithName:(NSString*)name
                                               email:(NSString*)email {
  NSString* accountButtonLabel;
  // `_hasAccountError` is only set if the primary identity has an error.
  if (name) {
    accountButtonLabel =
        _hasAccountError
            ? l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU_WITH_ERROR,
                  base::SysNSStringToUTF16(name),
                  base::SysNSStringToUTF16(email))
            : l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL_OPEN_ACCOUNT_MENU,
                  base::SysNSStringToUTF16(name),
                  base::SysNSStringToUTF16(email));
  } else {
    accountButtonLabel =
        _hasAccountError
            ? l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_EMAIL_OPEN_ACCOUNT_MENU_WITH_ERROR,
                  base::SysNSStringToUTF16(email))
            : l10n_util::GetNSStringF(
                  IDS_IOS_IDENTITY_DISC_WITH_EMAIL_OPEN_ACCOUNT_MENU,
                  base::SysNSStringToUTF16(email));
  }

  self.identityDiscAccessibilityLabel = accountButtonLabel;

  // `self.identityDiscButton` should not be updated if the view has not been
  // created yet.
  if (self.identityDiscButton) {
    [self updateIdentityDiscState];
  }
}

// Updates the fakebox or the header view when UITraits are modified on the
// device.
- (void)updateUIOnTraitChange:(UITraitCollection*)previousTraitCollection {
  if (self.traitCollection.horizontalSizeClass !=
          previousTraitCollection.horizontalSizeClass ||
      previousTraitCollection.preferredContentSizeCategory !=
          self.traitCollection.preferredContentSizeCategory) {
    [self updateFakeboxDisplay];
  }
}

// Returns the omnibox placeholder text.
- (NSString*)placeholderText {
  if (base::FeatureList::IsEnabled(omnibox::kOmniboxMobileParityUpdate)) {
    return l10n_util::GetNSStringF(IDS_OMNIBOX_EMPTY_HINT_WITH_DSE_NAME,
                                   self.defaultSearchEngineName.cr_UTF16String);
  } else {
    return l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  }
}

@end
