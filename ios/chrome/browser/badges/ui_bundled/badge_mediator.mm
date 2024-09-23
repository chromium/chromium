// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/badges/ui_bundled/badge_mediator.h"

#import <map>

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/infobars/model/badge_state.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper_delegate.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/model/infobar_type.h"
#import "ios/chrome/browser/infobars/model/overlays/default_infobar_overlay_request_factory.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/model/overlays/infobar_overlay_util.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter.h"
#import "ios/chrome/browser/overlays/model/public/overlay_presenter_observer_bridge.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_button.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_consumer.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_static_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_tappable_item.h"
#import "ios/chrome/browser/badges/ui_bundled/badge_type_util.h"
#import "ios/web/public/permissions/permissions.h"
#import "ios/web/public/web_state_observer_bridge.h"

namespace {
// Historgram name for when an overflow badge was tapped.
const char kInfobarOverflowBadgeTappedUserAction[] =
    "MobileMessagesOverflowBadgeTapped";
// Histogram name for when the overflow badge is shown
const char kInfobarOverflowBadgeShownUserAction[] =
    "MobileMessagesOverflowBadgeShown";

}  // namespace

@interface BadgeMediator () <CRWWebStateObserver,
                             InfobarBadgeTabHelperDelegate,
                             OverlayPresenterObserving,
                             WebStateListObserving> {
  std::unique_ptr<OverlayPresenterObserver> _overlayPresenterObserver;
  std::unique_ptr<WebStateListObserver> _webStateListObserver;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserver;
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
@property(nonatomic, strong, readonly) NSArray<id<BadgeItem>>* badges;

// The correct badge type for permissions infobar.
@property(nonatomic, assign, readonly) BadgeType permissionsBadgeType;

@end

@implementation BadgeMediator

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                    overlayPresenter:(OverlayPresenter*)overlayPresenter
                         isIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    // Create the incognito badge if `browser` is off-the-record.
    if (isIncognito) {
      _offTheRecordBadge =
          [[BadgeStaticItem alloc] initWithBadgeType:kBadgeTypeIncognito];
    }
    // Set up the OverlayPresenterObserver for the infobar banner presentation.
    _overlayPresenterObserver =
        std::make_unique<OverlayPresenterObserverBridge>(self);
    _overlayPresenter = overlayPresenter;
    _overlayPresenter->AddObserver(_overlayPresenterObserver.get());
    // Set up the WebStateList and its observer.
    _webStateList = webStateList;
    _webState = _webStateList->GetActiveWebState();

    _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
    _webStateList->AddObserver(_webStateListObserver.get());
    _webStateObserver = std::make_unique<web::WebStateObserverBridge>(self);

    if (_webState) {
      InfobarBadgeTabHelper::GetOrCreateForWebState(_webState)->SetDelegate(
          self);
      _webState->AddObserver(_webStateObserver.get());
    }
  }
  return self;
}

- (void)dealloc {
  // `-disconnect` must be called before deallocation.
  DCHECK(!_webStateList);
}

- (void)disconnect {
  self.consumer = nil;
  [self disconnectWebState];
  [self disconnectWebStateList];
  [self disconnectOverlayPresenter];
}

#pragma mark - Disconnect helpers

- (void)disconnectWebState {
  if (self.webState) {
    self.webState = nullptr;
    _webStateObserver = nullptr;
  }
}

- (void)disconnectWebStateList {
  if (_webStateList) {
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

- (NSArray<id<BadgeItem>>*)badges {
  if (!self.badgeTabHelper)
    return [NSArray array];

  NSMutableArray<id<BadgeItem>>* badges = [NSMutableArray array];
  std::map<InfobarType, BadgeState> badgeStatesForInfobarType =
      self.badgeTabHelper->GetInfobarBadgeStates();
  for (auto& infobarTypeBadgeStatePair : badgeStatesForInfobarType) {
    BadgeType badgeType =
        BadgeTypeForInfobarType(infobarTypeBadgeStatePair.first);
    // Update BadgeType for permissions to align with current permission states
    // of the web state.
    if (infobarTypeBadgeStatePair.first ==
        InfobarType::kInfobarTypePermissions) {
      badgeType = self.permissionsBadgeType;
    }
    BadgeTappableItem* item =
        [[BadgeTappableItem alloc] initWithBadgeType:badgeType];
    item.badgeState = infobarTypeBadgeStatePair.second;
    [badges addObject:item];
  }
  return badges;
}

- (void)setConsumer:(id<BadgeConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  [self updateConsumer];
}

- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState)
    return;
  if (_webState) {
    InfobarBadgeTabHelper::GetOrCreateForWebState(_webState)->SetDelegate(nil);
    _webState->RemoveObserver(_webStateObserver.get());
  }
  _webState = webState;
  if (_webState) {
    InfobarBadgeTabHelper::GetOrCreateForWebState(_webState)->SetDelegate(self);
    _webState->AddObserver(_webStateObserver.get());
  }
  [self updateConsumer];
}

