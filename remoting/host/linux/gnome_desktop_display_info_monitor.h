// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DESKTOP_DISPLAY_INFO_MONITOR_H_
#define REMOTING_HOST_LINUX_GNOME_DESKTOP_DISPLAY_INFO_MONITOR_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "remoting/host/desktop_display_info_monitor.h"
#include "remoting/host/linux/gnome_display_config_monitor.h"

namespace remoting {

class GnomeDesktopDisplayInfoMonitor : public DesktopDisplayInfoMonitor {
 public:
  explicit GnomeDesktopDisplayInfoMonitor(
      base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor);
  ~GnomeDesktopDisplayInfoMonitor() override;

  GnomeDesktopDisplayInfoMonitor(const GnomeDesktopDisplayInfoMonitor&) =
      delete;
  GnomeDesktopDisplayInfoMonitor& operator=(
      const GnomeDesktopDisplayInfoMonitor&) = delete;

  // DesktopDisplayInfoMonitor implementation.
  void Start() override;
  void QueryDisplayInfo() override;
  void AddCallback(Callback callback) override;

 private:
  void OnGnomeDisplayConfigReceived(const GnomeDisplayConfig& config);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<GnomeDisplayConfigMonitor> display_config_monitor_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<GnomeDisplayConfigMonitor::Subscription>
      monitors_changed_subscription_;

  // Callbacks which receive DesktopDisplayInfo updates.
  base::RepeatingCallbackList<CallbackSignature> callback_list_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DESKTOP_DISPLAY_INFO_MONITOR_H_
