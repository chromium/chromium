// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#include "ios/chrome/browser/push_notification/model/push_notification_service.h"
#include "ios/chrome/browser/signin/model/authentication_service.h"

// A KeyedService to managed the provisional push notification.
class ProvisionalPushNotificationService : public KeyedService {
 public:
  // Represents the state to store for the client ids.
  enum class ClientIdState {
    kEnabled,
    kDisabled,
  };

  ProvisionalPushNotificationService(
      AuthenticationService* authentication_service,
      syncer::DeviceInfoSyncService* device_info_sync_service,
      PushNotificationService* push_notification_service);
  ~ProvisionalPushNotificationService() override;

  // Enrolls the user for provisional push notifications, unless notifications
  // have previously been authorized or denied.
  void EnrollUserToProvisionalNotifications(
      ClientIdState client_id_state,
      std::vector<PushNotificationClientId> client_ids);

 private:
  // Invoked when a request to enable provisional push notifications completes
  // with success.
  void OnProvisionalPushNotificationEnrolled(
      ClientIdState client_id_state,
      std::vector<PushNotificationClientId> client_ids);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<AuthenticationService> authentication_service_;
  raw_ptr<syncer::DeviceInfoSyncService> device_info_sync_service_;
  raw_ptr<PushNotificationService> push_notification_service_;

  base::WeakPtrFactory<ProvisionalPushNotificationService> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PROVISIONAL_PUSH_NOTIFICATION_SERVICE_H_
