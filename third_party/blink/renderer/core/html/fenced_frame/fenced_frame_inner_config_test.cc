// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_inner_config.h"

#include <gtest/gtest.h>

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FencedFrameInnerConfigTest : private ScopedFencedFramesForTest,
                                   public testing::Test {
 public:
  FencedFrameInnerConfigTest() : ScopedFencedFramesForTest(true) {
    enabled_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {});
  }

 private:
  base::test::ScopedFeatureList enabled_feature_list_;
};

TEST_F(FencedFrameInnerConfigTest, FencedFrameInnerConfigConstructionWithURL) {
  FencedFrameInnerConfig inner_config("https://example.com");

  EXPECT_NE(inner_config.url(), nullptr);
  EXPECT_TRUE(inner_config.url()->IsOpaqueProperty());
  EXPECT_FALSE(inner_config.url()->IsUSVString());

  EXPECT_EQ(inner_config.width(), nullptr);
  EXPECT_EQ(inner_config.height(), nullptr);
}

}  // namespace blink
