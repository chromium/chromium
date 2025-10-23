// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/pipewire_mouse_cursor_capturer.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "remoting/base/constants.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
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
    base::WeakPtr<CaptureStreamManager> stream_manager) {
  if (display_config_monitor) {
    // Display config is used to calculate monitor DPIs.
    display_config_subscription_ = display_config_monitor->AddCallback(
        base::BindRepeating(&PipewireMouseCursorCapturer::OnDisplayConfig,
                            GetWeakPtr()),
        /*call_with_current_config=*/true);
  }
  if (stream_manager) {
    stream_manager_subscription_ = stream_manager->AddObserver(this);
  }
}

PipewireMouseCursorCapturer::~PipewireMouseCursorCapturer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

PipewireMouseCursorCapturer::Observer::Subscription
PipewireMouseCursorCapturer::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  observers_.AddObserver(observer);

  return base::ScopedClosureRunner(
      base::BindOnce(&PipewireMouseCursorCapturer::RemoveObserver,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

base::WeakPtr<PipewireMouseCursorCapturer>
PipewireMouseCursorCapturer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<webrtc::MouseCursor>
PipewireMouseCursorCapturer::GetLatestCursor() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!latest_cursor_frame_) {
    return nullptr;
  }

  return std::make_unique<webrtc::MouseCursor>(latest_cursor_frame_->Share(),
                                               latest_cursor_hotspot_);
}

const std::optional<webrtc::DesktopVector>&
PipewireMouseCursorCapturer::GetLatestGlobalCursorPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return latest_global_cursor_position_;
}

const std::optional<protocol::FractionalCoordinate>&
PipewireMouseCursorCapturer::GetLatestFractionalCursorPosition() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return latest_fractional_cursor_position_;
}

void PipewireMouseCursorCapturer::OnPipewireCaptureStreamAdded(
    base::WeakPtr<CaptureStream> stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_subscriptions_[stream->screen_id()] = stream->AddCursorObserver(this);
}

void PipewireMouseCursorCapturer::OnPipewireCaptureStreamRemoved(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  stream_subscriptions_.erase(screen_id);
}

void PipewireMouseCursorCapturer::OnCursorShapeChanged(CaptureStream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cursor = stream->CaptureCursor();
  if (!cursor) {
    // This happens when the cursor moves out of the stream's virtual monitor.
    // The stream where the cursor moves into will call OnCursorShapeChanged()
    // with a non-null cursor. It is unsafe to set `latest_cursor_hotspot_` to
    // null here since the stream that the cursor leaves might notify the
    // observer after the stream that the cursor enters.
    return;
  }
  latest_cursor_frame_ = webrtc::SharedDesktopFrame::Wrap(cursor->TakeImage());
  auto monitor_it = monitors_.find(stream->screen_id());
  if (monitor_it != monitors_.end()) {
    int dpi = static_cast<int>(kDefaultDpi * monitor_it->second.scale);
    latest_cursor_frame_->set_dpi({dpi, dpi});
  } else {
    LOG(ERROR) << "Cannot find monitor for screen ID: " << stream->screen_id();
  }
  latest_cursor_hotspot_ = cursor->hotspot();
  observers_.Notify(&Observer::OnCursorShapeChanged, this);
}

void PipewireMouseCursorCapturer::OnCursorPositionChanged(
    CaptureStream* stream) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto cursor_position = stream->CaptureCursorPosition();
  if (!cursor_position) {
    // This happens when the cursor moves out of the stream's virtual monitor.
    // The stream where the cursor moves into will call
    // OnCursorPositionChanged() with a non-null position. It is unsafe to set
    // latest positions to null here since the stream that the cursor leaves
    // might notify the observer after the stream that the cursor enters.
    return;
  }
  webrtc::ScreenId screen_id = stream->screen_id();
  auto monitor_it = monitors_.find(screen_id);
  if (monitor_it == monitors_.end()) {
    LOG(ERROR) << "Cannot find monitor for screen ID: " << screen_id;
    return;
  }
  // Note that PipeWire returns cursor positions in physical pixels, so they
  // need to be scaled to DIPs before adding the monitor offsets.
  webrtc::DesktopVector new_global_cursor_position{
      static_cast<int>(cursor_position->x() / monitor_it->second.scale +
                       monitor_it->second.left),
      static_cast<int>(cursor_position->y() / monitor_it->second.scale +
                       monitor_it->second.top)};
  if (latest_global_cursor_position_ &&
      latest_global_cursor_position_->equals(new_global_cursor_position)) {
    // CaptureStream sometimes calls OnCursorPositionChanged() even if the
    // position has not changed, so we need to ignore these bogus events.
    return;
  }
  latest_global_cursor_position_ = new_global_cursor_position;
  latest_fractional_cursor_position_.emplace();
  latest_fractional_cursor_position_->set_screen_id(screen_id);
  latest_fractional_cursor_position_->set_x(CalculateFractionalCoordinate(
      cursor_position->x(), monitor_it->second.width));
  latest_fractional_cursor_position_->set_y(CalculateFractionalCoordinate(
      cursor_position->y(), monitor_it->second.height));
  observers_.Notify(&Observer::OnCursorPositionChanged, this);
}

void PipewireMouseCursorCapturer::OnDisplayConfig(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto logical_config = config;
  // Logical layout is used throughout the Gnome Wayland host. If `config` is
  // physical, then `logical_config` is effectively re-layouted and will not
  // match the actual layout, but since we consistently call
  // SwitchLayoutMode(kLogical) throughout the code base, the coordinates will
  // agree.
  logical_config.SwitchLayoutMode(GnomeDisplayConfig::LayoutMode::kLogical);
  monitors_.clear();
  for (const auto& [name, monitor] : logical_config.monitors) {
    const auto* current_mode = monitor.GetCurrentMode();
    if (!current_mode) {
      LOG(WARNING) << "Ignored monitor without current mode: " << name;
      continue;
    }
    monitors_[GnomeDisplayConfig::GetScreenId(name)] = {
        .scale = monitor.scale,
        .left = monitor.x,
        .top = monitor.y,
        .width = current_mode->width,
        .height = current_mode->height,
    };
  }
}

void PipewireMouseCursorCapturer::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  observers_.RemoveObserver(observer);
}

}  // namespace remoting
