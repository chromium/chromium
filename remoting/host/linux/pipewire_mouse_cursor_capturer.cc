// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "remoting/base/constants.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

namespace {

float CalculateFractionalCoordinate(int val, int size) {
  if (size <= 1) {
    return 0.f;
  }
  // Clamp to prevent bogus values, in case the PipeWire coordinates are
  // out-of-sync with the display config.
  return std::clamp(static_cast<float>(val) / (size - 1), 0.f, 1.f);
}

}  // namespace

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

    auto monitor_it = monitors_.find(screen_id);

    if (need_position) {
      if (monitor_it != monitors_.end()) {
        std::optional<webrtc::DesktopVector> cursor_position =
            stream->CaptureCursorPosition();
        if (cursor_position.has_value()) {
          callback_->OnMouseCursorFractionalPosition(
              screen_id,
              CalculateFractionalCoordinate(cursor_position->x(),
                                            monitor_it->second.width),
              CalculateFractionalCoordinate(cursor_position->y(),
                                            monitor_it->second.height));
          need_position = false;
        }
      } else {
        // This is potentially spammy so we don't log at WARNING level.
        VLOG(1) << "Cannot provide fractional position since monitor "
                << screen_id << " is not found.";
      }
    }

    if (need_cursor) {
      // `cursor` will be nullptr if it hasn't changed since the last call of
      // CaptureCursor().
      std::unique_ptr<webrtc::MouseCursor> cursor = stream->CaptureCursor();
      if (cursor && cursor->image()->data()) {
        latest_cursor_frame_ =
            webrtc::SharedDesktopFrame::Wrap(cursor->TakeImage());
        if (monitor_it != monitors_.end()) {
          latest_cursor_frame_->set_dpi(
              {monitor_it->second.dpi, monitor_it->second.dpi});
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

  auto physical_config = config;
  physical_config.SwitchLayoutMode(GnomeDisplayConfig::LayoutMode::kPhysical);
  monitors_.clear();
  for (const auto& [name, monitor] : physical_config.monitors) {
    const auto* current_mode = monitor.GetCurrentMode();
    if (!current_mode) {
      LOG(WARNING) << "Ignored monitor without current mode: " << name;
      continue;
    }
    monitors_[GnomeDisplayConfig::GetScreenId(name)] = {
        .dpi = static_cast<int>(kDefaultDpi * monitor.scale),
        .width = current_mode->width,
        .height = current_mode->height,
    };
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
