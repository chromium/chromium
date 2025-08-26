// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include <functional>

#include "base/check.h"
#include "base/containers/flat_set.h"
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
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/glib/gsettings.h"

namespace remoting {

namespace {

inline double InverseIfLessThanOne(double v) {
  return v < 1.0 ? 1.0 / v : v;
}

// Pick the scale that is proportionally closest to `preferred_scale`. For
// example, the best scale for 1.5 with supported_scales=[1, 2] is 2, since
// 2 / 1.5 = 1.33, which is smaller than 1.5 / 1 = 1.5.
inline double FindBestScale(double preferred_scale,
                            const std::vector<double>& supported_scales,
                            bool ignore_fractional_scales) {
  DCHECK_GT(preferred_scale, 0.0);
  auto it = std::ranges::min_element(
      supported_scales,
      [preferred_scale, ignore_fractional_scales](double s1, double s2) {
        DCHECK_GT(s1, 0.0);
        DCHECK_GT(s2, 0.0);
        if (ignore_fractional_scales && trunc(s1) == s1 && trunc(s2) != s2) {
          // Make non-fractional scales better than fractional scales.
          return true;
        }
        return InverseIfLessThanOne(preferred_scale / s1) <
               InverseIfLessThanOne(preferred_scale / s2);
      });
  if (it == supported_scales.end()) {
    LOG(ERROR) << "Cannot find best scale for " << preferred_scale;
    return 1.0;
  }
  if (ignore_fractional_scales && trunc(*it) != *it) {
    LOG(ERROR) << "Cannot find non-fractional scales";
    return 1.0;
  }
  return *it;
}

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
  // text-scaling-factor. Also, the effective scale of non-primary displays are
  // dictated by the preferred scale of the primary display, which may need to
  // be reflected here.
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

