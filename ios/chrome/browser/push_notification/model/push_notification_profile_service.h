// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_H_
#define IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_H_

#import "base/files/file_path.h"
#import "base/memory/raw_ptr.h"
#import "components/keyed_service/core/keyed_service.h"
#import "components/signin/public/identity_manager/identity_manager.h"

// This is a KeyedService that encapsulates the push notification functionality
// that is coupled with a user profile.
class PushNotificationProfileService
    : public KeyedService,
      public signin::IdentityManager::Observer {
 public:
  PushNotificationProfileService(signin::IdentityManager* identity_manager,
                                 base::FilePath profile_state_path);
  ~PushNotificationProfileService() override;

  // KeyedService
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  // This object notifies the PushNotificationProfileService of the signin and
  // signout events.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The path of the profile with which the PushNotificationProfileService
  // instance is associated.
  const base::FilePath profile_state_path_;
};

#endif  // IOS_CHROME_BROWSER_PUSH_NOTIFICATION_MODEL_PUSH_NOTIFICATION_PROFILE_SERVICE_H_
