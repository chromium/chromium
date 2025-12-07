// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_resizer.h"

#include <functional>
#include <optional>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "remoting/base/constants.h"
#include "remoting/base/logging.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/gnome_capture_stream_manager.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_interaction_strategy.h"
#include "remoting/host/linux/pipewire_capture_stream.h"
#include "remoting/proto/control.pb.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/glib/gsettings.h"

namespace remoting {

namespace {

// Logical layout mode is preferred since it has better user experience with a
// mixed-DPI setup.
constexpr GnomeDisplayConfig::LayoutMode kPreferredLayoutMode =
    GnomeDisplayConfig::LayoutMode::kLogical;

constexpr base::TimeDelta kClearPreferredConfigDelay = base::Seconds(5);

// Minimum text scaling factor (inverted if less than 1) required to be applied,
// meaning the text scale will only be applied if
// `preferred_scale / best_monitor_scale` is higher than kTextScaleThreshold,
// or lower than `1 / kTextScaleThreshold`, otherwise it will be reverted to 1.
// This is to prevent setting the text scale when the monitor scale is close
// enough to the preferred scale, since a non-1 text scale usually negatively
// affects how the OS layouts UI elements.
constexpr double kTextScaleThreshold = 1.25;

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

// Note: this method only adds a monitor for the purpose of layout calculation.
// DO NOT call ApplyMonitorsConfig with the updated `config`.
void AddMonitorForLayoutCalculation(GnomeDisplayConfig& config,
                                    const webrtc::DesktopVector& position,
                                    const ScreenResolution& resolution) {
  // We can't use the screen_id as the key, since it may be empty.
  GnomeDisplayConfig::MonitorInfo& info =
      config.monitors[base::NumberToString(config.monitors.size())];
  info.x = position.x();
  info.y = position.y();
  info.scale = resolution.dpi().x() == 0.0
                   ? 1.0
                   : static_cast<double>(resolution.dpi().x()) / kDefaultDpi;
  GnomeDisplayConfig::MonitorMode mode;
  mode.width = resolution.dimensions().width();
  mode.height = resolution.dimensions().height();
  mode.is_current = true;
  info.modes.push_back(mode);
}

inline ScopedGObject<GSettings> CreateGsettingsRegistry() {
  auto registry = ui::GSettingsNew("org.gnome.desktop.interface");
  CHECK(registry)
      << "ui::GSettingsNew(\"org.gnome.desktop.interface\") failed.";
  return registry;
}

}  // namespace

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<CaptureStreamManager> stream_manager,
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client,
    base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor)
    : GnomeDesktopResizer(
          stream_manager,
          display_config_monitor,
          CreateGsettingsRegistry(),
          base::BindRepeating(
              &GnomeDisplayConfigDBusClient::ApplyMonitorsConfig,
              display_config_client)) {}

GnomeDesktopResizer::GnomeDesktopResizer(
    base::WeakPtr<CaptureStreamManager> stream_manager,
    base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor,
    ScopedGObject<GSettings> registry,
    base::RepeatingCallback<void(const GnomeDisplayConfig&)>
        apply_monitors_config)
    : stream_manager_(stream_manager),
      apply_monitors_config_(apply_monitors_config),
      registry_(std::move(registry)) {
  if (display_config_monitor) {
    monitors_changed_subscription_ = display_config_monitor->AddCallback(
        base::BindRepeating(&GnomeDesktopResizer::OnGnomeDisplayConfigReceived,
                            weak_ptr_factory_.GetWeakPtr()),
        /*call_with_current_config=*/true);
  }
}

GnomeDesktopResizer::~GnomeDesktopResizer() = default;

ScreenResolution GnomeDesktopResizer::GetCurrentResolution(
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return {};
  }
  base::WeakPtr<CaptureStream> stream = stream_manager_->GetStream(screen_id);
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

  // Sometimes the client will send multiple SetResolution requests. The display
  // layout will become horizontal start-aligned after
  // SetResolutionAndPosition(), so we only set the preferred layout if it
  // hasn't been set.
  if (!preferred_layout_) {
    preferred_layout_ = current_display_config_.GetLayoutInfo();
    MaybeDelayClearPreferredConfig();
  }
  // Note: When changing a monitor's resolution via PipeWire, the monitor order
  // will also be reset. We could try to preserve the monitor order, but that
  // will cause existing apps to act strangely. See crbug.com/441824091.
  SetResolutionAndPosition(resolution, /*position=*/std::nullopt, screen_id);
}

