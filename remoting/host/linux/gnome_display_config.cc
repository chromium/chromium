// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config.h"

#include "base/ranges/algorithm.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"

namespace remoting {

////////////////////////////////////////////////////////////////////////////////
// GnomeDisplayConfig::MonitorInfo

GnomeDisplayConfig::MonitorInfo::MonitorInfo() = default;
GnomeDisplayConfig::MonitorInfo::MonitorInfo(
    const GnomeDisplayConfig::MonitorInfo&) = default;
GnomeDisplayConfig::MonitorInfo& GnomeDisplayConfig::MonitorInfo::operator=(
    const GnomeDisplayConfig::MonitorInfo&) = default;
GnomeDisplayConfig::MonitorInfo::~MonitorInfo() = default;

const GnomeDisplayConfig::MonitorMode*
GnomeDisplayConfig::MonitorInfo::GetCurrentMode() const {
  for (auto& mode : modes) {
    if (mode.is_current) {
      return &mode;
    }
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// GnomeDisplayConfig

GnomeDisplayConfig::GnomeDisplayConfig() = default;
GnomeDisplayConfig::GnomeDisplayConfig(const GnomeDisplayConfig&) = default;
GnomeDisplayConfig& GnomeDisplayConfig::operator=(const GnomeDisplayConfig&) =
    default;
GnomeDisplayConfig::~GnomeDisplayConfig() = default;

void GnomeDisplayConfig::AddMonitorFromVariant(GVariant* monitor) {
  // With the Xorg "video_dummy" driver, the "Connector" value is the name of
  // the X11 RANDR Output: "DUMMYnn".
  webrtc::Scoped<char> connector;
  webrtc::Scoped<GVariantIter> modes;
  constexpr char kMonitorFormat[] = "((ssss)a(siiddada{sv})a{sv})";
  if (!g_variant_check_format_string(monitor, kMonitorFormat,
                                     /*copy_only=*/FALSE)) {
    LOG(ERROR) << __func__ << " : monitor has incorrect type.";
    return;
  }

  g_variant_get(monitor, kMonitorFormat, connector.receive(),
                /*vendor=*/nullptr, /*product_name=*/nullptr,
                /*product_serial=*/nullptr, modes.receive(),
                /*properties=*/nullptr);

  MonitorInfo info;

  while (true) {
    webrtc::Scoped<char> mode_id;
    gint32 mode_width;
    gint32 mode_height;
    gdouble mode_refresh;
    webrtc::Scoped<GVariant> mode_properties;
    if (!g_variant_iter_next(modes.get(), "(siiddad@a{sv})", mode_id.receive(),
                             &mode_width, &mode_height, &mode_refresh,
                             /*preferred_scale=*/nullptr,
                             /*supported_scales=*/nullptr,
                             mode_properties.receive())) {
      break;
    }

    MonitorMode mode;
    mode.name = mode_id.get();
    mode.width = mode_width;
    mode.height = mode_height;
    gboolean is_current = FALSE;
    g_variant_lookup(mode_properties.get(), "is-current", "b", &is_current);
    mode.is_current = is_current;
    info.modes.push_back(std::move(mode));
  }

  // Each connector name should be unique, since it is used as an
  // identifier by the ApplyMonitorsConfig DBus API. Since this information
  // comes from an external API, it is better to cleanly overwrite any
  // previous value, rather than risk duplicating some monitor-modes.
  monitors[connector.get()] = info;
}

void GnomeDisplayConfig::AddLogicalMonitorFromVariant(
    GVariant* logical_monitor) {
  gint32 x;
  gint32 y;
  gdouble scale;
  gboolean primary;
  webrtc::Scoped<GVariantIter> monitors_iter;
  constexpr char kLogicalMonitorFormat[] = "(iiduba(ssss)a{sv})";
  if (!g_variant_check_format_string(logical_monitor, kLogicalMonitorFormat,
                                     /*copy_only=*/FALSE)) {
    LOG(ERROR) << __func__ << " : logical_monitor has incorrect type.";
    return;
  }

  g_variant_get(logical_monitor, kLogicalMonitorFormat, &x, &y, &scale,
                /*rotation=*/nullptr, &primary, monitors_iter.receive(),
                /*properties=*/nullptr);
  gsize num_monitors = g_variant_iter_n_children(monitors_iter.get());
  if (num_monitors != 1) {
    LOG(ERROR) << "Logical monitor has unexpected number of monitors: "
               << num_monitors;
    return;
  }

  webrtc::Scoped<char> connector;
  bool result = g_variant_iter_next(monitors_iter.get(), "(ssss)",
                                    connector.receive(), /*vendor=*/nullptr,
                                    /*product=*/nullptr, /*serial=*/nullptr);
  if (!result) {
    LOG(ERROR) << "Failed to read monitor properties.";
    return;
  }

  MonitorInfo& info = monitors[connector.get()];
  info.x = x;
  info.y = y;
  info.scale = scale;
  info.is_primary = primary;
}

ScopedGVariant GnomeDisplayConfig::BuildMonitorsConfigParameters() const {
  GVariantBuilder logical_monitors_builder;
  g_variant_builder_init(&logical_monitors_builder,
                         G_VARIANT_TYPE("a(iiduba(ssa{sv}))"));
  for (const auto& [id, monitor] : monitors) {
    const auto* mode = monitor.GetCurrentMode();
    if (!mode) {
      // This monitor is disabled, and should be skipped. GNOME will disable
      // monitors that are not provided in the ApplyMonitorConfig() request.
      continue;
    }

    gint x = monitor.x;
    gint y = monitor.y;
    gdouble scale = monitor.scale;
    guint transform = 0;  // No rotation/reflection.
    gboolean is_primary = monitor.is_primary;
    GVariantBuilder monitor_list_builder;
    g_variant_builder_init(&monitor_list_builder, G_VARIANT_TYPE("a(ssa{sv})"));

    const gchar* connector = id.c_str();
    const gchar* mode_id = mode->name.c_str();
    g_variant_builder_add(&monitor_list_builder, "(ssa{sv})", connector,
                          mode_id, nullptr);

    g_variant_builder_add(&logical_monitors_builder, "(iiduba(ssa{sv}))", x, y,
                          scale, transform, is_primary, &monitor_list_builder);
  }

  return TakeGVariant(g_variant_new("(uua(iiduba(ssa{sv}))a{sv})", serial,
                                    base::to_underlying(method),
                                    &logical_monitors_builder,
                                    /*properties=*/nullptr));
}

}  // namespace remoting
