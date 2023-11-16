// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/power/activity_reporter_delegate_ash.h"

#include "ui/base/user_activity/user_activity_detector.h"

namespace extensions {

ActivityReporterDelegateAsh::ActivityReporterDelegateAsh() = default;

ActivityReporterDelegateAsh::~ActivityReporterDelegateAsh() = default;

std::optional<std::string> ActivityReporterDelegateAsh::ReportActivity() const {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  return std::nullopt;
}

}  // namespace extensions
