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
  auto initial_rects = stream_manager_->GetActiveStreamInitialRects();
  for (const auto& [id, stream] : stream_manager_->GetActiveStreams()) {
    if (!stream) {
      continue;
    }
    webrtc::DesktopRect rect;
    auto it = initial_rects.find(id);
    if (it != initial_rects.end() && it->second) {
      rect = *it->second;
    }
    const auto& res = stream->resolution();
    if (!res.is_empty()) {
      rect = webrtc::DesktopRect::MakeOriginSize(rect.top_left(), res);
    }
    // TODO: crbug.com/445973705 - We just assume that the left-most display is
    // the primary display, which may be wrong.
    display_info.AddDisplay(DisplayGeometry(id, rect.left(), rect.top(),
                                            rect.width(), rect.height(),
                                            kDefaultDpi, /*bpp=*/32,
                                            /*is_default=*/first, ""));
    first = false;
  }
  return display_info;
}

base::WeakPtr<PortalDisplayInfoLoader> PortalDisplayInfoLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace remoting
