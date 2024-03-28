// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_
#define COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_

#include "base/gtest_prod_util.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace webapps {

class PwaInstallPathTracker {
 public:
  PwaInstallPathTracker() = delete;
  PwaInstallPathTracker& operator=(const PwaInstallPathTracker&) = delete;
  PwaInstallPathTracker(const PwaInstallPathTracker&) = delete;

  // Tracks the route taken to an install of a PWA (whether the bottom sheet
  // was shown or the install message) and what triggered it (install source).
  static void TrackInstallPath(bool bottom_sheet,
                               WebappInstallSource install_source);

 private:
  FRIEND_TEST_ALL_PREFIXES(PwaInstallPathTrackerUnitTest, Events);

  // Keeps track of what install path was used to install a PWA. Note that these
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum class InstallPathMetric {
    // Unabled to determine install path.
    kUnknownMetric = 0,
    // The Ambient Badge was shown and used to trigger install via the install
    // dialog.
    kAmbientMessage = 1,
    // 'Install app' was selected in the App menu and used to trigger install
    // via the Install dialog.
    kAppMenuInstall = 2,
    // The Install dialog was shown at the request of a website and was used to
    // trigger install.
    kApiInitiatedInstall = 3,
    // The BottomSheet was shown ambiently (peeking) and used to trigger
    // install. It may or may not have been expanded before installation
    // started.
    kAmbientBottomSheet = 4,
    // The BottomSheet was shown expanded as a result of an App menu click and
    // was used to trigger install.
    kAppMenuBottomSheet = 5,
    // The BottomSheet was shown expanded at the request of a website and was
    // used to trigger install.
    kApiInitiatedBottomSheet = 6,
    // kAmbientInfobarWithIph = 7,           // Deprecated
    // kAppMenuInstallWithIph = 8,           // Deprecated
    // kApiInitiatedInstallWithIph = 9,      // Deprecated
    // kAmbientBottomSheetWithIph = 10,      // Deprecated
    // kAppMenuBottomSheetWithIph = 11,      // Deprecated
    // kApiInitiatedBottomSheetWithIph = 12, // Deprecated

    // Keeps track of the last entry
    kMaxValue = kApiInitiatedBottomSheet,
  };

  // Gets the metric for the current install path, if available, or
  // kUnknownMetric otherwise.
  static InstallPathMetric GetInstallPathMetric(
      bool bottom_sheet,
      WebappInstallSource install_source);
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_
