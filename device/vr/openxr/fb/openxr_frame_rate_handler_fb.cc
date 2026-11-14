// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/fb/openxr_frame_rate_handler_fb.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check.h"
#include "device/vr/openxr/openxr_extension_helper.h"

namespace device {

namespace {

// Rejects bad values from a faulty runtime, like NaN or infinity.
bool IsValidRate(float rate) {
  return std::isfinite(rate) && rate > 0;
}

}  // namespace

// static
const char* OpenXrFrameRateHandlerFb::GetExtension() {
  return XR_FB_DISPLAY_REFRESH_RATE_EXTENSION_NAME;
}

// static
bool OpenXrFrameRateHandlerFb::IsSupported(
    const OpenXrExtensionEnumeration& extension_enum) {
  return extension_enum.ExtensionSupported(GetExtension());
}

// static
std::unique_ptr<OpenXrFrameRateHandlerFb> OpenXrFrameRateHandlerFb::Create(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session) {
  if (!IsSupported(*extension_helper.ExtensionEnumeration())) {
    return nullptr;
  }
  const auto& methods = extension_helper.ExtensionMethods();
  return std::make_unique<OpenXrFrameRateHandlerFb>(
      Methods{
          .enumerate_display_refresh_rates =
              methods.xrEnumerateDisplayRefreshRatesFB,
          .get_display_refresh_rate = methods.xrGetDisplayRefreshRateFB,
          .request_display_refresh_rate = methods.xrRequestDisplayRefreshRateFB,
      },
      session);
}

OpenXrFrameRateHandlerFb::OpenXrFrameRateHandlerFb(const Methods& methods,
                                                   XrSession session)
    : methods_(methods), session_(session) {
  CHECK(methods_.enumerate_display_refresh_rates);
  CHECK(methods_.get_display_refresh_rate);
  CHECK(methods_.request_display_refresh_rate);

  ReadFrameRate();

  uint32_t count = 0;
  if (XR_FAILED(methods_.enumerate_display_refresh_rates(session_, 0, &count,
                                                         nullptr))) {
    return;
  }
  std::vector<float> rates(count);
  if (XR_FAILED(methods_.enumerate_display_refresh_rates(
          session_, rates.size(), &count, rates.data()))) {
    return;
  }
  std::erase_if(rates, [](float rate) { return !IsValidRate(rate); });
  // Runtimes may list a rate more than once.
  std::ranges::sort(rates);
  auto duplicates = std::ranges::unique(rates);
  rates.erase(duplicates.begin(), duplicates.end());
  supported_frame_rates_ = std::move(rates);
}

OpenXrFrameRateHandlerFb::~OpenXrFrameRateHandlerFb() = default;

void OpenXrFrameRateHandlerFb::RequestFrameRate(float rate) {
  CHECK(std::ranges::contains(supported_frame_rates_, rate));
  // Errors are ignored: the runtime may ignore requests anyway.
  methods_.request_display_refresh_rate(session_, rate);
}

void OpenXrFrameRateHandlerFb::OnSessionBegin() {
  ReadFrameRate();
}

void OpenXrFrameRateHandlerFb::ReadFrameRate() {
  float frame_rate = 0;
  if (XR_SUCCEEDED(methods_.get_display_refresh_rate(session_, &frame_rate)) &&
      IsValidRate(frame_rate)) {
    frame_rate_ = frame_rate;
  }
}

void OpenXrFrameRateHandlerFb::OnDisplayRefreshRateChanged(
    const XrEventDataDisplayRefreshRateChangedFB& event) {
  if (!IsValidRate(event.toDisplayRefreshRate)) {
    return;
  }
  frame_rate_ = event.toDisplayRefreshRate;
}

}  // namespace device
