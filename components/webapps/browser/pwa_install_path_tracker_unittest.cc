// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/pwa_install_path_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

class PwaInstallPathTrackerUnitTest : public testing::Test {};

TEST_F(PwaInstallPathTrackerUnitTest, Events) {
  PwaInstallPathTracker::InstallPathMetric metric;

  // Success case: Ambient Infobar.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ false,
      WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kAmbientMessage, metric);

  // Success case: App Menu initiated Infobar.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ false, WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kAppMenuInstall, metric);

  // Success case: API initiated infobar.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ false, WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kApiInitiatedInstall,
            metric);

  // Success case: Ambient BottomSheet.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ true, WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kAmbientBottomSheet,
            metric);

  // Success case: App Menu initiated BottomSheet.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ true, WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kAppMenuBottomSheet,
            metric);

  // Success case: API initiated BottomSheet.
  metric = PwaInstallPathTracker::GetInstallPathMetric(
      /* bottom_sheet= */ true, WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(PwaInstallPathTracker::InstallPathMetric::kApiInitiatedBottomSheet,
            metric);
}

}  // namespace webapps
