// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_desktop_display_info_monitor.h"

#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"

namespace remoting {

namespace {

constexpr int kDefaultDPI = 96;

webrtc::ScreenId GetScreenId(std::string_view monitor_name) {
  static_assert(sizeof(webrtc::ScreenId) == sizeof(int64_t));
  // Mutter backend is hardcoded to return `Meta-$virtualId` as the monitor
  // name, where $virtualId is sequentially numbered starting at 0 and recycled
  // after the pipewire stream is destroyed.
  // See:
  // https://gitlab.gnome.org/GNOME/mutter/-/blob/51a3c7e8d3cce425a7617aee22c47b4e8c238871/src/backends/native/meta-output-virtual.c#L46
  constexpr std::string_view kMetaPrefix = "Meta-";
  if (monitor_name.starts_with(kMetaPrefix)) {
    int64_t screen_id;
    if (base::StringToInt64(monitor_name.substr(kMetaPrefix.length()),
                            &screen_id)) {
      return screen_id;
    }
  }
  // If in any case it doesn't match the pattern, we just use the hash of the
  // monitor name as the screen ID. We add 1<<32 so that it doesn't conflict
  // with the Meta- displays. The hash value is 32-bit while the screen ID is 64
  // bit so there won't be overflow issues.
  return base::PersistentHash(monitor_name) + (1ULL << 32);
}

}  // namespace

GnomeDesktopDisplayInfoMonitor::GnomeDesktopDisplayInfoMonitor(
    base::WeakPtr<GnomeDisplayConfigDBusClient> display_config_client)
    : display_config_client_(display_config_client) {}

GnomeDesktopDisplayInfoMonitor::~GnomeDesktopDisplayInfoMonitor() = default;

void GnomeDesktopDisplayInfoMonitor::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    monitors_changed_subscription_ =
        display_config_client_->SubscribeMonitorsChanged(base::BindRepeating(
            &GnomeDesktopDisplayInfoMonitor::QueryDisplayInfo,
            base::Unretained(this)));
  }
  QueryDisplayInfo();
}

void GnomeDesktopDisplayInfoMonitor::QueryDisplayInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (display_config_client_) {
    display_config_client_->GetMonitorsConfig(base::BindOnce(
        &GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived,
        base::Unretained(this)));
  }
}

void GnomeDesktopDisplayInfoMonitor::AddCallback(Callback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  callback_list_.AddUnsafe(std::move(callback));
}

void GnomeDesktopDisplayInfoMonitor::OnGnomeDisplayConfigReceived(
    GnomeDisplayConfig config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DesktopDisplayInfo info;
  for (const auto& [name, monitor] : config.monitors) {
    const GnomeDisplayConfig::MonitorMode* current_mode =
        monitor.GetCurrentMode();
    if (!current_mode) {
      LOG(WARNING) << "Monitor " << name
                   << " ignored because it has no current mode";
      continue;
    }
    info.AddDisplay(DisplayGeometry(GetScreenId(name), monitor.x, monitor.y,
                                    current_mode->width, current_mode->height,
                                    kDefaultDPI * monitor.scale, /*bpp=*/24,
                                    monitor.is_primary, name));
  }
  callback_list_.Notify(info);
}

}  // namespace remoting
