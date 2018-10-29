// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_SOURCE_H_
#define CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_SOURCE_H_

#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

// A class to communicate screenlock state changes from each platform-specific
// sub-class source implementation to the screenlock monitor.
class CONTENT_EXPORT ScreenlockMonitorSource {
 public:
  ScreenlockMonitorSource();
  virtual ~ScreenlockMonitorSource();

  enum ScreenlockEvent { SCREEN_LOCK_EVENT, SCREEN_UNLOCK_EVENT };

  // ProcessScreenlockEvent should only be called from a single thread.
  static void ProcessScreenlockEvent(ScreenlockEvent event_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenlockMonitorSource);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_SOURCE_H_
