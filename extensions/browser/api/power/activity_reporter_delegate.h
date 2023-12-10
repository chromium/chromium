// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_H_

#include <memory>
#include <optional>
#include <string>

namespace extensions {

// Base class for platform dependent chrome.power.reportActivity()
// implementations.
class ActivityReporterDelegate {
 public:
  virtual ~ActivityReporterDelegate() = default;

  static std::unique_ptr<ActivityReporterDelegate> GetDelegate();
  virtual std::optional<std::string> ReportActivity() const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_POWER_ACTIVITY_REPORTER_DELEGATE_H_
