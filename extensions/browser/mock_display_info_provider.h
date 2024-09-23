// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MOCK_DISPLAY_INFO_PROVIDER_H_
#define EXTENSIONS_BROWSER_MOCK_DISPLAY_INFO_PROVIDER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/mock_screen.h"
#include "extensions/common/api/system_display.h"

namespace extensions {

class MockDisplayInfoProvider : public DisplayInfoProvider {
 public:
  MockDisplayInfoProvider();
  ~MockDisplayInfoProvider() override;
  MockDisplayInfoProvider(const MockDisplayInfoProvider&) = delete;
  MockDisplayInfoProvider& operator=(const MockDisplayInfoProvider&) = delete;

  // DisplayInfoProvider overrides.
  void SetDisplayProperties(
      const std::string& display_id,
      const api::system_display::DisplayProperties& properties,
      ErrorCallback callback) override;

  void EnableUnifiedDesktop(bool enable) override;

  bool OverscanCalibrationStart(const std::string& id) override;

  bool OverscanCalibrationAdjust(
      const std::string& id,
      const api::system_display::Insets& delta) override;

  bool OverscanCalibrationReset(const std::string& id) override;

  bool OverscanCalibrationComplete(const std::string& id) override;

  void ShowNativeTouchCalibration(const std::string& id,
                                  ErrorCallback callback) override;

  void SetMirrorMode(const api::system_display::MirrorModeInfo& info,
                     ErrorCallback callback) override;

  // Helpers, accessors.
  std::optional<base::Value::Dict> GetSetInfoValue() {
    return std::move(set_info_value_);
  }

  std::string GetSetInfoDisplayId() const { return set_info_display_id_; }

  bool unified_desktop_enabled() const { return unified_desktop_enabled_; }

  bool calibration_started(const std::string& id) const;

  bool calibration_changed(const std::string& id) const;

  const api::system_display::MirrorMode& mirror_mode() const {
    return mirror_mode_;
  }

  void SetTouchCalibrationWillSucceed(bool success) {
    native_touch_calibration_success_ = success;
  }

 private:
  // DisplayInfoProvider override.
  // Update the content of each unit in `units` obtained from the corresponding
  // display in `displays` using a platform specific method.
  void UpdateDisplayUnitInfoForPlatform(
      const std::vector<display::Display>& displays,
      DisplayUnitInfoList& units) const override;

  std::optional<base::Value::Dict> set_info_value_;
  std::string set_info_display_id_;
  bool unified_desktop_enabled_ = false;
  std::set<std::string> overscan_started_;
  std::set<std::string> overscan_adjusted_;

  bool native_touch_calibration_success_ = false;

  MockScreen screen_;

  api::system_display::MirrorMode mirror_mode_ =
      api::system_display::MirrorMode::kOff;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MOCK_DISPLAY_INFO_PROVIDER_H_
