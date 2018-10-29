// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_synchronizing.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller_delegate.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_constants.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/primary_toolbar_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/ui/logo_vendor.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

@interface ContentSuggestionsHeaderViewController ()

// |YES| when notifications indicate the omnibox is focused.
@property(nonatomic, assign, getter=isOmniboxFocused) BOOL omniboxFocused;

// |YES| if this consumer is has voice search enabled.
@property(nonatomic, assign) BOOL voiceSearchIsEnabled;

// Exposes view and methods to drive the doodle.
@property(nonatomic, weak) id<LogoVendor> logoVendor;

// |YES| if the google landing toolbar can show the forward arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoForward;

// |YES| if the google landing toolbar can show the back arrow, cached and
// pushed into the header view.
@property(nonatomic, assign) BOOL canGoBack;

// The number of tabs to show in the google landing fake toolbar.
@property(nonatomic, assign) int tabCount;

@property(nonatomic, strong) ContentSuggestionsHeaderView* headerView;
@property(nonatomic, strong) UIButton* fakeOmnibox;
@property(nonatomic, strong) UIButton* accessibilityButton;
@property(nonatomic, strong) UILabel* searchHintLabel;
@property(nonatomic, strong) NSLayoutConstraint* hintLabelLeadingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* voiceTapTrailingConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* doodleTopMarginConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxWidthConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxHeightConstraint;
@property(nonatomic, strong) NSLayoutConstraint* fakeOmniboxTopMarginConstraint;
@property(nonatomic, assign) BOOL logoFetched;

@end

@implementation ContentSuggestionsHeaderViewController

@synthesize dispatcher = _dispatcher;
@synthesize delegate = _delegate;
@synthesize commandHandler = _commandHandler;
@synthesize searchHintLabel = _searchHintLabel;
@synthesize collectionSynchronizer = _collectionSynchronizer;
@synthesize readingListModel = _readingListModel;
@synthesize toolbarDelegate = _toolbarDelegate;
@synthesize logoVendor = _logoVendor;
@synthesize promoCanShow = _promoCanShow;
@synthesize canGoForward = _canGoForward;
@synthesize canGoBack = _canGoBack;
@synthesize showing = _showing;
@synthesize omniboxFocused = _omniboxFocused;
@synthesize tabCount = _tabCount;

@synthesize headerView = _headerView;
@synthesize fakeOmnibox = _fakeOmnibox;
@synthesize accessibilityButton = _accessibilityButton;
@synthesize hintLabelLeadingConstraint = _hintLabelLeadingConstraint;
@synthesize voiceTapTrailingConstraint = _voiceTapTrailingConstraint;
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

  NSArray* constraints =
      @[ self.hintLabelLeadingConstraint, self.voiceTapTrailingConstraint ];

  [self.headerView updateSearchFieldWidth:self.fakeOmniboxWidthConstraint
                                   height:self.fakeOmniboxHeightConstraint
                                topMargin:self.fakeOmniboxTopMarginConstraint
                                hintLabel:self.searchHintLabel
                       subviewConstraints:constraints
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
    offsetY -= ntp_header::ToolbarHeight() + [self topInset];
  }

  return offsetY;
}

- (CGFloat)headerHeight {
  return content_suggestions::heightForLogoHeader(
      self.logoIsShowing, self.promoCanShow, YES, [self topInset]);
}

#pragma mark - ContentSuggestionsHeaderProvider

