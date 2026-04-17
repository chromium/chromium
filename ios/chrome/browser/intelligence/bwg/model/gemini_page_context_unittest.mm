// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"

#import <memory>

#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

class GeminiPageContextTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    page_context_ = [[GeminiPageContext alloc] init];
  }

  void TearDown() override {
    page_context_ = nil;
    PlatformTest::TearDown();
  }

  GeminiPageContext* page_context_;
};

// Tests that the initial state of GeminiPageContext is correct.
TEST_F(GeminiPageContextTest, InitialState) {
  EXPECT_NE(page_context_, nil);
  EXPECT_EQ(page_context_.uniquePageContext, nullptr);
  EXPECT_EQ(page_context_.geminiPageContextComputationState,
            ios::provider::GeminiPageContextComputationState::kUnknown);
  EXPECT_EQ(page_context_.geminiPageContextAttachmentState,
            ios::provider::GeminiPageContextAttachmentState::kUnknown);
  EXPECT_EQ(page_context_.favicon, nil);
}

// Tests setting and getting uniquePageContext.
TEST_F(GeminiPageContextTest, SetUniquePageContext) {
  auto proto_page_context =
      std::make_unique<optimization_guide::proto::PageContext>();

  page_context_.uniquePageContext = std::move(proto_page_context);

  std::unique_ptr<optimization_guide::proto::PageContext> retrieved_context =
      page_context_.uniquePageContext;

  EXPECT_NE(retrieved_context, nullptr);

  // Subsequent calls should return nullptr.
  EXPECT_EQ(page_context_.uniquePageContext, nullptr);
}

// Tests setting and getting states and favicon.
TEST_F(GeminiPageContextTest, SetStatesAndFavicon) {
  page_context_.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kSuccess;
  page_context_.geminiPageContextAttachmentState =
      ios::provider::GeminiPageContextAttachmentState::kAttached;

  UIImage* test_image = [[UIImage alloc] init];
  page_context_.favicon = test_image;

  EXPECT_EQ(page_context_.geminiPageContextComputationState,
            ios::provider::GeminiPageContextComputationState::kSuccess);
  EXPECT_EQ(page_context_.geminiPageContextAttachmentState,
            ios::provider::GeminiPageContextAttachmentState::kAttached);
  EXPECT_EQ(page_context_.favicon, test_image);
}

}  // namespace
