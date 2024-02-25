// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_SCOPED_NOTIFICATION_AUTH_SWIZZLER_H_
#define IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_SCOPED_NOTIFICATION_AUTH_SWIZZLER_H_

#import <memory>

class EarlGreyScopedBlockSwizzler;

// Class that allows swizzling the UNNotificationCenter's authorization status
// and authorization request response.
class ScopedNotificationAuthSwizzler {
 public:
  // Swizzles the auth status to return `initial_status`. If the app requests
  // authorization, the status will change to "authorized" if `grant` is YES or
  // to "denied" if `grant` is NO.
  ScopedNotificationAuthSwizzler(UNAuthorizationStatus initial_status,
                                 BOOL grant);

  // Swizzles an initial value of "Not Determined", and allows / disallows
  // granting authorization when requested.
  ScopedNotificationAuthSwizzler(BOOL grant);

  ~ScopedNotificationAuthSwizzler();

 protected:
  UNAuthorizationStatus status_;
  std::unique_ptr<EarlGreyScopedBlockSwizzler> status_swizzler_;
  std::unique_ptr<EarlGreyScopedBlockSwizzler> request_swizzler_;
};

#endif  // IOS_CHROME_BROWSER_UI_PUSH_NOTIFICATION_SCOPED_NOTIFICATION_AUTH_SWIZZLER_H_
