// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_MANAGER_H_

#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>
#import <memory>
#import <unordered_map>

#import "ios/chrome/browser/push_notification/model/push_notification_client.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"

class PushNotificationClient;

// A PushNotificationClientManager maintains a list of push notification enabled
// features. The PushNotificationClientManger's purpose is to associate and
// delegate push notifications and its processing logic to the features that own
// the notification. The PushNotificationClientManager routes each notification
// to its appropriate PushNotificationClient based on the incoming
// notification's `push_notification_client_id` property.
// TODO(crbug.com/325254943): Inject a profile to pass in to clients.
class PushNotificationClientManager {
 public:
  PushNotificationClientManager();
  ~PushNotificationClientManager();

  // This function dynamically adds a mapping between a PushNotificationClientId
  // and a PushNotificationClient to the PushNotificationClientManager.
  void AddPushNotificationClient(
      std::unique_ptr<PushNotificationClient> client);

  // This function removes the mapping between a PushNotificationClientId and a
  // PushNotificationClient from the manager.
  void RemovePushNotificationClient(PushNotificationClientId client_id);

  // This function returns a list of the PushNotificationClients stored by the
  // manager.
  std::vector<const PushNotificationClient*> GetPushNotificationClients();

  // This function is called when the user interacts with the delivered
  // notification. This function identifies and delegates the interacted with
  // notification to the appropriate PushNotificationClient.
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response);

  // When a push notification is sent from the server and delivered to the
  // device, UIApplicationDelegate::didReceiveRemoteNotification is invoked.
  // During that invocation, this function is called. This function identifies
  // and delegates the delivered notification to the appropriate
  // PushNotificationClient.
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* user_info);

  // Actionable Notifications are push notifications that provide the user
  // with predetermined actions that the user can select to manipulate the
  // application without ever entering the application. This function register
  // each PushNotificationClient's Actionable Notifications. To ensure that the
  // notifications are properly registered, this function must invoked
  // during application startup.
  void RegisterActionableNotifications();

  // This function returns a list of `PushNotificationClientId` for the features
  // that support push notifications.
  static std::vector<PushNotificationClientId> GetClients();

  // This function returns a the `client_id`'s string representation which is
  // used to store the client's push notification permission settings in the
  // pref service and preference key on the push notification server.
  static std::string PushNotificationClientIdToString(
      PushNotificationClientId client_id);

  // Signals to client manager that a browser with scene level
  // SceneActivationLevelForegroundActive is ready. Without this
  // URL opening code driven by push notifications may not be able to
  // access a browser appropriate for opening a URL (active & scene
  // level SceneActivationLevelForegroundActive) resulting in the URL
  // not being opened.
  void OnSceneActiveForegroundBrowserReady();

 private:
  using ClientMap = std::unordered_map<PushNotificationClientId,
                                       std::unique_ptr<PushNotificationClient>>;
  // A map of client ids to the features that support push notifications.
  ClientMap clients_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_MANAGER_H_