void GnomeDesktopResizer::RestoreResolution(const ScreenResolution& original,
                                            webrtc::ScreenId screen_id) {
  SetResolution(original, screen_id);
}

void GnomeDesktopResizer::SetVideoLayout(const protocol::VideoLayout& layout) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!stream_manager_) {
    return;
  }
  auto active_screen_ids = base::MakeFlatSet<webrtc::ScreenId>(
      stream_manager_->GetActiveStreams(), std::less<>(),
      [](const auto& kv) { return kv.first; });
  base::flat_set<webrtc::ScreenId> screen_ids_in_video_track;
  for (const auto& track : layout.video_track()) {
    if (track.has_screen_id()) {
      screen_ids_in_video_track.emplace(track.screen_id());
    }
  }
  auto streams_to_be_removed =
      base::STLSetDifference<base::flat_set<webrtc::ScreenId>>(
          active_screen_ids, screen_ids_in_video_track);
  if (!streams_to_be_removed.empty()) {
    if (!streams_being_removed_.empty()) {
      LOG(WARNING) << "Streams will not be removed since there are already "
                   << "streams being removed.";
    } else {
      // Set `streams_being_removed_` beforehand so that the
      // SetResolutionAndPosition() calls below know whether they should be
      // queued up or executed right away.
      streams_being_removed_ = streams_to_be_removed;
      for (webrtc::ScreenId stream_id : streams_being_removed_) {
        stream_manager_->RemoveVirtualStream(stream_id);
        preferred_monitors_config_.erase(stream_id);
      }
    }
  }

  // `display_config_for_layout_calculation` is just for calculating the
  // layout direction and alignment. It is not meant to be passed to
  // ApplyMonitorsConfig.
  GnomeDisplayConfig display_config_for_layout_calculation;
  switch (layout.pixel_type()) {
    case protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL:
      display_config_for_layout_calculation.layout_mode =
          GnomeDisplayConfig::LayoutMode::kLogical;
      break;
    // While we only use logical layout now, physical layout may be passed in
    // if we are restoring the display layout from a M141 host. We will switch
    // to logical layout when applying the config.
    case protocol::VideoLayout::PixelType::VideoLayout_PixelType_PHYSICAL:
    case protocol::VideoLayout::PixelType::
        VideoLayout_PixelType_UNSPECIFIED_PIXEL_TYPE:
      display_config_for_layout_calculation.layout_mode =
          GnomeDisplayConfig::LayoutMode::kPhysical;
      break;
  }
  for (const auto& track : layout.video_track()) {
    // The client doesn't seem to set the initial DPI, so we set it to 1 if
    // the calculated scale is 0. This allows the correct scale to be used if
    // the client later decides to send the initial DPI.
    double scale = static_cast<double>(track.x_dpi()) / kDefaultDpi;
    if (scale == 0.0) {
      scale = 1.0;
    }
    double physical_resolution_multiplier =
        layout.pixel_type() ==
                protocol::VideoLayout::PixelType::VideoLayout_PixelType_LOGICAL
            ? scale
            : 1.0;

    webrtc::DesktopSize physical_resolution{
        static_cast<int>(track.width() * physical_resolution_multiplier),
        static_cast<int>(track.height() * physical_resolution_multiplier)};
    ScreenResolution screen_resolution{physical_resolution,
                                       {track.x_dpi(), track.y_dpi()}};
    // If a physical layout is passed, the position will be invalid but the
    // Relayout() call in DoApplyPreferredMonitorsConfig() will fix it.
    webrtc::DesktopVector position{track.position_x(), track.position_y()};

    if (!track.has_screen_id()) {
      stream_manager_->AddVirtualStream(
          screen_resolution,
          base::BindOnce(&GnomeDesktopResizer::OnAddStreamResult,
                         weak_ptr_factory_.GetWeakPtr(),
                         PreferredMonitorConfig{
                             .expected_resolution = physical_resolution,
                             .position = position,
                             .scale = scale,
                         }));
    } else {
      SetResolutionAndPosition(screen_resolution, position, track.screen_id());
    }
    AddMonitorForLayoutCalculation(display_config_for_layout_calculation,
                                   position, screen_resolution);
  }
  preferred_layout_ = display_config_for_layout_calculation.GetLayoutInfo();
  MaybeDelayClearPreferredConfig();
}

