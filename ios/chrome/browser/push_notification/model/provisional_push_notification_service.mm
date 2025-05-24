// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/provisional_push_notification_service.h"

#import <UserNotifications/UserNotifications.h>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/functional/callback_helpers.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

namespace {

// Helper method invoked when the request to enable the provisional push
// notification completes. Invokes `success_closure` if the request was
// successfull.
void OnEnablePermissionRequestComplete(base::OnceClosure success_closure,
                                       BOOL permissions_granted,
                                       NSError* error) {
  if (!permissions_granted || error) {
    return;
  }

  std::move(success_closure).Run();
}

// Helper method invoked when the current permission has been determined.
void OnPermissionSettingsFetched(base::OnceClosure success_closure,
                                 UNNotificationSettings* settings) {
  // Only users with a "Not Determined" or "Provisional" notification status
  // are eligible for provisional notifications.
  if (settings.authorizationStatus != UNAuthorizationStatusNotDetermined &&
      settings.authorizationStatus != UNAuthorizationStatusProvisional) {
    return;
  }

  [PushNotificationUtil
      enableProvisionalPushNotificationPermission:
          base::CallbackToBlock(base::BindOnce(
              &OnEnablePermissionRequestComplete, std::move(success_closure)))];
}

}  // namespace

ProvisionalPushNotificationService::ProvisionalPushNotificationService(
    AuthenticationService* authentication_service,
    syncer::DeviceInfoSyncService* device_info_sync_service,
    PushNotificationService* push_notification_service)
    : authentication_service_(authentication_service),

      device_info_sync_service_(device_info_sync_service),
      push_notification_service_(push_notification_service) {
  CHECK(authentication_service);
  CHECK(push_notification_service_);
  CHECK(device_info_sync_service_);
}

ProvisionalPushNotificationService::~ProvisionalPushNotificationService() =
    default;

void ProvisionalPushNotificationService::EnrollUserToProvisionalNotifications(
    ClientIdState client_id_state,
    std::vector<PushNotificationClientId> client_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!authentication_service_->HasPrimaryIdentity(
          signin::ConsentLevel::kSignin)) {
    return;
  }

  // PushNotificationUtil may invoke the block on a background thread, so
  // use base::BindPostTask(...) to ensure the method is invoked on the
  // correct sequence nonetheless.
  using Self = ProvisionalPushNotificationService;
  base::OnceClosure success_closure = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(),
      base::BindOnce(&Self::OnProvisionalPushNotificationEnrolled,
                     weak_factory_.GetWeakPtr(), client_id_state,
                     std::move(client_ids)));

  [PushNotificationUtil
      getPermissionSettings:base::CallbackToBlock(
                                base::BindOnce(&OnPermissionSettingsFetched,
                                               std::move(success_closure)))];
}

void ProvisionalPushNotificationService::OnProvisionalPushNotificationEnrolled(
    ClientIdState client_id_state,
    std::vector<PushNotificationClientId> client_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  id<SystemIdentity> identity = authentication_service_->GetPrimaryIdentity(
      signin::ConsentLevel::kSignin);
  if (!identity) {
    return;
  }

  for (PushNotificationClientId client_id : client_ids) {
    push_notification_service_->SetPreference(
        identity.gaiaID, client_id, client_id_state == ClientIdState::kEnabled);
    if (client_id == PushNotificationClientId::kSendTab) {
      device_info_sync_service_->RefreshLocalDeviceInfo();
    }
  }
}
