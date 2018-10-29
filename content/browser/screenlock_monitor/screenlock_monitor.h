// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_H_
#define CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "content/common/content_export.h"
#include "content/public/browser/screenlock_observer.h"

namespace content {

class ScreenlockMonitorSource;

// A class used to monitor the screenlock state change and notify the observers
// about the change event.
// Currently available on Win/MacOSX/ChromOS only.
// TODO(crbug.com/887585): Keep studying for a solid way to do this on Linux.
class CONTENT_EXPORT ScreenlockMonitor {
 public:
  ScreenlockMonitor(std::unique_ptr<ScreenlockMonitorSource> source);
  ~ScreenlockMonitor();

  // Get the process-wide ScreenlockMonitor (if not present, returns NULL).
  static ScreenlockMonitor* Get();

  // Add and remove an observer.
  // Can be called from any thread. |observer| is notified on the sequence
  // from which it was registered.
  // Must not be called from within a notification callback.
  void AddObserver(ScreenlockObserver* observer);
  void RemoveObserver(ScreenlockObserver* observer);

 private:
  friend class ScreenlockMonitorSource;

  void NotifyScreenLocked();
  void NotifyScreenUnlocked();

  scoped_refptr<base::ObserverListThreadSafe<ScreenlockObserver>> observers_;
  std::unique_ptr<ScreenlockMonitorSource> source_;

  DISALLOW_COPY_AND_ASSIGN(ScreenlockMonitor);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREENLOCK_MONITOR_SCREENLOCK_MONITOR_H_