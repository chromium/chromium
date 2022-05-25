// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/push_notification/push_notification_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {

void InitializeConfiguration() {
  // Chromium does not initialize push notification configurations
}

void RegisterDevice() {
  // Chromium does not register devices for push notifications
}

}  // namespace provider
}  // namespace ios
