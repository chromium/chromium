// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_switches.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/ntp/logo_vendor.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

using base::UserMetricsAction;

namespace {

NSString* const kScribbleFakeboxElementId = @"fakebox";

}  // namespace

@interface NewTabPageHeaderViewController () <
    DoodleObserver,
    UIIndirectScribbleInteractionDelegate,
    UIPointerInteractionDelegate>

// `YES` if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak, readonly) id<LogoVendor> logoVendor;

@property(nonatomic, strong) NewTabPageHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) UIButton* accessibilityButton;
@property(nonatomic, strong) NSString* identityDiscAccessibilityLabel;
@property(nonatomic, strong, readwrite) UIButton* identityDiscButton;
@property(nonatomic, strong) UIImage* identityDiscImage;
@property(nonatomic, strong) UIButton* fakeTapButton;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* headerViewHeightConstraint;
@property(nonatomic, assign) BOOL logoFetched;

// Whether the Google logo or doodle is being shown.
@property(nonatomic, assign) BOOL logoIsShowing;

@end

@implementation NewTabPageHeaderViewController

- (instancetype)init {
  return [super initWithNibName:nil bundle:nil];
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
      [self.dispatcher onFakeboxAnimationComplete];
    }
  }
}

// TODO(crbug.com/1403613): Name animateScrollAnimation something more aligned
// to its true state indication. Why update the constraints only sometimes?
- (void)updateFakeOmniboxForOffset:(CGFloat)offset
                       screenWidth:(CGFloat)screenWidth
                    safeAreaInsets:(UIEdgeInsets)safeAreaInsets
            animateScrollAnimation:(BOOL)animateScrollAnimation {
  if (self.isShowing) {
    CGFloat progress =
        self.logoIsShowing || !IsRegularXRegularSizeClass(self)
            ? [self.headerView searchFieldProgressForOffset:offset]
            // RxR with no logo hides the fakebox, so always show the omnibox.
            : 1;
    [self updateLogoForOffset:offset];
    if (!IsSplitToolbarMode(self)) {
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

- (void)updateConstraints {
  self.doodleTopMarginConstraint.constant =
      content_suggestions::DoodleTopMargin(0, self.traitCollection);
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(self.logoIsShowing,
                                               self.logoVendor.isShowingDoodle,
                                               self.traitCollection);
}

- (CGFloat)pinnedOffsetY {
  CGFloat offsetY = [self headerHeight];
  if (IsSplitToolbarMode(self)) {
    offsetY -= ToolbarExpandedHeight(
        self.traitCollection.preferredContentSizeCategory);
  }

  return AlignValueToPixel(offsetY);
}

- (CGFloat)headerHeight {
  return content_suggestions::HeightForLogoHeader(
      self.logoIsShowing, self.logoVendor.isShowingDoodle,
      self.traitCollection);
}

- (void)viewDidLoad {
  [super viewDidLoad];

  if (!self.headerView) {
    self.view.translatesAutoresizingMaskIntoConstraints = NO;

    CGFloat width = self.view.frame.size.width;

    self.headerView = [[NewTabPageHeaderView alloc] init];
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

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Check if the identity disc button was properly set before the view appears.
  DCHECK(self.identityDiscButton);
  DCHECK(self.identityDiscImage);
  DCHECK(self.identityDiscButton.accessibilityLabel);
  DCHECK([self.identityDiscButton imageForState:UIControlStateNormal]);
}

#pragma mark - Private

// Initialize and add a search field tap target and a voice search button.
- (void)addFakeOmnibox {
  self.fakeOmnibox = [[UIButton alloc] init];
  // TODO(crbug.com/1418068): Remove after minimum version required is >=
  // iOS 15 and refactor with UIButtonConfiguration.
  SetAdjustsImageWhenHighlighted(self.fakeOmnibox, NO);

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
  self.identityDiscButton.accessibilityIdentifier = kNTPFeedHeaderIdentityDisc;
  [self.identityDiscButton addTarget:self.commandHandler
                              action:@selector(identityDiscWasTapped)
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
}

// Configures `identityDiscButton` with the current state of
// `identityDiscImage`.
- (void)updateIdentityDiscState {
  DCHECK(self.identityDiscImage);
  DCHECK(self.identityDiscAccessibilityLabel);
  self.identityDiscButton.accessibilityLabel =
      self.identityDiscAccessibilityLabel;
  [self.identityDiscButton setImage:self.identityDiscImage
                           forState:UIControlStateNormal];
  self.identityDiscButton.imageView.layer.cornerRadius =
      self.identityDiscImage.size.width / 2;
  self.identityDiscButton.imageView.layer.masksToBounds = YES;
}

- (void)openLens {
  [self.NTPMetricsRecorder recordLensTapped];
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::NewTabPage
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [self.dispatcher openLensInputSelection:command];
}

- (void)loadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [self.NTPMetricsRecorder recordVoiceSearchTapped];
  UIView* voiceSearchButton = base::apple::ObjCCastStrict<UIView>(sender);
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
  [self.NTPMetricsRecorder recordFakeTapViewTapped];
  [self.commandHandler fakeboxTapped];
}

- (void)fakeboxTapped {
  [self.NTPMetricsRecorder recordFakeOmniboxTapped];
  [self.commandHandler fakeboxTapped];
}

- (void)focusAccessibilityOnOmnibox {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.fakeOmnibox);
}

// TODO(crbug.com/807330) The fakebox is currently a collection of views spread
// between NewTabPageHeaderViewController and inside
// NewTabPageHeaderView.  Post refresh this can be coalesced into one
// control, and the KVO highlight logic below can be removed.
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
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(
                      self.logoVendor.showingLogo,
                      self.logoVendor.isShowingDoodle, self.traitCollection)];
  self.fakeOmnibox.hidden =
      IsRegularXRegularSizeClass(self) && !self.logoIsShowing;
  [self.headerView layoutIfNeeded];
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(self.logoIsShowing,
                                               self.logoVendor.isShowingDoodle,
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
                                  0, self.traitCollection)];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::DoodleHeight(
                                    self.logoVendor.showingLogo,
                                    self.logoVendor.isShowingDoodle,
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
  self.logoVendor.view.alpha =
      std::max(1 - [self.headerView searchFieldProgressForOffset:offset], 0.0);
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

#pragma mark - DoodleObserver

- (void)doodleDisplayStateChanged:(BOOL)doodleShowing {
  [self.doodleHeightConstraint
      setConstant:content_suggestions::DoodleHeight(self.logoVendor.showingLogo,
                                                    doodleShowing,
                                                    self.traitCollection)];
  self.headerViewHeightConstraint.constant =
      content_suggestions::HeightForLogoHeader(self.logoIsShowing,
                                               self.logoVendor.isShowingDoodle,
                                               self.traitCollection);
  // Trigger relayout so that it immediately returns the updated content height
  // for the NTP to update content inset.
  [self.view setNeedsLayout];
  [self.view layoutIfNeeded];
  [self.commandHandler updateForHeaderSizeChange];
}

#pragma mark - NewTabPageHeaderConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  [self updateLogoAndFakeboxDisplay];
}

- (void)setLogoVendor:(id<LogoVendor>)logoVendor {
  _logoVendor = logoVendor;
  _logoVendor.doodleObserver = self;
}

- (void)setVoiceSearchIsEnabled:(BOOL)voiceSearchIsEnabled {
  if (_voiceSearchIsEnabled == voiceSearchIsEnabled) {
    return;
  }
  _voiceSearchIsEnabled = voiceSearchIsEnabled;
  [self updateVoiceSearchDisplay];
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)setSignedOutAccountImage {
  self.identityDiscImage = DefaultSymbolTemplateWithPointSize(
      kPersonCropCircleSymbol, ntp_home::kSignedOutIdentityIconDimension);

  self.identityDiscAccessibilityLabel =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? l10n_util::GetNSString(
                IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL)
          : l10n_util::GetNSString(
                IDS_IOS_IDENTITY_DISC_SIGNED_OUT_ACCESSIBILITY_LABEL_WITH_SYNC);
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
  if (name) {
    self.identityDiscAccessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_IDENTITY_DISC_WITH_NAME_AND_EMAIL,
        base::SysNSStringToUTF16(name), base::SysNSStringToUTF16(email));
  } else {
    self.identityDiscAccessibilityLabel = l10n_util::GetNSStringF(
        IDS_IOS_IDENTITY_DISC_WITH_EMAIL, base::SysNSStringToUTF16(email));
  }
  // `self.identityDiscButton` should not be updated if the view has not been
  // created yet.
  if (self.identityDiscButton) {
    [self updateIdentityDiscState];
  }
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

@end
