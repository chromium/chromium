// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_STORE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_STORE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "components/password_manager/core/browser/password_store/password_store_interface.h"

// Protocol to observe changes on the Password Store.
@protocol PasswordStoreObserver <NSObject>

// Called when the logins in the Password Store are changed.
- (void)loginsDidChangeInStore:(password_manager::PasswordStoreInterface*)store;

@end

// Objective-C bridge to observe changes in the Password Store.
class PasswordStoreObserverBridge
    : public password_manager::PasswordStoreInterface::Observer {
 public:
  explicit PasswordStoreObserverBridge(id<PasswordStoreObserver> observer);

 private:
  PasswordStoreObserverBridge(const PasswordStoreObserverBridge&) = delete;
  PasswordStoreObserverBridge& operator=(const PasswordStoreObserverBridge&) =
      delete;

  // PasswordStoreInterface::Observer:
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  __weak id<PasswordStoreObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_STORE_OBSERVER_BRIDGE_H_
