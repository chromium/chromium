// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_display/display_info_provider.h"

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extensions_browser_client.h"
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

DisplayInfoProvider::DisplayInfoProvider(display::Screen* screen)
    : provided_screen_(screen) {
  // Do not use/call on the screen object in this constructor yet because a
  // subclass may pass not-yet-initialized screen instance.
}

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
  if (g_display_info_provider) {
    delete g_display_info_provider;
  }
  g_display_info_provider = display_info_provider;
}

// static
void DisplayInfoProvider::ResetForTesting() {
  g_display_info_provider = nullptr;
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
  unit.active_state = display.detected()
                          ? api::system_display::ActiveState::kActive
                          : api::system_display::ActiveState::kInactive;
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
  NOTREACHED_IN_MIGRATION() << "SetDisplayProperties not implemented";
}

void DisplayInfoProvider::SetDisplayLayout(const DisplayLayoutList& layouts,
                                           ErrorCallback callback) {
  NOTREACHED_IN_MIGRATION() << "SetDisplayLayout not implemented";
}

void DisplayInfoProvider::EnableUnifiedDesktop(bool enable) {}

DisplayInfoProvider::DisplayUnitInfoList
DisplayInfoProvider::GetAllDisplaysInfoList(
    const std::vector<display::Display>& displays,
    int64_t primary_id) const {
  DisplayUnitInfoList all_displays;

  for (const display::Display& display : displays) {
    api::system_display::DisplayUnitInfo unit =
        CreateDisplayUnitInfo(display, primary_id);
    all_displays.push_back(std::move(unit));
  }
  UpdateDisplayUnitInfoForPlatform(displays, all_displays);
  return all_displays;
}

void DisplayInfoProvider::GetAllDisplaysInfo(
    bool /* single_unified*/,
    base::OnceCallback<void(DisplayUnitInfoList result)> callback) {
  const display::Screen* screen =
      provided_screen_ ? provided_screen_.get() : display::Screen::GetScreen();
  int64_t primary_id = screen->GetPrimaryDisplay().id();
  std::vector<display::Display> displays = screen->GetAllDisplays();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DisplayInfoProvider::GetAllDisplaysInfoList,
                     base::Unretained(this),  // `this` is a global singleton.
                     displays, primary_id),
      std::move(callback));
}

void DisplayInfoProvider::GetDisplayLayout(
    base::OnceCallback<void(DisplayLayoutList result)> callback) {
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), DisplayLayoutList()));
}

void DisplayInfoProvider::StartObserving() {
  display_observer_.emplace(this);
}

void DisplayInfoProvider::StopObserving() {
  display_observer_.reset();
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
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
}

bool DisplayInfoProvider::StartCustomTouchCalibration(const std::string& id) {
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
  return false;
}

bool DisplayInfoProvider::CompleteCustomTouchCalibration(
    const api::system_display::TouchCalibrationPairQuad& pairs,
    const api::system_display::Bounds& bounds) {
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
  return false;
}

bool DisplayInfoProvider::ClearTouchCalibration(const std::string& id) {
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
  return false;
}

void DisplayInfoProvider::SetMirrorMode(
    const api::system_display::MirrorModeInfo& info,
    ErrorCallback callback) {
  NOTREACHED_IN_MIGRATION();  // Implemented on Chrome OS only in override.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
      base::Value::List(), dispatch_to_off_the_record_profiles);
}

void DisplayInfoProvider::UpdateDisplayUnitInfoForPlatform(
    const std::vector<display::Display>& displays,
    DisplayUnitInfoList& units) const {
  NOTIMPLEMENTED_LOG_ONCE();
}

void DisplayInfoProvider::OnDisplayAdded(const display::Display& new_display) {
  DispatchOnDisplayChangedEvent();
}

void DisplayInfoProvider::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  DispatchOnDisplayChangedEvent();
}

void DisplayInfoProvider::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  DispatchOnDisplayChangedEvent();
}

}  // namespace extensions