- (UIView*)headerForWidth:(CGFloat)width {
  if (!self.headerView) {
    self.headerView = [[ContentSuggestionsHeaderView alloc] init];
    [self addFakeTapView];
    [self addFakeOmnibox];

    [self.headerView addSubview:self.logoVendor.view];
    [self.headerView addSubview:self.fakeOmnibox];
    self.logoVendor.view.translatesAutoresizingMaskIntoConstraints = NO;
    self.fakeOmnibox.translatesAutoresizingMaskIntoConstraints = NO;

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
    UIEdgeInsets safeAreaInsets = SafeAreaInsetsForView(insetsView);
    width = std::max<CGFloat>(
        0, width - safeAreaInsets.left - safeAreaInsets.right);

    self.fakeOmniboxWidthConstraint = [self.fakeOmnibox.widthAnchor
        constraintEqualToConstant:content_suggestions::searchFieldWidth(width)];
    [self addConstraintsForLogoView:self.logoVendor.view
                        fakeOmnibox:self.fakeOmnibox
                      andHeaderView:self.headerView];

    [self.headerView addViewsToSearchField:self.fakeOmnibox];
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

  // Set up fakebox hint label.
  self.searchHintLabel = [[UILabel alloc] init];
  content_suggestions::configureSearchHintLabel(self.searchHintLabel,
                                                self.fakeOmnibox);

  self.hintLabelLeadingConstraint = [self.searchHintLabel.leadingAnchor
      constraintGreaterThanOrEqualToAnchor:[self.fakeOmnibox leadingAnchor]
                                  constant:ntp_header::kHintLabelSidePadding];
  self.hintLabelLeadingConstraint.active = YES;

  // Set a button the same size as the fake omnibox as the accessibility
  // element. If the hint is the only accessible element, when the fake omnibox
  // is taking the full width, there are few points that are not accessible and
  // allow to select the content below it.
  self.searchHintLabel.isAccessibilityElement = NO;
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

  // Add a voice search button.
  UIButton* voiceTapTarget = [[UIButton alloc] init];
  content_suggestions::configureVoiceSearchButton(voiceTapTarget,
                                                  self.fakeOmnibox);

  self.voiceTapTrailingConstraint = [voiceTapTarget.trailingAnchor
      constraintEqualToAnchor:[self.fakeOmnibox trailingAnchor]];
  [NSLayoutConstraint activateConstraints:@[
    [self.searchHintLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:voiceTapTarget.leadingAnchor],
    _voiceTapTrailingConstraint
  ]];

  if (self.voiceSearchIsEnabled) {
    [voiceTapTarget addTarget:self
                       action:@selector(loadVoiceSearch:)
             forControlEvents:UIControlEventTouchUpInside];
    [voiceTapTarget addTarget:self
                       action:@selector(preloadVoiceSearch:)
             forControlEvents:UIControlEventTouchDown];
  } else {
    [voiceTapTarget setEnabled:NO];
  }
}

- (void)addFakeTapView {
  UIButton* fakeTapButton = [[UIButton alloc] init];
  fakeTapButton.translatesAutoresizingMaskIntoConstraints = NO;
  fakeTapButton.accessibilityLabel =
      l10n_util::GetNSString(IDS_ACCNAME_LOCATION);
  [self.headerView addToolbarView:fakeTapButton];
  [fakeTapButton addTarget:self
                    action:@selector(fakeboxTapped)
          forControlEvents:UIControlEventTouchUpInside];
}

- (void)loadVoiceSearch:(id)sender {
  [self.commandHandler dismissModals];

  DCHECK(self.voiceSearchIsEnabled);
  base::RecordAction(UserMetricsAction("MobileNTPMostVisitedVoiceSearch"));
  UIView* voiceSearchButton = base::mac::ObjCCastStrict<UIView>(sender);
  if (base::ios::IsRunningOnIOS12OrLater()) {
    [NamedGuide guideWithName:kVoiceSearchButtonGuide view:voiceSearchButton]
        .constrainedView = voiceSearchButton;
  } else {
    // On iOS 11 and below, constraining the layout guide to a view instead of
    // using frame freeze the app. The root cause wasn't found. See
    // https://crbug.com/874017.
    NamedGuide* voiceSearchGuide =
        [NamedGuide guideWithName:kVoiceSearchButtonGuide
                             view:voiceSearchButton];
    voiceSearchGuide.constrainedFrame =
        [voiceSearchGuide.owningView convertRect:voiceSearchButton.bounds
                                        fromView:voiceSearchButton];
  }
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
  base::RecordAction(base::UserMetricsAction("MobileFakeboxNTPTapped"));
  [self focusFakebox];
}

- (void)focusFakebox {
  [self shiftTilesUp];
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
      constraintEqualToConstant:content_suggestions::kSearchFieldHeight];
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
  void (^completionBlock)() = ^{
    [self.dispatcher fakeboxFocused];
    if (IsSplitToolbarMode()) {
      [self.dispatcher onFakeboxAnimationComplete];
    }
  };
  [self.collectionSynchronizer shiftTilesUpWithCompletionBlock:completionBlock];
}

- (CGFloat)topInset {
  if (@available(iOS 11, *))
    return self.parentViewController.view.safeAreaInsets.top;

  // TODO(crbug.com/826369) Replace this when the NTP is contained by the
  // BVC with |self.parentViewController.topLayoutGuide.length|.
  return StatusBarHeight();
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

@end
