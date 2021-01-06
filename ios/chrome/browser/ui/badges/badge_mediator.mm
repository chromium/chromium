// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_ios.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#include "ios/chrome/browser/infobars/overlays/infobar_overlay_util.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/public/overlay_presenter_observer_bridge.h"
#include "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#include "ios/chrome/browser/ui/badges/badge_type_util.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"
#import "ios/chrome/browser/ui/infobars/infobar_feature.h"
#import "ios/chrome/browser/ui/infobars/infobar_ui_delegate.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The minimum number of non-Fullscreen badges to display the overflow popup
// menu.
const int kMinimumNonFullScreenBadgesForOverflow = 2;
// Historgram name for when an overflow badge was tapped.
const char kInfobarOverflowBadgeTappedUserAction[] =
    "MobileMessagesOverflowBadgeTapped";
// Histogram name for when the overflow badge is shown
const char kInfobarOverflowBadgeShownUserAction[] =
    "MobileMessagesOverflowBadgeShown";

}  // namespace

@interface BadgeMediator () <InfobarBadgeTabHelperDelegate,
                             OverlayPresenterObserving,
                             WebStateListObserving> {
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
}

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, readonly) WebStateList* webStateList;

// The WebStateList's active WebState.
@property(nonatomic, assign) web::WebState* webState;

// The active WebState's badge tab helper.
@property(nonatomic, readonly) InfobarBadgeTabHelper* badgeTabHelper;

// The infobar banner OverlayPresenter.
@property(nonatomic, readonly) OverlayPresenter* overlayPresenter;

// The incognito badge, or nil if the Browser is not off-the-record.
@property(nonatomic, readonly) id<BadgeItem> offTheRecordBadge;

// Array of all available badges.
@property(nonatomic, strong) NSMutableArray<id<BadgeItem>>* badges;

@end

@implementation BadgeMediator

