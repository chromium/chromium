// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"

PasswordCheckObserverBridge::PasswordCheckObserverBridge(
    id<PasswordCheckObserver> delegate,
    IOSChromePasswordCheckManager* manager)
    : delegate_(delegate) {
  DCHECK(manager);
  password_check_manager_observation_.Observe(manager);
}

PasswordCheckObserverBridge::~PasswordCheckObserverBridge() = default;

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
