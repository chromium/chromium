// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_AUTO_FILL_STATUS_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_AUTO_FILL_STATUS_OBSERVER_H_

// Observer protocol informed about Chrome password auto fill status changes.
// Implements <NSObject> to make hash IDs for each instance accessible.
@protocol PasswordAutoFillStatusObserver

// Indicates that the credential store state is changed.
- (void)passwordAutoFillStatusDidChange;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_UTILS_PASSWORD_AUTO_FILL_STATUS_OBSERVER_H_
