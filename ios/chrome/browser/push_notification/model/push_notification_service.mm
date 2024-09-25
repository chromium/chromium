// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/model/push_notification_service.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/types/cxx23_to_underlying.h"
#import "base/values.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_registry_simple.h"
#import "ios/chrome/browser/push_notification/model/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_manager.h"
#import "ios/chrome/browser/push_notification/model/push_notification_configuration.h"
#import "ios/chrome/browser/push_notification/model/push_notification_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"

PushNotificationService::PushNotificationService()
    : client_manager_(std::make_unique<PushNotificationClientManager>()) {
  context_manager_ = [[PushNotificationAccountContextManager alloc]
      initWithProfileManager:GetApplicationContext()->GetProfileManager()];
}

PushNotificationService::~PushNotificationService() = default;

PushNotificationClientManager*
PushNotificationService::GetPushNotificationClientManager() {
  return client_manager_.get();
}

PushNotificationAccountContextManager*
PushNotificationService::GetAccountContextManager() {
  return context_manager_;
}

void PushNotificationService::SetPreference(NSString* account_id,
                                            PushNotificationClientId client_id,
                                            bool enabled) {
  DCHECK(context_manager_);
  if (enabled) {
    [context_manager_
        enablePushNotification:client_id
                    forAccount:base::SysNSStringToUTF8(account_id)];
  } else {
    [context_manager_
        disablePushNotification:client_id
                     forAccount:base::SysNSStringToUTF8(account_id)];
  }
  SetPreferences(
      account_id,
      [context_manager_
          preferenceMapForAccount:base::SysNSStringToUTF8(account_id)],
      ^(NSError* error){
      });
}

void PushNotificationService::RegisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ addAccount:base::SysNSStringToUTF8(account_id)]) {
    SetAccountsToDevice([context_manager_ accountIDs], completion_handler);
  }
}

void PushNotificationService::UnregisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ removeAccount:base::SysNSStringToUTF8(account_id)]) {
    SetAccountsToDevice([context_manager_ accountIDs], completion_handler);
  }
}

// TODO(crbug.com/343495515): remove after downstream implementation is added.
std::string PushNotificationService::GetRepresentativeTargetIdForGaiaId(
    NSString* gaia_id) {
  return "";
}

void PushNotificationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kFeaturePushNotificationPermissions);
  registry->RegisterBooleanPref(prefs::kSendTabNotificationsPreviouslyDisabled,
                                false);
}

void PushNotificationService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kPushNotificationAuthorizationStatus,
      base::to_underlying(
          push_notification::SettingsAuthorizationStatus::NOTDETERMINED));
  registry->RegisterDictionaryPref(prefs::kAppLevelPushNotificationPermissions);
}

void PushNotificationService::SetPreferences(
    NSString* account_id,
    PreferenceMap preference_map,
    CompletionHandler completion_handler) {}