- (instancetype)initWithBrowser:(Browser*)browser {
  self = [super init];
  if (self) {
    DCHECK(browser);
    // Create the incognito badge if |browser| is off-the-record.
    if (browser->GetBrowserState()->IsOffTheRecord()) {
      _offTheRecordBadge = [[BadgeStaticItem alloc]
          initWithBadgeType:BadgeType::kBadgeTypeIncognito];
    }
    // Set up the OverlayPresenterObserver for the infobar banner presentation.
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _overlayPresenter =
        OverlayPresenter::FromBrowser(browser, OverlayModality::kInfobarBanner);
    _overlayPresenter->AddObserver(_overlayPresenterObserver.get());
    // Set up the WebStateList and its observer.
    _webStateList = browser->GetWebStateList();
    _webState = _webStateList->GetActiveWebState();
    if (_webState) {
      InfobarBadgeTabHelper::FromWebState(_webState)->SetDelegate(self);
    }
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  // |-disconnect| must be called before deallocation.
  DCHECK(!_webStateList);
}

- (void)disconnect {
  [self disconnectWebStateList];
  [self disconnectOverlayPresenter];
}

#pragma mark - Disconnect helpers

- (void)disconnectWebStateList {
  if (_webStateList) {
    self.webState = nullptr;
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver = nullptr;
    _webStateList = nullptr;
  }
}

- (void)disconnectOverlayPresenter {
  if (_overlayPresenter) {
    _overlayPresenter->RemoveObserver(_overlayPresenterObserver.get());
    _overlayPresenterObserver = nullptr;
    _overlayPresenter = nullptr;
  }
}

#pragma mark - Accessors

- (void)setConsumer:(id<BadgeConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;
  if (_webState)
    InfobarBadgeTabHelper::FromWebState(_webState)->SetDelegate(nil);
  _webState = webState;
  if (_webState)
    InfobarBadgeTabHelper::FromWebState(_webState)->SetDelegate(self);
  [self updateBadgesForActiveWebState];
  [self updateConsumer];
}

- (InfobarBadgeTabHelper*)badgeTabHelper {
  return self.webState ? InfobarBadgeTabHelper::FromWebState(self.webState)
                       : nullptr;
}

#pragma mark - Accessor helpers

- (void)updateBadgesForActiveWebState {
  if (self.webState) {
    self.badges = [self.badgeTabHelper->GetInfobarBadgeItems() mutableCopy];
  } else {
    self.badges = [NSMutableArray<id<BadgeItem>> array];
  }
}

// Updates the consumer for the current active WebState.
- (void)updateConsumer {
  if (!self.consumer)
    return;

  // Update the badges array if necessary.
  if (!self.badges)
    [self updateBadgesForActiveWebState];

  BOOL shouldDisplayOverflowBadge =
      self.badges.count >= kMinimumNonFullScreenBadgesForOverflow;
  id<BadgeItem> displayedBadge = nil;
  if (shouldDisplayOverflowBadge) {
    displayedBadge = [[BadgeTappableItem alloc]
        initWithBadgeType:BadgeType::kBadgeTypeOverflow];
  } else {
    displayedBadge = [self.badges firstObject];
  }
  // Update the consumer with the new badge items.
  [self.consumer setupWithDisplayedBadge:displayedBadge
                         fullScreenBadge:self.offTheRecordBadge];
}

#pragma mark - BadgeDelegate

- (void)passwordsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK(badgeButton.badgeType == BadgeType::kBadgeTypePasswordSave ||
         badgeButton.badgeType == BadgeType::kBadgeTypePasswordUpdate);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)saveCardBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, BadgeType::kBadgeTypeSaveCard);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)translateBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, BadgeType::kBadgeTypeTranslate);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)overflowBadgeButtonTapped:(id)sender {
  NSMutableArray<id<BadgeItem>>* popupMenuBadges =
      [[NSMutableArray alloc] init];
  // Get all non-fullscreen badges.
  for (id<BadgeItem> item in self.badges) {
    if (!item.fullScreen) {
      // Mark each badge as read since the overflow menu is about to be
      // displayed.
      item.badgeState |= BadgeStateRead;
      [popupMenuBadges addObject:item];
    }
  }
  // Log overflow badge tap.
  base::RecordAction(
      base::UserMetricsAction(kInfobarOverflowBadgeTappedUserAction));
  [self.dispatcher displayPopupMenuWithBadgeItems:popupMenuBadges];
  [self updateConsumerReadStatus];
  // TODO(crbug.com/976901): Add metric for this action.
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (void)addInfobarBadge:(id<BadgeItem>)badgeItem
            forWebState:(web::WebState*)webState {
  if (webState != self.webStateList->GetActiveWebState()) {
    // Don't add badge if |badgeItem| is not coming from the currently active
    // WebState.
    return;
  }
  [self.badges addObject:badgeItem];
  [self updateBadgesShown];
}

- (void)removeInfobarBadge:(id<BadgeItem>)badgeItem
               forWebState:(web::WebState*)webState {
  if (webState != self.webStateList->GetActiveWebState()) {
    // Don't remove badge if |badgeItem| is not coming from the currently active
    // WebState.
    return;
  }
  for (id<BadgeItem> item in self.badges) {
    if (item.badgeType == badgeItem.badgeType) {
      [self.badges removeObject:item];
      [self updateBadgesShown];
      return;
    }
  }
}

- (void)updateInfobarBadge:(id<BadgeItem>)badgeItem
               forWebState:(web::WebState*)webState {
  if (webState != self.webStateList->GetActiveWebState()) {
    // Don't update badge if |badgeItem| is not coming from the currently active
    // WebState.
    return;
  }
  for (id<BadgeItem> item in self.badges) {
    if (item.badgeType == badgeItem.badgeType) {
      item.badgeState = badgeItem.badgeState;
      [self updateBadgesShown];
      return;
    }
  }
}

#pragma mark - OverlayPresenterObserving

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didShowOverlayForRequest:(OverlayRequest*)request {
  DCHECK_EQ(self.overlayPresenter, presenter);
  InfobarBadgeTabHelper* badgeTabHelper = self.badgeTabHelper;
  if (badgeTabHelper) {
    self.badgeTabHelper->UpdateBadgeForInfobarBannerPresented(
        GetOverlayRequestInfobarType(request));
  }
}

- (void)overlayPresenter:(OverlayPresenter*)presenter
    didHideOverlayForRequest:(OverlayRequest*)request {
  DCHECK_EQ(self.overlayPresenter, presenter);
  InfobarBadgeTabHelper* badgeTabHelper = self.badgeTabHelper;
  if (badgeTabHelper) {
    self.badgeTabHelper->UpdateBadgeForInfobarBannerDismissed(
        GetOverlayRequestInfobarType(request));
  }
}

- (void)overlayPresenterDestroyed:(OverlayPresenter*)presenter {
  DCHECK_EQ(self.overlayPresenter, presenter);
  [self disconnectOverlayPresenter];
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(self.webStateList, webStateList);
  if (atIndex == webStateList->active_index())
    self.webState = newWebState;
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(self.webStateList, webStateList);
  self.webState = newWebState;
}

#pragma mark - Private

// Directs consumer to update read status depending on the state of the
// non-fullscreen badges.
- (void)updateConsumerReadStatus {
  for (id<BadgeItem> item in self.badges) {
    if (!(item.badgeState & BadgeStateRead)) {
      [self.consumer markDisplayedBadgeAsRead:NO];
      return;
    }
  }
  [self.consumer markDisplayedBadgeAsRead:YES];
}

// Gets the last fullscreen and non-fullscreen badges.
// This assumes that there is only ever one fullscreen badge, so the last badge
// in |badges| should be the only one.
- (void)updateBadgesShown {
  // The badge to be displayed alongside the fullscreen badge. Logic below
  // currently assigns it to the last non-fullscreen badge in the list, since it
  // works if there is only one non-fullscreen badge. Otherwise, where there are
  // multiple non-fullscreen badges, additional logic below determines what
  // badge will be shown.
  id<BadgeItem> displayedBadge;
  // The badge that is current displaying its banner. This will be set as the
  // displayedBadge if there are multiple badges.
  id<BadgeItem> presentingBadge;
  for (id<BadgeItem> item in self.badges) {
      if (item.badgeState & BadgeStatePresented) {
        presentingBadge = item;
      }
      displayedBadge = item;
  }

  // Figure out what displayedBadge should be showing if there are multiple
  // non-Fullscreen badges.
  NSInteger count = [self.badges count];
  if (count >= kMinimumNonFullScreenBadgesForOverflow) {
    // If a badge's banner is being presented, then show that badge as the
    // displayed badge. Otherwise, show the overflow badge.
    displayedBadge = presentingBadge
                         ? presentingBadge
                         : [[BadgeTappableItem alloc]
                               initWithBadgeType:BadgeType::kBadgeTypeOverflow];
  } else {
    // Since there is only one non-fullscreen badge, it will be fixed as the
    // displayed badge, so mark it as read.
    displayedBadge.badgeState |= BadgeStateRead;
  }
  if (displayedBadge.badgeType == BadgeType::kBadgeTypeOverflow) {
    // Log that the overflow badge is being shown.
    base::RecordAction(
        base::UserMetricsAction(kInfobarOverflowBadgeShownUserAction));
  }
  [self.consumer updateDisplayedBadge:displayedBadge
                      fullScreenBadge:self.offTheRecordBadge];
  [self updateConsumerReadStatus];
}

// Shows the modal UI when |button| is tapped.
- (void)handleTappedBadgeButton:(BadgeButton*)button {
  InfobarType infobarType = InfobarTypeForBadgeType(button.badgeType);
  if (base::FeatureList::IsEnabled(kInfobarOverlayUI)) {
    DCHECK(self.webState);
    InfoBarIOS* infobar = [self infobarWithType:infobarType];
    if (infobar) {
      InfobarOverlayRequestInserter::CreateForWebState(self.webState);
      InsertParams params(infobar);
      params.overlay_type = InfobarOverlayType::kModal;
      params.insertion_index =
          OverlayRequestQueue::FromWebState(self.webState,
                                            OverlayModality::kInfobarModal)
              ->size();
      params.source = InfobarOverlayInsertionSource::kBadge;
      InfobarOverlayRequestInserter::FromWebState(self.webState)
          ->InsertOverlayRequest(params);
    }
  } else {
    [self.dispatcher displayModalInfobar:infobarType];
  }
  [self recordMetricsForBadgeButton:button infobarType:infobarType];
}

// Returns the infobar in the active WebState's InfoBarManager with |type|.
- (InfoBarIOS*)infobarWithType:(InfobarType)type {
  InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(self.webState);
  for (size_t index = 0; index < manager->infobar_count(); ++index) {
    InfoBarIOS* infobar = static_cast<InfoBarIOS*>(manager->infobar_at(index));
    if (infobar->infobar_type() == type)
      return infobar;
  }
  return nullptr;
}

// Records Badge tap Histograms through the InfobarMetricsRecorder and then
// records UserActions.
- (void)recordMetricsForBadgeButton:(BadgeButton*)badgeButton
                        infobarType:(InfobarType)infobarType {
  MobileMessagesBadgeState badgeState =
      badgeButton.accepted ? MobileMessagesBadgeState::Active
                           : MobileMessagesBadgeState::Inactive;

  InfobarMetricsRecorder* metricsRecorder =
      [[InfobarMetricsRecorder alloc] initWithType:infobarType];
  [metricsRecorder recordBadgeTappedInState:badgeState];

  switch (badgeState) {
    case MobileMessagesBadgeState::Active:
      base::RecordAction(
          base::UserMetricsAction("MobileMessagesBadgeAcceptedTapped"));
      break;
    case MobileMessagesBadgeState::Inactive:
      base::RecordAction(
          base::UserMetricsAction("MobileMessagesBadgeNonAcceptedTapped"));
      break;
  }
}

@end
