// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"

#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#include "ios/chrome/browser/feature_engagement/tracker_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/named_guide_util.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBubblePresentationDelay = 1;
}  // namespace

@interface BubblePresenter ()

// Used to display the bottom toolbar tip in-product help promotion bubble.
// |nil| if the tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that |userEngaged| remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* bottomToolbarTipBubblePresenter;
// Used to display the long press on toolbar buttons tip in-product help
// promotion bubble. |nil| if the tip bubble has not yet been presented. Once
// the bubble is dismissed, it remains allocated so that |userEngaged| remains
// accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* longPressToolbarTipBubblePresenter;
// Used to display the new tab tip in-product help promotion bubble. |nil| if
// the new tab tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that |userEngaged| remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* tabTipBubblePresenter;
@property(nonatomic, strong, readwrite)
    BubbleViewControllerPresenter* incognitoTabTipBubblePresenter;

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;
@property(nonatomic, weak) id<BubblePresenterDelegate> delegate;
@property(nonatomic, weak) UIViewController* rootViewController;

@end

@implementation BubblePresenter

@synthesize bottomToolbarTipBubblePresenter = _bottomToolbarTipBubblePresenter;
@synthesize longPressToolbarTipBubblePresenter =
    _longPressToolbarTipBubblePresenter;
@synthesize tabTipBubblePresenter = _tabTipBubblePresenter;
@synthesize incognitoTabTipBubblePresenter = _incognitoTabTipBubblePresenter;
@synthesize browserState = _browserState;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize rootViewController = _rootViewController;

#pragma mark - Public

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                            delegate:(id<BubblePresenterDelegate>)delegate
                  rootViewController:(UIViewController*)rootViewController {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _delegate = delegate;
    _rootViewController = rootViewController;
  }
  return self;
}

- (void)presentBubblesIfEligible {
  DCHECK(self.browserState);
  // Waits to present the bubbles until the feature engagement tracker database
  // is fully initialized. This method requires that |self.browserState| is not
  // NULL.
  __weak BubblePresenter* weakSelf = self;
  void (^onInitializedBlock)(bool) = ^(bool successfullyLoaded) {
    if (!successfullyLoaded)
      return;
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      (int64_t)(kBubblePresentationDelay * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          [weakSelf presentBubbles];
        });
  };

  // Because the new tab tip occurs on startup, the feature engagement
  // tracker's database is not guaranteed to be loaded by this time. For the
  // bubble to appear properly, a callback is used to guarantee the event data
  // is loaded before the check to see if the promotion should be displayed.
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->AddOnInitializedCallback(base::BindRepeating(onInitializedBlock));
}

- (void)presentLongPressBubbleIfEligible {
  DCHECK(self.browserState);
  // Waits to present the bubble until the feature engagement tracker database
  // is fully initialized. This method requires that |self.browserState| is not
  // NULL.
  __weak BubblePresenter* weakSelf = self;
  void (^onInitializedBlock)(bool) = ^(bool successfullyLoaded) {
    if (!successfullyLoaded)
      return;
    [weakSelf presentLongPressBubble];
  };

  // Because the new tab tip occurs on startup, the feature engagement
  // tracker's database is not guaranteed to be loaded by this time. For the
  // bubble to appear properly, a callback is used to guarantee the event data
  // is loaded before the check to see if the promotion should be displayed.
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->AddOnInitializedCallback(base::BindRepeating(onInitializedBlock));
}

- (void)dismissBubbles {
  [self.tabTipBubblePresenter dismissAnimated:NO];
  [self.incognitoTabTipBubblePresenter dismissAnimated:NO];
  [self.bottomToolbarTipBubblePresenter dismissAnimated:NO];
  [self.longPressToolbarTipBubblePresenter dismissAnimated:NO];
}

- (void)userEnteredTabSwitcher {
  if ([self.tabTipBubblePresenter isUserEngaged]) {
    base::RecordAction(base::UserMetricsAction("NewTabTipTargetSelected"));
  }
}

- (void)toolsMenuDisplayed {
  if (self.incognitoTabTipBubblePresenter.isUserEngaged) {
    base::RecordAction(
        base::UserMetricsAction("NewIncognitoTabTipTargetSelected"));
  }
}

