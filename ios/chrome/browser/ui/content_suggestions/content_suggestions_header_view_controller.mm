// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_metrics.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

NSString* const kScribbleFakeboxElementId = @"fakebox";
NSString* const kSignOutIdentityIconName = @"sign_out_icon";

}  // namespace

@interface ContentSuggestionsHeaderViewController () <
    DoodleObserver,
    UIIndirectScribbleInteractionDelegate,
    UIPointerInteractionDelegate,
    UserAccountImageUpdateDelegate>

// If YES the animations of the fake omnibox triggered when the collection is
// scrolled (expansion) are disabled. This is used for the fake omnibox focus
// animations so the constraints aren't changed while the ntp is scrolled.
@property(nonatomic, assign) BOOL disableScrollAnimation;

// `YES` when notifications indicate the omnibox is focused.
@property(nonatomic, assign, getter=isOmniboxFocused) BOOL omniboxFocused;

// `YES` if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak, readonly) id<LogoVendor> logoVendor;

@property(nonatomic, strong) ContentSuggestionsHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) UIButton* accessibilityButton;
@property(nonatomic, strong, readwrite) UIButton* identityDiscButton;
@property(nonatomic, strong) UIButton* fakeTapButton;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* headerViewHeightConstraint;
@property(nonatomic, assign) BOOL logoFetched;

@end

@implementation ContentSuggestionsHeaderViewController

@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize showing = _showing;
@synthesize omniboxFocused = _omniboxFocused;
@synthesize headerView = _headerView;
@synthesize fakeOmnibox = _fakeOmnibox;
@synthesize accessibilityButton = _accessibilityButton;
@synthesize doodleHeightConstraint = _doodleHeightConstraint;
@synthesize doodleTopMarginConstraint = _doodleTopMarginConstraint;
@synthesize fakeOmniboxWidthConstraint = _fakeOmniboxWidthConstraint;
@synthesize fakeOmniboxHeightConstraint = _fakeOmniboxHeightConstraint;
@synthesize fakeOmniboxTopMarginConstraint = _fakeOmniboxTopMarginConstraint;
@synthesize voiceSearchIsEnabled = _voiceSearchIsEnabled;
@synthesize logoIsShowing = _logoIsShowing;
@synthesize logoFetched = _logoFetched;
@synthesize layoutGuideCenter = _layoutGuideCenter;

- (instancetype)init {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _focusOmniboxWhenViewAppears = YES;
  }
  return self;
}

#pragma mark - Public

