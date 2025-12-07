// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_display_config.h"

#include <glib.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/hash/hash.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "remoting/base/logging.h"
#include "third_party/webrtc/modules/portal/scoped_glib.h"
#include "ui/gfx/geometry/rect.h"

namespace remoting {

// static
webrtc::ScreenId GnomeDisplayConfig::GetScreenId(
    std::string_view monitor_name) {
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
  // with the Meta- displays. On 64-bit machines, the hash value is 32-bit while
  // the screen ID is 64 bit so there won't be overflow issues. On 32-bit
  // machines, we can't do this trick, but the likelihood of a collision is
  // still pretty low.
  webrtc::ScreenId screen_id_adjustment = 0;
  if constexpr (sizeof(webrtc::ScreenId) >= sizeof(int64_t)) {
    screen_id_adjustment = 1ULL << 32;
  }
  return base::PersistentHash(monitor_name) + screen_id_adjustment;
}

////////////////////////////////////////////////////////////////////////////////
// GnomeDisplayConfig::MonitorInfo

GnomeDisplayConfig::MonitorMode::MonitorMode() = default;
GnomeDisplayConfig::MonitorMode::MonitorMode(const MonitorMode&) = default;
GnomeDisplayConfig::MonitorMode& GnomeDisplayConfig::MonitorMode::operator=(
    const MonitorMode&) = default;
GnomeDisplayConfig::MonitorMode::~MonitorMode() = default;
bool GnomeDisplayConfig::MonitorMode::operator==(
    const MonitorMode& other) const = default;

GnomeDisplayConfig::MonitorInfo::MonitorInfo() = default;
GnomeDisplayConfig::MonitorInfo::MonitorInfo(
    const GnomeDisplayConfig::MonitorInfo&) = default;
GnomeDisplayConfig::MonitorInfo& GnomeDisplayConfig::MonitorInfo::operator=(
    const GnomeDisplayConfig::MonitorInfo&) = default;
GnomeDisplayConfig::MonitorInfo::~MonitorInfo() = default;
bool GnomeDisplayConfig::MonitorInfo::operator==(
    const GnomeDisplayConfig::MonitorInfo& other) const = default;

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
    webrtc::Scoped<GVariantIter> supported_scales_iter;
    webrtc::Scoped<GVariant> mode_properties;
    if (!g_variant_iter_next(modes.get(), "(siiddad@a{sv})", mode_id.receive(),
                             &mode_width, &mode_height, &mode_refresh,
                             /*preferred_scale=*/nullptr,
                             supported_scales_iter.receive(),
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

    gdouble scale;
    while (g_variant_iter_next(supported_scales_iter.get(), "d", &scale)) {
      mode.supported_scales.push_back(scale);
    }

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

  GVariantBuilder properties_builder;
  g_variant_builder_init(&properties_builder, G_VARIANT_TYPE("a{sv}"));
  g_variant_builder_add(&properties_builder, "{sv}", "layout-mode",
                        g_variant_new_uint32(base::to_underlying(layout_mode)));

  return TakeGVariant(g_variant_new(
      "(uua(iiduba(ssa{sv}))a{sv})", serial, base::to_underlying(method),
      &logical_monitors_builder, &properties_builder));
}

std::map<std::string, GnomeDisplayConfig::MonitorInfo>::iterator
GnomeDisplayConfig::FindMonitor(webrtc::ScreenId screen_id) {
  return std::ranges::find_if(monitors, [screen_id](const auto& kv) {
    return GnomeDisplayConfig::GetScreenId(kv.first) == screen_id;
  });
}

void GnomeDisplayConfig::SwitchLayoutMode(LayoutMode new_layout_mode) {
  if (layout_mode != new_layout_mode) {
    LayoutInfo info = GetLayoutInfo();
    info.layout_mode = new_layout_mode;
    Relayout(info);
  }
}

void GnomeDisplayConfig::Relayout(const LayoutInfo& layout_info) {
  if (monitors.size() <= 1) {
    // No need to recalculate monitor offset since it's always (0, 0).
    layout_mode = layout_info.layout_mode;
    return;
  }

  if (layout_info.direction == LayoutDirection::kVertical) {
    PackVertically(layout_info.layout_mode, layout_info.alignment);
  } else {
    Transpose();
    PackVertically(layout_info.layout_mode, layout_info.alignment);
    Transpose();
  }
  NormalizeMonitorOffsets();
}

void GnomeDisplayConfig::RemoveInvalidMonitors() {
  std::erase_if(monitors,
                [](const auto& kv) { return !kv.second.GetCurrentMode(); });
}

GnomeDisplayConfig::LayoutInfo GnomeDisplayConfig::GetLayoutInfo() const {
  LayoutDirection direction = GetLayoutDirection();
  return {layout_mode, direction, GetLayoutAlignment(direction)};
}

GnomeDisplayConfig::LayoutDirection GnomeDisplayConfig::GetLayoutDirection()
    const {
  if (monitors.size() <= 1) {
    return LayoutDirection::kHorizontal;
  }

  const MonitorInfo& monitor1 = monitors.begin()->second;
  const MonitorInfo& monitor2 = std::next(monitors.begin())->second;

  // Determine the layout of the first two monitors, per the algorithm at
  // //ui/gfx/x/x11_crtc_resizer.cc.
  int left1 = monitor1.x;
  int right1 = left1 + GetLayoutSize(monitor1, &MonitorMode::width);

  int left2 = monitor2.x;
  int right2 = left2 + GetLayoutSize(monitor2, &MonitorMode::width);
  return (right1 > left2 && right2 > left1) ? LayoutDirection::kVertical
                                            : LayoutDirection::kHorizontal;
}

GnomeDisplayConfig::LayoutAlignment GnomeDisplayConfig::GetLayoutAlignment(
    LayoutDirection direction) const {
  if (monitors.empty()) {
    return LayoutAlignment::kUnknown;
  }
  if (direction == LayoutDirection::kHorizontal) {
    const_cast<GnomeDisplayConfig*>(this)->Transpose();
  }
  bool is_left_aligned = true;
  bool is_middle_aligned = true;
  bool is_right_aligned = true;
  const MonitorInfo& first_monitor = monitors.begin()->second;
  auto first_monitor_left = first_monitor.x;
  auto first_monitor_layout_width =
      GetLayoutSize(first_monitor, &MonitorMode::width);
  auto first_monitor_middle =
      first_monitor_left + first_monitor_layout_width / 2;
  auto first_monitor_right = first_monitor_left + first_monitor_layout_width;
  for (const auto& [_, monitor_info] : monitors) {
    auto monitor_left = monitor_info.x;
    auto layout_width = GetLayoutSize(monitor_info, &MonitorMode::width);
    auto monitor_middle = monitor_info.x + layout_width / 2;
    auto monitor_right = monitor_info.x + layout_width;
    if (monitor_left != first_monitor_left) {
      is_left_aligned = false;
    }
    if (abs(monitor_middle - first_monitor_middle) > 1) {
      is_middle_aligned = false;
    }
    if (monitor_right != first_monitor_right) {
      is_right_aligned = false;
    }
  }
  if (direction == LayoutDirection::kHorizontal) {
    const_cast<GnomeDisplayConfig*>(this)->Transpose();
  }
  return is_left_aligned     ? LayoutAlignment::kStart
         : is_middle_aligned ? LayoutAlignment::kCenter
         : is_right_aligned  ? LayoutAlignment::kEnd
                             : LayoutAlignment::kUnknown;
}

void GnomeDisplayConfig::PackVertically(LayoutMode new_layout_mode,
                                        LayoutAlignment alignment) {
  DCHECK(!monitors.empty());

  std::vector<MonitorInfo*> monitor_list;
  monitor_list.reserve(monitors.size());
  for (auto& kv : monitors) {
    monitor_list.push_back(&kv.second);
  }

  // Sort vertically before packing.
  std::ranges::sort(monitor_list,
                    [](MonitorInfo* a, MonitorInfo* b) { return a->y < b->y; });

  // Pack the monitors by setting their y-offsets. If necessary, change the
  // x-offset for right-alignment.
  int current_y = 0;
  layout_mode = new_layout_mode;
  for (auto* monitor_info : monitor_list) {
    monitor_info->y = current_y;
    current_y += GetLayoutSize(*monitor_info, &MonitorMode::height);

    // Place all monitors, respecting any alignment preference. If there are
    // multiple possible alignments, prioritize left, then right, then middle.
    // TODO: crbug.com/40225767 - Implement a more sophisticated algorithm that
    // tries to preserve pairwise alignment. It is not enough to leave the
    // x-offsets unchanged here - this tends to result in the monitors being
    // arranged roughly diagonally, wasting lots of space. Some amount of
    // horizontal compression is needed to prevent this from happening.
    int layout_width = GetLayoutSize(*monitor_info, &MonitorMode::width);
    switch (alignment) {
      case LayoutAlignment::kStart:
        monitor_info->x = 0;
        break;
      case LayoutAlignment::kCenter:
        monitor_info->x = -layout_width / 2;
        break;
      case LayoutAlignment::kEnd:
        monitor_info->x = -layout_width;
        break;
      default:
        // The current implementation left-aligns the monitors if no other
        // alignment is detected.
        // TODO: crbug.com/40225767 - A future enhancement may be to detect and
        // report one of {left, middle, right, none}. The "none" case (for
        // vertical and horizontal layouts) could be treated as a
        // client-controlled layout, where the host does not attempt any
        // repositioning. In this case, the host could still support
        // resize-to-fit, but in a simplified way - resize would be allowed
        // whenever it creates no overlaps.
        monitor_info->x = 0;
        break;
    }
  }
}

void GnomeDisplayConfig::Transpose() {
  for (auto& [_, monitor_info] : monitors) {
    std::swap(monitor_info.x, monitor_info.y);
    for (auto& mode : monitor_info.modes) {
      std::swap(mode.width, mode.height);
    }
  }
}

void GnomeDisplayConfig::NormalizeMonitorOffsets() {
  gfx::Rect bounding_box;
  for (const auto& [_, monitor] : monitors) {
    bounding_box.Union(gfx::Rect(monitor.x, monitor.y,
                                 GetLayoutSize(monitor, &MonitorMode::width),
                                 GetLayoutSize(monitor, &MonitorMode::height)));
  }
  gfx::Vector2d adjustment =
      gfx::Vector2d(-bounding_box.origin().x(), -bounding_box.origin().y());
  if (adjustment.IsZero()) {
    return;
  }
  for (auto& [_, monitor] : monitors) {
    monitor.x += adjustment.x();
    monitor.y += adjustment.y();
  }
}

int GnomeDisplayConfig::GetLayoutSize(
    const MonitorInfo& monitor,
    int MonitorMode::* width_or_height) const {
  const MonitorMode* current_mode = monitor.GetCurrentMode();
  if (!current_mode) {
    LOG(WARNING) << "Cannot find current mode for monitor";
    return 0;
  }
  return current_mode->*width_or_height /
         (layout_mode == LayoutMode::kLogical ? monitor.scale : 1.0);
}

}  // namespace remoting
