// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_
#define REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_

#include <gio/gio.h>

#include <map>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "remoting/host/linux/scoped_glib.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

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

  // Represents in what way logical monitors are laid out on the screen. The
  // layout mode can be either of the ones listed below.
  enum class LayoutMode : guint {
    // The dimension of a logical monitor is derived from the monitor modes
    // associated with it, then scaled using the logical monitor scale.
    kLogical = 1,

    // The dimension of a logical monitor is derived from the monitor modes
    // associated with it.
    kPhysical = 2,
  };

  // Monitor layout: horizontal or vertical.
  enum class LayoutDirection {
    kHorizontal,
    kVertical,
  };

  // Monitor alignment: start (left- or top-aligned), center, or end (right- or
  // bottom-aligned).
  enum class LayoutAlignment {
    kUnknown,
    kStart,
    kCenter,
    kEnd,
  };

  // Struct that includes all layout information that is needed for Relayout().
  struct LayoutInfo {
    LayoutMode layout_mode;
    LayoutDirection direction;
    LayoutAlignment alignment;
  };

  struct MonitorMode {
    MonitorMode();
    MonitorMode(const MonitorMode&);
    MonitorMode& operator=(const MonitorMode&);
    ~MonitorMode();
    bool operator==(const MonitorMode& other) const;

    std::string name;
    int width;
    int height;
    bool is_current;
    std::vector<double> supported_scales;
  };

  struct MonitorInfo {
    MonitorInfo();
    MonitorInfo(const MonitorInfo&);
    MonitorInfo& operator=(const MonitorInfo&);
    ~MonitorInfo();
    bool operator==(const MonitorInfo& other) const;

    // Returns the first mode with is_current set, or nullptr.
    const MonitorMode* GetCurrentMode() const;

    std::vector<MonitorMode> modes;
    int x = 0;
    int y = 0;
    double scale = 1.0;
    bool is_primary = false;
  };

  // Computes the screen ID for the given monitor name. The monitor name should
  // be the key of `monitors`.
  static webrtc::ScreenId GetScreenId(std::string_view monitor_name);

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

  std::map<std::string, MonitorInfo>::iterator FindMonitor(
      webrtc::ScreenId screen_id);

  // Switches to the specified layout mode and recalculates monitor offsets.
  // Note that we currently only support horizontal and vertical display
  // layouts.
  void SwitchLayoutMode(LayoutMode new_layout_mode);

  // Relayouts monitors. Closes gaps between monitors and removes overlaps. The
  // layout direction and alignment will be maintained. Note that we currently
  // only support horizontal and vertical display layouts.
  //
  // Typical usage:
  //
  // 1. Call GetLayoutInfo() on the display config prior to monitor resizing.
  // 2. Resize the monitor, which results in a new display config with an
  //    updated monitor size, and a broken layout.
  // 3. Call Relayout() with the layout info from step #1.
  void Relayout(const LayoutInfo& layout_info);

  // Remove all monitors that do not have a current mode.
  void RemoveInvalidMonitors();

  LayoutInfo GetLayoutInfo() const;

  // The serial number returned by GNOME. When applying a new monitor config,
  // GNOME will check that the serial number matches, to avoid race-conditions
  // from trying to modify a stale config.
  guint serial = 0;

  // This property is returned by the GNOME API. If true, GNOME will enforce
  // that all monitors have the same scale property.
  bool global_scale_required = false;

  // The "method" parameter to pass to the ApplyMonitorsConfig() API.
  Method method = Method::kPersistent;

  // The "layout-mode" property. Use SwitchLayoutMode() if you want to change
  // the layout mode and recalculate monitor offsets.
  LayoutMode layout_mode = LayoutMode::kLogical;

  std::map<std::string, MonitorInfo> monitors;

 private:
  LayoutDirection GetLayoutDirection() const;

  LayoutAlignment GetLayoutAlignment(LayoutDirection direction) const;

  // Stacks monitors vertically without gaps for the new layout mode. If they
  // are not all right-aligned, this will position the monitors against the left
  // edge of the desktop.
  // On return, the layout mode will become `new_layout_mode`.
  void PackVertically(LayoutMode new_layout_mode, LayoutAlignment alignment);

  // Transposes all the monitors by swapping x and y coordinates. This allows
  // the vertical layout code to be reused for horizontal layout.
  void Transpose();

  // Computes the bounding-box of the monitors. If the bounding-box is not
  // aligned at the origin, all the monitor offsets will be shifted by the same
  // amount so that the new bounding-box's top-left corner is at (0, 0).
  void NormalizeMonitorOffsets();

  // Returns the width or height for the current layout mode. For physical
  // layout, the returned value will be in physical pixels; for logical layout,
  // it will be in density-independent pixels.
  int GetLayoutSize(const MonitorInfo& monitor,
                    int MonitorMode::* width_or_height) const;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_DISPLAY_CONFIG_H_
