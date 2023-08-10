// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_NET_COOKIES_COOKIE_NOTIFICATION_OBSERVER_H_
#define IOS_NET_COOKIES_COOKIE_NOTIFICATION_OBSERVER_H_

#import "base/observer_list_types.h"

// Observer for changes on `NSHTTPCookieStorage sharedHTTPCookieStorage`.
class CookieNotificationObserver : public base::CheckedObserver {
 public:
  explicit CookieNotificationObserver() = default;
  ~CookieNotificationObserver() override;
  // Called when any cookie is added, deleted or changed in
  // `NSHTTPCookieStorage sharedHTTPCookieStorage`.
  virtual void OnSystemCookiesChanged() = 0;
};

#endif  // IOS_NET_COOKIES_COOKIE_NOTIFICATION_OBSERVER_H_