- (UIView*)toolBarView {
  return self.headerView.toolBarView;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (self.traitCollection.horizontalSizeClass !=
      previousTraitCollection.horizontalSizeClass) {
    [self updateFakeboxDisplay];
  }
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

#pragma mark - ContentSuggestionsHeaderControlling

- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets {
  if (self.isShowing) {
    CGFloat progress =
        self.logoIsShowing || !IsRegularXRegularSizeClass(self)
            ? [self.headerView searchFieldProgressForOffset:offset
                                             safeAreaInsets:safeAreaInsets]
            // RxR with no logo hides the fakebox, so always show the omnibox.
            : 1;
    if (!IsSplitToolbarMode(self)) {
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:progress];
    } else {
      // Ensure omnibox is reset when not a regular tablet.
      [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
    }
  }

  if (self.disableScrollAnimation)
    return;

  [self.headerView updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                                   height:self.fakeOmniboxHeightConstraint
                                topMargin:self.fakeOmniboxTopMarginConstraint
                                forOffset:offset
                              screenWidth:screenWidth
                           safeAreaInsets:safeAreaInsets];
}

- (void)updateFakeOmniboxForWidth:(CGFloat)width {
  self.fakeOmniboxWidthConstraint.constant =
      content_suggestions::SearchFieldWidth(width, self.traitCollection);
}

- (void)unfocusOmnibox {
  if (self.omniboxFocused) {
    [self.dispatcher cancelOmniboxEdit];
  } else {
    [self locationBarResignsFirstResponder];
  }
}

- (void)layoutHeader {
  [self.headerView layoutIfNeeded];
}

// Update the doodle top margin to the new value.
- (void)updateConstraints {
  self.doodleTopMarginConstraint.constant =
      content_suggestions::DoodleTopMargin([self topInset],
                                           self.traitCollection);
  [self.headerView updateForTopSafeAreaInset:[self topInset]];
}

- (CGFloat)pinnedOffsetY {
  CGFloat offsetY =
      [self headerHeight] - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (IsSplitToolbarMode(self)) {
    offsetY -= ToolbarExpandedHeight(
                   self.traitCollection.preferredContentSizeCategory) +
               [self topInset];
  }

  return AlignValueToPixel(offsetY);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  if (self.focusOmniboxWhenViewAppears && !self.omniboxFocused) {
    [self focusAccessibilityOnOmnibox];
  }
}

- (CGFloat)headerHeight {
  return content_suggestions::HeightForLogoHeader(
      self.logoIsShowing, self.logoVendor.isShowingDoodle, [self topInset],
      self.traitCollection);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (!self.headerView) {
    self.view.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat width = self.view.frame.size.width;

    self.headerView = [[ContentSuggestionsHeaderView alloc] init];
    self.headerView.isGoogleDefaultSearchEngine =
        self.isGoogleDefaultSearchEngine;
    self.headerView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:self.headerView];
    AddSameConstraints(self.headerView, self.view);

    [self addFakeOmnibox];

    [self.headerView addSubview:self.logoVendor.view];
    // Fake Tap View has identity disc, which should render above the doodle.
    [self addFakeTapView];
    [self.headerView addSubview:self.fakeOmnibox];
    self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
    self.logoVendor.view.accessibilityIdentifier =
        ntp_home::NTPLogoAccessibilityID();
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

    [self.headerView addSeparatorToSearchField:self.fakeOmnibox];

    // Identity disc needs to be added after the Google logo/doodle since it
    // needs to respond to user taps first.
    [self addIdentityDisc];

    UIEdgeInsets safeAreaInsets = self.baseViewController.view.safeAreaInsets;
    width = std::max<CGFloat>(
        0, width - safeAreaInsets.left - safeAreaInsets.right);

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::SearchFieldWidth(
                                      width, self.traitCollection)];
    [self addConstraintsForLogoView:self.logoVendor.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.headerView];

    [self.logoVendor fetchDoodle];
  }
}

#pragma mark - Private

// Initialize and add a search field tap target and a voice search button.
- (void)addFakeOmnibox {
  self.fakeOmnibox = [[UIButton alloc] init];
  [self.fakeOmnibox setAdjustsImageWhenHighlighted:NO];

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
  // ContentSuggestionsHeaderView, KVO the highlight events of
  // `accessibilityButton` and pass them along.
  [self.accessibilityButton addObserver:self
                             forKeyPath:@"highlighted"
                                options:NSKeyValueObservingOptionNew
                                context:NULL];
  self.accessibilityButton.isAccessibilityElement = YES;
  self.accessibilityButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self.fakeOmnibox addSubview:self.accessibilityButton];
  self.accessibilityButton.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.fakeOmnibox, self.accessibilityButton);

  [self.fakeOmnibox
      addInteraction:[[UIPointerInteraction alloc] initWithDelegate:self]];

  [self.headerView addViewsToSearchField:self.fakeOmnibox];

  UIIndirectScribbleInteraction* scribbleInteraction =
      [[UIIndirectScribbleInteraction alloc] initWithDelegate:self];
  [self.fakeOmnibox addInteraction:scribbleInteraction];

  [self.headerView.voiceSearchButton addTarget:self
                                        action:@selector(loadVoiceSearch:)
                              forControlEvents:UIControlEventTouchUpInside];
  [self.headerView.voiceSearchButton addTarget:self
                                        action:@selector(preloadVoiceSearch:)
                              forControlEvents:UIControlEventTouchDown];
  if (self.headerView.lensButton) {
    [self.headerView.lensButton addTarget:self
                                   action:@selector(openLens)
                         forControlEvents:UIControlEventTouchUpInside];
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
  self.identityDiscButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_PARTICLE_DISC);
  [self.identityDiscButton addTarget:self
                              action:@selector(identityDiscTapped)
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

    // TODO(crbug.com/965958): Set action on button to launch into Settings.
    [self.headerView setIdentityDiscView:self.identityDiscButton];

  // Register to receive the avatar of the currently signed in user.
  [self.delegate registerImageUpdater:self];
}

