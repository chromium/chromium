// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_consumer.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_item_identifier.h"
#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_presenter.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"

@implementation NotificationsOptInMediator {
  // Used to check the user's primary identity and sign-in status.
  raw_ptr<AuthenticationService> _authenticationService;
  // Stores whether items are selected.
  std::map<NotificationsOptInItemIdentifier, BOOL> _selected;
}

- (instancetype)initWithAuthenticationService:
    (AuthenticationService*)authenticationService {
  if (self = [super init]) {
    _authenticationService = authenticationService;
    _selected = {{NotificationsOptInItemIdentifier::kContent, NO},
                 {NotificationsOptInItemIdentifier::kTips, NO},
                 {NotificationsOptInItemIdentifier::kPriceTracking, NO}};
  }
  return self;
}

- (void)configureConsumer {
  std::string gaiaID = base::SysNSStringToUTF8([self primaryIdentity].gaiaID);
  for (auto [item, selection] : _selected) {
    selection = push_notification_settings::
        GetMobileNotificationPermissionStatusForClient(
            [self clientIDForItem:item], gaiaID);
    [self.consumer setOptInItem:item enabled:selection];
  }
}

- (void)disableUserSelectionForItem:
    (NotificationsOptInItemIdentifier)itemIdentifier {
  _selected[itemIdentifier] = NO;
  [self.consumer setOptInItem:itemIdentifier enabled:NO];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // TODO(crbug.com/41492138): record metrics.
  std::vector<PushNotificationClientId> selectedClientIds;
  std::vector<PushNotificationClientId> deselectedClientIds;
  for (auto [item, selection] : _selected) {
    if (selection) {
      selectedClientIds.push_back([self clientIDForItem:item]);
    } else {
      deselectedClientIds.push_back([self clientIDForItem:item]);
    }
  }
  [self disableNotifications:deselectedClientIds];
  [self.presenter presentNotificationsAlertForClientIds:selectedClientIds];
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/41492138): record metrics.
  [self.presenter dismiss];
}

#pragma mark - NotificationsOptInViewControllerDelegate

- (void)selectionChangedForItemType:
            (NotificationsOptInItemIdentifier)itemIdentifier
                           selected:(BOOL)selected {
  _selected[itemIdentifier] = selected;
  // Present the sign in view if Content is selected and user is not signed in.
  if (itemIdentifier == kContent && selected && ![self primaryIdentity]) {
    [self.presenter presentSignIn];
  }
}

#pragma mark - Private

// Returns the user's primary identity, or nil if not signed in.
- (id<SystemIdentity>)primaryIdentity {
  return _authenticationService->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
}

- (PushNotificationClientId)clientIDForItem:
    (NotificationsOptInItemIdentifier)item {
  switch (item) {
    case kContent:
      return PushNotificationClientId::kContent;
    case kTips:
      return PushNotificationClientId::kTips;
    case kPriceTracking:
      return PushNotificationClientId::kCommerce;
  }
}

// Disables notifications in prefs for the clients in `clientIds`.
- (void)disableNotifications:(std::vector<PushNotificationClientId>)clientIds {
  NSString* gaiaID = [self primaryIdentity].gaiaID;
  for (PushNotificationClientId clientId : clientIds) {
    GetApplicationContext()->GetPushNotificationService()->SetPreference(
        gaiaID, clientId, false);
  }
}

@end
