// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_info_loader.h"

#include "remoting/host/linux/gnome_interaction_strategy.h"

namespace remoting {

GnomeDisplayInfoLoader::GnomeDisplayInfoLoader(
    base::WeakPtr<GnomeInteractionStrategy> session)
    : session_(std::move(session)) {}

GnomeDisplayInfoLoader::~GnomeDisplayInfoLoader() = default;

DesktopDisplayInfo GnomeDisplayInfoLoader::GetCurrentDisplayInfo() {
  DesktopDisplayInfo info;
  if (session_) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(session_->sequence_checker_);

    // Since there is only a single capture-stream, the layout info can be
    // determined just from the stream's resolution.
    // TODO: crbug.com/432217140 - Support multiple displays by using GNOME's
    // DisplayConfig D-Bus API to get the display layout.
    const ScreenResolution& resolution = session_->capture_stream_.resolution();
    info.AddDisplay(
        DisplayGeometry{/*id=*/0, /*x=*/0, /*y=*/0,
                        static_cast<uint32_t>(resolution.dimensions().width()),
                        static_cast<uint32_t>(resolution.dimensions().height()),
                        static_cast<uint32_t>(resolution.dpi().x()), /*bpp=*/24,
                        /*is_default=*/true, "Default display"});
  }
  return info;
}

}  // namespace remoting
