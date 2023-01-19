// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_presenter.h"

#import "base/functional/bind.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/commerce/push_notification/push_notification_feature.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/ui/bubble/bubble_presenter_delegate.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/commands/toolbar_commands.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kBubblePresentationDelay = 1;
}  // namespace

@interface BubblePresenter ()

// Used to display the bottom toolbar tip in-product help promotion bubble.
// `nil` if the tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that `userEngaged` remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* bottomToolbarTipBubblePresenter;
// Used to display the long press on toolbar buttons tip in-product help
// promotion bubble. `nil` if the tip bubble has not yet been presented. Once
// the bubble is dismissed, it remains allocated so that `userEngaged` remains
// accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* longPressToolbarTipBubblePresenter;
// Used to display the new tab tip in-product help promotion bubble. `nil` if
// the new tab tip bubble has not yet been presented. Once the bubble is
// dismissed, it remains allocated so that `userEngaged` remains accessible.
@property(nonatomic, strong)
    BubbleViewControllerPresenter* tabTipBubblePresenter;
@property(nonatomic, strong, readwrite)
    BubbleViewControllerPresenter* incognitoTabTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* discoverFeedHeaderMenuTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* readingListTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* followWhileBrowsingBubbleTipPresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* defaultPageModeTipBubblePresenter;
@property(nonatomic, strong)
    BubbleViewControllerPresenter* whatsNewBubblePresenter;
@property(nonatomic, strong) BubbleViewControllerPresenter*
    priceNotificationsWhileBrowsingBubbleTipPresenter;

@property(nonatomic, assign) ChromeBrowserState* browserState;

@end

@implementation BubblePresenter

#pragma mark - Public

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (void)stop {
  _browserState = nil;
}

- (void)showHelpBubbleIfEligible {
  DCHECK(self.browserState);
  // Waits to present the bubbles until the feature engagement tracker database
  // is fully initialized. This method requires that `self.browserState` is not
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

- (void)showLongPressHelpBubbleIfEligible {
  DCHECK(self.browserState);
  // Waits to present the bubble until the feature engagement tracker database
  // is fully initialized. This method requires that `self.browserState` is not
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

- (void)hideAllHelpBubbles {
  [self.tabTipBubblePresenter dismissAnimated:NO];
  [self.incognitoTabTipBubblePresenter dismissAnimated:NO];
  [self.bottomToolbarTipBubblePresenter dismissAnimated:NO];
  [self.longPressToolbarTipBubblePresenter dismissAnimated:NO];
  [self.discoverFeedHeaderMenuTipBubblePresenter dismissAnimated:NO];
  [self.readingListTipBubblePresenter dismissAnimated:NO];
  [self.followWhileBrowsingBubbleTipPresenter dismissAnimated:NO];
  [self.defaultPageModeTipBubblePresenter dismissAnimated:NO];
}

- (void)userEnteredTabSwitcher {
  if (self.tabTipBubblePresenter.userEngaged) {
    base::RecordAction(base::UserMetricsAction("NewTabTipTargetSelected"));
  }
}

- (void)toolsMenuDisplayed {
  if (self.incognitoTabTipBubblePresenter.userEngaged) {
    base::RecordAction(
        base::UserMetricsAction("NewIncognitoTabTipTargetSelected"));
  }
}

- (void)presentDiscoverFeedHeaderTipBubble {
  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_DISCOVER_FEED_HEADER_IPH);

  UIView* menuButton = [self.layoutGuideCenter
      referencedViewUnderName:kDiscoverFeedHeaderMenuGuide];
  // Checks "canPresentBubble" after checking that the NTP with feed is visible.
  // This ensures that the feature tracker doesn't trigger the IPH event if the
  // bubble isn't shown, which would prevent it from ever being shown again.
  if (!menuButton || ![self canPresentBubble]) {
    return;
  }
  CGPoint discoverFeedHeaderAnchor =
      [menuButton.superview convertPoint:menuButton.frame.origin toView:nil];
  // Anchor the IPH 1/3 of the way through the button. Anchoring it midway
  // doesn't work since the button is too close to the edge, which would cause
  // the bubble to bleed out the screen.
  discoverFeedHeaderAnchor.x += menuButton.frame.size.width / 3;

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `discoverFeedHeaderMenuTipBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHDiscoverFeedHeaderFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:nil
                  anchorPoint:discoverFeedHeaderAnchor];
  if (!presenter)
    return;

  self.discoverFeedHeaderMenuTipBubblePresenter = presenter;
}

- (void)presentFollowWhileBrowsingTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_FOLLOW_WHILE_BROWSING_IPH);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `followWhileBrowsingBubbleTipPresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHFollowWhileBrowsingFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(
                                  IDS_IOS_FOLLOW_WHILE_BROWSING_IPH)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.followWhileBrowsingBubbleTipPresenter = presenter;
}

- (void)presentDefaultSiteViewTipBubble {
  if (![self canPresentBubble])
    return;

  web::WebState* webState =
      [self.delegate currentWebStateForBubblePresenter:self];
  if (!webState ||
      ShouldLoadUrlInDesktopMode(webState->GetVisibleURL(), self.browserState))
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_DEFAULT_PAGE_MODE_TIP);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing presenter to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHDefaultSiteViewFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(
                                  IDS_IOS_DEFAULT_PAGE_MODE_TIP_VOICE_OVER)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.defaultPageModeTipBubblePresenter = presenter;
  base::UmaHistogramBoolean("IOS.IPH.DefaultSite.Presented", true);
}

- (void)presentWhatsNewBottomToolbarBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(IDS_IOS_WHATS_NEW_IPH_TEXT);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `whatsNewBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHFollowWhileBrowsingFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:l10n_util::GetNSString(IDS_IOS_WHATS_NEW_IPH_TEXT)
                  anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.whatsNewBubblePresenter = presenter;
}

- (void)presentPriceNotificationsWhileBrowsingTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSString(
      IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TOAST_IPH_TEXT);
  CGPoint toolsMenuAnchor = [self anchorPointToGuide:kToolsMenuGuide
                                           direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `whatsNewBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter =
      [self presentBubbleForFeature:
                feature_engagement::kIPHPriceNotificationsWhileBrowsingFeature
                          direction:arrowDirection
                          alignment:BubbleAlignmentTrailing
                               text:text
              voiceOverAnnouncement:text
                        anchorPoint:toolsMenuAnchor];
  if (!presenter)
    return;

  self.priceNotificationsWhileBrowsingBubbleTipPresenter = presenter;
}

#pragma mark - Private

- (void)presentBubbles {
  // If the tip bubble has already been presented and the user is still
  // considered engaged, it can't be overwritten or set to `nil` or else it will
  // reset the `userEngaged` property. Once the user is not engaged, the bubble
  // can be safely overwritten or set to `nil`.
  if (!self.tabTipBubblePresenter.userEngaged)
    [self presentNewTabTipBubble];
  if (!self.incognitoTabTipBubblePresenter.userEngaged)
    [self presentNewIncognitoTabTipBubble];

  // The bottom toolbar and Discover feed header menu don't use the
  // isUserEngaged, so don't check if the user is engaged here.
  [self presentBottomToolbarTipBubble];
}

