// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_blink_platform_impl.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {

constexpr int kBundledCodeCacheResourceId = 42;

class TestContentRendererClient : public ContentRendererClient {
 public:
  std::optional<int> GetWebUIBundledCodeCacheResourceId(
      const GURL& webui_resource_url) const override {
    if (webui_resource_url == GURL("chrome://resources/js/util.js")) {
      return kBundledCodeCacheResourceId;
    }
    return std::nullopt;
  }
};

class RendererBlinkPlatformImplTest : public testing::Test {
 protected:
  void SetUp() override {
    // Disable renderer sandbox initialization in unit tests.
    command_line_.GetProcessCommandLine()->AppendSwitch(
        switches::kSingleProcess);
    old_renderer_client_ = SetRendererClientForTesting(&renderer_client_);
  }

  void TearDown() override {
    SetRendererClientForTesting(old_renderer_client_.ExtractAsDangling());
  }

 private:
  base::test::ScopedCommandLine command_line_;
  TestContentRendererClient renderer_client_;
  raw_ptr<ContentRendererClient> old_renderer_client_ = nullptr;
};

TEST_F(RendererBlinkPlatformImplTest, GetWebUIBundledCodeCacheResourceId) {
  RendererBlinkPlatformImpl platform(/*main_thread_scheduler=*/nullptr,
                                     /*io_thread_task_runner=*/nullptr);
  const GURL cached_url("chrome://resources/js/util.js");
  const GURL uncached_url("chrome://settings/settings.js");

  // Neither feature nor `--disallow-v8-feature-flag-overrides` switch enabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(features::kWebUIBundledCodeCache);
    EXPECT_EQ(std::nullopt,
              platform.GetWebUIBundledCodeCacheResourceId(cached_url));
  }

  // Feature enabled without `--disallow-v8-feature-flag-overrides` switch.
  {
    base::test::ScopedFeatureList features(features::kWebUIBundledCodeCache);
    EXPECT_EQ(std::nullopt,
              platform.GetWebUIBundledCodeCacheResourceId(cached_url));
  }

  // `--disallow-v8-feature-flag-overrides` switch present with feature
  // disabled.
  {
    base::test::ScopedFeatureList features;
    features.InitAndDisableFeature(features::kWebUIBundledCodeCache);
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kDisallowV8FeatureFlagOverrides);
    EXPECT_EQ(std::nullopt,
              platform.GetWebUIBundledCodeCacheResourceId(cached_url));
  }

  // Both feature and `--disallow-v8-feature-flag-overrides` switch enabled.
  {
    base::test::ScopedFeatureList features(features::kWebUIBundledCodeCache);
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitch(
        switches::kDisallowV8FeatureFlagOverrides);
    EXPECT_EQ(kBundledCodeCacheResourceId,
              platform.GetWebUIBundledCodeCacheResourceId(cached_url));
    EXPECT_EQ(std::nullopt,
              platform.GetWebUIBundledCodeCacheResourceId(uncached_url));
  }
}

}  // namespace
}  // namespace content
