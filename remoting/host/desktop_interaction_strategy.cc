// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_interaction_strategy.h"

#include <memory>
#include <utility>

#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer_differ_wrapper.h"

namespace remoting {
std::unique_ptr<webrtc::DesktopCapturer>
DesktopInteractionStrategy::CreateCapturerFromRaw(
    std::unique_ptr<webrtc::DesktopCapturer> raw_capturer,
    const webrtc::DesktopCaptureOptions& options,
    webrtc::ScreenId id) {
  if (!raw_capturer) {
    return nullptr;
  }
  auto desktop_capturer =
      options.detect_updated_region()
          ? std::make_unique<webrtc::DesktopCapturerDifferWrapper>(
                std::move(raw_capturer))
          : std::move(raw_capturer);
  desktop_capturer->SelectSource(id);
  return desktop_capturer;
}

}  // namespace remoting
