// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_display_info_loader.h"

#include "base/logging.h"
#include "remoting/base/constants.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

PortalDisplayInfoLoader::PortalDisplayInfoLoader(
    PortalCaptureStreamManager& stream_manager)
    : stream_manager_(&stream_manager) {}

PortalDisplayInfoLoader::~PortalDisplayInfoLoader() = default;

DesktopDisplayInfo PortalDisplayInfoLoader::GetCurrentDisplayInfo() {
  DesktopDisplayInfo display_info;
  // TODO: crbug.com/445973705 - Use PipeWire metadata instead of the initial
  // rects if it has been implemented, since the display layout and sizes can
  // change over time.
  // TODO: crbug.com/445973705 - Fix this for high-DPI/mixed-DPI setups. On
  // GNOME in logical layout mode, the initial rects are all in DIPs. However,
  // the Portal API does not return the display scales or pixel type.
  display_info.set_pixel_type(DesktopDisplayInfo::PixelType::PHYSICAL);
  bool first = true;
  for (const auto& [id, initial_rect] :
       stream_manager_->GetActiveStreamInitialRects()) {
    // TODO: crbug.com/445973705 - We just assume that the left-most display is
    // the primary display, which may be wrong.
    display_info.AddDisplay(
        DisplayGeometry(id, initial_rect->left(), initial_rect->top(),
                        initial_rect->width(), initial_rect->height(),
                        kDefaultDpi, /*bpp=*/32, /*is_default=*/first, ""));
  }
  return display_info;
}

base::WeakPtr<PortalDisplayInfoLoader> PortalDisplayInfoLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace remoting
