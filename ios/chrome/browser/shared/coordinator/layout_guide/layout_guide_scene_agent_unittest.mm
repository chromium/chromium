// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"

#import "testing/platform_test.h"

namespace {

typedef PlatformTest LayoutGuideSceneAgentTest;

// Checks that the layout guide scene agent has a layout guide center.
TEST_F(LayoutGuideSceneAgentTest, Init) {
  LayoutGuideSceneAgent* agent = [[LayoutGuideSceneAgent alloc] init];

  EXPECT_NE(agent.layoutGuideCenter, nil);
}

}  // anonymous namespace
