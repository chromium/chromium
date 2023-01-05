// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/push_notification/push_notification_service.h"

#import "base/strings/string_number_conversions.h"
#import "base/values.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/push_notification/push_notification_account_context_manager.h"
#import "ios/chrome/browser/push_notification/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/push_notification_client_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PushNotificationService::PushNotificationService()
    : client_manager_(std::make_unique<PushNotificationClientManager>()) {}

PushNotificationService::~PushNotificationService() = default;

PushNotificationClientManager*
PushNotificationService::GetPushNotificationClientManager() {
  return client_manager_.get();
}

PushNotificationAccountContext* PushNotificationService::GetAccountContext(
    NSString* account_id) {
  return context_manager_.contextMap[account_id];
}

void PushNotificationService::InitializeAccountContextManager(
    ios::ChromeBrowserStateManager* manager) {
  context_manager_ = [[PushNotificationAccountContextManager alloc]
      initWithChromeBrowserStateManager:manager];
}

void PushNotificationService::RegisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ addAccount:account_id]) {
    SetAccountsToDevice(context_manager_.contextMap.allKeys,
                        completion_handler);
  }
}

void PushNotificationService::UnregisterAccount(
    NSString* account_id,
    CompletionHandler completion_handler) {
  if ([context_manager_ removeAccount:account_id]) {
    SetAccountsToDevice(context_manager_.contextMap.allKeys,
                        completion_handler);
  }
}

void PushNotificationService::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  base::Value::Dict feature_push_notification_permission = base::Value::Dict();
  std::vector<PushNotificationClientId> clients =
      PushNotificationClientManager::GetClients();
  for (PushNotificationClientId client_id : clients) {
    feature_push_notification_permission.Set(
        base::NumberToString(static_cast<int>(client_id)), false);
  }
  registry->RegisterDictionaryPref(
      prefs::kFeaturePushNotificationPermissions,
      std::move(feature_push_notification_permission));
}
