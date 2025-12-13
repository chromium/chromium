// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/portal_desktop_resizer.h"

#include "base/logging.h"
#include "base/notimplemented.h"
#include "remoting/base/constants.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

PortalDesktopResizer::PortalDesktopResizer(CaptureStreamManager& stream_manager)
    : stream_manager_(&stream_manager) {}

PortalDesktopResizer::~PortalDesktopResizer() = default;

ScreenResolution PortalDesktopResizer::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  base::WeakPtr<CaptureStream> stream = stream_manager_->GetStream(screen_id);
  if (!stream) {
    return ScreenResolution();
  }
  // TODO: crbug.com/445973705 - Provide actual DPI if that becomes available.
  return ScreenResolution(stream->resolution(), {kDefaultDpi, kDefaultDpi});
}

std::list<ScreenResolution> PortalDesktopResizer::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  return {preferred};
}

void PortalDesktopResizer::SetResolution(const ScreenResolution& resolution,
                                         webrtc::ScreenId screen_id) {
  base::WeakPtr<CaptureStream> stream = stream_manager_->GetStream(screen_id);
  if (!stream) {
    LOG(ERROR) << "Stream not found for screen_id: " << screen_id;
    return;
  }
  if (resolution.dpi().x() != kDefaultDpi ||
      resolution.dpi().y() != kDefaultDpi) {
    LOG(WARNING) << "PortalDesktopResizer only supports default DPI of "
                 << kDefaultDpi;
  }
  stream->SetResolution(webrtc::DesktopSize(resolution.dimensions().width(),
                                            resolution.dimensions().height()));
}

void PortalDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                             webrtc::ScreenId screen_id) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void PortalDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {
  NOTIMPLEMENTED_LOG_ONCE();
}

base::WeakPtr<PortalDesktopResizer> PortalDesktopResizer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace remoting
