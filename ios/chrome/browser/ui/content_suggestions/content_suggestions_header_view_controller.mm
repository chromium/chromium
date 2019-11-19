// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"

#include "base/feature_list.h"
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/content_suggestions/user_account_image_update_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_utils.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface ContentSuggestionsHeaderViewController () <
    UserAccountImageUpdateDelegate>

// If YES the animations of the fake omnibox triggered when the collection is
// scrolled (expansion) are disabled. This is used for the fake omnibox focus
// animations so the constraints aren't changed while the ntp is scrolled.
@property(nonatomic, assign) BOOL disableScrollAnimation;

// |YES| when notifications indicate the omnibox is focused.
@property(nonatomic, assign, getter=isOmniboxFocused) BOOL omniboxFocused;

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

@property(nonatomic, strong) ContentSuggestionsHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) UIButton* accessibilityButton;
@property(nonatomic, strong) UIButton* identityDiscButton;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, assign) BOOL logoFetched;

@end

@implementation ContentSuggestionsHeaderViewController

@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize logoVendor = _logoVendor;
@synthesize promoCanShow = _promoCanShow;
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

#pragma mark - Public

- (instancetype)initWithVoiceSearchEnabled:(BOOL)voiceSearchIsEnabled {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _voiceSearchIsEnabled = voiceSearchIsEnabled;
  }
  return self;
}

- (UIView*)toolBarView {
  return self.headerView.toolBarView;
}

- (void)willTransitionToTraitCollection:(UITraitCollection*)newCollection
              withTransitionCoordinator:
                  (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super willTransitionToTraitCollection:newCollection
               withTransitionCoordinator:coordinator];
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        // Ensure omnibox is reset when not a regular tablet.
        if (IsSplitToolbarMode()) {
          [self.toolbarDelegate setScrollProgressForTabletOmnibox:1];
        }
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
        self.logoIsShowing || !IsRegularXRegularSizeClass()
            ? [self.headerView searchFieldProgressForOffset:offset
                                             safeAreaInsets:safeAreaInsets]
            // RxR with no logo hides the fakebox, so always show the omnibox.
            : 1;
    if (!IsSplitToolbarMode()) {
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
      content_suggestions::searchFieldWidth(width);
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

// Update the doodle top margin to the new -doodleTopMargin value.
- (void)updateConstraints {
  self.doodleTopMarginConstraint.constant =
      content_suggestions::doodleTopMargin(YES, [self topInset]);
}

- (CGFloat)pinnedOffsetY {
  CGFloat headerHeight = content_suggestions::heightForLogoHeader(
      self.logoIsShowing, self.promoCanShow, YES, [self topInset]);

  CGFloat offsetY =
      headerHeight - ntp_header::kScrolledToTopOmniboxBottomMargin;
  if (!IsRegularXRegularSizeClass(self)) {
    offsetY -= ToolbarExpandedHeight(
                   self.traitCollection.preferredContentSizeCategory) +
               [self topInset];
  }

  return AlignValueToPixel(offsetY);
}

- (void)loadView {
  self.view = [[ContentSuggestionsHeaderView alloc] init];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.fakeOmnibox);
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(
      self.logoIsShowing, self.promoCanShow, YES, [self topInset]);
}

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  if (!self.headerView) {
    self.headerView =
        base::mac::ObjCCastStrict<ContentSuggestionsHeaderView>(self.view);
    [self addFakeTapView];
    [self addFakeOmnibox];

    [self.headerView addSubview:self.logoVendor.view];
    [self.headerView addSubview:self.fakeOmnibox];
    self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

    [self.headerView addSeparatorToSearchField:self.fakeOmnibox];

    // Identity disc needs to be added after the Google logo/doodle since it
    // needs to respond to user taps first.
    [self addIdentityDisc];

    // -headerForView is regularly called before self.headerView has been added
    // to the view hierarchy, so there's no simple way to get the correct
    // safeAreaInsets.  Since this situation is universally called for the full
    // screen new tab animation, it's safe to check the rootViewController's
    // view instead.
    // TODO(crbug.com/791784) : Remove use of rootViewController.
    UIView* insetsView = self.headerView;
    if (!self.headerView.window) {
      insetsView =
          [[UIApplication sharedApplication] keyWindow].rootViewController.view;
    }
    UIEdgeInsets safeAreaInsets = insetsView.safeAreaInsets;
    width = std::max<CGFloat>(
        0, width - safeAreaInsets.left - safeAreaInsets.right);

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::searchFieldWidth(width)];
    [self addConstraintsForLogoView:self.logoVendor.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.headerView];

    [self.logoVendor fetchDoodle];
  }
  return self.headerView;
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
  // |accessibilityButton| and pass them along.
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

  [self.headerView addViewsToSearchField:self.fakeOmnibox];

  if (self.voiceSearchIsEnabled) {
    [self.headerView.voiceSearchButton addTarget:self
                                          action:@selector(loadVoiceSearch:)
                                forControlEvents:UIControlEventTouchUpInside];
    [self.headerView.voiceSearchButton addTarget:self
                                          action:@selector(preloadVoiceSearch:)
                                forControlEvents:UIControlEventTouchDown];
  } else {
    [self.headerView.voiceSearchButton setEnabled:NO];
  }
}

