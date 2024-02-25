// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_

#include <gio/gio.h>

#include <map>
#include <string>
#include <vector>

#include "remoting/host/linux/scoped_glib.h"

namespace remoting {

// This object stores information returned from GNOME's D-Bus API:
// org.gnome.Mutter.DisplayConfig.GetCurrentState(). The API returns information
// as GObject types (GVariant and so on), and this class provides helper
// methods to convert this to C++ types. The caller can read and modify this C++
// data directly. A helper method can then be used to build a GVariant that can
// be passed to the D-Bus method:
// org.gnome.Mutter.DisplayConfig.ApplyMonitorsConfig().
struct GnomeDisplayConfig {
  // Type of the "method" parameter in the ApplyMonitorsConfig() API. The
  // valid values are documented in the XML schema.
  enum class Method : guint {
    kVerify = 0,
    kTemporary = 1,
    kPersistent = 2,
  };

  struct MonitorMode {
    std::string name;
    int width;
    int height;
    bool is_current;
  };

  struct MonitorInfo {
    MonitorInfo();
    MonitorInfo(const MonitorInfo&);
    MonitorInfo& operator=(const MonitorInfo&);
    ~MonitorInfo();

    // Returns the first mode with is_current set, or nullptr.
    const MonitorMode* GetCurrentMode() const;

    std::vector<MonitorMode> modes;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    bool is_primary = false;
  };

  GnomeDisplayConfig();
  GnomeDisplayConfig(const GnomeDisplayConfig&);
  GnomeDisplayConfig& operator=(const GnomeDisplayConfig&);
  ~GnomeDisplayConfig();

  // Called for each entry of the 'monitors' property returned by D-Bus
  // GetCurrentState().
  void AddMonitorFromVariant(GVariant* monitor);

  // Called for each entry of the 'logical_monitors' property returned by D-Bus
  // GetCurrentState(). Each logical monitor is assumed to have a single
  // physical monitor (identified by connector-name), previously added by
  // AddMonitorFromVariant().
  void AddLogicalMonitorFromVariant(GVariant* logical_monitor);

  // This converts the C++ data into a GVariant `logical_monitors` parameter
  // to pass to D-Bus ApplyMonitorsConfig().
  ScopedGVariant BuildMonitorsConfigParameters() const;

  // The serial number returned by GNOME. When applying a new monitor config,
  // GNOME will check that the serial number matches, to avoid race-conditions
  // from trying to modify a stale config.
  guint serial = 0;

  // This property is returned by the GNOME API. If true, GNOME will enforce
  // that all monitors have the same scale property.
  bool global_scale_required = false;

  // The "method" parameter to pass to the ApplyMonitorsConfig() API.
  Method method = Method::kPersistent;

  std::map<std::string, MonitorInfo> monitors;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_
