// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_

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
  struct PendingMonitorConfig {
    webrtc::DesktopSize expected_resolution;
    webrtc::DesktopVector position;
    double scale;
  };

  void OnAddStreamResult(PipewireCaptureStreamManager::AddStreamResult result);

  void QueryDisplayInfo();
  void OnGnomeDisplayConfigReceived(GnomeDisplayConfig config);

  // Schedules an ApplyMonitorsConfig call to apply `current_display_config_` in
  // the next event loop iteration of the current sequence, if it hasn't been
  // called in the current event loop iteration yet. This is used to bundle
  // multiple display config changes and avoid race conditions.
  void ScheduleApplyMonitorsConfig();
  void DoApplyMonitorsConfig();

  base::WeakPtr<PipewireCaptureStreamManager> stream_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GnomeDisplayConfigDBusClient::Subscription>
      monitors_changed_subscription_;

  // Represents the current display config state, plus all pending changes this
  // class wants to apply. This may not match the actual state, if there are any
  // pending changes, or external changes that hasn't been sync'ed into this
  // class yet.
  GnomeDisplayConfig current_display_config_;
  bool apply_monitors_config_scheduled_ = false;

  // Pending monitors config to be applied.
  // Screen resolutions need to be changed on the pipewire stream, while monitor
  // positions and scales need to be changed with the Mutter DisplayConfig D-Bus
  // API. So we need to change the resolutions first, wait for the screen
  // resolutions of all monitors in the map match the value in
  // `expected_resolution`, then apply the config.
  // We can't use flat_map since we may remove elements during iteration.
  std::map<std::string /* monitor_name */, PendingMonitorConfig>
      pending_monitors_config_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<GnomeDesktopResizer> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_RESIZER_H_
