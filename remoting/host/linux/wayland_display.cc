// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/wayland_display.h"

#include <vector>

#include "base/check.h"
#include "base/hash/hash.h"
#include "remoting/base/logging.h"

namespace remoting {

namespace {

constexpr int kOutputInterfaceVersion = 2;
constexpr int kZxdgOutputManagerVersion = 3;

}  // namespace

WaylandDisplay::WaylandDisplay()
    : wl_output_listener_({
          .geometry = OnGeometryEvent,
          .mode = OnModeEvent,
          .done = OnDoneEvent,
          .scale = OnScaleEvent,
      }),
      xdg_output_listener_({
          OnXdgOutputLogicalPositionEvent,
          OnXdgOutputLogicalSizeEvent,
          OnXdgOutputDoneEvent,
          OnXdgOutputNameEvent,
          OnXdgOutputDescriptionEvent,
      }) {}

WaylandDisplay::~WaylandDisplay() = default;

void WaylandDisplay::HandleGlobalDisplayEvent(struct wl_registry* registry,
                                              uint32_t name,
                                              const char* interface,
                                              uint32_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(registry);
  DCHECK(strcmp(interface, wl_output_interface.name) == 0 ||
         strcmp(interface, zxdg_output_manager_v1_interface.name) == 0);
  if (strcmp(interface, wl_output_interface.name) == 0) {
    DisplayInfo& display_info = display_info_.emplace_back();
    display_info.id = name;
    display_info.output = static_cast<wl_output*>(wl_registry_bind(
        registry, name, &wl_output_interface, kOutputInterfaceVersion));
    wl_output_add_listener(display_info.output, &wl_output_listener_,
                           &display_info);
    InitXdgOutputIfPossible(display_info);

  } else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
    xdg_output_manager_ = static_cast<zxdg_output_manager_v1*>(
        wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface,
                         kZxdgOutputManagerVersion));

    // Ensure that any partially initialized displays are now initialized.
    FinishPartialXdgOutputInitializations();
  }
}

bool WaylandDisplay::HandleGlobalRemoveDisplayEvent(uint32_t name) {
  size_t num_removed = std::erase_if(
      display_info_,
      [name](const auto& display_info) { return display_info.id == name; });
  DCHECK(num_removed <= 1)
      << "Duplicate displays removed. This is either chromoting bug or a bug "
      << "in wayland compositor";
  return num_removed >= 1;
}

void WaylandDisplay::InitXdgOutputIfPossible(DisplayInfo& display_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!xdg_output_manager_) {
    return;
  }
  display_info.xdg_output = zxdg_output_manager_v1_get_xdg_output(
      xdg_output_manager_, display_info.output);
  zxdg_output_v1_add_listener(display_info.xdg_output, &xdg_output_listener_,
                              &display_info);
}

void WaylandDisplay::FinishPartialXdgOutputInitializations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& display_info : display_info_) {
    if (display_info.xdg_output) {
      continue;
    }
    display_info.xdg_output = zxdg_output_manager_v1_get_xdg_output(
        xdg_output_manager_.get(), display_info.output);
    zxdg_output_v1_add_listener(display_info.xdg_output, &xdg_output_listener_,
                                &display_info);
  }
}

// static
void WaylandDisplay::UpdateDisplayInfo(DisplayInfo display_info,
                                       DisplayInfo& existing_info) {
  if (!existing_info.last_write_from_xdg) {
    if (display_info.x > -1) {
      existing_info.x = display_info.x;
    }
    if (display_info.y > -1) {
      existing_info.y = display_info.y;
    }
  }
  if (display_info.width > -1) {
    existing_info.width = display_info.width;
  }
  if (display_info.height > -1) {
    existing_info.height = display_info.height;
  }
  if (display_info.physical_width > -1) {
    existing_info.physical_width = display_info.physical_width;
  }
  if (display_info.physical_height > -1) {
    existing_info.physical_height = display_info.physical_height;
  }
  if (display_info.transform > -1) {
    existing_info.transform = display_info.transform;
  }
  if (display_info.refresh > -1) {
    existing_info.refresh = display_info.refresh;
  }
  if (display_info.subpixel > -1) {
    existing_info.subpixel = display_info.subpixel;
  }
  if (display_info.scale_factor != 1) {
    existing_info.scale_factor = display_info.scale_factor;
  }
}

