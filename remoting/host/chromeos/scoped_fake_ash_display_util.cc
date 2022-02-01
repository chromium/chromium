// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/scoped_fake_ash_display_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/manager/managed_display_info.h"

namespace remoting {
namespace test {

ScreenshotRequest::ScreenshotRequest(
    DisplayId display,
    AshDisplayUtil::ScreenshotCallback callback)
    : display(display), callback(std::move(callback)) {}

ScreenshotRequest::ScreenshotRequest(ScreenshotRequest&&) = default;
ScreenshotRequest& ScreenshotRequest::operator=(ScreenshotRequest&&) = default;
ScreenshotRequest::~ScreenshotRequest() = default;

ScopedFakeAshDisplayUtil::ScopedFakeAshDisplayUtil() {
  AshDisplayUtil::SetInstanceForTesting(this);
}

ScopedFakeAshDisplayUtil::~ScopedFakeAshDisplayUtil() {
  AshDisplayUtil::SetInstanceForTesting(nullptr);
}

display::Display& ScopedFakeAshDisplayUtil::AddPrimaryDisplay(DisplayId id) {
  primary_display_id_ = id;
  // Give the display a valid size.
  return AddDisplayFromSpecWithId("800x600", id);
}

display::Display& ScopedFakeAshDisplayUtil::AddDisplayWithId(DisplayId id) {
  // Give the display a valid size.
  return AddDisplayFromSpecWithId("800x600", id);
}

display::Display& ScopedFakeAshDisplayUtil::AddDisplayFromSpecWithId(
    const std::string& spec,
    DisplayId id) {
  auto display_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID(spec, id);

  display::Display new_display(display_info.id());

  // We use size_in_pixel() and not bounds_in_native() because size_in_pixel()
  // takes rotation into account (which is also what happens when adding a real
  // Display in the DisplayManager).
  gfx::Rect bounds_in_pixels(display_info.bounds_in_native().origin(),
                             display_info.size_in_pixel());

  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();
  new_display.SetScaleAndBounds(device_scale_factor, bounds_in_pixels);
  new_display.set_rotation(display_info.GetActiveRotation());

  return AddDisplay(new_display);
}

display::Display& ScopedFakeAshDisplayUtil::AddDisplay(
    display::Display new_display) {
  displays_.push_back(new_display);
  return displays_.back();
}

void ScopedFakeAshDisplayUtil::RemoveDisplay(DisplayId id) {
  for (auto it = displays_.begin(); it != displays_.end(); it++) {
    if (it->id() == id) {
      displays_.erase(it);
      return;
    }
    NOTREACHED();
  }
}

ScreenshotRequest ScopedFakeAshDisplayUtil::WaitForScreenshotRequest() {
  return screenshot_request_.Take();
}

void ScopedFakeAshDisplayUtil::ReplyWithScreenshot(
    const absl::optional<SkBitmap>& screenshot) {
  ScreenshotRequest request = WaitForScreenshotRequest();
  std::move(request.callback).Run(screenshot);
}

DisplayId ScopedFakeAshDisplayUtil::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

const std::vector<display::Display>&
ScopedFakeAshDisplayUtil::GetActiveDisplays() const {
  return displays_;
}

const display::Display* ScopedFakeAshDisplayUtil::GetDisplayForId(
    DisplayId display_id) const {
  for (const auto& display : displays_) {
    if (display_id == display.id())
      return &display;
  }
  return nullptr;
}

void ScopedFakeAshDisplayUtil::TakeScreenshotOfDisplay(
    DisplayId display_id,
    ScreenshotCallback callback) {
  screenshot_request_.SetValue(
      ScreenshotRequest{display_id, std::move(callback)});
}

}  // namespace test
}  // namespace remoting