#pragma mark - Private

- (void)presentBubbles {
  // If the tip bubble has already been presented and the user is still
  // considered engaged, it can't be overwritten or set to |nil| or else it will
  // reset the |userEngaged| property. Once the user is not engaged, the bubble
  // can be safely overwritten or set to |nil|.
  if (!self.tabTipBubblePresenter.isUserEngaged)
    [self presentNewTabTipBubble];
  if (!self.incognitoTabTipBubblePresenter.isUserEngaged)
    [self presentNewIncognitoTabTipBubble];

  // The bottom toolbar doesn't use the isUserEngaged, so don't check if the
  // user is engaged here.
  [self presentBottomToolbarTipBubble];
}

- (void)presentLongPressBubble {
  if (self.longPressToolbarTipBubblePresenter.isUserEngaged)
    return;

  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode() ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_TEXT);
  CGPoint searchButtonAnchor =
      IsRegularXRegularSizeClass()
          ? [self anchorPointToGuide:kTabStripTabSwitcherGuide
                           direction:arrowDirection]
          : [self anchorPointToGuide:kTabSwitcherGuide
                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing |longPressToolbarTipBubblePresenter| to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHLongPressToolbarTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:
            l10n_util::GetNSString(
                IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_VOICE_OVER)
                  anchorPoint:searchButtonAnchor];
  if (!presenter)
    return;

  self.longPressToolbarTipBubblePresenter = presenter;
}

// Presents and returns a bubble view controller for the |feature| with an arrow
// |direction|, an arrow |alignment| and a |text| on an |anchorPoint|.
- (BubbleViewControllerPresenter*)
presentBubbleForFeature:(const base::Feature&)feature
              direction:(BubbleArrowDirection)direction
              alignment:(BubbleAlignment)alignment
                   text:(NSString*)text
  voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
            anchorPoint:(CGPoint)anchorPoint {
  BubbleViewControllerPresenter* presenter =
      [self bubblePresenterForFeature:feature
                            direction:direction
                            alignment:alignment
                                 text:text];

  presenter.voiceOverAnnouncement = voiceOverAnnouncement;

  [presenter presentInViewController:self.rootViewController
                                view:self.rootViewController.view
                         anchorPoint:anchorPoint];

  return presenter;
}

// Presents a bubble associated with the bottom toolbar tip in-product help
// promotion. This method requires that |self.browserState| is not NULL.
- (void)presentBottomToolbarTipBubble {
  if (!IsSplitToolbarMode())
    return;

  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text = l10n_util::GetNSStringWithFixup(
      IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_TEXT);
  CGPoint searchButtonAnchor =
      [self anchorPointToGuide:kSearchButtonGuide direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing |bottomToolbarTipBubblePresenter| to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHBottomToolbarTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentCenter
                         text:text
        voiceOverAnnouncement:
            l10n_util::GetNSString(
                IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_VOICE_OVER)
                  anchorPoint:searchButtonAnchor];
  if (!presenter)
    return;

  self.bottomToolbarTipBubblePresenter = presenter;
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->NotifyEvent(feature_engagement::events::kBottomToolbarOpened);
}

// Optionally presents a bubble associated with the new tab tip in-product help
// promotion. If the feature engagement tracker determines it is valid to show
// the new tab tip, then it initializes |tabTipBubblePresenter| and presents
// the bubble. If it is not valid to show the new tab tip,
// |tabTipBubblePresenter| is set to |nil| and no bubble is shown. This method
// requires that |self.browserState| is not NULL.
- (void)presentNewTabTipBubble {
  if (![self canPresentBubble])
    return;

  // Do not present the new tab tips on NTP.
  if (![self.delegate currentWebStateForBubblePresenter:self] ||
      [self.delegate currentWebStateForBubblePresenter:self]->GetVisibleURL() ==
          kChromeUINewTabURL)
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode() ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_TAB_IPH_PROMOTION_TEXT);
  CGPoint tabSwitcherAnchor;
  if (IsRegularXRegularSizeClass()) {
    tabSwitcherAnchor = [self anchorPointToGuide:kTabStripTabSwitcherGuide
                                       direction:arrowDirection];
  } else {
    tabSwitcherAnchor =
        [self anchorPointToGuide:kTabSwitcherGuide direction:arrowDirection];
  }

  // If the feature engagement tracker does not consider it valid to display
  // the new tab tip, then end early to prevent the potential reassignment
  // of the existing |tabTipBubblePresenter| to nil.
  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:feature_engagement::kIPHNewTabTipFeature
                          direction:arrowDirection
                          alignment:BubbleAlignmentTrailing
                               text:text
              voiceOverAnnouncement:nil
                        anchorPoint:tabSwitcherAnchor];
  if (!presenter)
    return;

  self.tabTipBubblePresenter = presenter;
}

