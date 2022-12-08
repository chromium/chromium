// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"

#include <gtest/gtest.h>

#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class FencedFrameConfigTest : private ScopedFencedFramesForTest,
                              public testing::Test {
 public:
  FencedFrameConfigTest() : ScopedFencedFramesForTest(true) {
    enabled_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {});
  }

 private:
  base::test::ScopedFeatureList enabled_feature_list_;
};

TEST_F(FencedFrameConfigTest, FencedFrameConfigConstructionWithURL) {
  String url = "https://example.com/";
  FencedFrameConfig config(url);

  EXPECT_NE(config.url(), nullptr);
  EXPECT_FALSE(config.url()->IsOpaqueProperty());
  EXPECT_TRUE(config.url()->IsUSVString());
  EXPECT_EQ(config.url()->GetAsUSVString(), url);
  EXPECT_EQ(
      config.GetValueIgnoringVisibility<FencedFrameConfig::Attribute::kURL>(),
      url);

  EXPECT_EQ(config.width(), nullptr);
  EXPECT_EQ(config.height(), nullptr);
}

TEST_F(FencedFrameConfigTest, FencedFrameConfigCreateWithURL) {
  String url = "https://example.com/";
  FencedFrameConfig* config = FencedFrameConfig::Create(url);

  EXPECT_NE(config->url(), nullptr);
  EXPECT_FALSE(config->url()->IsOpaqueProperty());
  EXPECT_TRUE(config->url()->IsUSVString());
  EXPECT_EQ(config->url()->GetAsUSVString(), url);
  EXPECT_EQ(
      config->GetValueIgnoringVisibility<FencedFrameConfig::Attribute::kURL>(),
      url);

  EXPECT_EQ(config->width(), nullptr);
  EXPECT_EQ(config->height(), nullptr);
}

}  // namespace blink