- (void)addFakeTapView {
  UIButton* fakeTapButton = [[UIButton alloc] init];
  fakeTapButton.translatesAutoresizingMaskIntoConstraints = NO;
  fakeTapButton.isAccessibilityElement = NO;
  [self.headerView addToolbarView:fakeTapButton];
  [fakeTapButton addTarget:self
                    action:@selector(fakeboxTapped)
          forControlEvents:UIControlEventTouchUpInside];
}

- (void)addIdentityDisc {
  // Set up a button. Details for the button will be set through delegate
  // implementation of UserAccountImageUpdateDelegate.
  self.identityDiscButton = [UIButton buttonWithType:UIButtonTypeCustom];
  self.identityDiscButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_PARTICLE_DISC);
  self.identityDiscButton.imageEdgeInsets = UIEdgeInsetsMake(
      ntp_home::kIdentityAvatarMargin, ntp_home::kIdentityAvatarMargin,
      ntp_home::kIdentityAvatarMargin, ntp_home::kIdentityAvatarMargin);
  [self.identityDiscButton addTarget:self
                              action:@selector(identityDiscTapped)
                    forControlEvents:UIControlEventTouchUpInside];
  // TODO(crbug.com/965958): Set action on button to launch into Settings.
  [self.headerView setIdentityDiscView:self.identityDiscButton];

  // Register to receive the avatar of the currently signed in user.
  [self.delegate registerImageUpdater:self];
}

- (void)loadVoiceSearch:(id)sender {
  [self.commandHandler dismissModals];

  if ([self.delegate ignoreLoadRequests])
    return;
  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  UIView* voiceSearchButton = base::mac::ObjCCastStrict<UIView>(sender);
  [NamedGuide guideWithName:kVoiceSearchButtonGuide view:voiceSearchButton]
      .constrainedView = voiceSearchButton;
  [self.dispatcher startVoiceSearch];
}

- (void)preloadVoiceSearch:(id)sender {
  DCHECK(self.voiceSearchIsEnabled);
  [sender removeTarget:self
                action:@selector(preloadVoiceSearch:)
      forControlEvents:UIControlEventTouchDown];
  [self.dispatcher preloadVoiceSearch];
}

- (void)fakeboxTapped {
  if ([self.delegate ignoreLoadRequests])
    return;
  base::RecordAction(base::UserMetricsAction("MobileFakeboxNTPTapped"));
  [self focusFakebox];
}

- (void)focusFakebox {
  if ([self.delegate ignoreLoadRequests])
    return;
  [self shiftTilesUp];
}

