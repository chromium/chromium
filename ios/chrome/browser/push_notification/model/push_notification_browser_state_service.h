// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_BROWSER_STATE_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_BROWSER_STATE_SERVICE_H_

#import "base/files/file_path.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"

// This is a KeyedService that encapsulates the push notification functionality
// that is coupled with a user profile.
class PushNotificationBrowserStateService
    : public KeyedService,
      public signin::IdentityManager::Observer {
 public:
  PushNotificationBrowserStateService(signin::IdentityManager* identity_manager,
                                      base::FilePath browser_state_path);
  ~PushNotificationBrowserStateService() override;

  // KeyedService
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  // This object notifies the PushNotificationProfileService of the signin and
  // signout events.
  signin::IdentityManager* const identity_manager_;
  // The path of the browser state with which the
  // PushNotificationBrowserStateService instance is associated.
  const base::FilePath browser_state_path_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_BROWSER_STATE_SERVICE_H_
