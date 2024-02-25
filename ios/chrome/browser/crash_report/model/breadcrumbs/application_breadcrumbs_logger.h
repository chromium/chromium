// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_

#import <UIKit/UIKit.h>

#include <optional>

#include "components/breadcrumbs/core/application_breadcrumbs_logger.h"

namespace base {
class FilePath;
}

// Name of event logged when device orientation is changed.
extern const char kBreadcrumbOrientation[];

// Listens for and logs application-wide breadcrumb events to the
// BreadcrumbManager. Includes iOS-specific events such as device orientation.
class ApplicationBreadcrumbsLogger
    : public breadcrumbs::ApplicationBreadcrumbsLogger {
 public:
  explicit ApplicationBreadcrumbsLogger(const base::FilePath& storage_dir);
  ApplicationBreadcrumbsLogger(const ApplicationBreadcrumbsLogger&) = delete;
  ApplicationBreadcrumbsLogger& operator=(const ApplicationBreadcrumbsLogger&) =
      delete;
  ~ApplicationBreadcrumbsLogger();

 private:
  // Observes device orientation.
  id<NSObject> orientation_observer_;

  // Used to avoid logging the same orientation twice.
  std::optional<UIDeviceOrientation> last_orientation_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_H_
