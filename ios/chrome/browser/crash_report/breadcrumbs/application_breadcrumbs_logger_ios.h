// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_IOS_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_IOS_H_

#import <UIKit/UIKit.h>

#include "components/breadcrumbs/core/application_breadcrumbs_logger.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Name of event logged when device orientation is changed.
extern const char kBreadcrumbOrientation[];

// Listens for and logs application-wide breadcrumb events to the
// BreadcrumbManager passed in the constructor. Includes iOS-specific events
// such as device orientation.
class ApplicationBreadcrumbsLoggerIOS
    : public breadcrumbs::ApplicationBreadcrumbsLogger {
 public:
  explicit ApplicationBreadcrumbsLoggerIOS(
      breadcrumbs::BreadcrumbManager* breadcrumb_manager);
  ApplicationBreadcrumbsLoggerIOS(const ApplicationBreadcrumbsLoggerIOS&) =
      delete;
  ~ApplicationBreadcrumbsLoggerIOS();

 private:
  // Observes device orientation.
  id<NSObject> orientation_observer_;

  // Used to avoid logging the same orientation twice.
  absl::optional<UIDeviceOrientation> last_orientation_;
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_APPLICATION_BREADCRUMBS_LOGGER_IOS_H_
