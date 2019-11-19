// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper.h"
#include "ios/chrome/browser/infobars/infobar_badge_tab_helper_delegate.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/browser/ui/badges/badge_static_item.h"
#import "ios/chrome/browser/ui/badges/badge_tappable_item.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The number of Fullscreen badges
const int kNumberOfFullScrenBadges = 1;
// The minimum number of non-Fullscreen badges to display the overflow popup
// menu.
const int kMinimumNonFullScreenBadgesForOverflow = 2;
}

@interface BadgeMediator () <InfobarBadgeTabHelperDelegate,
                             WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// Array of all available badges.
@property(nonatomic, strong) NSMutableArray<id<BadgeItem>>* badges;

// The consumer of the mediator.
@property(nonatomic, weak) id<BadgeConsumer> consumer;

@end

@implementation BadgeMediator
@synthesize webStateList = _webStateList;

- (instancetype)initWithConsumer:(id<BadgeConsumer>)consumer
                    webStateList:(WebStateList*)webStateList {
  self = [super init];
  if (self) {
    _consumer = consumer;
    _webStateList = webStateList;
    web::WebState* activeWebState = webStateList->GetActiveWebState();
    if (activeWebState) {
      [self updateNewWebState:activeWebState withWebStateList:webStateList];
    }
    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
  }
  return self;
}

- (void)dealloc {
  [self disconnect];
}

- (void)disconnect {
  if (_webStateList) {
    _webStateList->RemoveObserver(_webStateListObserver.get());
    _webStateListObserver.reset();
    _webStateList = nullptr;
  }
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (void)addInfobarBadge:(id<BadgeItem>)badgeItem {
  if (!self.badges) {
    self.badges = [NSMutableArray array];
  }
  [self.badges addObject:badgeItem];
  [self updateBadgesShown];
}

- (void)removeInfobarBadge:(id<BadgeItem>)badgeItem {
  for (id<BadgeItem> item in self.badges) {
    if (item.badgeType == badgeItem.badgeType) {
      [self.badges removeObject:item];
      [self updateBadgesShown];
      return;
    }
  }
}

- (void)updateInfobarBadge:(id<BadgeItem>)badgeItem {
  for (id<BadgeItem> item in self.badges) {
    if (item.badgeType == badgeItem.badgeType) {
      item.badgeState = badgeItem.badgeState;
      [self updateBadgesShown];
      return;
    }
  }
}

#pragma mark - BadgeDelegate

- (void)passwordsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK(badgeButton.badgeType == BadgeType::kBadgeTypePasswordSave ||
         badgeButton.badgeType == BadgeType::kBadgeTypePasswordUpdate);

  if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordSave) {
    [self.dispatcher displayModalInfobar:InfobarType::kInfobarTypePasswordSave];
    [self recordMetricsForBadgeButton:badgeButton
                          infobarType:InfobarType::kInfobarTypePasswordSave];
  } else if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordUpdate) {
    [self.dispatcher
        displayModalInfobar:InfobarType::kInfobarTypePasswordUpdate];
    [self recordMetricsForBadgeButton:badgeButton
                          infobarType:InfobarType::kInfobarTypePasswordUpdate];
  }
}

- (void)saveCardBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, BadgeType::kBadgeTypeSaveCard);

  [self.dispatcher displayModalInfobar:InfobarType::kInfobarTypeSaveCard];
  [self recordMetricsForBadgeButton:badgeButton
                        infobarType:InfobarType::kInfobarTypeSaveCard];
}

- (void)translateBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, BadgeType::kBadgeTypeTranslate);

  [self.dispatcher displayModalInfobar:InfobarType::kInfobarTypeTranslate];
  [self recordMetricsForBadgeButton:badgeButton
                        infobarType:InfobarType::kInfobarTypeTranslate];
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
  [self.dispatcher displayPopupMenuWithBadgeItems:popupMenuBadges];
  [self updateConsumerReadStatus];
  // TODO(crbug.com/976901): Add metric for this action.
}

