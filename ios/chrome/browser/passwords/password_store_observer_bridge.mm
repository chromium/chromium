// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_store_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

PasswordStoreObserverBridge::PasswordStoreObserverBridge(
    id<PasswordStoreObserver> observer)
    : observer_(observer) {}

void PasswordStoreObserverBridge::OnLoginsChanged(
    password_manager::PasswordStoreInterface* /*store*/,
    const password_manager::PasswordStoreChangeList& /*changes*/) {
  [observer_ loginsDidChange];
}

void PasswordStoreObserverBridge::OnLoginsRetained(
    password_manager::PasswordStoreInterface* /*store*/,
    const std::vector<password_manager::PasswordForm>& /*retained_passwords*/) {
  [observer_ loginsDidChange];
}
