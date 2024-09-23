// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_counter_delegate_bridge.h"

PasswordCounterDelegateBridge::PasswordCounterDelegateBridge(
    id<PasswordCounterObserver> observer,
    password_manager::PasswordStoreInterface* profile_store,
    password_manager::PasswordStoreInterface* account_store)
    : observer_(observer), counter_(profile_store, account_store, this) {}

void PasswordCounterDelegateBridge::OnPasswordCounterChanged() {
  [observer_ passwordCounterChanged:(counter_.profile_passwords() +
                                     counter_.account_passwords())];
}
