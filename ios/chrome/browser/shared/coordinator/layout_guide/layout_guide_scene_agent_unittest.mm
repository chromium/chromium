// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_scene_agent.h"

#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "testing/platform_test.h"

namespace {

typedef PlatformTest LayoutGuideSceneAgentTest;

// Checks that the layout guide scene agent has layout guide centers.
TEST_F(LayoutGuideSceneAgentTest, Init) {
  LayoutGuideSceneAgent* agent = [[LayoutGuideSceneAgent alloc] init];

  EXPECT_NE(agent.sceneLayoutGuideCenter, nil);
  EXPECT_NE(agent.regularLayoutGuideCenter, nil);
  EXPECT_NE(agent.incognitoLayoutGuideCenter, nil);

  EXPECT_EQ(agent.regularLayoutGuideCenter.parent,
            agent.sceneLayoutGuideCenter);
  EXPECT_EQ(agent.incognitoLayoutGuideCenter.parent,
            agent.sceneLayoutGuideCenter);
}

}  // anonymous namespace
