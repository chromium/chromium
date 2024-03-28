// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_
#define COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_

#include "components/webapps/browser/installable/installable_metrics.h"

namespace webapps {

class PwaInstallPathTracker {
 public:
  PwaInstallPathTracker();
  PwaInstallPathTracker& operator=(const PwaInstallPathTracker&) = delete;
  PwaInstallPathTracker(const PwaInstallPathTracker&) = delete;
  virtual ~PwaInstallPathTracker();

  // Keeps track of what install path was used to install a PWA. Note that these
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum class InstallPathMetric {
    // Unabled to determine install path.
    kUnknownMetric = 0,
    // The Ambient Badge was shown and used to trigger install via the install
    // dialog.
    kAmbientInfobar = 1,
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

  // Tracks the route taken to an install of a PWA (whether the bottom sheet
  // was shown or the infobar/install) and what triggered it (install source).
  void TrackInstallPath(bool bottom_sheet, WebappInstallSource install_source);

  // Resets the tracker (forgets previously recorder events).
  void Reset();

  // Gets the metric for the current install path, if available, or
  // kUnknownMetric otherwise.
  InstallPathMetric GetInstallPathMetric();

 private:
  // The source that initiated the install, for example: App menu, API or
  // ambient badge.
  WebappInstallSource install_source_ = WebappInstallSource::COUNT;

  // Whether the bottom sheet install UI was shown or the infobar/install modal.
  bool bottom_sheet_ = false;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_PWA_INSTALL_PATH_TRACKER_H_