// Presents a bubble associated with the new incognito tab tip in-product help
// promotion. This method requires that |self.browserState| is not NULL.
- (void)presentNewIncognitoTabTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode() ? BubbleArrowDirectionDown : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSStringWithFixup(
      IDS_IOS_NEW_INCOGNITO_TAB_IPH_PROMOTION_TEXT);

  CGPoint toolsButtonAnchor =
      [self anchorPointToGuide:kToolsMenuGuide direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the incognito tab tip, then end early to prevent the potential reassignment
  // of the existing |incognitoTabTipBubblePresenter| to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHNewIncognitoTabTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:nil
                  anchorPoint:toolsButtonAnchor];
  if (!presenter)
    return;

  self.incognitoTabTipBubblePresenter = presenter;

  [self.dispatcher triggerToolsMenuButtonAnimation];
}

#pragma mark - Private Utils

// Returns the anchor point for a bubble with an |arrowDirection| pointing to a
// |guideName|. The point is in the window coordinates.
- (CGPoint)anchorPointToGuide:(GuideName*)guideName
                    direction:(BubbleArrowDirection)arrowDirection {
  UILayoutGuide* guide =
      [NamedGuide guideWithName:guideName view:self.rootViewController.view];
  DCHECK(guide);
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(guide.layoutFrame, arrowDirection);
  return [guide.owningView convertPoint:anchorPoint
                                 toView:guide.owningView.window];
}

// Returns whether the tab can present a bubble tip.
- (BOOL)canPresentBubble {
  DCHECK(self.browserState);
  // If the BVC is not visible, do not present the bubble.
  if (![self.delegate rootViewVisibleForBubblePresenter:self])
    return NO;
  // Do not present the bubble if there is no current tab.
  web::WebState* currentWebState =
      [self.delegate currentWebStateForBubblePresenter:self];
  if (!currentWebState)
    return NO;

  // Do not present the bubble if the tab is not scrolled to the top.
  if (![self.delegate isTabScrolledToTopForBubblePresenter:self])
    return NO;

  return YES;
}

// Returns a bubble associated with an in-product help promotion if
// it is valid to show the promotion and |nil| otherwise. |feature| is the
// base::Feature object associated with the given promotion. |direction| is the
// direction the bubble's arrow is pointing. |alignment| is the alignment of the
// arrow on the button. |text| is the text displayed by the bubble. This method
// requires that |self.browserState| is not NULL.
- (BubbleViewControllerPresenter*)
bubblePresenterForFeature:(const base::Feature&)feature
                direction:(BubbleArrowDirection)direction
                alignment:(BubbleAlignment)alignment
                     text:(NSString*)text {
  DCHECK(self.browserState);
  if (!feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
           ->ShouldTriggerHelpUI(feature)) {
    return nil;
  }
  // Capture |weakSelf| instead of the feature engagement tracker object
  // because |weakSelf| will safely become |nil| if it is deallocated, whereas
  // the feature engagement tracker will remain pointing to invalid memory if
  // its owner (the ChromeBrowserState) is deallocated.
  __weak BubblePresenter* weakSelf = self;
  void (^dismissalCallback)(void) = ^{
    BubblePresenter* strongSelf = weakSelf;
    if (strongSelf) {
      feature_engagement::TrackerFactory::GetForBrowserState(
          strongSelf.browserState)
          ->Dismissed(feature);
    }
  };

  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc] initWithText:text
                                           arrowDirection:direction
                                                alignment:alignment
                                        dismissalCallback:dismissalCallback];

  return bubbleViewControllerPresenter;
}

@end
