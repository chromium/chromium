// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/net/cookies/cookie_notification_observer.h"

CookieNotificationObserver::~CookieNotificationObserver() {
  CHECK(!IsInObserverList())
      << "CookieNotificationObserver needs to be removed from "
         "CookieNotification observer list before their destruction.";
}
