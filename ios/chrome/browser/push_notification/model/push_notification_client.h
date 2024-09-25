// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <UserNotifications/UserNotifications.h>

#import <string>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "url/gurl.h"

class CommercePushNotificationClientTest;

class Browser;

// The PushNotificationClient class is an abstract class that provides a
// framework for implementing push notification support. Feature teams that
// intend to support push notifications should create a class that inherits from
// the PushNotificationClient class.
// TODO(crbug.com/325254943): Update this class and subclasses to accept an
// injected ProfileIOS* and not internally fetch a profile via
// GetlastUsedProfile. Update tests as well.
class PushNotificationClient {
 public:
  PushNotificationClient(PushNotificationClientId client_id);
  virtual ~PushNotificationClient() = 0;

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
  PushNotificationClientId GetClientId();

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

 protected:
  // The unique string that is used to associate incoming push notifications to
  // their destination feature. This identifier must match the identifier
  // used inside the notification's payload when sending the notification to the
  // push notification server.
  PushNotificationClientId client_id_;

  // Returns an arbitrary profile amongst the currently loaded profile. This
  // means that this API is not safe when there are multiple profiles. Instead
  // the push notification system should be re-designed to not depend on this
  // method (either create specific manager per-profile, or include in the
  // notification an identifier for the profile, e.g. gaia id).
  // TODO(crbug.com/41497027): This API should be redesigned.
  ProfileIOS* GetAnyProfile();

  // Returns the first active browser found with scene level
  // SceneActivationLevelForegroundActive.
  Browser* GetSceneLevelForegroundActiveBrowser();

 private:
  friend class ::CommercePushNotificationClientTest;
  std::vector<std::pair<GURL, base::OnceCallback<void(Browser*)>>>
      urls_delayed_for_loading_;

  // Stores whether or not the feedback view controller should be shown when a
  // Browser is ready.
  bool feedback_presentation_delayed_ = false;

  // Stores which client sent the delayed feedback request.
  PushNotificationClientId feedback_presentation_delayed_client_;

  // Stores the feedback payload to be sent with the notification feedback.
  NSDictionary<NSString*, NSString*>* feedback_data_ = nil;

  // Loads a url in a new tab for a given browser.
  void LoadUrlInNewTab(const GURL& url,
                       Browser* browser,
                       base::OnceCallback<void(Browser*)> callback);
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_CLIENT_H_
