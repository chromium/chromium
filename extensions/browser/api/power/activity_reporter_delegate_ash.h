// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_ASH_H_
#define EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_ASH_H_

#include "extensions/browser/api/power/activity_reporter_delegate.h"

namespace extensions {

// Ash ActivityReporterDelegate implementation.
class ActivityReporterDelegateAsh : public ActivityReporterDelegate {
 public:
  ActivityReporterDelegateAsh();
  ActivityReporterDelegateAsh(const ActivityReporterDelegateAsh&) = delete;
  ActivityReporterDelegateAsh& operator=(const ActivityReporterDelegateAsh&) =
      delete;
  ~ActivityReporterDelegateAsh() override;

  // PowerApi
  std::optional<std::string> ReportActivity() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_ASH_H_