- (InfobarBadgeTabHelper*)badgeTabHelper {
  return self.webState
             ? InfobarBadgeTabHelper::GetOrCreateForWebState(self.webState)
             : nullptr;
}

- (BadgeType)permissionsBadgeType {
  DCHECK(self.webState != nullptr);
  NSDictionary<NSNumber*, NSNumber*>* permissionStates =
      self.webState->GetStatesForAllPermissions();
  return permissionStates[@(web::PermissionMicrophone)].unsignedIntValue >
                 permissionStates[@(web::PermissionCamera)].unsignedIntValue
             ? kBadgeTypePermissionsMicrophone
             : kBadgeTypePermissionsCamera;
}

#pragma mark - Accessor helpers

// Updates the consumer for the current active WebState.
- (void)updateConsumer {
  if (!self.consumer)
    return;
  NSArray<id<BadgeItem>>* badges = self.badges;

  BOOL shouldDisplayOverflowBadge = badges.count > 1;
  id<BadgeItem> displayedBadge = nil;
  if (shouldDisplayOverflowBadge) {
    displayedBadge =
        [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypeOverflow];
  } else {
    displayedBadge = [badges firstObject];
  }
  // Update the consumer with the new badge items.
  [self.consumer setupWithDisplayedBadge:displayedBadge
                         fullScreenBadge:self.offTheRecordBadge];
}

#pragma mark - BadgeDelegate

- (NSArray<NSNumber*>*)badgeTypesForOverflowMenu {
  NSMutableArray<NSNumber*>* badgeTypes = [NSMutableArray array];
  for (id<BadgeItem> badgeItem in self.badges) {
    [badgeTypes addObject:@(badgeItem.badgeType)];
  }
  return badgeTypes;
}

- (void)passwordsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK(badgeButton.badgeType == kBadgeTypePasswordSave ||
         badgeButton.badgeType == kBadgeTypePasswordUpdate);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)saveAddressProfileBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, kBadgeTypeSaveAddressProfile);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)saveCardBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, kBadgeTypeSaveCard);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)translateBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, kBadgeTypeTranslate);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)permissionsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(InfobarTypeForBadgeType(badgeButton.badgeType),
            InfobarType::kInfobarTypePermissions);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)overflowBadgeButtonTapped:(id)sender {
  // Log overflow badge tap.
  base::RecordAction(
      base::UserMetricsAction(kInfobarOverflowBadgeTappedUserAction));
  [self updateConsumerReadStatus];
}

- (void)parcelTrackingBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::apple::ObjCCastStrict<BadgeButton>(sender);
  DCHECK_EQ(badgeButton.badgeType, kBadgeTypeParcelTracking);

  [self handleTappedBadgeButton:badgeButton];
}

- (void)showModalForBadgeType:(BadgeType)badgeType {
  [self addModalRequestForInfobarType:InfobarTypeForBadgeType(badgeType)];
}

#pragma mark - InfobarBadgeTabHelperDelegate

- (BOOL)badgeSupportedForInfobarType:(InfobarType)infobarType {
  return BadgeTypeForInfobarType(infobarType) != kBadgeTypeNone;
}