// static
void WaylandDisplay::OnGeometryEvent(void* data,
                                     wl_output* wl_output,
                                     int x,
                                     int y,
                                     int physical_width,
                                     int physical_height,
                                     int subpixel,
                                     const char* make,
                                     const char* model,
                                     int transform) {
  UpdateDisplayInfo(
      DisplayInfo(x, y, transform, physical_width, physical_height, subpixel),
      *static_cast<DisplayInfo*>(data));
}

// static
void WaylandDisplay::OnModeEvent(void* data,
                                 wl_output* wl_output,
                                 uint flags,
                                 int width,
                                 int height,
                                 int refresh) {
  UpdateDisplayInfo(DisplayInfo(width, height, refresh),
                    *static_cast<DisplayInfo*>(data));
}

// static
void WaylandDisplay::OnDoneEvent(void* data, wl_output* wl_output) {}

// static
void WaylandDisplay::OnScaleEvent(void* data,
                                  wl_output* wl_output,
                                  int factor) {
  UpdateDisplayInfo(DisplayInfo(factor), *static_cast<DisplayInfo*>(data));
}

// static
void WaylandDisplay::OnXdgOutputLogicalPositionEvent(
    void* data,
    struct zxdg_output_v1* xdg_output,
    int32_t x,
    int32_t y) {
  DisplayInfo* display_info = static_cast<DisplayInfo*>(data);
  display_info->x = x;
  display_info->y = y;
  display_info->last_write_from_xdg = true;
}

// static
void WaylandDisplay::OnXdgOutputLogicalSizeEvent(
    void* data,
    struct zxdg_output_v1* xdg_output,
    int32_t width,
    int32_t height) {
  DisplayInfo* display_info = static_cast<DisplayInfo*>(data);
  display_info->width = width;
  display_info->height = height;
}

// static
void WaylandDisplay::OnXdgOutputDoneEvent(void* data,
                                          struct zxdg_output_v1* xdg_output) {}

// static
void WaylandDisplay::OnXdgOutputNameEvent(void* data,
                                          struct zxdg_output_v1* xdg_output,
                                          const char* name) {
  if (!name) {
    LOG(WARNING) << __func__ << " No monitor name found";
    return;
  }
  DisplayInfo* display_info = static_cast<DisplayInfo*>(data);
  DCHECK(display_info);
  display_info->name = name;
}

// static
void WaylandDisplay::OnXdgOutputDescriptionEvent(
    void* data,
    struct zxdg_output_v1* xdg_output,
    const char* description) {
  if (!description) {
    LOG(WARNING) << __func__ << " No monitor description found";
    return;
  }
  DisplayInfo* display_info = static_cast<DisplayInfo*>(data);
  display_info->description = description;
}

DesktopDisplayInfo WaylandDisplay::GetCurrentDisplayInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DesktopDisplayInfo result;
  for (const auto& display_info : display_info_) {
    if (display_info.name.empty()) {
      continue;
    }
    DCHECK(display_info.x >= 0);
    DCHECK(display_info.y >= 0);
    DCHECK(display_info.width >= 0);
    DCHECK(display_info.height >= 0);
    result.AddDisplay({
        .id = static_cast<webrtc::ScreenId>(base::Hash(display_info.name)),
        .x = display_info.x,
        .y = display_info.y,
        .width = static_cast<uint32_t>(display_info.width),
        .height = static_cast<uint32_t>(display_info.height),
    });
  }
  return result;
}

}  // namespace remoting
