// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mock_display_info_provider.h"

#include <stdint.h>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace extensions {

MockDisplayInfoProvider::MockDisplayInfoProvider()
    : DisplayInfoProvider(&screen_) {}

MockDisplayInfoProvider::~MockDisplayInfoProvider() = default;

void MockDisplayInfoProvider::SetDisplayProperties(
    const std::string& display_id,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  // Should get called only once per test case.
  DCHECK(!set_info_value_);
  set_info_value_ = properties.ToValue();
  set_info_display_id_ = display_id;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void MockDisplayInfoProvider::EnableUnifiedDesktop(bool enable) {
  unified_desktop_enabled_ = enable;
}

bool MockDisplayInfoProvider::OverscanCalibrationStart(const std::string& id) {
  if (base::Contains(overscan_started_, id))
    return false;
  overscan_started_.insert(id);
  return true;
}

bool MockDisplayInfoProvider::OverscanCalibrationAdjust(
    const std::string& id,
    const api::system_display::Insets& delta) {
  if (!base::Contains(overscan_started_, id))
    return false;
  overscan_adjusted_.insert(id);
  return true;
}

bool MockDisplayInfoProvider::OverscanCalibrationReset(const std::string& id) {
  if (!base::Contains(overscan_started_, id))
    return false;
  overscan_adjusted_.erase(id);
  return true;
}

bool MockDisplayInfoProvider::OverscanCalibrationComplete(
    const std::string& id) {
  if (!base::Contains(overscan_started_, id))
    return false;
  overscan_started_.erase(id);
  return true;
}

bool MockDisplayInfoProvider::calibration_started(const std::string& id) const {
  return base::Contains(overscan_started_, id);
}

bool MockDisplayInfoProvider::calibration_changed(const std::string& id) const {
  return base::Contains(overscan_adjusted_, id);
}

void MockDisplayInfoProvider::ShowNativeTouchCalibration(
    const std::string& id,
    ErrorCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                native_touch_calibration_success_
                                    ? std::nullopt
                                    : std::optional<std::string>("failed")));
}

void MockDisplayInfoProvider::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  mirror_mode_ = info.mode;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
}

void MockDisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const std::vector<display::Display>& displays,
    DisplayUnitInfoList& units) const {
  for (size_t i = 0; i < displays.size(); i++) {
    int64_t id = displays[i].id();
    units[i].name = "DISPLAY NAME FOR " + base::NumberToString(id);
    if (id == 1)
      units[i].mirroring_source_id = "0";

    units[i].is_primary = (id == 0);
    units[i].is_internal = (id == 0);
    units[i].is_enabled = true;
    units[i].rotation = (90 * id) % 360;
    units[i].dpi_x = 96.0;
    units[i].dpi_y = 96.0;
    if (id == 0) {
      units[i].overscan.left = 20;
      units[i].overscan.top = 40;
      units[i].overscan.right = 60;
      units[i].overscan.bottom = 80;
    }
  }
}

}  // namespace extensions