- (void)openLens {
  base::RecordAction(
      UserMetricsAction("Mobile.LensIOS.NewTabPageEntrypointTapped"));
  [self.dispatcher openInputSelectionForEntrypoint:LensEntrypoint::NewTabPage];
}

- (void)loadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  UIView* voiceSearchButton = base::mac::ObjCCastStrict<UIView>(sender);
  [self.layoutGuideCenter referenceView:voiceSearchButton
                              underName:kVoiceSearchButtonGuide];
  [self.dispatcher startVoiceSearch];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  [self.dispatcher preloadVoiceSearch];
}

- (void)fakeTapViewTapped {
  base::RecordAction(base::UserMetricsAction("MobileFakeViewNTPTapped"));
  [self logOmniboxAction];
  [self focusFakebox];
}

- (void)fakeboxTapped {
  base::RecordAction(base::UserMetricsAction("MobileFakeboxNTPTapped"));
  [self logOmniboxAction];
  [self focusFakebox];
}

- (void)logOmniboxAction {
  if (self.isStartShowing) {
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnStartSurface",
                              IOSContentSuggestionsActionType::kFakebox);
  } else {
    UMA_HISTOGRAM_ENUMERATION("IOS.ContentSuggestions.ActionOnNTP",
                              IOSContentSuggestionsActionType::kFakebox);
  }
}

- (void)focusFakebox {
  [self shiftTilesUp];
}

- (void)focusAccessibilityOnOmnibox {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.fakeOmnibox);
}

- (void)identityDiscTapped {
  base::RecordAction(base::UserMetricsAction("MobileNTPIdentityDiscTapped"));
  if ([self.delegate isSignedIn]) {
    [self.dispatcher showSettingsFromViewController:self.baseViewController];
  } else {
    ShowSigninCommand* const showSigninCommand = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperationSigninAndSync
              accessPoint:signin_metrics::AccessPoint::
                              ACCESS_POINT_NTP_SIGNED_OUT_ICON];
    [self.dispatcher showSignin:showSigninCommand
             baseViewController:self.baseViewController];
  }
}

// TODO(crbug.com/807330) The fakebox is currently a collection of views spread
// between ContentSuggestionsHeaderViewController and inside
// ContentSuggestionsHeaderView.  Post refresh this can be coalesced into one
// control, and the KVO highlight logic below can be removed.
- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([@"highlighted" isEqualToString:keyPath])
    [self.headerView setFakeboxHighlighted:[object isHighlighted]];
}

// If display is compact size, shows fakebox. If display is regular size,
// shows fakebox if the logo is visible and hides otherwise
- (void)updateFakeboxDisplay {
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(
                      self.logoVendor.showingLogo,
                      self.logoVendor.isShowingDoodle, self.traitCollection)];
  self.fakeOmnibox.hidden =
      IsRegularXRegularSizeClass(self) && !self.logoIsShowing;
  [self.headerView layoutIfNeeded];
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(
          self.logoIsShowing, self.logoVendor.isShowingDoodle, [self topInset],
          self.traitCollection);
}

// If Google is not the default search engine, hides the logo, doodle and
// fakebox. Makes them appear if Google is set as default.
- (void)updateLogoAndFakeboxDisplay {
  if (self.logoVendor.showingLogo != self.logoIsShowing) {
    self.logoVendor.showingLogo = self.logoIsShowing;
    [self updateFakeboxDisplay];
  }
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
                                  [self topInset], self.traitCollection)];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::DoodleHeight(
                                    self.logoVendor.showingLogo,
                                    self.logoVendor.isShowingDoodle,
                                    self.traitCollection)];
  self.fakeOmniboxHeightConstraint = [fakeOmnibox.heightAnchor
      constraintEqualToConstant:ToolbarExpandedHeight(
                                    self.traitCollection
                                        .preferredContentSizeCategory)];
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

