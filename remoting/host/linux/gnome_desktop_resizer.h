// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

#include <gio/gio.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/gnome_display_config.h"
#include "remoting/host/linux/gnome_display_config_dbus_client.h"
#include "remoting/host/linux/pipewire_capture_stream_manager.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/base/glib/scoped_gobject.h"

namespace remoting {

class GnomeDesktopResizer : public DesktopResizer {
 public:
  GnomeDesktopResizer(
      base::WeakPtr<PipewireCaptureStreamManager> stream_manager,
      base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client);
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

 private:
  struct PreferredMonitorConfig {
    webrtc::DesktopSize expected_resolution;
    webrtc::DesktopVector position;
    double scale;
  };

  void OnAddStreamResult(PipewireCaptureStreamManager::AddStreamResult result);

  void QueryDisplayInfo();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);

  // Schedules an ApplyMonitorsConfig call to apply `preferred_monitors_config_`
  // in the next event loop iteration of the current sequence, if it hasn't been
  // called in the current event loop iteration yet. This is used to bundle
  // multiple display config changes and avoid race conditions.
  void ScheduleApplyPreferredMonitorsConfig();
  void DoApplyPreferredMonitorsConfig();

  double GetTextScalingFactor() const;

  base::WeakPtr<PipewireCaptureStreamManager> stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GnomeDisplayConfigDBusClient::Subscription>
      monitors_changed_subscription_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Represents the latest known display config state reported by Mutter. This
  // may potentially not be up-to-date, and does not include any pending
  // changes.
  GnomeDisplayConfig current_display_config_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool apply_monitors_config_scheduled_ GUARDED_BY_CONTEXT(sequence_checker_) =
      false;

  // Preferred monitors config, which may or may be be reflected in
  // `current_display_config_`. This field is used to:
  //
  // 1. Store pending config so that it can be applied later to prevent race
  //    conditions. For example, we wait for screen resolution changes via
  //    pipewire to be reflected in the display config before we apply display
  //    scales or offsets.
  // 2. Make adjustments if the display config has changed externally. For
  //    example, if the preferred scale for the primary display is 2 and we have
  //    previously set the text scale to 2, if the user manually sets the
  //    monitor scale to 2, then we use this map to look up the preferred scale
  //    and change the text scale to 1 so that the combined scale is 2.
  //
  // We can't use flat_map since we may remove elements during iteration.
  std::map<std::string /* monitor_name */, PreferredMonitorConfig>
      preferred_monitors_config_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to set the text-scaling-factor.
  ScopedGObject<GSettings> registry_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDesktopResizer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
