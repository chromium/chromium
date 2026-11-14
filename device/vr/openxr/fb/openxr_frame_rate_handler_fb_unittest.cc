// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/fb/openxr_frame_rate_handler_fb.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

namespace {

// What the fake runtime reports, and the rates it was asked for.
struct FakeRuntime {
  std::vector<float> rates;
  XrResult enumerate_result = XR_SUCCESS;
  float rate = 0;
  std::vector<float> requested_rates;
};

FakeRuntime* g_runtime = nullptr;

XrResult XRAPI_PTR EnumerateDisplayRefreshRates(XrSession session,
                                                uint32_t capacity,
                                                uint32_t* count,
                                                float* rates) {
  if (XR_FAILED(g_runtime->enumerate_result)) {
    return g_runtime->enumerate_result;
  }
  *count = g_runtime->rates.size();
  if (capacity == 0) {
    return XR_SUCCESS;
  }
  if (capacity < g_runtime->rates.size()) {
    return XR_ERROR_SIZE_INSUFFICIENT;
  }
  std::ranges::copy(g_runtime->rates, rates);
  return XR_SUCCESS;
}

XrResult XRAPI_PTR GetDisplayRefreshRate(XrSession session, float* rate) {
  *rate = g_runtime->rate;
  return XR_SUCCESS;
}

XrResult XRAPI_PTR RequestDisplayRefreshRate(XrSession session, float rate) {
  g_runtime->requested_rates.push_back(rate);
  return XR_SUCCESS;
}

XrEventDataDisplayRefreshRateChangedFB RateChangeEvent(float from, float to) {
  XrEventDataDisplayRefreshRateChangedFB event{
      XR_TYPE_EVENT_DATA_DISPLAY_REFRESH_RATE_CHANGED_FB};
  event.fromDisplayRefreshRate = from;
  event.toDisplayRefreshRate = to;
  return event;
}

class OpenXrFrameRateHandlerFbTest : public testing::Test {
 protected:
  void SetUp() override { g_runtime = &runtime_; }
  void TearDown() override { g_runtime = nullptr; }

  std::unique_ptr<OpenXrFrameRateHandlerFb> CreateHandler() {
    return std::make_unique<OpenXrFrameRateHandlerFb>(
        OpenXrFrameRateHandlerFb::Methods{
            .enumerate_display_refresh_rates = &EnumerateDisplayRefreshRates,
            .get_display_refresh_rate = &GetDisplayRefreshRate,
            .request_display_refresh_rate = &RequestDisplayRefreshRate,
        },
        XR_NULL_HANDLE);
  }

  FakeRuntime runtime_{.rates = {72.0f, 90.0f, 120.0f}, .rate = 90.0f};
};

}  // namespace

TEST_F(OpenXrFrameRateHandlerFbTest, ReportsRuntimeRates) {
  auto handler = CreateHandler();
  EXPECT_THAT(handler->supported_frame_rates(),
              testing::ElementsAre(72.0f, 90.0f, 120.0f));
  EXPECT_EQ(handler->frame_rate(), 90.0f);
}

TEST_F(OpenXrFrameRateHandlerFbTest, DropsInvalidRates) {
  runtime_.rates = {0.0f, -90.0f, std::numeric_limits<float>::quiet_NaN(),
                    std::numeric_limits<float>::infinity(), 90.0f};
  runtime_.rate = std::numeric_limits<float>::infinity();
  auto handler = CreateHandler();
  EXPECT_THAT(handler->supported_frame_rates(), testing::ElementsAre(90.0f));
  EXPECT_FALSE(handler->frame_rate());
}

TEST_F(OpenXrFrameRateHandlerFbTest, SortsRatesWithoutDuplicates) {
  runtime_.rates = {120.0f, 72.0f, 90.0f, 72.0f};
  auto handler = CreateHandler();
  EXPECT_THAT(handler->supported_frame_rates(),
              testing::ElementsAre(72.0f, 90.0f, 120.0f));
}

TEST_F(OpenXrFrameRateHandlerFbTest, NoRatesIfEnumerationFails) {
  runtime_.enumerate_result = XR_ERROR_RUNTIME_FAILURE;
  auto handler = CreateHandler();
  EXPECT_TRUE(handler->supported_frame_rates().empty());
  EXPECT_EQ(handler->frame_rate(), 90.0f);
}

TEST_F(OpenXrFrameRateHandlerFbTest, RateChangesOnRuntimeEvent) {
  auto handler = CreateHandler();
  handler->RequestFrameRate(120.0f);
  EXPECT_THAT(runtime_.requested_rates, testing::ElementsAre(120.0f));
  // The rate only changes when the runtime's event arrives.
  EXPECT_EQ(handler->frame_rate(), 90.0f);

  handler->OnDisplayRefreshRateChanged(RateChangeEvent(90.0f, 120.0f));
  EXPECT_EQ(handler->frame_rate(), 120.0f);
}

TEST_F(OpenXrFrameRateHandlerFbTest, ReadsRateAgainWhenSessionBegins) {
  auto handler = CreateHandler();
  // Some runtimes report another rate once the session runs.
  runtime_.rate = 72.0f;
  handler->OnSessionBegin();
  EXPECT_EQ(handler->frame_rate(), 72.0f);
}

TEST_F(OpenXrFrameRateHandlerFbTest, IgnoresInvalidRateEvents) {
  auto handler = CreateHandler();
  for (float rate : {0.0f, -90.0f, std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity()}) {
    handler->OnDisplayRefreshRateChanged(RateChangeEvent(90.0f, rate));
    EXPECT_EQ(handler->frame_rate(), 90.0f);
  }
}

}  // namespace device