- (void)updateBadgesShownForWebState:(web::WebState*)webState {
  if (webState != self.webStateList->GetActiveWebState()) {
    // Don't update badges if the update request is not coming from the
    // currently active WebState.
    return;
  }
  NSArray<id<BadgeItem>>* badges = self.badges;

  // The badge to be displayed alongside the fullscreen badge. Logic below
  // currently assigns it to the last non-fullscreen badge in the list, since it
  // works if there is only one non-fullscreen badge. Otherwise, where there are
  // multiple non-fullscreen badges, additional logic below determines what
  // badge will be shown.
  id<BadgeItem> displayedBadge;
  // The badge that is current displaying its banner. This will be set as the
  // displayedBadge if there are multiple badges.
  id<BadgeItem> presentingBadge;

  for (id<BadgeItem> item in badges) {
    if (item.badgeState & BadgeStatePresented) {
      presentingBadge = item;
    }
    displayedBadge = item;
  }

  // Figure out what displayedBadge should be showing if there are multiple
  // non-Fullscreen badges.
  NSInteger count = [badges count];
  if (count > 1) {
    // If a badge's banner is being presented, then show that badge as the
    // displayed badge. Otherwise, show the overflow badge.
    displayedBadge =
        presentingBadge
            ? presentingBadge
            : [[BadgeTappableItem alloc] initWithBadgeType:kBadgeTypeOverflow];
  } else if (count == 1) {
    // Since there is only one non-fullscreen badge, it will be fixed as the
    // displayed badge, so mark it as read.
    [self onBadgeItemRead:displayedBadge];
  }

  if (displayedBadge.badgeType == kBadgeTypeOverflow) {
    // Log that the overflow badge is being shown.
    base::RecordAction(
        base::UserMetricsAction(kInfobarOverflowBadgeShownUserAction));
  }

  InfoBarIOS* infoBar = nullptr;
  if (displayedBadge.badgeType == kBadgeTypeSaveAddressProfile) {
    infoBar = [self
        infobarWithType:InfobarTypeForBadgeType(displayedBadge.badgeType)];
  }

  [self.consumer updateDisplayedBadge:displayedBadge
                      fullScreenBadge:self.offTheRecordBadge
                              infoBar:infoBar];
  [self updateConsumerReadStatus];
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

#pragma mark - WebStateListObserving

- (void)didChangeWebStateList:(WebStateList*)webStateList
                       change:(const WebStateListChange&)change
                       status:(const WebStateListStatus&)status {
  DCHECK_EQ(self.webStateList, webStateList);
  if (status.active_web_state_change()) {
    self.webState = status.new_active_web_state;
  }
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState
    didChangeStateForPermission:(web::Permission)permission {
  DCHECK_EQ(webState, self.webState);
  [self updateBadgesShownForWebState:webState];
}

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(webState, self.webState);
  [self disconnectWebState];
}

#pragma mark - Private

// Mark the `item`'s infobar type's read status to YES.
- (void)onBadgeItemRead:(id<BadgeItem>)item {
  item.badgeState |= BadgeStateRead;
  if (self.badgeTabHelper) {
    self.badgeTabHelper->UpdateBadgeForInfobarRead(
        InfobarTypeForBadgeType(item.badgeType));
  }
}

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

// Shows the modal UI when `button` is tapped.
- (void)handleTappedBadgeButton:(BadgeButton*)button {
  InfobarType infobarType = InfobarTypeForBadgeType(button.badgeType);
  [self addModalRequestForInfobarType:infobarType];
  [self recordMetricsForBadgeButton:button infobarType:infobarType];
}

// Adds a modal request for the Infobar of `infobarType`.
- (void)addModalRequestForInfobarType:(InfobarType)infobarType {
  DCHECK(self.webState);
  InfoBarIOS* infobar = [self infobarWithType:infobarType];
  DCHECK(infobar);
  if (infobar) {
    InfobarOverlayRequestInserter::CreateForWebState(
        self.webState, &DefaultInfobarOverlayRequestFactory);
    InsertParams params(infobar);
    params.overlay_type = InfobarOverlayType::kModal;
    params.insertion_index = OverlayRequestQueue::FromWebState(
                                 self.webState, OverlayModality::kInfobarModal)
                                 ->size();
    params.source = InfobarOverlayInsertionSource::kBadge;
    InfobarOverlayRequestInserter::FromWebState(self.webState)
        ->InsertOverlayRequest(params);
  }
}

// Returns the infobar in the active WebState's InfoBarManager with `type`.
- (InfoBarIOS*)infobarWithType:(InfobarType)type {
  InfoBarManagerImpl* manager = InfoBarManagerImpl::FromWebState(self.webState);
  const auto it = base::ranges::find(
      manager->infobars(), type, [](const infobars::InfoBar* infobar) {
        return static_cast<const InfoBarIOS*>(infobar)->infobar_type();
      });
  return it != manager->infobars().cend() ? static_cast<InfoBarIOS*>(*it)
                                          : nullptr;
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
