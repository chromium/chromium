// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/protocol/emulation_handler.h"

#include <optional>
#include <vector>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "headless/lib/browser/headless_screen.h"
#include "ui/display/display.h"
#include "ui/display/display_util.h"
#include "ui/display/headless/headless_screen_manager.h"
#include "ui/display/mojom/screen_orientation.mojom-shared.h"
#include "ui/display/screen.h"
#include "ui/display/screen_info.h"
#include "ui/display/util/display_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

namespace headless::protocol {

namespace {

const std::vector<display::Display>& GetAllDisplays() {
  // In Chrome, the screen is a collection of displays, whereas in protocol /
  // Web Platform we only have a collection of screens. So the protocol screen
  // is referring to Chrome's display. This is consistent with
  // window.getScreenDetails() API naming conventions.
  display::Screen* screen = display::Screen::Get();
  CHECK(screen);

  return screen->GetAllDisplays();
}

std::optional<display::Display> GetDisplay(int64_t display_id) {
  for (const display::Display& display : GetAllDisplays()) {
    if (display.id() == display_id) {
      return display;
    }
  }

  return std::nullopt;
}

bool IsPrimaryDisplay(int64_t display_id) {
  display::Screen* screen = display::Screen::Get();
  CHECK(screen);

  return screen->GetPrimaryDisplay().id() == display_id;
}

std::string GetProtocolScreenOrientation(
    display::mojom::ScreenOrientation screen_orientation) {
  switch (screen_orientation) {
    case display::mojom::ScreenOrientation::kPortraitPrimary:
      return Emulation::ScreenOrientation::TypeEnum::PortraitPrimary;
    case display::mojom::ScreenOrientation::kPortraitSecondary:
      return Emulation::ScreenOrientation::TypeEnum::PortraitSecondary;
    case display::mojom::ScreenOrientation::kLandscapePrimary:
      return Emulation::ScreenOrientation::TypeEnum::LandscapePrimary;
    case display::mojom::ScreenOrientation::kLandscapeSecondary:
      return Emulation::ScreenOrientation::TypeEnum::LandscapeSecondary;
    case display::mojom::ScreenOrientation::kUndefined:
      NOTREACHED();
  }
}

std::unique_ptr<protocol::Emulation::ScreenOrientation> CreateScreenOrientation(
    const display::ScreenInfo& screen_info) {
  return Emulation::ScreenOrientation::Create()
      .SetType(GetProtocolScreenOrientation(screen_info.orientation_type))
      .SetAngle(screen_info.orientation_angle)
      .Build();
}

std::unique_ptr<protocol::Emulation::ScreenInfo> CreateScreenInfo(
    const display::Display& display) {
  display::ScreenInfo screen_info;
  display::DisplayUtil::DisplayToScreenInfo(&screen_info, display);

  return Emulation::ScreenInfo::Create()
      .SetLeft(screen_info.rect.x())
      .SetTop(screen_info.rect.y())
      .SetWidth(screen_info.rect.width())
      .SetHeight(screen_info.rect.height())
      .SetAvailLeft(screen_info.available_rect.x())
      .SetAvailTop(screen_info.available_rect.y())
      .SetAvailWidth(screen_info.available_rect.width())
      .SetAvailHeight(screen_info.available_rect.height())
      .SetDevicePixelRatio(screen_info.device_scale_factor)
      .SetOrientation(CreateScreenOrientation(screen_info))
      .SetColorDepth(screen_info.depth)
      .SetIsExtended(screen_info.is_extended)
      .SetIsInternal(screen_info.is_internal)
      .SetIsPrimary(screen_info.is_primary)
      .SetLabel(screen_info.label)
      .SetId(base::NumberToString(screen_info.display_id))
      .Build();
}

}  // namespace

EmulationHandler::EmulationHandler() = default;

EmulationHandler::~EmulationHandler() = default;

void EmulationHandler::Wire(UberDispatcher* dispatcher) {
  Emulation::Dispatcher::wire(dispatcher, this);
}

Response EmulationHandler::Disable() {
  return Response::Success();
}

Response EmulationHandler::GetScreenInfos(
    std::unique_ptr<protocol::Array<protocol::Emulation::ScreenInfo>>*
        out_screen_infos) {
  *out_screen_infos =
      std::make_unique<protocol::Array<protocol::Emulation::ScreenInfo>>();

  for (const display::Display& display : GetAllDisplays()) {
    (*out_screen_infos)->push_back(CreateScreenInfo(display));
  }

  return Response::Success();
}

Response EmulationHandler::AddScreen(
    int left,
    int top,
    int width,
    int height,
    std::unique_ptr<protocol::Emulation::WorkAreaInsets> work_area_insets,
    std::optional<double> device_pixel_ratio,
    std::optional<int> rotation,
    std::optional<int> color_depth,
    std::optional<String> label,
    std::optional<bool> is_internal,
    std::unique_ptr<protocol::Emulation::ScreenInfo>* out_screen_info) {
  CHECK(display::Screen::Get()->IsHeadless());

  gfx::Rect bounds(left, top, width, height);

  gfx::Insets insets;
  if (work_area_insets) {
    insets.set_top(work_area_insets->GetTop(0));
    insets.set_left(work_area_insets->GetLeft(0));
    insets.set_bottom(work_area_insets->GetBottom(0));
    insets.set_right(work_area_insets->GetRight(0));
  }

  display::Display display;
  display::HeadlessScreenManager::SetDisplayGeometry(
      display, bounds, insets, device_pixel_ratio.value_or(1.0f));

  if (rotation) {
    if (!display::Display::IsValidRotation(*rotation)) {
      return Response::InvalidParams("Invalid screen rotation: " +
                                     base::NumberToString(*rotation));
    }
    display.SetRotationAsDegree(*rotation);
  }

  display.set_color_depth(color_depth.value_or(24));
  display.set_label(label.value_or(""));

  int64_t display_id = HeadlessScreen::AddDisplay(display);

  auto new_display = GetDisplay(display_id);
  if (!new_display) {
    return Response::InvalidParams("Failed to add screen id: " +
                                   base::NumberToString(display_id));
  }

  CHECK_EQ(new_display->id(), display_id);

  if (is_internal.value_or(false)) {
    display::AddInternalDisplayId(display_id);
  }

  *out_screen_info = CreateScreenInfo(*new_display);

  return Response::Success();
}

Response EmulationHandler::RemoveScreen(const String& screen_id) {
  CHECK(display::Screen::Get()->IsHeadless());

  int64_t display_id;
  if (!base::StringToInt64(screen_id, &display_id)) {
    return Response::InvalidParams("Invalid screen id: " + screen_id);
  }

  if (!GetDisplay(display_id)) {
    return Response::InvalidParams("Unknown screen id: " + screen_id);
  }

  if (GetAllDisplays().size() == 1) {
    return Response::InvalidParams(
        "Cannot remove the only screen in the system");
  }

  if (IsPrimaryDisplay(display_id)) {
    return Response::InvalidParams("Cannot remove the primary screen");
  }

  HeadlessScreen::RemoveDisplay(display_id);

  return Response::Success();
}

}  // namespace headless::protocol
