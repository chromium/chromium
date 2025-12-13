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
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "remoting/base/constants.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/protocol/coordinate_conversion.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor.h"
#include "third_party/webrtc/modules/desktop_capture/mouse_cursor_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace remoting {

PipewireMouseCursorCapturer::PipewireMouseCursorCapturer(
    std::unique_ptr<DesktopDisplayInfoMonitor> display_info_monitor,
    base::WeakPtr<CaptureStreamManager> stream_manager)
    : display_info_monitor_(std::move(display_info_monitor)) {
  if (display_info_monitor_) {
    display_info_monitor_->AddCallback(base::BindRepeating(
        &PipewireMouseCursorCapturer::OnDisplayInfo, GetWeakPtr()));
    display_info_monitor_->Start();
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

  auto physical_cursor_position = stream->CaptureCursorPosition();
  if (!physical_cursor_position) {
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
  // need to be scaled to DIPs if `pixel_type_` is LOGICAL.
  double physical_size_multiplier =
      pixel_type_ == DesktopDisplayInfo::PixelType::LOGICAL
          ? monitor_it->second.scale
          : 1.0;
  webrtc::DesktopVector new_local_cursor_position{
      static_cast<int>(physical_cursor_position->x() /
                       physical_size_multiplier),
      static_cast<int>(physical_cursor_position->y() /
                       physical_size_multiplier)};
  webrtc::DesktopVector new_global_cursor_position =
      new_local_cursor_position.add(
          {monitor_it->second.left, monitor_it->second.top});
  if (latest_global_cursor_position_ &&
      latest_global_cursor_position_->equals(new_global_cursor_position)) {
    // CaptureStream sometimes calls OnCursorPositionChanged() even if the
    // position has not changed, so we need to ignore these bogus events.
    return;
  }
  latest_global_cursor_position_ = new_global_cursor_position;

  // Fractional coordinates are always based on physical pixels.
  webrtc::DesktopSize physical_size{
      static_cast<int>(monitor_it->second.width * physical_size_multiplier),
      static_cast<int>(monitor_it->second.height * physical_size_multiplier)};
  latest_fractional_cursor_position_ = protocol::ToFractionalCoordinate(
      screen_id, physical_size, *physical_cursor_position);
  observers_.Notify(&Observer::OnCursorPositionChanged, this);
}

void PipewireMouseCursorCapturer::OnDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const DesktopDisplayInfo* display_info =
      display_info_monitor_->GetLatestDisplayInfo();
  if (!display_info) {
    return;
  }

  monitors_.clear();
  for (const auto& display : display_info->displays()) {
    double scale = static_cast<double>(display.dpi) / kDefaultDpi;
    monitors_[display.id] = {
        .scale = scale,
        .left = display.x,
        .top = display.y,
        .width = static_cast<int>(display.width),
        .height = static_cast<int>(display.height),
    };
  }
  pixel_type_ = display_info->pixel_type().value_or(
      DesktopDisplayInfo::PixelType::PHYSICAL);
}

void PipewireMouseCursorCapturer::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);

  observers_.RemoveObserver(observer);
}

}  // namespace remoting
