// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECK_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECK_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"

// Objective-C protocol mirroring IOSChromePasswordCheckManager::Observer.
@protocol PasswordCheckObserver <NSObject>

// Notifies delegate about a new state of password check. Mirroring
// IOSChromePasswordCheckManager::Observer::PasswordCheckStatusChanged.
- (void)passwordCheckStateDidChange:(PasswordCheckState)state;

// Notifies delegate about a change in insecure credentials. Mirroring
// IOSChromePasswordCheckManager::Observer::InsecureCredentialsChanged.
- (void)insecureCredentialsDidChange;

// Notifies the observer that the Password Check Manager has begun shutting
// down. Observers should reset their `PasswordCheckObserverBridge` observation
// when this happens.
- (void)passwordCheckManagerWillShutdown;

@end

// Simple observer bridge that forwards all events to its delegate observer.
class PasswordCheckObserverBridge
    : public IOSChromePasswordCheckManager::Observer {
 public:
  PasswordCheckObserverBridge(id<PasswordCheckObserver> delegate,
                              IOSChromePasswordCheckManager* manager);
  ~PasswordCheckObserverBridge() override;

  void PasswordCheckStatusChanged(PasswordCheckState state) override;
  void InsecureCredentialsChanged() override;
  void ManagerWillShutdown(
      IOSChromePasswordCheckManager* password_check_manager) override;

 private:
  __weak id<PasswordCheckObserver> delegate_ = nil;

  base::ScopedObservation<IOSChromePasswordCheckManager,
                          IOSChromePasswordCheckManager::Observer>
      password_check_manager_observation_{this};

  raw_ptr<IOSChromePasswordCheckManager> password_check_manager_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_CHECK_OBSERVER_BRIDGE_H_
