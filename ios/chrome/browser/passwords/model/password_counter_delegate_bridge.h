// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_COUNTER_DELEGATE_BRIDGE_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_COUNTER_DELEGATE_BRIDGE_H_

#import "components/password_manager/core/browser/password_counter.h"

// Protocol to be notified when number of passwords in the store changes.
@protocol PasswordCounterObserver <NSObject>

- (void)passwordCounterChanged:(size_t)totalPasswords;

@end

class PasswordCounterDelegateBridge
    : public password_manager::PasswordCounter::Delegate {
 public:
  explicit PasswordCounterDelegateBridge(
      id<PasswordCounterObserver> observer,
      password_manager::PasswordStoreInterface* profile_store,
      password_manager::PasswordStoreInterface* account_store);
  PasswordCounterDelegateBridge(const PasswordCounterDelegateBridge&) = delete;
  PasswordCounterDelegateBridge& operator=(
      const PasswordCounterDelegateBridge&) = delete;

  // PasswordCounter::Delegate:
  void OnPasswordCounterChanged() override;

 private:
  __weak id<PasswordCounterObserver> observer_ = nil;
  password_manager::PasswordCounter counter_;
};

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_PASSWORD_COUNTER_DELEGATE_BRIDGE_H_
