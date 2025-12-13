// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

#include <gio/gio.h>

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/capture_stream_manager.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "remoting/host/linux/gnome_display_config_monitor.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

class GnomeDesktopResizer : public DesktopResizer {
 public:
  GnomeDesktopResizer(
      base::WeakPtr<CaptureStreamManager> stream_manager,
      base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client,
      base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor);
  GnomeDesktopResizer(const GnomeDesktopResizer&) = delete;
  GnomeDesktopResizer& operator=(const GnomeDesktopResizer&) = delete;
  ~GnomeDesktopResizer() override;

  ScreenResolution GetCurrentResolution(webrtc::ScreenId screen_id) override;
  std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred,
      webrtc::ScreenId screen_id) override;
  void SetResolution(const ScreenResolution& resolution,
                     webrtc::ScreenId screen_id) override;
  void RestoreResolution(const ScreenResolution& original,
                         webrtc::ScreenId screen_id) override;
  void SetVideoLayout(const protocol::VideoLayout& layout) override;

  base::WeakPtr<GnomeDesktopResizer> GetWeakPtr();

 private:
  friend class GnomeDesktopResizerTest;

  // TODO: yuweih - There is an open feature request in mutter for changing
  // virtual monitor scales and offsets via PipeWire, which will significantly
  // simplify things. Use that when the feature is ready.
  // See: https://gitlab.gnome.org/GNOME/mutter/-/issues/4275

  struct PreferredMonitorConfig {
    // The expected resolution in physical screen pixels. The preferred monitor
    // config is not applied until the screen resolution in
    // `current_display_config_` matches this.
    webrtc::DesktopSize expected_resolution;

    // The preferred position of the monitor in DIPs.
    webrtc::DesktopVector position;

    // The preferred scale. A supported monitor scale that is proportionally
    // closest to this scale will be used. For the primary monitor, an
    // additional text scale will be applied to adjust for the discrepancy
    // between the monitor scale and the preferred scale; it won't be applied
    // for non-primary monitors. Note that the text scale is global, so it won't
    // work very well with a mixed DPI setup.
    double scale = 1.0;
  };

  GnomeDesktopResizer(
      base::WeakPtr<CaptureStreamManager> stream_manager,
      base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor,
      ScopedGObject<GSettings> registry,
      base::RepeatingCallback<void(const GnomeDisplayConfig&)>
          apply_monitors_config);

  void SetResolutionAndPosition(const ScreenResolution& resolution,
                                std::optional<webrtc::DesktopVector> position,
                                webrtc::ScreenId screen_id);

  void OnAddStreamResult(const PreferredMonitorConfig& monitor_config,
                         CaptureStreamManager::AddStreamResult result);

  void OnGnomeDisplayConfigReceived(const GnomeDisplayConfig& config);

  // Schedules an ApplyMonitorsConfig call to apply `preferred_monitors_config_`
  // in the next event loop iteration of the current sequence, if it hasn't been
  // called in the current event loop iteration yet. This is used to bundle
  // multiple display config changes and avoid race conditions.
  void ScheduleApplyPreferredMonitorsConfig();
  void DoApplyPreferredMonitorsConfig();

  void ClearPreferredConfig();

  // Delays `clear_preferred_config_timer_` if it's running; otherwise do
  // nothing.
  void MaybeDelayClearPreferredConfig();

  double GetTextScalingFactor() const;
  void SetTextScalingFactor(double text_scaling_factor);

  base::WeakPtr<CaptureStreamManager> stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::RepeatingCallback<void(const GnomeDisplayConfig&)>
      apply_monitors_config_ GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GnomeDisplayConfigMonitor::Subscription>
      monitors_changed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Represents the latest known display config state reported by Mutter. This
  // may potentially not be up-to-date, and does not include any pending
  // changes.
  // We always use the logical layout mode for better mixed-DPI support.
  GnomeDisplayConfig current_display_config_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool apply_monitors_config_scheduled_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // Fields below will be cleared by `clear_preferred_config_timer_`.

  // Preferred monitors config, which may or may not be reflected in
  // `current_display_config_`. This field is used to:
  //
  // 1. Store pending config so that it can be applied later to prevent race
  //    conditions. For example, we wait for screen resolution changes via
  //    pipewire to be reflected in the display config before we apply display
  //    scales or offsets.
  // 2. Prevent the preferred config from being reverted, since Mutter tends to
  //    change the monitor layout multiple times during and after resizes, and
  //    we can't tell which change is the last one.
  //
  // We can't use flat_map since we may remove elements during iteration.
  std::map<webrtc::ScreenId /* screen_id */, PreferredMonitorConfig>
      preferred_monitors_config_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The preferred layout calculated from either the VideoLayout protobuf or
  // the Gnome display config prior to monitor resizes. If this is set, it
  // will be used to relayout monitors before passing the new config to
  // ApplyMonitorsConfig.
  // Mutter tends to switch to the horizontal start-aligned monitor layout
  // whenever a monitor is resize, which is subpar to our relayout algorithm.
  // This field allows us to maintain the layout direction and alignment after
  // resizes.
  std::optional<GnomeDisplayConfig::LayoutInfo> preferred_layout_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::flat_set<webrtc::ScreenId> streams_being_removed_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Stores tasks (mostly SetResolutionAndPosition calls) to be run after all
  // streams in `streams_being_removed_` are absent from the current gnome
  // display config. This is to prevent mutter crashes when trying to delete a
  // pipewire stream while setting the resolution of another pipewire stream at
  // the same time.
  base::OnceClosureList do_after_stream_removal_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Timer to clear the fields above. Mutter tends to have multiple intermediate
  // display config changes after resizes, so they need to be kept for a while
  // so that the changes won't be reverted. Once the display config has
  // stabilized, we clear these fields so that the display config can be changed
  // externally, e.g. via the settings app.
  base::RetainingOneShotTimer clear_preferred_config_timer_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Closure to be run when DoApplyPreferredMonitorsConfig() is called. Used for
  // testing only.
  base::OnceClosure on_trying_to_apply_preferred_monitors_config_for_testing_;

  // Flag to allow disabling the ignore-fractional-scale behavior for testing.
  // See comments in DoApplyPreferredMonitorsConfig().
  bool ignore_fractional_scales_in_multimon_ = true;

  // Used to set the text-scaling-factor.
  ScopedGObject<GSettings> registry_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDesktopResizer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
