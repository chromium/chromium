// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_LACROS_H_
#define EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_LACROS_H_

#include "extensions/browser/api/power/activity_reporter_delegate.h"

namespace extensions {

// Lacros ActivityReporterDelegate implementation.
class ActivityReporterDelegateLacros : public ActivityReporterDelegate {
 public:
  ActivityReporterDelegateLacros();
  ActivityReporterDelegateLacros(const ActivityReporterDelegateLacros&) =
      delete;
  ActivityReporterDelegateLacros& operator=(
      const ActivityReporterDelegateLacros&) = delete;
  ~ActivityReporterDelegateLacros() override;

  // PowerApi
  absl::optional<std::string> ReportActivity() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_LACROS_H_
