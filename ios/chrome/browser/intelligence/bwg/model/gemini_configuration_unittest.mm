// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_configuration.h"

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class GeminiConfigurationTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    config_ = [[GeminiConfiguration alloc] init];
  }

  void TearDown() override {
    config_ = nil;
    PlatformTest::TearDown();
  }

  GeminiConfiguration* config_;
};

// Tests that uniquePageContext handles ownership transfer correctly.
TEST_F(GeminiConfigurationTest, TestUniquePageContext) {
  auto page_context =
      std::make_unique<optimization_guide::proto::PageContext>();
  page_context->set_url("https://example.com");

  config_.uniquePageContext = std::move(page_context);

  // First call should return the object and move it out.
  std::unique_ptr<optimization_guide::proto::PageContext> retrieved_context =
      config_.uniquePageContext;
  EXPECT_NE(retrieved_context, nullptr);
  EXPECT_EQ(retrieved_context->url(), "https://example.com");

  // Subsequent calls should return nullptr.
  EXPECT_EQ(config_.uniquePageContext, nullptr);
}

}  // namespace
