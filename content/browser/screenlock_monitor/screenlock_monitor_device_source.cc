// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/screenlock_monitor/screenlock_monitor_device_source.h"

namespace content {

ScreenlockMonitorDeviceSource::ScreenlockMonitorDeviceSource() {
#if defined(OS_MACOSX)
  StartListeningForScreenlock();
#endif  // OS_MACOSX
}

ScreenlockMonitorDeviceSource::~ScreenlockMonitorDeviceSource() {
#if defined(OS_MACOSX)
  StopListeningForScreenlock();
#endif  // OS_MACOSX
}

}  // namespace content