  DCHECK_EQ(resolution.dpi().x(), resolution.dpi().y());
  const auto monitor_it = current_display_config_.FindMonitor(screen_id);
  if (monitor_it == current_display_config_.monitors.end()) {
    LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
    return;
  }
  const auto& monitor = monitor_it->second;
  double preferred_scale =
      static_cast<double>(resolution.dpi().x()) / kDefaultDpi;
  preferred_monitors_config_[monitor_it->first] = {
      resolution.dimensions(), {monitor.x, monitor.y}, preferred_scale};
  // If the resolution has not changed, then we can immediately apply the
  // preferred monitors config, otherwise we wait for an updated displays config
  // to be received with a matching screen resolution to learn the list of
  // supported scales and prevent race conditions.
  if (!resolution_changed) {
    ScheduleApplyPreferredMonitorsConfig();
  }
}

void GnomeDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {}

void GnomeDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  // TODO: crbug.com/432217140 - Implement support for change of primary
  // display, monitor offsets and scales.
  auto unseen_screen_ids = base::MakeFlatSet<webrtc::ScreenId>(
      stream_manager_->GetActiveStreams(), std::less<>(),
      [](const auto& kv) { return kv.first; });
  for (const auto& track : layout.video_track()) {
    if (!track.has_screen_id()) {
      stream_manager_->AddStream(
          {{track.width(), track.height()}, {track.x_dpi(), track.y_dpi()}},
          base::BindOnce(&GnomeDesktopResizer::OnAddStreamResult,
                         weak_ptr_factory_.GetWeakPtr()));
    } else if (unseen_screen_ids.erase(track.screen_id()) == 0) {
      LOG(ERROR) << "Found unexpected screen ID: " << track.screen_id();
    }
  }
  // Remove pipewire streams that are no longer in the video layout.
  for (const auto& screen_id : unseen_screen_ids) {
    stream_manager_->RemoveStream(screen_id);
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
  // Switch to the physical layout mode, since otherwise monitor offsets would
  // need to be recalculated whenever a monitor scale is changed.
  current_display_config_.SwitchLayoutMode(
      GnomeDisplayConfig::LayoutMode::kPhysical);
  ScheduleApplyPreferredMonitorsConfig();
}

void GnomeDesktopResizer::ScheduleApplyPreferredMonitorsConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (apply_monitors_config_scheduled_) {
    return;
  }
  apply_monitors_config_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&GnomeDesktopResizer::DoApplyPreferredMonitorsConfig,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GnomeDesktopResizer::DoApplyPreferredMonitorsConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!display_config_client_) {
    return;
  }
  apply_monitors_config_scheduled_ = false;

  if (preferred_monitors_config_.empty()) {
    return;
  }
  // Check if all resolution changes are reflected in the new config. If so,
  // apply the display scales. Sometimes the new config may already have the
  // desired scales and positions, in which case we don't want to apply the
  // display config, since it would show a confirmation dialog.
  GnomeDisplayConfig new_config = current_display_config_;
  // There is a bug in mutter such that fractional scales may be reported as
  // supported but will fail to be applied when there are multiple virtual
  // monitors, so we ignore fractional scales in that case.
  // See: https://gitlab.gnome.org/GNOME/mutter/-/issues/4277
  bool ignore_fractional_scales = new_config.monitors.size() > 1;
  bool all_resolution_changes_reflected = true;
  bool config_changed = false;
  for (auto preferred_monitor_config_it = preferred_monitors_config_.begin();
       preferred_monitor_config_it != preferred_monitors_config_.end();) {
    const auto [monitor_name, preferred_config] = *preferred_monitor_config_it;
    auto monitor_it = new_config.monitors.find(monitor_name);
    if (monitor_it == new_config.monitors.end()) {
      HOST_LOG << "Monitor " << monitor_name << " no longer exists";
      preferred_monitor_config_it =
          preferred_monitors_config_.erase(preferred_monitor_config_it);
      break;
    }

    GnomeDisplayConfig::MonitorInfo& monitor = monitor_it->second;
    const GnomeDisplayConfig::MonitorMode* mode = monitor.GetCurrentMode();
    if (!mode) {
      LOG(ERROR) << "Cannot find current mode for monitor " << monitor_name;
      all_resolution_changes_reflected = false;
    } else if (!preferred_config.expected_resolution.equals(
                   webrtc::DesktopSize{mode->width, mode->height})) {
      // Resolution change not reflected in display config yet.
      all_resolution_changes_reflected = false;
    } else {
      if (monitor.x != preferred_config.position.x() ||
          monitor.y != preferred_config.position.y()) {
        monitor.x = preferred_config.position.x();
        monitor.y = preferred_config.position.y();
        config_changed = true;
      }
      double best_monitor_scale = FindBestScale(
          preferred_config.scale, monitor.GetCurrentMode()->supported_scales,
          ignore_fractional_scales);
      if (monitor.scale != best_monitor_scale) {
        monitor.scale = best_monitor_scale;
        config_changed = true;
      }
      // For the primary monitor, we correct the effective scale by applying
      // a text scale. We can't do this for all monitors, since the text scale
      // is globally applied, so we only do this for the primary monitor.
      // Note: an integer scale is usually supported, so this is usually only
      // applied when the client requests a fractional scale for a monitor.
      if (monitor.is_primary &&
          !IsSameScale(monitor.scale * GetTextScalingFactor(),
                       preferred_config.scale)) {
        if (!g_settings_set_double(registry_.get(), "text-scaling-factor",
                                   preferred_config.scale / monitor.scale)) {
          LOG(ERROR) << "Failed to set text-scaling-factor";
        }
      }
    }
    preferred_monitor_config_it++;
  }

  if (all_resolution_changes_reflected && config_changed) {
    // Setting `method` to `kPersistent` would trigger a confirmation dialog
    // that would revert the change if the user hasn't clicked "Keep Changes"
    // within 15 seconds.
    // See:
    // https://gitlab.gnome.org/GNOME/mutter/-/blob/1c6532ee18fd72ad324f8f53ccc03bfdf31e90e2/src/backends/meta-monitor-manager.c#L3180
    // The difference between kTemporary and kPersistent is that the former will
    // not write the current display config to the disk, such that, e.g. the
    // display config will get reverted after device reboots. For CRD, all the
    // virtual displays are ephemeral, and we track the current display config
    // and make changes whenever necessary, so kTemporary suffices and there is
    // no benefit using kPersistent.
    new_config.method = GnomeDisplayConfig::Method::kTemporary;
    display_config_client_->ApplyMonitorsConfig(new_config);
  }
}

double GnomeDesktopResizer::GetTextScalingFactor() const {
  return g_settings_get_double(registry_.get(), "text-scaling-factor");
}

}  // namespace remoting