- (void)identityDiscTapped {
  base::RecordAction(base::UserMetricsAction("MobileNTPIdentityDiscTapped"));
  [self.dispatcher showGoogleServicesSettingsFromViewController:nil];
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

// If Google is not the default search engine, hide the logo, doodle and
// fakebox. Make them appear if Google is set as default.
- (void)updateLogoAndFakeboxDisplay {
  if (self.logoVendor.showingLogo != self.logoIsShowing) {
    self.logoVendor.showingLogo = self.logoIsShowing;
    [self.doodleHeightConstraint
        setConstant:content_suggestions::doodleHeight(self.logoIsShowing)];
    if (IsRegularXRegularSizeClass(self))
      [self.fakeOmnibox setHidden:!self.logoIsShowing];
    [self.collectionSynchronizer invalidateLayout];
  }
}

// Adds the constraints for the |logoView|, the |fakeomnibox| related to the
// |headerView|. It also sets the properties constraints related to those views.
- (void)addConstraintsForLogoView:(UIView*)logoView
                      fakeOmnibox:(UIView*)fakeOmnibox
                    andHeaderView:(UIView*)headerView {
  self.doodleTopMarginConstraint = [logoView.topAnchor
      constraintEqualToAnchor:headerView.topAnchor
                     constant:content_suggestions::doodleTopMargin(
                                  YES, [self topInset])];
  self.doodleHeightConstraint = [logoView.heightAnchor
      constraintEqualToConstant:content_suggestions::doodleHeight(
                                    self.logoIsShowing)];
  self.fakeOmniboxHeightConstraint = [fakeOmnibox.heightAnchor
      constraintEqualToConstant:ToolbarExpandedHeight(
                                    self.traitCollection
                                        .preferredContentSizeCategory)];
  self.fakeOmniboxTopMarginConstraint = [logoView.bottomAnchor
      constraintEqualToAnchor:fakeOmnibox.topAnchor
                     constant:-content_suggestions::searchFieldTopMargin()];
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
  if (IsSplitToolbarMode()) {
    [self.dispatcher onFakeboxBlur];
  }
  [self.collectionSynchronizer shiftTilesDown];

  [self.commandHandler dismissModals];
}

- (void)shiftTilesUp {
  if (self.disableScrollAnimation)
    return;

  void (^animations)() = nil;
  if (![self.delegate isScrolledToTop]) {
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

  void (^completionBlock)() = ^{
    self.headerView.omnibox.hidden = YES;
    self.headerView.cancelButton.hidden = YES;
    self.headerView.searchHintLabel.alpha = 1;
    self.headerView.voiceSearchButton.alpha = 1;
    self.disableScrollAnimation = NO;
    [self.dispatcher fakeboxFocused];
    if (IsSplitToolbarMode()) {
      [self.dispatcher onFakeboxAnimationComplete];
    }
  };

  [self.collectionSynchronizer shiftTilesUpWithAnimations:animations
                                               completion:completionBlock];
}

- (CGFloat)topInset {
  return self.parentViewController.view.safeAreaInsets.top;
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

#pragma mark - NTPHomeConsumer

- (void)setLogoIsShowing:(BOOL)logoIsShowing {
  _logoIsShowing = logoIsShowing;
  [self updateLogoAndFakeboxDisplay];
}

- (void)locationBarBecomesFirstResponder {
  if (!self.isShowing)
    return;

  self.omniboxFocused = YES;

  [self shiftTilesUp];
}

- (void)locationBarResignsFirstResponder {
  if (!self.isShowing && ![self.delegate isScrolledToTop])
    return;

  self.omniboxFocused = NO;
  if ([self.delegate isContextMenuVisible]) {
    return;
  }

  [self shiftTilesDown];
}

#pragma mark - UserAccountImageUpdateDelegate

- (void)updateAccountImage:(UIImage*)image {
  [self.identityDiscButton setImage:image forState:UIControlStateNormal];
  self.identityDiscButton.imageView.layer.cornerRadius = image.size.width / 2;
  self.identityDiscButton.imageView.layer.masksToBounds = YES;
}

@end