- (void)shiftTilesDown {
  if (IsSplitToolbarMode(self)) {
    [self.dispatcher onFakeboxBlur];
  }
  [self.collectionSynchronizer shiftTilesDown];
}

- (void)shiftTilesUp {
  if (self.disableScrollAnimation)
    return;

  void (^animations)() = nil;
  if (![self.delegate isScrolledToMinimumHeight]) {
    // Only trigger the fake omnibox animation if the header isn't scrolled to
    // the top. Otherwise just rely on the normal animation.
    self.disableScrollAnimation = YES;
    [self.dispatcher focusOmniboxNoAnimation];
    NamedGuide* omniboxGuide = [NamedGuide guideWithName:kOmniboxGuide
                                                    view:self.headerView];
    // Layout the owning view to make sure that the constrains are applied.
    [omniboxGuide.owningView layoutIfNeeded];

    self.headerView.omnibox.hidden = NO;
    self.headerView.cancelButton.hidden = NO;
    self.headerView.omnibox.alpha = 0;
    self.headerView.cancelButton.alpha = 0;
    animations = ^{
      // Make sure that the offset is after the pinned offset to have the fake
      // omnibox taking the full width.
      CGFloat offset = 9000;
      [self.headerView
          updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                          height:self.fakeOmniboxHeightConstraint
                       topMargin:self.fakeOmniboxTopMarginConstraint
                       forOffset:offset
                     screenWidth:self.headerView.bounds.size.width
                  safeAreaInsets:self.view.safeAreaInsets];

      self.fakeOmniboxWidthConstraint.constant =
          self.headerView.bounds.size.width;
      [self.headerView layoutIfNeeded];
      CGRect omniboxFrameInFakebox =
          [[omniboxGuide owningView] convertRect:[omniboxGuide layoutFrame]
                                          toView:self.fakeOmnibox];
      self.headerView.fakeLocationBarLeadingConstraint.constant =
          omniboxFrameInFakebox.origin.x;
      self.headerView.fakeLocationBarTrailingConstraint.constant = -(
          self.fakeOmnibox.bounds.size.width -
          (omniboxFrameInFakebox.origin.x + omniboxFrameInFakebox.size.width));
      self.headerView.voiceSearchButton.alpha = 0;
      self.headerView.cancelButton.alpha = 0.7;
      self.headerView.omnibox.alpha = 1;
      self.headerView.searchHintLabel.alpha = 0;
      [self.headerView layoutIfNeeded];
    };
  }

  void (^completionBlock)(UIViewAnimatingPosition) =
      ^(UIViewAnimatingPosition finalPosition) {
        self.headerView.omnibox.hidden = YES;
        self.headerView.cancelButton.hidden = YES;
        self.headerView.searchHintLabel.alpha = 1;
        self.headerView.voiceSearchButton.alpha = 1;
        self.disableScrollAnimation = NO;
        if (finalPosition == UIViewAnimatingPositionEnd &&
            [self.delegate isScrolledToMinimumHeight]) {
          // Check to see if the collection are still scrolled to the top --
          // it's possible (and difficult) to unfocus the omnibox and initiate a
          // -shiftTilesDown before the animation here completes.
          [self.dispatcher fakeboxFocused];
          if (IsSplitToolbarMode(self)) {
            [self.dispatcher onFakeboxAnimationComplete];
          }
        }
      };

  [self.collectionSynchronizer shiftTilesUpWithAnimations:animations
                                               completion:completionBlock];
}

- (CGFloat)topInset {
  return 0;
}

#pragma mark - UIIndirectScribbleInteractionDelegate

- (void)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
              requestElementsInRect:(CGRect)rect
                         completion:
                             (void (^)(NSArray<UIScribbleElementIdentifier>*
                                           elements))completion
    API_AVAILABLE(ios(14.0)) {
  completion(@[ kScribbleFakeboxElementId ]);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                   isElementFocused:
                       (UIScribbleElementIdentifier)elementIdentifier
    API_AVAILABLE(ios(14.0)) {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);
  return self.toolbarDelegate.fakeboxScribbleForwardingTarget.isFirstResponder;
}

