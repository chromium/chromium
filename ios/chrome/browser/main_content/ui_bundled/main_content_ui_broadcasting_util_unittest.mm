// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main_content/ui_bundled/main_content_ui_broadcasting_util.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/main_content/ui_bundled/test/fake_main_content_ui.h"
#import "ios/chrome/browser/main_content/ui_bundled/test/main_content_broadcast_test_util.h"
#import "ios/web/common/features.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class MainContentUIBroadcastingUtilTest : public PlatformTest {
 public:
  MainContentUIBroadcastingUtilTest() {
    scoped_feature_list_.InitAndDisableFeature(
        web::features::kSmoothScrollingUseDelegate);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the MainContentUIBroadcastingUtil functions successfully start
// and stop broadcasting main content properties.
TEST_F(MainContentUIBroadcastingUtilTest, StartStop) {
  FakeMainContentUI* ui = [[FakeMainContentUI alloc] init];
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, false);
  StartBroadcastingMainContentUI(ui, broadcaster);
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, true);
  StopBroadcastingMainContentUI(broadcaster);
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, false);
}
