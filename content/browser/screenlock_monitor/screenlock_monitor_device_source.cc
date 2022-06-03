// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

namespace content {

ScreenlockMonitorDeviceSource::ScreenlockMonitorDeviceSource() {
#if defined(OS_MAC)
  StartListeningForScreenlock();
#endif  // OS_MAC
}

ScreenlockMonitorDeviceSource::~ScreenlockMonitorDeviceSource() {
#if defined(OS_MAC)
  StopListeningForScreenlock();
#endif  // OS_MAC
}

}  // namespace content