- (void)presentLongPressBubble {
  if (self.longPressToolbarTipBubblePresenter.userEngaged)
    return;

  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSString(IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_TEXT);
  CGPoint tabGridButtonAnchor = [self anchorPointToGuide:kTabSwitcherGuide
                                               direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `longPressToolbarTipBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHLongPressToolbarTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentTrailing
                         text:text
        voiceOverAnnouncement:
            l10n_util::GetNSString(
                IDS_IOS_LONG_PRESS_TOOLBAR_IPH_PROMOTION_VOICE_OVER)
                  anchorPoint:tabGridButtonAnchor];
  if (!presenter)
    return;

  self.longPressToolbarTipBubblePresenter = presenter;
}

// Presents and returns a bubble view controller for the `feature` with an arrow
// `direction`, an arrow `alignment` and a `text` on an `anchorPoint`.
- (BubbleViewControllerPresenter*)
presentBubbleForFeature:(const base::Feature&)feature
              direction:(BubbleArrowDirection)direction
              alignment:(BubbleAlignment)alignment
                   text:(NSString*)text
  voiceOverAnnouncement:(NSString*)voiceOverAnnouncement
            anchorPoint:(CGPoint)anchorPoint {
  DCHECK(self.browserState);
  BubbleViewControllerPresenter* presenter =
      [self bubblePresenterForFeature:feature
                            direction:direction
                            alignment:alignment
                                 text:text];
  if (!presenter)
    return nil;
  presenter.voiceOverAnnouncement = voiceOverAnnouncement;
  if ([presenter canPresentInView:self.rootViewController.view
                      anchorPoint:anchorPoint] &&
      ([self shouldForcePresentBubbleForFeature:feature] ||
       feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
           ->ShouldTriggerHelpUI(feature))) {
    [presenter presentInViewController:self.rootViewController
                                  view:self.rootViewController.view
                           anchorPoint:anchorPoint];
  }
  return presenter;
}

// Presents a bubble associated with the bottom toolbar tip in-product help
// promotion. This method requires that `self.browserState` is not NULL.
- (void)presentBottomToolbarTipBubble {
  if (!IsSplitToolbarMode(self.rootViewController))
    return;

  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection = BubbleArrowDirectionDown;
  NSString* text = l10n_util::GetNSStringWithFixup(
      IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_TEXT);
  CGPoint newTabButtonAnchor = [self anchorPointToGuide:kNewTabButtonGuide
                                              direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the tip, then end early to prevent the potential reassignment of the
  // existing `bottomToolbarTipBubblePresenter` to nil.
  BubbleViewControllerPresenter* presenter = [self
      presentBubbleForFeature:feature_engagement::kIPHBottomToolbarTipFeature
                    direction:arrowDirection
                    alignment:BubbleAlignmentCenter
                         text:text
        voiceOverAnnouncement:
            l10n_util::GetNSString(
                IDS_IOS_BOTTOM_TOOLBAR_IPH_PROMOTION_VOICE_OVER)
                  anchorPoint:newTabButtonAnchor];
  if (!presenter)
    return;

  self.bottomToolbarTipBubblePresenter = presenter;
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->NotifyEvent(feature_engagement::events::kBottomToolbarOpened);
}

// Optionally presents a bubble associated with the new tab tip in-product help
// promotion. If the feature engagement tracker determines it is valid to show
// the new tab tip, then it initializes `tabTipBubblePresenter` and presents
// the bubble. If it is not valid to show the new tab tip,
// `tabTipBubblePresenter` is set to `nil` and no bubble is shown. This method
// requires that `self.browserState` is not NULL.
- (void)presentNewTabTipBubble {
  if (![self canPresentBubble])
    return;

  // Do not present the new tab tips on NTP.
  if (![self.delegate currentWebStateForBubblePresenter:self] ||
      [self.delegate currentWebStateForBubblePresenter:self]->GetVisibleURL() ==
          kChromeUINewTabURL)
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text =
      l10n_util::GetNSStringWithFixup(IDS_IOS_NEW_TAB_IPH_PROMOTION_TEXT);
  CGPoint tabSwitcherAnchor = [self anchorPointToGuide:kTabSwitcherGuide
                                             direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the new tab tip, then end early to prevent the potential reassignment
  // of the existing `tabTipBubblePresenter` to nil.
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
// promotion. This method requires that `self.browserState` is not NULL.
- (void)presentNewIncognitoTabTipBubble {
  if (![self canPresentBubble])
    return;

  BubbleArrowDirection arrowDirection =
      IsSplitToolbarMode(self.rootViewController) ? BubbleArrowDirectionDown
                                                  : BubbleArrowDirectionUp;
  NSString* text = l10n_util::GetNSStringWithFixup(
      IDS_IOS_NEW_INCOGNITO_TAB_IPH_PROMOTION_TEXT);

  CGPoint toolsButtonAnchor =
      [self anchorPointToGuide:kToolsMenuGuide direction:arrowDirection];

  // If the feature engagement tracker does not consider it valid to display
  // the incognito tab tip, then end early to prevent the potential reassignment
  // of the existing `incognitoTabTipBubblePresenter` to nil.
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

  [self.toolbarHandler triggerToolsMenuButtonAnimation];
}

#pragma mark - Private Utils

// Returns the anchor point for a bubble with an `arrowDirection` pointing to a
// `guideName`. The point is in the window coordinates.
- (CGPoint)anchorPointToGuide:(GuideName*)guideName
                    direction:(BubbleArrowDirection)arrowDirection {
  UILayoutGuide* guide =
      [self.layoutGuideCenter makeLayoutGuideNamed:guideName];
  DCHECK(guide);
  [self.rootViewController.view addLayoutGuide:guide];
  CGPoint anchorPoint =
      bubble_util::AnchorPoint(guide.layoutFrame, arrowDirection);
  return [guide.owningView convertPoint:anchorPoint
                                 toView:guide.owningView.window];
}

// Returns whether the tab can present a bubble tip.
- (BOOL)canPresentBubble {
  // If BubblePresenter has been stopped, do not present the bubble.
  if (!self.browserState)
    return NO;
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
// it is valid to show the promotion and `nil` otherwise. `feature` is the
// base::Feature object associated with the given promotion. `direction` is the
// direction the bubble's arrow is pointing. `alignment` is the alignment of the
// arrow on the button. `text` is the text displayed by the bubble. This method
// requires that `self.browserState` is not NULL.
- (BubbleViewControllerPresenter*)
bubblePresenterForFeature:(const base::Feature&)feature
                direction:(BubbleArrowDirection)direction
                alignment:(BubbleAlignment)alignment
                     text:(NSString*)text {
  DCHECK(self.browserState);
  if ([self shouldForcePresentBubbleForFeature:feature] ||
      feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
          ->WouldTriggerHelpUI(feature)) {
    // Capture `weakSelf` instead of the feature engagement tracker object
    // because `weakSelf` will safely become `nil` if it is deallocated, whereas
    // the feature engagement tracker will remain pointing to invalid memory if
    // its owner (the ChromeBrowserState) is deallocated.
    __weak BubblePresenter* weakSelf = self;
    ProceduralBlockWithSnoozeAction dismissalCallback =
        ^(feature_engagement::Tracker::SnoozeAction snoozeAction) {
          [weakSelf featureDismissed:feature withSnooze:snoozeAction];
        };

    BubbleViewControllerPresenter* bubbleViewControllerPresenter =
        [[BubbleViewControllerPresenter alloc]
            initDefaultBubbleWithText:text
                       arrowDirection:direction
                            alignment:alignment
                 isLongDurationBubble:[self isLongDurationBubble:feature]
                    dismissalCallback:dismissalCallback];

    return bubbleViewControllerPresenter;
  }
  return nil;
}

- (void)featureDismissed:(const base::Feature&)feature
              withSnooze:
                  (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  if (!self.browserState)
    return;
  feature_engagement::TrackerFactory::GetForBrowserState(self.browserState)
      ->DismissedWithSnooze(feature, snoozeAction);
}

// Returns YES if the bubble for `feature` has a long duration.
- (BOOL)isLongDurationBubble:(const base::Feature&)feature {
  // Display follow iph bubble with long duration.
  return feature.name ==
         feature_engagement::kIPHFollowWhileBrowsingFeature.name;
}

// Return YES if the bubble should always be presented. Ex. if force present
// bubble set by system experimental settings.
- (BOOL)shouldForcePresentBubbleForFeature:(const base::Feature&)feature {
  // Always present follow IPH if it's triggered by system experimental
  // settings.
  if (feature.name == feature_engagement::kIPHFollowWhileBrowsingFeature.name &&
      experimental_flags::ShouldAlwaysShowFollowIPH()) {
    return YES;
  }

  return NO;
}

@end
