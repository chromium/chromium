// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_interaction_strategy.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/glib/gsettings.h"

namespace remoting {

namespace {

inline bool IsSameScale(double s1, double s2) {
  return std::abs(s1 - s2) < 0.01;
}

}  // namespace

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<PipewireCaptureStreamManager> stream_manager,
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client)
    : stream_manager_(stream_manager),
      display_config_client_(display_config_client) {
  registry_ = ui::GSettingsNew("org.gnome.desktop.interface");
  CHECK(registry_)
      << "ui::GSettingsNew(\"org.gnome.desktop.interface\") failed.";
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

  double text_scaling_factor = GetTextScalingFactor();
  double dpi = kDefaultDpi * text_scaling_factor;
  auto monitor_it = current_display_config_.FindMonitor(screen_id);
  if (monitor_it == current_display_config_.monitors.end()) {
    LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
  } else {
    dpi *= monitor_it->second.scale;
  }
  return {stream->resolution(), {static_cast<int>(dpi), static_cast<int>(dpi)}};
}

std::list<ScreenResolution> GnomeDesktopResizer::GetSupportedResolutions(
    const ScreenResolution& preferred,
    webrtc::ScreenId screen_id) {
  // TODO: crbug.com/431816005 - clamp scale to the supported range of
  // text-scaling-factor.
  return {preferred};
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

  // Now change the display scale by setting text-scaling-factor to the desired
  // scale and setting the monitor scale to 1. There are several reasons why we
  // do it this way for now:
  //
  // 1. Due to crbug.com/433312809, a virtual monitor with a scale other than 1
  //    will not render correctly due to incorrect damage area coordinates.
  // 2. Gnome has a list of supported scales for a specific screen resolution.
  //    You won't be able to apply a monitor scale if Gnome thinks it is not
  //    supported for the current resolution. You would either need to replicate
  //    Gnome's support scales calculation logic, or do it in a try-and-error
  //    manner. Meanwhile you are allowed to set any fractional number between
  //    [0.5, 3.0] as the text-scaling-factor.
  // 3. Currently Gnome will show a confirmation dialog whenever the monitor
  //    scale is changed, which is disruptive to the users. With the current
  //    approach, the user will likely only see the confirmation dialog once, or
  //    never, if Gnome believe the recommended scaling is 1x.
  //
  // But there are some caveats:
  //
  // 1. Not all UI elements are scaled by text-scaling-factor, and unfortunately
  //    the mouse cursor isn't scaled so it will be tiny in high-DPI mode.
  //    TODO: crbug.com/431816005 - maybe add DPI/scale info to the
  //    CursorShapeInfo so that the client can scale the mouse cursor properly.
  // 2. text-scaling-factor is applied on all monitors, so if the user has
  //    a multi-monitor setup with mixed DPIs, then the scaling factor may
  //    change back and forth when the monitor is resized.
  //    TODO: crbug.com/431816005 - Fix this properly by tracking individual
  //    monitors' preferred scales and choosing the lowest scale.
  //
  // Given these caveats, we would still prefer to set monitor scales rather
  // than text scales when that becomes possible, but we can do it for now.
  // TODO: crbug.com/431816005 - Just set monitor scales to the desired scales
  // and text-scaling-factor to 1 once the blockers have been resolved.
  DCHECK_EQ(resolution.dpi().x(), resolution.dpi().y());
  if (!g_settings_set_double(
          registry_.get(), "text-scaling-factor",
          static_cast<double>(resolution.dpi().x()) / kDefaultDpi)) {
    LOG(ERROR) << "Failed to set text-scaling-factor";
    return;
  }

  // Now set the monitor scale to 1. Gnome infers the default monitor scale for
  // the given resolution, and sometimes that is not 1. A none-1 monitor scale
  // will cause rendering issues. See: crbug.com/433312809
  auto monitor_it = current_display_config_.FindMonitor(screen_id);
  if (monitor_it == current_display_config_.monitors.end()) {
    LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
    return;
  }
  auto& monitor = monitor_it->second;
  double monitor_scale = 1.0;
  if (!resolution_changed && !IsSameScale(monitor_scale, monitor.scale)) {
    // Only the display scale is changed, so we don't need to wait for the
    // resolution change to take effect.
    monitor.scale = monitor_scale;
    ScheduleApplyMonitorsConfig();
    return;
  }
  // If the resolution is changed, Gnome may potentially pick a different
  // monitor scale, so we need to wait for the resolution change to be reflected
  // on the display config before we check and set the scale, otherwise
  // ApplyMonitorsConfig will fail due to mismatched serials, i.e. race
  // condition.
  pending_monitors_config_[monitor_it->first] = {
      resolution.dimensions(), {monitor.x, monitor.y}, monitor_scale};
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
  // apply the display scales. Sometimes the new config may already have the
  // desired scales and positions, in which case we don't want to apply the
  // display config, since it would show a confirmation dialog.
  GnomeDisplayConfig new_config = current_display_config_;
  bool all_resolution_changes_reflected = true;
  bool config_changed = false;
  for (auto pending_monitor_config_it = pending_monitors_config_.begin();
       pending_monitor_config_it != pending_monitors_config_.end();) {
    const auto [monitor_name, pending_change] = *pending_monitor_config_it;
    auto monitor_it = new_config.monitors.find(monitor_name);
    if (monitor_it == new_config.monitors.end()) {
      LOG(ERROR) << "Cannot find monitor with name: " << monitor_name;
      all_resolution_changes_reflected = false;
      // Remove the change that's no longer valid.
      pending_monitor_config_it =
          pending_monitors_config_.erase(pending_monitor_config_it);
      break;
    }

    GnomeDisplayConfig::MonitorInfo& monitor = monitor_it->second;
    const GnomeDisplayConfig::MonitorMode* mode = monitor.GetCurrentMode();
    if (!mode) {
      LOG(ERROR) << "Cannot find current mode for monitor " << monitor_name;
      all_resolution_changes_reflected = false;
    } else if (pending_change.expected_resolution.width() != mode->width ||
               pending_change.expected_resolution.height() != mode->height) {
      // Resolution change not reflected in display config yet.
      all_resolution_changes_reflected = false;
    } else if (monitor.x != pending_change.position.x() ||
               monitor.y != pending_change.position.y() ||
               !IsSameScale(monitor.scale, pending_change.scale)) {
      monitor.x = pending_change.position.x();
      monitor.y = pending_change.position.y();
      monitor.scale = pending_change.scale;
      config_changed = true;
    }
    pending_monitor_config_it++;
  }

  if (all_resolution_changes_reflected) {
    pending_monitors_config_.clear();
    if (config_changed) {
      current_display_config_ = std::move(new_config);
      ScheduleApplyMonitorsConfig();
    }
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

double GnomeDesktopResizer::GetTextScalingFactor() const {
  return g_settings_get_double(registry_.get(), "text-scaling-factor");
}

}  // namespace remoting
