// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/api/system_display.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace extensions {

namespace {

// Created on demand and will leak when the process exits.
DisplayInfoProvider* g_display_info_provider = nullptr;

// Converts Rotation enum to integer.
int RotationToDegrees(display::Display::Rotation rotation) {
  switch (rotation) {
    case display::Display::ROTATE_0:
      return 0;
    case display::Display::ROTATE_90:
      return 90;
    case display::Display::ROTATE_180:
      return 180;
    case display::Display::ROTATE_270:
      return 270;
  }
  return 0;
}

}  // namespace

DisplayInfoProvider::DisplayInfoProvider() = default;

DisplayInfoProvider::~DisplayInfoProvider() = default;

// static
DisplayInfoProvider* DisplayInfoProvider::Get() {
  if (!g_display_info_provider) {
    // Let the DisplayInfoProvider leak.
    g_display_info_provider =
        ExtensionsAPIClient::Get()->CreateDisplayInfoProvider().release();
  }
  return g_display_info_provider;
}

// static
void DisplayInfoProvider::InitializeForTesting(
    DisplayInfoProvider* display_info_provider) {
  if (g_display_info_provider)
    delete g_display_info_provider;
  g_display_info_provider = display_info_provider;
}

// static
// Creates new DisplayUnitInfo struct for |display|.
api::system_display::DisplayUnitInfo DisplayInfoProvider::CreateDisplayUnitInfo(
    const display::Display& display,
    int64_t primary_display_id) {
  api::system_display::DisplayUnitInfo unit;
  const gfx::Rect& bounds = display.bounds();
  const gfx::Rect& work_area = display.work_area();
  unit.id = base::NumberToString(display.id());
  unit.is_primary = (display.id() == primary_display_id);
  unit.is_internal = display.IsInternal();
  unit.is_enabled = true;
  unit.is_unified = false;
  unit.rotation = RotationToDegrees(display.rotation());
  unit.bounds.left = bounds.x();
  unit.bounds.top = bounds.y();
  unit.bounds.width = bounds.width();
  unit.bounds.height = bounds.height();
  unit.work_area.left = work_area.x();
  unit.work_area.top = work_area.y();
  unit.work_area.width = work_area.width();
  unit.work_area.height = work_area.height();
  unit.has_touch_support =
      display.touch_support() == display::Display::TouchSupport::AVAILABLE;
  unit.has_accelerometer_support =
      display.accelerometer_support() ==
      display::Display::AccelerometerSupport::AVAILABLE;
  return unit;
}

void DisplayInfoProvider::SetDisplayProperties(
    const std::string& display_id,
    const api::system_display::DisplayProperties& properties,
    ErrorCallback callback) {
  NOTREACHED() << "SetDisplayProperties not implemented";
}

void DisplayInfoProvider::SetDisplayLayout(const DisplayLayoutList& layouts,
                                           ErrorCallback callback) {
  NOTREACHED() << "SetDisplayLayout not implemented";
}

void DisplayInfoProvider::EnableUnifiedDesktop(bool enable) {}

void DisplayInfoProvider::GetAllDisplaysInfo(
    bool /* single_unified*/,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback) {
  display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  DisplayUnitInfoList all_displays;
  for (const display::Display& display : displays) {
    api::system_display::DisplayUnitInfo unit =
        CreateDisplayUnitInfo(display, primary_id);
    UpdateDisplayUnitInfoForPlatform(display, &unit);
    all_displays.push_back(std::move(unit));
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(all_displays)));
}

void DisplayInfoProvider::GetDisplayLayout(
    base::OnceCallback<void(DisplayLayoutList result)> callback) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), DisplayLayoutList()));
}

void DisplayInfoProvider::StartObserving() {
  display::Screen* screen = display::Screen::GetScreen();
  if (screen)
    screen->AddObserver(this);
}

void DisplayInfoProvider::StopObserving() {
  display::Screen* screen = display::Screen::GetScreen();
  if (screen)
    screen->RemoveObserver(this);
}

bool DisplayInfoProvider::OverscanCalibrationStart(const std::string& id) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationAdjust(
    const std::string& id,
    const api::system_display::Insets& delta) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationReset(const std::string& id) {
  return false;
}

bool DisplayInfoProvider::OverscanCalibrationComplete(const std::string& id) {
  return false;
}

void DisplayInfoProvider::ShowNativeTouchCalibration(const std::string& id,
                                                     ErrorCallback callback) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
}

bool DisplayInfoProvider::StartCustomTouchCalibration(const std::string& id) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  return false;
}

bool DisplayInfoProvider::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  return false;
}

bool DisplayInfoProvider::ClearTouchCalibration(const std::string& id) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  return false;
}

void DisplayInfoProvider::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  NOTREACHED();  // Implemented on Chrome OS only in override.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), "Not supported"));
}

void DisplayInfoProvider::DispatchOnDisplayChangedEvent() {
  // This function will dispatch the OnDisplayChangedEvent to both on-the-record
  // and off-the-record profiles. This allows extensions running in incognito
  // to be notified mirroring is enabled / disabled, which allows the Virtual
  // keyboard on ChromeOS to correctly disable key highlighting when typing
  // passwords on the login page (crbug/824656)
  constexpr bool dispatch_to_off_the_record_profiles = true;
  ExtensionsBrowserClient::Get()->BroadcastEventToRenderers(
      events::SYSTEM_DISPLAY_ON_DISPLAY_CHANGED,
      extensions::api::system_display::OnDisplayChanged::kEventName,
      std::make_unique<base::ListValue>(), dispatch_to_off_the_record_profiles);
}

void DisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const display::Display& display,
    extensions::api::system_display::DisplayUnitInfo* unit) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DisplayInfoProvider::OnDisplayAdded(const display::Display& new_display) {
  DispatchOnDisplayChangedEvent();
}

void DisplayInfoProvider::OnDisplayRemoved(
    const display::Display& old_display) {
  DispatchOnDisplayChangedEvent();
}

void DisplayInfoProvider::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  DispatchOnDisplayChangedEvent();
}

}  // namespace extensions
