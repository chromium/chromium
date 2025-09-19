// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"

#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "remoting/base/constants.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

PipewireMouseCursorCapturer::PipewireMouseCursorCapturer(
    base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor,
    base::WeakPtr<PipewireCaptureStreamManager> stream_manager)
    : stream_manager_(std::move(stream_manager)) {
  if (display_config_monitor) {
    // Display config is used to calculate monitor DPIs.
    display_config_subscription_ = display_config_monitor->AddCallback(
        base::BindRepeating(&PipewireMouseCursorCapturer::OnDisplayConfig,
                            GetWeakPtr()),
        /*call_with_current_config=*/true);
  }
}

PipewireMouseCursorCapturer::~PipewireMouseCursorCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PipewireMouseCursorCapturer::SetCallback(Callback* callback, Mode mode) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_ = callback;
  report_position_ = mode == Mode::SHAPE_AND_POSITION;
  want_latest_cursor_ = callback_ != nullptr;
}

void PipewireMouseCursorCapturer::Capture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_ || !callback_) {
    return;
  }

  auto active_streams = stream_manager_->GetActiveStreams();
  bool need_position = report_position_;
  bool need_cursor = true;
  for (auto [screen_id, stream] : active_streams) {
    if (!stream) {
      continue;
    }

    if (need_position) {
      // Any stream can capture the cursor position.
      std::optional<webrtc::DesktopVector> cursor_position =
          stream->CaptureCursorPosition();
      if (cursor_position.has_value()) {
        callback_->OnMouseCursorPosition(*cursor_position);
        need_position = false;
      }
    }

    if (need_cursor) {
      // Only the stream where the cursor is currently located can capture the
      // cursor. `cursor` will be nullptr if it hasn't changed since the last
      // call of CaptureCursor().
      std::unique_ptr<webrtc::MouseCursor> cursor = stream->CaptureCursor();
      if (cursor && cursor->image()->data()) {
        // TODO: yuweih - Add release_image() to webrtc::MouseCursor to allow
        // transfer of the cursor image's ownership.
        latest_cursor_frame_ =
            webrtc::SharedDesktopFrame::Wrap(base::WrapUnique(
                webrtc::BasicDesktopFrame::CopyOf(*cursor->image())));
        auto dpi_it = monitor_dpis_.find(screen_id);
        if (dpi_it != monitor_dpis_.end()) {
          latest_cursor_frame_->set_dpi({dpi_it->second, dpi_it->second});
        }
        latest_cursor_hotspot_ = cursor->hotspot();
        callback_->OnMouseCursor(ShareLatestCursor().release());
        need_cursor = false;
      }
    }

    if (!need_position && !need_cursor) {
      break;
    }
  }

  if (need_cursor && want_latest_cursor_ && latest_cursor_frame_) {
    callback_->OnMouseCursor(ShareLatestCursor().release());
  }
  want_latest_cursor_ = false;
}

base::WeakPtr<PipewireMouseCursorCapturer>
PipewireMouseCursorCapturer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PipewireMouseCursorCapturer::OnDisplayConfig(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  monitor_dpis_.clear();
  for (const auto& [name, monitor] : config.monitors) {
    monitor_dpis_[GnomeDisplayConfig::GetScreenId(name)] =
        kDefaultDpi * monitor.scale;
  }
}

std::unique_ptr<webrtc::MouseCursor>
PipewireMouseCursorCapturer::ShareLatestCursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(latest_cursor_frame_);

  return std::make_unique<webrtc::MouseCursor>(
      latest_cursor_frame_->Share().release(), latest_cursor_hotspot_);
}

}  // namespace remoting
