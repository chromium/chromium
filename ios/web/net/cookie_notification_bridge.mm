// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookie_notification_bridge.h"

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/task/post_task.h"
#import "ios/net/cookies/cookie_store_ios.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

CookieNotificationBridge::CookieNotificationBridge() {
  id<NSObject> registration = [[NSNotificationCenter defaultCenter]
      addObserverForName:NSHTTPCookieManagerCookiesChangedNotification
                  object:[NSHTTPCookieStorage sharedHTTPCookieStorage]
                   queue:nil
              usingBlock:^(NSNotification* notification) {
                OnNotificationReceived(notification);
              }];
  registration_ = registration;
}

CookieNotificationBridge::~CookieNotificationBridge() {
  [[NSNotificationCenter defaultCenter] removeObserver:registration_];
}

void CookieNotificationBridge::OnNotificationReceived(
    NSNotification* notification) {
  DCHECK([[notification name]
      isEqualToString:NSHTTPCookieManagerCookiesChangedNotification]);
  base::PostTask(
      FROM_HERE, {web::WebThread::IO},
      base::BindOnce(&net::CookieStoreIOS::NotifySystemCookiesChanged));
}

}  // namespace web
