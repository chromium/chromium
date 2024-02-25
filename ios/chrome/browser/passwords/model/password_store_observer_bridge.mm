// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_store_observer_bridge.h"

PasswordStoreObserverBridge::PasswordStoreObserverBridge(
    id<PasswordStoreObserver> observer)
    : observer_(observer) {}

void PasswordStoreObserverBridge::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& /*changes*/) {
  [observer_ loginsDidChangeInStore:store];
}

void PasswordStoreObserverBridge::OnLoginsRetained(
    password_manager::PasswordStoreInterface* store,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
  [observer_ loginsDidChangeInStore:store];
}
