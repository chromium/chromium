// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_interaction_strategy.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

namespace {

constexpr int kDefaultDPI = 96;
const webrtc::DesktopVector kDefaultDPIVector = {kDefaultDPI, kDefaultDPI};

}  // namespace

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<PipewireCaptureStreamManager> stream_manager,
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client)
    : stream_manager_(stream_manager),
      display_config_client_(display_config_client) {
  monitors_changed_subscription_ =
      display_config_client->SubscribeMonitorsChanged(
          base::BindRepeating(&GnomeDesktopResizer::QueryDisplayInfo,
                              weak_ptr_factory_.GetWeakPtr()));
  // Query the initial display info.
  QueryDisplayInfo();
}

GnomeDesktopResizer::~GnomeDesktopResizer() = default;

ScreenResolution GnomeDesktopResizer::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return {};
  }
  base::WeakPtr<PipewireCaptureStream> stream =
      stream_manager_->GetStream(screen_id);
  if (!stream) {
    LOG(ERROR) << "Cannot find pipewire stream for screen ID: " << screen_id;
    return {};
  }

  auto monitor_it = current_display_config_.FindMonitor(screen_id);
  if (monitor_it == current_display_config_.monitors.end()) {
    LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
    return {stream->resolution(), kDefaultDPIVector};
  }
  int dpi = kDefaultDPI * monitor_it->second.scale;
  return {stream->resolution(), {dpi, dpi}};
}

std::list<ScreenResolution> GnomeDesktopResizer::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  // We currently can't support monitor scales other than 1 because of
  // crbug.com/433312809 and the inability to infer supported scales for a given
  // resolution (without actually setting it to the monitor resolution), so we
  // calculate and return the screen resolution with the default DPI.
  ScreenResolution default_dpi_resolution = {
      preferred.ScaleDimensionsToDpi(kDefaultDPIVector), kDefaultDPIVector};
  return {default_dpi_resolution};
}

void GnomeDesktopResizer::SetResolution(const ScreenResolution& resolution,
                                        webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  // Change the screen resolution.
  base::WeakPtr<PipewireCaptureStream> stream =
      stream_manager_->GetStream(screen_id);
  if (!stream) {
    LOG(ERROR) << "Cannot find pipewire stream for screen ID: " << screen_id;
    return;
  }
  bool resolution_changed =
      !resolution.dimensions().equals(stream->resolution());
  if (resolution_changed) {
    stream->SetResolution(resolution.dimensions());
  }

  // Now change the display scale.
  auto monitor_it = current_display_config_.FindMonitor(screen_id);
  if (monitor_it == current_display_config_.monitors.end()) {
    LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
    return;
  }
  auto& monitor = monitor_it->second;
  double scale = static_cast<double>(resolution.dpi().x()) / kDefaultDPI;
  if (std::abs(monitor.scale - scale) < 0.01) {
    // Display scale is unchanged.
    return;
  }
  HOST_LOG << "Monitor scale for screen ID " << screen_id << " changed from "
           << monitor.scale << " to " << scale;
  monitor.scale = scale;
  if (!resolution_changed) {
    // Only the display scale is changed, so we don't need to wait for the
    // resolution change to take effect.
    ScheduleApplyMonitorsConfig();
    return;
  }
  // We need to wait for the resolution change to be reflected on the display
  // config before we set the scale, otherwise ApplyMonitorsConfig will fail due
  // to mismatched serials, i.e. race condition.
  pending_monitors_config_[monitor_it->first] = {
      resolution.dimensions(), {monitor.x, monitor.y}, scale};
}

void GnomeDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {}

void GnomeDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  for (const auto& track : layout.video_track()) {
    if (!track.has_screen_id()) {
      stream_manager_->AddStream(
          {{track.width(), track.height()}, {track.x_dpi(), track.y_dpi()}},
          base::BindOnce(&GnomeDesktopResizer::OnAddStreamResult,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void GnomeDesktopResizer::OnAddStreamResult(
    PipewireCaptureStreamManager::AddStreamResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to add stream: " << result.error();
    return;
  }
  // TODO: crbug.com/432217140 - Configure offset and scale by calling
  // ApplyMonitorsConfig via D-Bus.
}

void GnomeDesktopResizer::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    display_config_client_->GetMonitorsConfig(
        base::BindOnce(&GnomeDesktopResizer::OnGnomeDisplayConfigReceived,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void GnomeDesktopResizer::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_display_config_ = std::move(config);
  if (pending_monitors_config_.empty()) {
    return;
  }
  // Check if all resolution changes are reflected in the new config. If so,
  // apply the display scales.
  bool should_apply_monitors_config = true;
  for (auto pending_monitor_config_it = pending_monitors_config_.begin();
       pending_monitor_config_it != pending_monitors_config_.end();) {
    const auto [monitor_name, pending_change] = *pending_monitor_config_it;
    auto monitor_it = current_display_config_.monitors.find(monitor_name);
    if (monitor_it == current_display_config_.monitors.end()) {
      LOG(ERROR) << "Cannot find monitor with name: " << monitor_name;
      should_apply_monitors_config = false;
      // Remove the change that's no longer valid.
      pending_monitor_config_it =
          pending_monitors_config_.erase(pending_monitor_config_it);
      break;
    }

    GnomeDisplayConfig::MonitorInfo& monitor = monitor_it->second;
    const GnomeDisplayConfig::MonitorMode* mode = monitor.GetCurrentMode();
    if (!mode) {
      LOG(ERROR) << "Cannot find current mode for monitor " << monitor_name;
      should_apply_monitors_config = false;
    } else if (pending_change.expected_resolution.width() != mode->width ||
               pending_change.expected_resolution.height() != mode->height) {
      // Resolution change not reflected in display config yet.
      should_apply_monitors_config = false;
    } else {
      monitor.x = pending_change.position.x();
      monitor.y = pending_change.position.y();
      monitor.scale = pending_change.scale;
    }
    pending_monitor_config_it++;
  }

  if (should_apply_monitors_config) {
    pending_monitors_config_.clear();
    ScheduleApplyMonitorsConfig();
  }
}

void GnomeDesktopResizer::ScheduleApplyMonitorsConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (apply_monitors_config_scheduled_) {
    return;
  }
  apply_monitors_config_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&GnomeDesktopResizer::DoApplyMonitorsConfig,
                                weak_ptr_factory_.GetWeakPtr()));
}

void GnomeDesktopResizer::DoApplyMonitorsConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!display_config_client_) {
    return;
  }
  apply_monitors_config_scheduled_ = false;
  display_config_client_->ApplyMonitorsConfig(current_display_config_);
}

}  // namespace remoting
