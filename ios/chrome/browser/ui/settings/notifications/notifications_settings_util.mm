// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/notifications/notifications_settings_util.h"

#import "components/commerce/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace notifications_settings {

ClientPermissionState GetNotificationPermissionState(
    const std::string& gaia_id,
    PrefService* pref_service) {
  static std::vector<PushNotificationClientId> client_ids =
      PushNotificationClientManager::GetClients();
  size_t enabled_clients_count = 0;
  size_t disabled_clients_count = 0;
  size_t indeterminant_clients_count = 0;

  for (PushNotificationClientId client_id : client_ids) {
    ClientPermissionState permission_state =
        GetClientPermissionState(client_id, gaia_id, pref_service);
    if (permission_state == ClientPermissionState::ENABLED) {
      enabled_clients_count++;
    } else if (permission_state == ClientPermissionState::DISABLED) {
      disabled_clients_count++;
    } else {
      indeterminant_clients_count++;
    }
  }

  if (!disabled_clients_count && !indeterminant_clients_count) {
    return ClientPermissionState::ENABLED;
  }

  if (!enabled_clients_count && !indeterminant_clients_count) {
    return ClientPermissionState::DISABLED;
  }

  return ClientPermissionState::INDETERMINANT;
}

ClientPermissionState GetClientPermissionState(
    PushNotificationClientId client_id,
    const std::string& gaia_id,
    PrefService* pref_service) {
  switch (client_id) {
    case PushNotificationClientId::kCommerce: {
      BOOL mobile_notifications =
          GetMobileNotificationPermissionStatusForClient(client_id, gaia_id);
      BOOL email_notifications =
          pref_service->GetBoolean(commerce::kPriceEmailNotificationsEnabled);

      if (mobile_notifications && email_notifications) {
        return ClientPermissionState::ENABLED;
      } else if (!mobile_notifications && !email_notifications) {
        return ClientPermissionState::DISABLED;
      }

      return ClientPermissionState::INDETERMINANT;
    }
  }
}

BOOL GetMobileNotificationPermissionStatusForClient(
    PushNotificationClientId client_id,
    const std::string& gaia_id) {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  PushNotificationAccountContextManager* manager =
      service->GetAccountContextManager();

  return [manager isPushNotificationEnabledForClient:client_id
                                          forAccount:gaia_id];
}

}  // namespace notifications_settings