base::WeakPtr<GnomeDesktopResizer> GnomeDesktopResizer::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void GnomeDesktopResizer::SetResolutionAndPosition(
    const ScreenResolution& resolution,
    std::optional<webrtc::DesktopVector> position,
    webrtc::ScreenId screen_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // It is unsafe to call `stream->SetResolution()` when there are streams being
  // deleted. See comments on `do_after_stream_removal_`.
  if (!streams_being_removed_.empty()) {
    do_after_stream_removal_.AddUnsafe(base::BindOnce(
        &GnomeDesktopResizer::SetResolutionAndPosition,
        weak_ptr_factory_.GetWeakPtr(), resolution, position, screen_id));
    return;
  }

  if (!stream_manager_) {
    return;
  }
  // Change the screen resolution.
  base::WeakPtr<CaptureStream> stream = stream_manager_->GetStream(screen_id);
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
  double preferred_scale =
      static_cast<double>(resolution.dpi().x()) / kDefaultDpi;
  bool has_preferred_config = preferred_monitors_config_.find(screen_id) !=
                              preferred_monitors_config_.end();
  PreferredMonitorConfig& preferred_config =
      preferred_monitors_config_[screen_id];
  preferred_config.expected_resolution = resolution.dimensions(),
  preferred_config.scale = preferred_scale;
  if (position.has_value()) {
    preferred_config.position = *position;
  } else if (!has_preferred_config) {
    // If this is a new config and no position is specified, then we should keep
    // the current position reported by the DisplayConfig API.
    auto monitor_it = current_display_config_.FindMonitor(screen_id);
    if (monitor_it == current_display_config_.monitors.end()) {
      LOG(ERROR) << "Cannot find monitor with screen ID: " << screen_id;
    } else {
      preferred_config.position = {monitor_it->second.x, monitor_it->second.y};
    }
  }
  MaybeDelayClearPreferredConfig();

  // If the resolution has not changed, then we can immediately apply the
  // preferred monitors config, otherwise we wait for an updated displays config
  // to be received with a matching screen resolution to learn the list of
  // supported scales and prevent race conditions.
  if (!resolution_changed) {
    ScheduleApplyPreferredMonitorsConfig();
  }
}

void GnomeDesktopResizer::OnAddStreamResult(
    const PreferredMonitorConfig& monitor_config,
    CaptureStreamManager::AddStreamResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!result.has_value()) {
    LOG(ERROR) << "Failed to add stream: " << result.error();
    return;
  }
  preferred_monitors_config_[result.value()->screen_id()] = monitor_config;
  MaybeDelayClearPreferredConfig();
  ScheduleApplyPreferredMonitorsConfig();
}

