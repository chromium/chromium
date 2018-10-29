// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

#include <CoreFoundation/CoreFoundation.h>

namespace content {

namespace {

CFStringRef kScreenLockedEvent = CFSTR("com.apple.screenIsLocked");
CFStringRef kScreenUnlockedEvent = CFSTR("com.apple.screenIsUnlocked");

void OnScreenlockNotificationReceived(CFNotificationCenterRef center,
                                      void* observer,
                                      CFStringRef name,
                                      const void* object,
                                      CFDictionaryRef userInfo) {
  ScreenlockMonitorSource::ScreenlockEvent screenlock_event;
  if (CFEqual(name, kScreenLockedEvent)) {
    screenlock_event = ScreenlockMonitorSource::SCREEN_LOCK_EVENT;
  } else if (CFEqual(name, kScreenUnlockedEvent)) {
    screenlock_event = ScreenlockMonitorSource::SCREEN_UNLOCK_EVENT;
  } else {
    return;
  }

  ScreenlockMonitorSource::ProcessScreenlockEvent(screenlock_event);
}

}  //  namespace

void ScreenlockMonitorDeviceSource::StartListeningForScreenlock() {
  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this,
      &OnScreenlockNotificationReceived, kScreenLockedEvent, nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);

  CFNotificationCenterAddObserver(
      CFNotificationCenterGetDistributedCenter(), this,
      &OnScreenlockNotificationReceived, kScreenUnlockedEvent, nullptr,
      CFNotificationSuspensionBehaviorDeliverImmediately);
}

void ScreenlockMonitorDeviceSource::StopListeningForScreenlock() {
  CFNotificationCenterRemoveEveryObserver(
      CFNotificationCenterGetDistributedCenter(), this);
}

}  // namespace content
