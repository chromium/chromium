// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_startup_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol BWGLinkOpeningDelegate <NSObject>
@end

namespace {

// Test fixture for `GeminiStartupConfiguration`.
class GeminiStartupConfigurationTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    config_ = [[GeminiStartupConfiguration alloc] init];
  }

  __strong GeminiStartupConfiguration* config_;
};

// Tests the default state of `GeminiStartupConfiguration` properties.
TEST_F(GeminiStartupConfigurationTest, TestDefaultState) {
  EXPECT_EQ(config_.authService, nullptr);
  EXPECT_EQ(config_.linkOpeningHandler, nil);
  EXPECT_FALSE(config_.imageRemixEnabled);
  EXPECT_FALSE(config_.geminiLiveEnabled);
}

// Tests that the properties of `GeminiStartupConfiguration` can be correctly
// assigned and retrieved.
TEST_F(GeminiStartupConfigurationTest, TestProperties) {
  const id<BWGLinkOpeningDelegate> mock_link_opening_handler =
      OCMProtocolMock(@protocol(BWGLinkOpeningDelegate));
  AuthenticationService* const dummy_auth_service =
      reinterpret_cast<AuthenticationService*>(0x1234);

  config_.linkOpeningHandler = mock_link_opening_handler;
  config_.authService = dummy_auth_service;
  config_.imageRemixEnabled = YES;
  config_.geminiLiveEnabled = YES;

  EXPECT_EQ(config_.linkOpeningHandler, mock_link_opening_handler);
  EXPECT_EQ(config_.authService, dummy_auth_service);
  EXPECT_TRUE(config_.imageRemixEnabled);
  EXPECT_TRUE(config_.geminiLiveEnabled);
}

// Tests that the weak reference to the `linkOpeningHandler` property is zeroed
// out when the referenced object is deallocated.
TEST_F(GeminiStartupConfigurationTest, TestWeakLinkOpeningHandlerReference) {
  @autoreleasepool {
    id<BWGLinkOpeningDelegate> mock_link_opening_handler =
        OCMProtocolMock(@protocol(BWGLinkOpeningDelegate));
    config_.linkOpeningHandler = mock_link_opening_handler;
    EXPECT_EQ(config_.linkOpeningHandler, mock_link_opening_handler);
  }

  EXPECT_EQ(config_.linkOpeningHandler, nil);
}

}  // namespace
