// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/pwa_install_path_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace webapps {

using InstallPathMetric = PwaInstallPathTracker::InstallPathMetric;

class PwaInstallPathTrackerUnitTest : public testing::Test {};

TEST_F(PwaInstallPathTrackerUnitTest, Events) {
  PwaInstallPathTracker tracker;

  // Resetting twice should not be problem.
  tracker.Reset();
  tracker.Reset();

  // Success case: Ambient Infobar.
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAmbientInfobar, tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: App Menu initiated Infobar.
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAppMenuInstall, tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: API initiated infobar.
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kApiInitiatedInstall,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: Ambient Infobar with IPH.
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAmbientInfobarWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: App Menu initiated Infobar with IPH.
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAppMenuInstallWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: API initiated Infobar with IPH (included only for
  // completeness, as IPH should not show with deferred prompts).
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ false,
                           WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kApiInitiatedInstallWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: Ambient BottomSheet.
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAmbientBottomSheet,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: App Menu initiated BottomSheet.
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAppMenuBottomSheet,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: API initiated BottomSheet.
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kApiInitiatedBottomSheet,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: Ambient BottomSheet with IPH.
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::AMBIENT_BADGE_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAmbientBottomSheetWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: App Menu initiated BottomSheet with IPH.
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::MENU_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kAppMenuBottomSheetWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();

  // Success case: API initiated BottomSheet with IPH (included only for
  // completeness, as IPH should not show with deferred prompts).
  tracker.TrackIphWasShown();
  tracker.TrackInstallPath(/* bottom_sheet= */ true,
                           WebappInstallSource::API_BROWSER_TAB);
  EXPECT_EQ(InstallPathMetric::kApiInitiatedBottomSheetWithIph,
            tracker.GetInstallPathMetric());
  tracker.Reset();
}

}  // namespace webapps
