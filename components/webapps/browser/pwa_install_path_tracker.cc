// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/pwa_install_path_tracker.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace webapps {

void PwaInstallPathTracker::TrackInstallPath(
    bool bottom_sheet,
    WebappInstallSource install_source) {
  base::UmaHistogramEnumeration(
      "WebApk.Install.PathToInstall",
      GetInstallPathMetric(bottom_sheet, install_source));
}

PwaInstallPathTracker::InstallPathMetric
PwaInstallPathTracker::GetInstallPathMetric(
    bool bottom_sheet,
    WebappInstallSource install_source) {
  if (bottom_sheet) {
    switch (install_source) {
      case WebappInstallSource::MENU_BROWSER_TAB:
      case WebappInstallSource::MENU_CUSTOM_TAB:
        return InstallPathMetric::kAppMenuBottomSheet;
      case WebappInstallSource::API_BROWSER_TAB:
      case WebappInstallSource::API_CUSTOM_TAB:
        return InstallPathMetric::kApiInitiatedBottomSheet;
      case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
      case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
        return InstallPathMetric::kAmbientBottomSheet;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  } else {
    switch (install_source) {
      case WebappInstallSource::MENU_BROWSER_TAB:
      case WebappInstallSource::MENU_CUSTOM_TAB:
        return InstallPathMetric::kAppMenuInstall;
      case WebappInstallSource::API_BROWSER_TAB:
      case WebappInstallSource::API_CUSTOM_TAB:
        return InstallPathMetric::kApiInitiatedInstall;
      case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
      case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
        return InstallPathMetric::kAmbientMessage;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  return InstallPathMetric::kUnknownMetric;
}

}  // namespace webapps