#pragma mark - WebStateListObserver

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  if (newWebState && newWebState == webStateList->GetActiveWebState()) {
    [self updateNewWebState:newWebState withWebStateList:webStateList];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  // Only attempt to retrieve badges if there is a new current web state, since
  // |newWebState| can be null.
  if (newWebState) {
    [self updateNewWebState:newWebState withWebStateList:webStateList];
  }
}

#pragma mark - Helpers

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

#pragma mark - Private

// Directs consumer to update read status depending on the state of the
// non-fullscreen badges.
- (void)updateConsumerReadStatus {
  for (id<BadgeItem> item in self.badges) {
    if (!item.fullScreen && !(item.badgeState & BadgeStateRead)) {
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
  // The fullscreen badge to show. There currently should only be one fullscreen
  // badge at a given time.
  id<BadgeItem> fullScreenBadge;
  // The badge that is current displaying its banner. This will be set as the
  // displayedBadge if there are multiple badges.
  id<BadgeItem> presentingBadge;
  for (id<BadgeItem> item in self.badges) {
    if (item.fullScreen) {
      fullScreenBadge = item;
    } else {
      if (item.badgeState & BadgeStatePresented) {
        presentingBadge = item;
      }
      displayedBadge = item;
    }
  }

  // Figure out what displayedBadge should be showing if there are multiple
  // non-Fullscreen badges.
  NSInteger count = [self.badges count];
  if (fullScreenBadge) {
    count -= kNumberOfFullScrenBadges;
  }
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
  [self.consumer updateDisplayedBadge:displayedBadge
                      fullScreenBadge:fullScreenBadge];
  [self updateConsumerReadStatus];
}

- (void)updateNewWebState:(web::WebState*)newWebState
         withWebStateList:(WebStateList*)webStateList {
  DCHECK_EQ(_webStateList, webStateList);
  InfobarBadgeTabHelper* infobarBadgeTabHelper =
      InfobarBadgeTabHelper::FromWebState(newWebState);
  DCHECK(infobarBadgeTabHelper);
  infobarBadgeTabHelper->SetDelegate(self);
  // Whenever the WebState changes ask the corresponding
  // InfobarBadgeTabHelper for all the badges for that WebState.
  std::vector<id<BadgeItem>> infobarBadges =
      infobarBadgeTabHelper->GetInfobarBadgeItems();
  if (infobarBadges.size() == 0) {
    // If there are no Infobar badges, then just set |badges| to nil since
    // &infobarBadges[0] will fail.
    self.badges = nil;
  } else {
    // Copy all contents of vector into array.
    self.badges = [NSMutableArray arrayWithObjects:&infobarBadges[0]
                                             count:infobarBadges.size()];
  }
  id<BadgeItem> displayedBadge;
  if ([self.badges count] > 1) {
    // Show the overflow menu badge when there are multiple badges.
    displayedBadge = [[BadgeTappableItem alloc]
        initWithBadgeType:BadgeType::kBadgeTypeOverflow];
  } else if ([self.badges count] == 1) {
    displayedBadge = [self.badges lastObject];
  }
  id<BadgeItem> fullScreenBadge;
  if (newWebState->GetBrowserState()->IsOffTheRecord()) {
    BadgeStaticItem* incognitoItem = [[BadgeStaticItem alloc]
        initWithBadgeType:BadgeType::kBadgeTypeIncognito];
    fullScreenBadge = incognitoItem;
    // Keep track of presence of an incognito badge so that the mediator knows
    // whether or not there is a fullscreen badge when calling
    // updateDisplayedBadge:fullScreenBadge:.
    [self.badges addObject:incognitoItem];
  }
  [self.consumer setupWithDisplayedBadge:displayedBadge
                         fullScreenBadge:fullScreenBadge];
}

@end