void GnomeDesktopResizer::OnGnomeDisplayConfigReceived(
    const GnomeDisplayConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  current_display_config_ = config;

  bool found_stream_pending_removal = false;
  for (const auto& screen_id : streams_being_removed_) {
    if (current_display_config_.FindMonitor(screen_id) !=
        current_display_config_.monitors.end()) {
      found_stream_pending_removal = true;
      break;
    }
  }

  // Remove monitors that do not have a current mode, which may exist when they
  // are being created or destroyed. It is safe to ignore them since a new
  // display config will be received if they later gain a current mode.
  current_display_config_.RemoveInvalidMonitors();
  // This is no-op if the current layout mode is already the preferred layout
  // mode, otherwise it will recalculate the monitor offsets under the new mode.
  current_display_config_.SwitchLayoutMode(kPreferredLayoutMode);

  if (!streams_being_removed_.empty() && !found_stream_pending_removal) {
    streams_being_removed_.clear();
    do_after_stream_removal_.Notify();
  }
  // This is no-op if all the preferred config is reflected in the current
  // display config.
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

  apply_monitors_config_scheduled_ = false;

  if (on_trying_to_apply_preferred_monitors_config_for_testing_) {
    std::move(on_trying_to_apply_preferred_monitors_config_for_testing_).Run();
  }

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
  bool ignore_fractional_scales =
      ignore_fractional_scales_in_multimon_ && new_config.monitors.size() > 1;
  bool config_changed = false;
  // Code below will early return if not all expected resolution changes have
  // been reflected in the display config.
  for (auto [screen_id, preferred_config] : preferred_monitors_config_) {
    auto monitor_it = new_config.FindMonitor(screen_id);
    if (monitor_it == new_config.monitors.end()) {
      // This may happen for newly added monitors that may not be reflected in
      // `current_display_config_` yet.
      return;
    }

    GnomeDisplayConfig::MonitorInfo& monitor = monitor_it->second;
    const GnomeDisplayConfig::MonitorMode* mode = monitor.GetCurrentMode();
    if (!mode) {
      LOG(ERROR) << "Cannot find current mode for monitor with screen ID: "
                 << screen_id;
      return;
    } else if (!preferred_config.expected_resolution.equals(
                   webrtc::DesktopSize{mode->width, mode->height})) {
      // Resolution change not reflected in display config yet.
      return;
    }
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
    if (monitor.is_primary) {
      SetTextScalingFactor(preferred_config.scale / best_monitor_scale);
    }
  }

  if (preferred_layout_.has_value()) {
    preferred_layout_->layout_mode = kPreferredLayoutMode;
    new_config.Relayout(*preferred_layout_);
    for (const auto& [monitor_name, monitor] : new_config.monitors) {
      if (!config_changed) {
        // Check if relayout changes the monitor offsets and update
        // `config_changed`. Relayout never changes monitor sizes so we don't
        // need to worry about that.
        auto current_monitor_it =
            current_display_config_.monitors.find(monitor_name);
        DCHECK(current_monitor_it != current_display_config_.monitors.end());
        if (current_monitor_it->second.x != monitor.x ||
            current_monitor_it->second.y != monitor.y) {
          config_changed = true;
        }
      }

      // Write the new offsets back to the preferred config, if it exists.
      auto it = preferred_monitors_config_.find(
          GnomeDisplayConfig::GetScreenId(monitor_name));
      if (it != preferred_monitors_config_.end()) {
        it->second.position.set(monitor.x, monitor.y);
      }
    }
  }

  if (config_changed) {
    // Setting `method` to `kPersistent` would trigger a confirmation dialog
    // that would revert the change if the user hasn't clicked "Keep Changes"
    // within 15 seconds.
    // See:
    // https://gitlab.gnome.org/GNOME/mutter/-/blob/1c6532ee18fd72ad324f8f53ccc03bfdf31e90e2/src/backends/meta-monitor-manager.c#L3180
    // The difference between kTemporary and kPersistent is that the former
    // will not write the current display config to the disk, such that, e.g.
    // the display config will get reverted after device reboots. For CRD, all
    // the virtual displays are ephemeral, and we track the current display
    // config and make changes whenever necessary, so kTemporary suffices and
    // there is no benefit using kPersistent.
    new_config.method = GnomeDisplayConfig::Method::kTemporary;
    apply_monitors_config_.Run(new_config);
    // Only start the timer when it's not running, so that the preferred
    // config will be cleared if the display config keeps changing/never
    // stabilizes.
    if (!clear_preferred_config_timer_.IsRunning()) {
      clear_preferred_config_timer_.Start(
          FROM_HERE, kClearPreferredConfigDelay, this,
          &GnomeDesktopResizer::ClearPreferredConfig);
    }
  }
}

void GnomeDesktopResizer::ClearPreferredConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  preferred_monitors_config_.clear();
  preferred_layout_.reset();
  streams_being_removed_.clear();
  do_after_stream_removal_.Clear();
}

void GnomeDesktopResizer::MaybeDelayClearPreferredConfig() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (clear_preferred_config_timer_.IsRunning()) {
    clear_preferred_config_timer_.Reset();
  }
}

double GnomeDesktopResizer::GetTextScalingFactor() const {
  if (!registry_) {
    return 1.0;
  }
  return g_settings_get_double(registry_.get(), "text-scaling-factor");
}

void GnomeDesktopResizer::SetTextScalingFactor(double text_scaling_factor) {
  if (!registry_) {
    return;
  }
  if (InverseIfLessThanOne(text_scaling_factor) < kTextScaleThreshold) {
    // Revert text scale to 1 if it doesn't exceed the threshold.
    text_scaling_factor = 1.0;
  }
  if (!IsSameScale(GetTextScalingFactor(), text_scaling_factor) &&
      !g_settings_set_double(registry_.get(), "text-scaling-factor",
                             text_scaling_factor)) {
    LOG(ERROR) << "Failed to set text-scaling-factor";
  }
}

}  // namespace remoting
