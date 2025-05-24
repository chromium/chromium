// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import <string>
#import <string_view>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "url/gurl.h"

class CommercePushNotificationClientTest;

class Browser;

// Holds the configuration information for a UNNNotificationRequest.
struct ScheduledNotificationRequest {
  NSString* identifier;
  UNNotificationContent* content;
  base::TimeDelta time_interval;
};

// The PushNotificationClient class is an abstract class that provides a
// framework for implementing push notification support. Feature teams that
// intend to support push notifications should create a class that inherits from
// the PushNotificationClient class.
class PushNotificationClient {
 public:
  // Constructor for `PushNotificationClient`s that are scoped per-Profile.
  // This constructor should be used for clients whose `scope` is implicitly
  // `PushNotificationClientScope::kPerProfile`. It is intended for use when
  // multi-Profile push notification handling is enabled (i.e.,
  // `IsMultiProfilePushNotificationHandlingEnabled()` returns YES).
  PushNotificationClient(PushNotificationClientId client_id,
                         ProfileIOS* profile);
  // Constructor for `PushNotificationClient`s that are app-scoped (i.e., not
  // tied to a specific user Profile).
  // This constructor should be used for clients where `scope` is not
  // `PushNotificationClientScope::kPerProfile` (e.g., typically
  // `PushNotificationClientScope::kAppWide`).
  PushNotificationClient(PushNotificationClientId client_id,
                         PushNotificationClientScope scope);
  virtual ~PushNotificationClient() = 0;

  // Returns true if this client can handle the given `notification`.
  virtual bool CanHandleNotification(UNNotification* notification) = 0;

  // When the user interacts with a push notification, this function is called
  // to route the user to the appropriate destination. Returns `true` if the
  // interaction was handled or `false` if it is not relevant to this client.
  virtual bool HandleNotificationInteraction(
      UNNotificationResponse* notification_response) = 0;

  // When the device receives a push notification, this function is called to
  // allow the client to process any logic needed at this point in time. The
  // function's return value represents the state of data that the
  // PushNotificationClient fetched. Returns nullopt if this client did not
  // handle the notification reception.
  virtual std::optional<UIBackgroundFetchResult> HandleNotificationReception(
      NSDictionary<NSString*, id>* user_info) = 0;

  // Actionable Notifications are push notifications that provide the user
  // with predetermined actions that the user can select to manipulate the
  // application without ever entering the application. Actionable
  // notifications must be registered during application startup.
  virtual NSArray<UNNotificationCategory*>*
  RegisterActionableNotifications() = 0;

  // Signals to the client that a browser with scene level
  // SceneActivationLevelForegroundActive is ready. Without this
  // URL opening code driven by push notifications may not be able to
  // access a browser appropriate for opening a URL (active & scene
  // level SceneActivationLevelForegroundActive) resulting in the URL
  // not being opened.
  virtual void OnSceneActiveForegroundBrowserReady();

  // Returns the feature's `client_id_`.
  PushNotificationClientId GetClientId() const;

  // Returns the feature's `client_scope_`.
  PushNotificationClientScope GetClientScope() const;

  // Loads a url in a new tab once an active browser is ready.
  // TODO(crbug.com/41497027): This API should includes an identifier of the
  // Profile that should be used to open the URL which should come from the
  // notification (maybe by including the gaia id of the associated profile).
  void LoadUrlInNewTab(const GURL& url);

  // Loads a url in a new tab once an active browser is ready. Runs `callback`
  // with the browser the URL is loaded in.
  void LoadUrlInNewTab(const GURL& url,
                       base::OnceCallback<void(Browser*)> callback);

  // Loads the feedback view controller once an active browser is ready.
  // TODO(crbug.com/41497027): This API should includes an identifier of the
  // Profile that should be used to open the URL which should come from the
  // notification (maybe by including the gaia id of the associated profile).
  void LoadFeedbackWithPayloadAndClientId(
      NSDictionary<NSString*, NSString*>* data,
      PushNotificationClientId clientId);

  // Schedules a notification `request` associated with a specific
  // `profile_name`. The `profile_name` will be embedded in the notification
  // metadata, ensuring it's routed to the correct notification client during
  // interactions. `profile_name` must not be empty. Calls `completion` upon
  // finish.
  //
  // `IsMultiProfilePushNotificationHandlingEnabled()` must return YES.
  void ScheduleProfileNotification(
      ScheduledNotificationRequest request,
      base::OnceCallback<void(NSError*)> completion,
      std::string_view profile_name);

  // Checks additional constraints before scheduling a notification `request`
  // with `completion` callback.
  void CheckRateLimitBeforeSchedulingNotification(
      ScheduledNotificationRequest request,
      base::OnceCallback<void(NSError*)> completion);

 protected:
  // The unique string that is used to associate incoming push notifications to
  // their destination feature. This identifier must match the identifier
  // used inside the notification's payload when sending the notification to the
  // push notification server.
  const PushNotificationClientId client_id_;

  // The operational scope of this client, indicating whether it's app-wide
  // or per-Profile.
  const PushNotificationClientScope client_scope_;

  // Returns the most appropriate active foreground browser based on the
  // client's scope. Encapsulates the logic for choosing between
  // Profile-specific and arbitrary browser lookups. Returns `nullptr` if no
  // suitable browser is found.
  Browser* GetActiveForegroundBrowser();

  // Returns the `ProfileIOS` associated with this client instance. Set during
  // construction, primarily for clients with `kPerProfile` scope.
  ProfileIOS* GetProfile();

 private:
  friend class ::CommercePushNotificationClientTest;

  // Pointer to the user Profile if this client is per-Profile scoped
  // (`client_scope_` is `PushNotificationClientScope::kPerProfile`).
  base::WeakPtr<ProfileIOS> profile_;

  std::vector<std::pair<GURL, base::OnceCallback<void(Browser*)>>>
      urls_delayed_for_loading_;

  // Stores whether or not the feedback view controller should be shown when a
  // Browser is ready.
  bool feedback_presentation_delayed_ = false;

  // Stores which client sent the delayed feedback request.
  PushNotificationClientId feedback_presentation_delayed_client_;

  // Stores the feedback payload to be sent with the notification feedback.
  NSDictionary<NSString*, NSString*>* feedback_data_ = nil;

  base::WeakPtrFactory<PushNotificationClient> weak_ptr_factory_{this};

  // Loads a url in a new tab for a given browser.
  void LoadUrlInNewTab(const GURL& url,
                       Browser* browser,
                       base::OnceCallback<void(Browser*)> callback);

  // Receives the result of getting all scheduled notifications as a part of
  // scheduling notification `notif_request`.
  void HandlePendingNotificationResult(
      ScheduledNotificationRequest notification,
      base::OnceCallback<void(NSError*)> completion,
      NSArray<UNNotificationRequest*>* requests);

  // Schedules a notification `request` `completion` upon finish.
  void ScheduleNotification(ScheduledNotificationRequest request,
                            base::OnceCallback<void(NSError*)> completion);

  // Creates a `UNNotificationRequest` for an app-wide notification using the
  // provided `request.content` and triggering after `request.time_interval`.
  UNNotificationRequest* CreateRequest(ScheduledNotificationRequest request);

  // Creates a `UNNotificationRequest` specific to the given `profile_name`.
  // Uses the provided `request.content` and triggers after
  // `request.time_interval`. Requires multi-profile handling to be enabled
  // (`IsMultiProfilePushNotificationHandlingEnabled()` must return YES).
  UNNotificationRequest* CreateRequestForProfile(
      ScheduledNotificationRequest request,
      std::string_view profile_name);
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_
