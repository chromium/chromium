// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_H_

namespace attribution_reporting {
class OsSource;
class OsTrigger;
}  // namespace attribution_reporting

namespace content {

// Interface between the browser's Attribution Reporting implementation and the
// operating system's.
class OsLevelAttributionManager {
 public:
  virtual ~OsLevelAttributionManager() = default;

  // Registers the given source with the OS.
  virtual void RegisterSource(attribution_reporting::OsSource) = 0;

  // Registers the given trigger with the OS.
  virtual void RegisterTrigger(attribution_reporting::OsTrigger) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_H_
