// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_STORE_OBSERVER_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_STORE_OBSERVER_BRIDGE_H_

#import <Foundation/Foundation.h>


#include "components/password_manager/core/browser/password_store.h"

// Protocol to observe changes on the Password Store.
@protocol PasswordStoreObserver <NSObject>

// Called when the logins in the Password Store are changed.
- (void)loginsDidChange;

@end

// Objective-C bridge to observe changes in the Password Store.
class PasswordStoreObserverBridge
    : public password_manager::PasswordStore::Observer {
 public:
  explicit PasswordStoreObserverBridge(id<PasswordStoreObserver> observer);

 private:
  PasswordStoreObserverBridge(const PasswordStoreObserverBridge&) = delete;
  PasswordStoreObserverBridge& operator=(const PasswordStoreObserverBridge&) =
      delete;

  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;
  __weak id<PasswordStoreObserver> observer_ = nil;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_PASSWORD_STORE_OBSERVER_BRIDGE_H_
