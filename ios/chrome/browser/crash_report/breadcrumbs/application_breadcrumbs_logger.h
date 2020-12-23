// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_

#import <UIKit/UIKit.h>

#include <memory>

#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/user_metrics.h"

namespace base {
class TimeTicks;
}  // namespace base

class BreadcrumbManager;
class BreadcrumbPersistentStorageManager;

// Name of event logged when device orientation is changed.
extern const char kBreadcrumbOrientation[];

// Listens for and logs application wide breadcrumb events to the
// BreadcrumbManager passed in the constructor.
class ApplicationBreadcrumbsLogger {
 public:
  explicit ApplicationBreadcrumbsLogger(BreadcrumbManager* breadcrumb_manager);
  ~ApplicationBreadcrumbsLogger();

  // Sets a BreadcrumbPersistentStorageManager to persist application breadcrumb
  // events logged by this ApplicationBreadcrumbsLogger instance.
  void SetPersistentStorageManager(
      std::unique_ptr<BreadcrumbPersistentStorageManager>
          persistent_storage_manager);

  // Returns a pointer to the BreadcrumbPersistentStorageManager owned by this
  // instance. May be null.
  BreadcrumbPersistentStorageManager* GetPersistentStorageManager() const;

 private:
  ApplicationBreadcrumbsLogger(const ApplicationBreadcrumbsLogger&) = delete;

  // Callback which processes and logs the user action |action| to
  // |breadcrumb_manager_|.
  void OnUserAction(const std::string& action, base::TimeTicks action_time);

  // Callback which processes and logs memory pressure warnings to
  // |breadcrumb_manager_|.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Returns true if |action| (UMA User Action) is user triggered.
  static bool IsUserTriggeredAction(const std::string& action);

  // The BreadcrumbManager to log events.
  BreadcrumbManager* breadcrumb_manager_;
  // The callback invoked whenever a user action is registered.
  base::ActionCallback user_action_callback_;
  // A memory pressure listener which observes memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  // Observes device orientation.
  id<NSObject> orientation_observer_;

  // A strong pointer to the persistent breadcrumb manager listening for events
  // from |breadcrumb_manager_| to store to disk.
  std::unique_ptr<BreadcrumbPersistentStorageManager>
      persistent_storage_manager_;

  // Used to avoid logging the same orientation twice.
  base::Optional<UIDeviceOrientation> last_orientation_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_
