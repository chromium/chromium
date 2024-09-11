// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_profile_service.h"

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "ios/chrome/browser/commerce/model/price_alert_util.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_service.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"

namespace {

// A helper function for removing a signed out user from the push notification
// server.
void OnPrimaryAccountCleared(CoreAccountInfo primary_account) {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();

  NSString* gaia = base::SysUTF8ToNSString(primary_account.gaia);
  service->UnregisterAccount(gaia, nullptr);
}

// A helper function for adding a signin user to the push notification server.
void OnPrimaryAccountSet(CoreAccountInfo primary_account) {
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  if (!service->DeviceTokenIsSet()) {
    [PushNotificationUtil
        registerDeviceWithAPNSWithProvisionalNotificationsAvailable:NO];
  }

  NSString* gaia = base::SysUTF8ToNSString(primary_account.gaia);
  service->RegisterAccount(gaia, nullptr);
}

}  // namespace

PushNotificationProfileService::PushNotificationProfileService(
    signin::IdentityManager* identity_manager,
    base::FilePath profile_state_path)
    : identity_manager_(identity_manager),
      profile_state_path_(profile_state_path) {
  identity_manager->AddObserver(this);
}

PushNotificationProfileService::~PushNotificationProfileService() = default;

void PushNotificationProfileService::Shutdown() {
  identity_manager_->RemoveObserver(this);
}

void PushNotificationProfileService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  // This check prevents registering/unregistering accounts with the
  // PushNotificationService when the user has not received an APNS token. The
  // user will not have an APNS token if they have not allowed push
  // notification permissions on the device. Because the initialization of the
  // PushNotificationService's account manager is dependent on the
  // initialization of the PushNotificationService with the Push Notification
  // server (which only occurs when the user has allowed push notification
  // permissions), this check must exist to avoid interacting with an
  // uninitialized account manager.
  PushNotificationService* service =
      GetApplicationContext()->GetPushNotificationService();
  if (!service->DeviceTokenIsSet()) {
    return;
  }
  const signin::ConsentLevel consent_level = signin::ConsentLevel::kSignin;

  switch (event.GetEventTypeFor(consent_level)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      // The PostTask function must be used for the OnPrimaryAccountSet to
      // ensure that the appropriate Profile has been updated with the newly
      // signed in account's gaia id. The class that is responsible for updating
      // the Profile's gaia ID is SigninBrowserStateInfoUpdater.
      //
      // Since SigninBrowserStateInfoUpdater is also a
      // signin::IdentityManager::Observer. Thus, the PostTask() function
      // guarantees that the Profile's gaia id will have been updated by the
      // time OnPrimaryAccountSet() method is invoked.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&OnPrimaryAccountSet,
                                    event.GetCurrentState().primary_account));
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&OnPrimaryAccountCleared,
                                    event.GetPreviousState().primary_account));
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone: {
      break;
    }
  }
}
