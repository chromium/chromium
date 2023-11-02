// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/pwa_install_path_tracker.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"

namespace webapps {

namespace {

void LogHistogram(PwaInstallPathTracker::InstallPathMetric metric) {
  base::UmaHistogramEnumeration("WebApk.Install.PathToInstall", metric);
}

}  // anonymous namespace

PwaInstallPathTracker::PwaInstallPathTracker() = default;

PwaInstallPathTracker::~PwaInstallPathTracker() = default;

void PwaInstallPathTracker::TrackInstallPath(
    bool bottom_sheet,
    WebappInstallSource install_source) {
  bottom_sheet_ = bottom_sheet;
  install_source_ = install_source;
  PwaInstallPathTracker::InstallPathMetric metric = GetInstallPathMetric();
  if (metric != InstallPathMetric::kUnknownMetric)
    LogHistogram(metric);
}

void PwaInstallPathTracker::TrackIphWasShown() {
  iph_was_shown_ = true;
}

PwaInstallPathTracker::InstallPathMetric
PwaInstallPathTracker::GetInstallPathMetric() {
  if (bottom_sheet_) {
    switch (install_source_) {
      case WebappInstallSource::MENU_BROWSER_TAB:
      case WebappInstallSource::MENU_CUSTOM_TAB:
        return iph_was_shown_ ? InstallPathMetric::kAppMenuBottomSheetWithIph
                              : InstallPathMetric::kAppMenuBottomSheet;
      case WebappInstallSource::API_BROWSER_TAB:
      case WebappInstallSource::API_CUSTOM_TAB:
        return iph_was_shown_
                   ? InstallPathMetric::kApiInitiatedBottomSheetWithIph
                   : InstallPathMetric::kApiInitiatedBottomSheet;
      case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
      case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
      case WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
        return iph_was_shown_ ? InstallPathMetric::kAmbientBottomSheetWithIph
                              : InstallPathMetric::kAmbientBottomSheet;
      default:
        NOTREACHED();
        break;
    }
  } else {
    switch (install_source_) {
      case WebappInstallSource::MENU_BROWSER_TAB:
      case WebappInstallSource::MENU_CUSTOM_TAB:
        return iph_was_shown_ ? InstallPathMetric::kAppMenuInstallWithIph
                              : InstallPathMetric::kAppMenuInstall;
      case WebappInstallSource::API_BROWSER_TAB:
      case WebappInstallSource::API_CUSTOM_TAB:
        return iph_was_shown_ ? InstallPathMetric::kApiInitiatedInstallWithIph
                              : InstallPathMetric::kApiInitiatedInstall;
      case WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB:
      case WebappInstallSource::AMBIENT_BADGE_CUSTOM_TAB:
      case WebappInstallSource::RICH_INSTALL_UI_WEBLAYER:
        return iph_was_shown_ ? InstallPathMetric::kAmbientInfobarWithIph
                              : InstallPathMetric::kAmbientInfobar;
      default:
        NOTREACHED();
        break;
    }
  }

  return InstallPathMetric::kUnknownMetric;
}

void PwaInstallPathTracker::Reset() {
  install_source_ = WebappInstallSource::COUNT;
  bottom_sheet_ = false;
  iph_was_shown_ = false;
}

}  // namespace webapps
