// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/push_notification/notifications_opt_in_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/consent_level.h"
#import "ios/chrome/browser/ntp/model/set_up_list_item_type.h"
#import "ios/chrome/browser/ntp/model/set_up_list_prefs.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/push_notification/metrics.h"
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
  if ((self = [super init])) {
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
        GetMobileNotificationPermissionStatusForMultipleClients(
            [self clientIDsForItem:item], gaiaID);
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
  std::vector<PushNotificationClientId> selectedClientIds;
  std::vector<PushNotificationClientId> deselectedClientIds;
  std::string gaiaID = base::SysNSStringToUTF8([self primaryIdentity].gaiaID);

  for (auto [item, selection] : _selected) {
    std::vector<PushNotificationClientId> clientIDs =
        [self clientIDsForItem:item];
    BOOL enabled = push_notification_settings::
        GetMobileNotificationPermissionStatusForMultipleClients(clientIDs,
                                                                gaiaID);
    // Only add the clientId if there has been a change from the original opt-in
    // status.
    if (selection && !enabled) {
      selectedClientIds.insert(selectedClientIds.end(), clientIDs.begin(),
                               clientIDs.end());
    } else if (!selection && enabled) {
      deselectedClientIds.insert(deselectedClientIds.end(), clientIDs.begin(),
                                 clientIDs.end());
    }
  }

  [self disableNotifications:deselectedClientIds];
  [self.presenter presentNotificationsAlertForClientIds:selectedClientIds];
  base::UmaHistogramEnumeration(
      kNotificationsOptInPromptActionHistogram,
      NotificationsOptInPromptActionType::kEnableNotificationsTapped);
}

- (void)didTapSecondaryActionButton {
  PrefService* localState = GetApplicationContext()->GetLocalState();
  set_up_list_prefs::MarkItemComplete(localState,
                                      SetUpListItemType::kNotifications);
  base::UmaHistogramEnumeration(
      kNotificationsOptInPromptActionHistogram,
      NotificationsOptInPromptActionType::kNoThanksTapped);
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

- (std::vector<PushNotificationClientId>)clientIDsForItem:
    (NotificationsOptInItemIdentifier)item {
  switch (item) {
    case kContent:
      return {PushNotificationClientId::kContent,
              PushNotificationClientId::kSports};
    case kTips:
      return {PushNotificationClientId::kTips};
    case kPriceTracking:
      return {PushNotificationClientId::kCommerce};
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
