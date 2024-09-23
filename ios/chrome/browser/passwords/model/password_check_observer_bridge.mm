// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"

#import "base/check.h"

PasswordCheckObserverBridge::PasswordCheckObserverBridge(
    id<PasswordCheckObserver> delegate,
    IOSChromePasswordCheckManager* manager)
    : delegate_(delegate), password_check_manager_(manager) {
  CHECK(delegate_);
  CHECK(password_check_manager_);

  password_check_manager_observation_.Observe(manager);
}

PasswordCheckObserverBridge::~PasswordCheckObserverBridge() {
  if (password_check_manager_) {
    password_check_manager_->RemoveObserver(this);
  }
}

void PasswordCheckObserverBridge::PasswordCheckStatusChanged(
    PasswordCheckState status) {
  // Since password check state update can be called with delay from the
  // background thread, dispatch aync should be used to update main UI thread.
  __weak id<PasswordCheckObserver> weakDelegate = delegate_;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakDelegate passwordCheckStateDidChange:status];
  });
}

void PasswordCheckObserverBridge::InsecureCredentialsChanged() {
  [delegate_ insecureCredentialsDidChange];
}

void PasswordCheckObserverBridge::ManagerWillShutdown(
    IOSChromePasswordCheckManager* password_check_manager) {
  [delegate_ passwordCheckManagerWillShutdown];
}
