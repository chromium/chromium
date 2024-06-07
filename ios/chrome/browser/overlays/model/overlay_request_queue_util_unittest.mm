// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/model/public/overlay_request_queue_util.h"

#import "base/functional/bind.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/overlays/model/test/overlay_test_macros.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"

namespace {
// Configs used in tests.
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(FirstConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(SecondConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(ThirdConfig);
DEFINE_TEST_OVERLAY_REQUEST_CONFIG(NonMatched);

// Creates a matcher callback for ConfigType.
template <class ConfigType>
base::RepeatingCallback<bool(OverlayRequest*)> ConfigMatcher() {
  return base::BindRepeating([](OverlayRequest* request) -> bool {
    return request->GetConfig<ConfigType>();
  });
}
}  // namespace

using OverlayRequestQueueUtilTest = PlatformTest;

// Tests that the expected indices for matching configs returned.
TEST_F(OverlayRequestQueueUtilTest, MatchingConfigs) {
  // Add requests to `web_state`'s queue.
  web::FakeWebState web_state;
  OverlayRequestQueue::CreateForWebState(&web_state);
  OverlayRequestQueue* queue =
      OverlayRequestQueue::FromWebState(&web_state, OverlayModality::kTesting);
  queue->AddRequest(OverlayRequest::CreateWithConfig<FirstConfig>());
  queue->AddRequest(OverlayRequest::CreateWithConfig<SecondConfig>());
  queue->AddRequest(OverlayRequest::CreateWithConfig<ThirdConfig>());

  // Verify that the requests are found using the matchers.
  size_t index = 0;
  EXPECT_TRUE(
      GetIndexOfMatchingRequest(queue, &index, ConfigMatcher<FirstConfig>()));
  EXPECT_EQ(0U, index);
  EXPECT_TRUE(
      GetIndexOfMatchingRequest(queue, &index, ConfigMatcher<SecondConfig>()));
  EXPECT_EQ(1U, index);
  EXPECT_TRUE(
      GetIndexOfMatchingRequest(queue, &index, ConfigMatcher<ThirdConfig>()));
  EXPECT_EQ(2U, index);

  // Verify that no requests are found for NonMatched config.
  EXPECT_FALSE(
      GetIndexOfMatchingRequest(queue, &index, ConfigMatcher<NonMatched>()));
}
