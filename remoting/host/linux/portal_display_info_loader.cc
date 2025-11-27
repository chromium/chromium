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
    CaptureStreamManager& stream_manager)
    : stream_manager_(&stream_manager) {}

PortalDisplayInfoLoader::~PortalDisplayInfoLoader() = default;

DesktopDisplayInfo PortalDisplayInfoLoader::GetCurrentDisplayInfo() {
  DesktopDisplayInfo display_info;
  // TODO: crbug.com/445973705 - See if logical layout makes sense on other DEs.
  display_info.set_pixel_type(DesktopDisplayInfo::PixelType::PHYSICAL);
  int current_x = 0;
  bool first = true;
  for (const auto& [id, stream] : stream_manager_->GetActiveStreams()) {
    if (!stream) {
      LOG(ERROR) << "Stream for id " << id << " is null";
      continue;
    }
    webrtc::DesktopSize resolution = stream->resolution();
    // TODO: crbug.com/445973705 - We just assume that the monitors are
    // horizontal start-aligned, and the left-most display is the primary
    // display, which may be wrong.
    display_info.AddDisplay(
        DisplayGeometry(id, current_x, 0, resolution.width(),
                        resolution.height(), kDefaultDpi, 32, first, ""));
    current_x += resolution.width();
    first = false;
  }
  return display_info;
}

base::WeakPtr<PortalDisplayInfoLoader> PortalDisplayInfoLoader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace remoting
