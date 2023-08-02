// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/net/cookie_notification_bridge.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/location.h"
#import "ios/net/cookies/cookie_store_ios.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"

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
  web::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&net::CookieStoreIOS::NotifySystemCookiesChanged));
}

}  // namespace web
