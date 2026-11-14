// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_FB_OPENXR_FRAME_RATE_HANDLER_FB_H_
#define DEVICE_VR_OPENXR_FB_OPENXR_FRAME_RATE_HANDLER_FB_H_

#include <memory>
#include <optional>
#include <vector>

#include "device/vr/vr_export.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

class OpenXrExtensionEnumeration;
class OpenXrExtensionHelper;

// Reports and changes the display refresh rate with XR_FB_display_refresh_rate.
class DEVICE_VR_EXPORT OpenXrFrameRateHandlerFb {
 public:
  // The extension functions the handler calls. All must be set.
  struct Methods {
    PFN_xrEnumerateDisplayRefreshRatesFB enumerate_display_refresh_rates =
        nullptr;
    PFN_xrGetDisplayRefreshRateFB get_display_refresh_rate = nullptr;
    PFN_xrRequestDisplayRefreshRateFB request_display_refresh_rate = nullptr;
  };

  static const char* GetExtension();
  static bool IsSupported(const OpenXrExtensionEnumeration& extension_enum);

  // Returns null if the runtime does not support the extension.
  static std::unique_ptr<OpenXrFrameRateHandlerFb> Create(
      const OpenXrExtensionHelper& extension_helper,
      XrSession session);

  OpenXrFrameRateHandlerFb(const Methods& methods, XrSession session);

  OpenXrFrameRateHandlerFb(const OpenXrFrameRateHandlerFb&) = delete;
  OpenXrFrameRateHandlerFb& operator=(const OpenXrFrameRateHandlerFb&) = delete;

  ~OpenXrFrameRateHandlerFb();

  // Sorted and unique. Empty if the runtime lists none.
  const std::vector<float>& supported_frame_rates() const {
    return supported_frame_rates_;
  }
  std::optional<float> frame_rate() const { return frame_rate_; }

  // |rate| must be a supported rate. The runtime may switch to it later, or
  // not at all.
  void RequestFrameRate(float rate);

  // Reads the rate again. Some runtimes report a different one before the
  // session begins.
  void OnSessionBegin();

  void OnDisplayRefreshRateChanged(
      const XrEventDataDisplayRefreshRateChangedFB& event);

 private:
  void ReadFrameRate();

  const Methods methods_;
  const XrSession session_;
  std::vector<float> supported_frame_rates_;
  std::optional<float> frame_rate_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_FB_OPENXR_FRAME_RATE_HANDLER_FB_H_