- (CGRect)
    indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
                frameForElement:(UIScribbleElementIdentifier)elementIdentifier
    API_AVAILABLE(ios(14.0)) {
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
                                 completion API_AVAILABLE(ios(14.0)) {
  if (!self.toolbarDelegate.fakeboxScribbleForwardingTarget.isFirstResponder) {
    [self.toolbarDelegate.fakeboxScribbleForwardingTarget becomeFirstResponder];
  }

  completion(self.toolbarDelegate.fakeboxScribbleForwardingTarget);
}

- (BOOL)indirectScribbleInteraction:(UIIndirectScribbleInteraction*)interaction
         shouldDelayFocusForElement:
             (UIScribbleElementIdentifier)elementIdentifier
    API_AVAILABLE(ios(14.0)) {
  DCHECK(elementIdentifier == kScribbleFakeboxElementId);
  return YES;
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  // Only return the logo vendor's animation controller owner if the logo view
  // is fully visible.  This prevents the logo from being used in transition
  // animations if the logo has been scrolled off screen.
  UIView* logoView = self.logoVendor.view;
  UIView* parentView = self.parentViewController.view;
  CGRect logoFrame = [parentView convertRect:logoView.bounds fromView:logoView];
  BOOL isLogoFullyVisible = CGRectEqualToRect(
      CGRectIntersection(logoFrame, parentView.bounds), logoFrame);
  return isLogoFullyVisible ? [self.logoVendor logoAnimationControllerOwner]
                            : nil;
}

#pragma mark - DoodleObserver

- (void)doodleDisplayStateChanged:(BOOL)doodleShowing {
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(self.logoVendor.showingLogo,
                                                    doodleShowing,
                                                    self.traitCollection)];
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(
          self.logoIsShowing, self.logoVendor.isShowingDoodle, [self topInset],
          self.traitCollection);
  [self.commandHandler updateForHeaderSizeChange];
}

#pragma mark - NTPHomeConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  [self updateLogoAndFakeboxDisplay];
}

- (void)setLogoVendor:(id<LogoVendor>)logoVendor {
  _logoVendor = logoVendor;
  _logoVendor.doodleObserver = self;
}

- (void)locationBarBecomesFirstResponder {
  if (!self.isShowing)
    return;

  self.omniboxFocused = YES;

  [self shiftTilesUp];
}

- (void)locationBarResignsFirstResponder {
  if (!self.isShowing && ![self.delegate isScrolledToMinimumHeight])
    return;

  self.omniboxFocused = NO;

  [self shiftTilesDown];
}

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled {
  if (_voiceSearchIsEnabled == voiceSearchIsEnabled)
    return;
  _voiceSearchIsEnabled = voiceSearchIsEnabled;
  [self updateVoiceSearchDisplay];
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)updateAccountImage:(UIImage*)image {
  if (![self.delegate isSignedIn] &&
      base::FeatureList::IsEnabled(switches::kIdentityStatusConsistency)) {
    DCHECK(!image);
    if (UseSymbols()) {
      image = DefaultSymbolTemplateWithPointSize(
          kPersonCropCircleSymbol, ntp_home::kSignedOutIdentityIconDimension);
    } else {
      image = [UIImage imageNamed:kSignOutIdentityIconName];
    }
  } else {
    // TODO(crbug.com/1385758): Update this logic after
    // kIdentityStatusConsistency is rolled out as image can't be
    // null when the user is signed-in.
    self.identityDiscButton.hidden = !image;
    DCHECK(image == nil ||
           (image.size.width == ntp_home::kIdentityAvatarDimension &&
            image.size.height == ntp_home::kIdentityAvatarDimension))
        << base::SysNSStringToUTF8([image description]);
  }
  [self.identityDiscButton setImage:image forState:UIControlStateNormal];
  self.identityDiscButton.imageView.layer.cornerRadius = image.size.width / 2;
  self.identityDiscButton.imageView.layer.masksToBounds = YES;
}

#pragma mark UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion
    API_AVAILABLE(ios(13.4)) {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region
    API_AVAILABLE(ios(13.4)) {
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

@end
