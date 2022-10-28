// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_IMPL_H_

#include "content/browser/attribution_reporting/os_level_attribution_manager.h"

namespace content {

class OsLevelAttributionManagerImpl : public OsLevelAttributionManager {
 public:
  OsLevelAttributionManagerImpl();

  ~OsLevelAttributionManagerImpl() override;

  OsLevelAttributionManagerImpl(const OsLevelAttributionManagerImpl&) = delete;
  OsLevelAttributionManagerImpl& operator=(
      const OsLevelAttributionManagerImpl&) = delete;

  OsLevelAttributionManagerImpl(OsLevelAttributionManagerImpl&&) = delete;
  OsLevelAttributionManagerImpl& operator=(OsLevelAttributionManagerImpl&&) =
      delete;

 private:
  // OsLevelAttributionManager:
  void RegisterSource(attribution_reporting::OsSource) override;
  void RegisterTrigger(attribution_reporting::OsTrigger) override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_OS_LEVEL_ATTRIBUTION_MANAGER_IMPL_H_
