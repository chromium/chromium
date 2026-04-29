// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_startup_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@protocol BWGGatewayProtocol <NSObject>
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
  EXPECT_EQ(config_.gateway, nil);
}

// Tests that the properties of `GeminiStartupConfiguration` can be correctly
// assigned and retrieved.
TEST_F(GeminiStartupConfigurationTest, TestProperties) {
  const id<BWGGatewayProtocol> mock_gateway =
      OCMProtocolMock(@protocol(BWGGatewayProtocol));
  AuthenticationService* const dummy_auth_service =
      reinterpret_cast<AuthenticationService*>(0x1234);

  config_.gateway = mock_gateway;
  config_.authService = dummy_auth_service;

  EXPECT_EQ(config_.gateway, mock_gateway);
  EXPECT_EQ(config_.authService, dummy_auth_service);
}

// Tests that the weak reference to the `gateway` property is zeroed out
// when the referenced object is deallocated.
TEST_F(GeminiStartupConfigurationTest, TestWeakGatewayReference) {
  @autoreleasepool {
    id<BWGGatewayProtocol> mock_gateway =
        OCMProtocolMock(@protocol(BWGGatewayProtocol));
    config_.gateway = mock_gateway;
    EXPECT_EQ(config_.gateway, mock_gateway);
  }

  EXPECT_EQ(config_.gateway, nil);
}

}  // namespace
