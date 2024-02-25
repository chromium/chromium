// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>
#import <optional>

#import "components/prefs/pref_change_registrar.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client.h"

enum class TipsNotificationType;
class PrefRegistrySimple;

// A notification client responsible for registering notification requests and
// handling the receiving of user notifications that are user-ed "Tips".
class TipsNotificationClient : public PushNotificationClient {
 public:
  TipsNotificationClient();
  ~TipsNotificationClient() override;

  // Override PushNotificationClient::
  void HandleNotificationInteraction(
      UNNotificationResponse* notification_response) override;
  UIBackgroundFetchResult HandleNotificationReception(
      NSDictionary<NSString*, id>* notification) override;
  NSArray<UNNotificationCategory*>* RegisterActionableNotifications() override;
  void OnSceneActiveForegroundBrowserReady() override;

  // Called when the scene becomes "active foreground" and the browser is
  // ready. The closure will be called when all async operations are done.
  void OnSceneActiveForegroundBrowserReady(base::OnceClosure closure);

  // Handles a tips notification interaction by opening the appropriate UI.
  void HandleNotificationInteraction(TipsNotificationType type);

  // Registers local state prefs used to store state.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

 private:
  // Callback type used with `GetPendingRequest`.
  using GetPendingRequestCallback =
      base::OnceCallback<void(UNNotificationRequest*)>;

  // Calls the completion block with a pending request if there is one, or nil
  // if there isn't one.
  void GetPendingRequest(GetPendingRequestCallback callback);

  // Clears any existing request(s) and requests a new notification if the
  // conditions are right.
  void ClearAndMaybeRequestNotification(base::OnceClosure callback);

  // Clears any previously requested notification(s), and calls `completion`.
  void ClearNotification(base::OnceClosure callback);
  void OnNotificationCleared(UNNotificationRequest* request);

  // Request a new tips notification, if the conditions are right (i.e. the
  // user has opted-in, etc).
  void MaybeRequestNotification();

  // Request a notification of the given `type`.
  void RequestNotification(TipsNotificationType type);
  void OnNotificationRequested(TipsNotificationType type, NSError* error);

  // Returns true if a notification of the given `type` should be sent.
  bool ShouldSendNotification(TipsNotificationType type);

  // Returns true if a Signin notification should be sent.
  bool ShouldSendSignin();

  // Returns true if a WhatsNew notification should be sent.
  bool ShouldSendWhatsNew();

  // Returns `true` if there is foreground active browser.
  bool IsSceneLevelForegroundActive();

  // Helpers to handle notification interactions.
  void ShowDefaultBrowserPromo();
  void ShowWhatsNew();
  void ShowSignin();

  // Helpers to store state in local state prefs.
  void MarkNotificationTypeSent(TipsNotificationType type);
  void MarkNotificationTypeNotSent(TipsNotificationType type);

  // Returns true if Tips notifications are permitted.
  bool IsPermitted();

  // Called when the pref that stores whether Tips notifications are permitted
  // changes.
  void OnPermittedPrefChanged(const std::string& name);

  // Stores whether Tips notifications are permitted.
  bool permitted_ = false;

  // When the user interacts with a Tips notification but there are no
  // foreground scenes, this will store the notification type so it can
  // be handled when there is a foreground scene.
  std::optional<TipsNotificationType> interacted_type_;

  // Observes changes to permitted pref.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to assert that asynchronous callback are invoked on the correct
  // sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TipsNotificationClient> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_TIPS_NOTIFICATIONS_MODEL_TIPS_NOTIFICATION_CLIENT_H_
