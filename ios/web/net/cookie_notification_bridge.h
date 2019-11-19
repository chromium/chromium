// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NET_COOKIE_NOTIFICATION_BRIDGE_H_
#define IOS_WEB_NET_COOKIE_NOTIFICATION_BRIDGE_H_

#include "base/macros.h"

@protocol NSObject;
@class NSNotification;

namespace web {

// CookieNotificationBridge listens to
// NSHTTPCookieManagerCookiesChangedNotification on the posting thread and
// re-sends it to the cookie store on the IO thread.
class CookieNotificationBridge {
 public:
  CookieNotificationBridge();
  ~CookieNotificationBridge();

 private:
  static void OnNotificationReceived(NSNotification* notification);

  // Token returned by NSNotificationCenter upon registration. Owned by the
  // bridge and used to unregister from NSNotificationCenter in destructor.
  __strong id<NSObject> registration_;

  DISALLOW_COPY_AND_ASSIGN(CookieNotificationBridge);
};

}  // namespace web

#endif  // IOS_WEB_NET_COOKIE_NOTIFICATION_BRIDGE_H_
