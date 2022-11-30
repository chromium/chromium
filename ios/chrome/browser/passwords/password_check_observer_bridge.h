// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CHECK_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CHECK_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "base/scoped_observation.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"

// Objective-C protocol mirroring IOSChromePasswordCheckManager::Observer.
@protocol PasswordCheckObserver <NSObject>

// Notifies delegate about a new state of password check. Mirroring
// IOSChromePasswordCheckManager::Observer::PasswordCheckStatusChanged.
- (void)passwordCheckStateDidChange:(PasswordCheckState)state;

// Notifies delegate about a change in a compromised credentials. Mirroring
// IOSChromePasswordCheckManager::Observer::CompromisedCredentialsChanged.
- (void)compromisedCredentialsDidChange;
@end

// Simple observer bridge that forwards all events to its delegate observer.
class PasswordCheckObserverBridge
    : public IOSChromePasswordCheckManager::Observer {
 public:
  PasswordCheckObserverBridge(id<PasswordCheckObserver> delegate,
                              IOSChromePasswordCheckManager* manager);
  ~PasswordCheckObserverBridge() override;

  void PasswordCheckStatusChanged(PasswordCheckState state) override;
  void CompromisedCredentialsChanged() override;

 private:
  __weak id<PasswordCheckObserver> delegate_ = nil;
  base::ScopedObservation<IOSChromePasswordCheckManager,
                          IOSChromePasswordCheckManager::Observer>
      password_check_manager_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_CHECK_OBSERVER_BRIDGE_H_
