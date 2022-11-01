// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/gfx/font_render_params.h"

namespace blink {

// User preferences passed between the browser and renderer processes.
// See //third_party/blink/public/mojom/renderer_preferences.mojom for a
// description of what each field is about.
struct BLINK_COMMON_EXPORT RendererPreferences {
  bool can_accept_load_drops{true};
  bool should_antialias_text{true};
  gfx::FontRenderParams::Hinting hinting{gfx::FontRenderParams::HINTING_MEDIUM};
  bool use_autohinter{false};
  bool use_bitmaps{false};
  gfx::FontRenderParams::SubpixelRendering subpixel_rendering{
      gfx::FontRenderParams::SUBPIXEL_RENDERING_NONE};
  bool use_subpixel_positioning{false};
  uint32_t focus_ring_color{0xFFE59700};
  uint32_t active_selection_bg_color{0xFF1E90FF};
  uint32_t active_selection_fg_color{0xFFFFFFFF};
  uint32_t inactive_selection_bg_color{0xFFC8C8C8};
  uint32_t inactive_selection_fg_color{0xFF323232};
  bool browser_handles_all_top_level_requests{false};
  absl::optional<base::TimeDelta> caret_blink_interval;
  bool use_custom_colors{true};
  bool enable_referrers{true};
  bool allow_cross_origin_auth_prompt{false};
  bool enable_do_not_track{false};
  bool enable_encrypted_media{true};
  std::string webrtc_ip_handling_policy;
  uint16_t webrtc_udp_min_port{0};
  uint16_t webrtc_udp_max_port{0};
  std::vector<std::string> webrtc_local_ips_allowed_urls;
  bool webrtc_allow_legacy_tls_protocols{false};
  UserAgentOverride user_agent_override;
  std::string accept_languages;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::string system_font_family_name;
#endif
#if BUILDFLAG(IS_WIN)
  std::u16string caption_font_family_name;
  int32_t caption_font_height{0};
  std::u16string small_caption_font_family_name;
  int32_t small_caption_font_height{0};
  std::u16string menu_font_family_name;
  int32_t menu_font_height{0};
  std::u16string status_font_family_name;
  int32_t status_font_height{0};
  std::u16string message_font_family_name;
  int32_t message_font_height{0};
  int32_t vertical_scroll_bar_width_in_dips{0};
  int32_t horizontal_scroll_bar_height_in_dips{0};
  int32_t arrow_bitmap_height_vertical_scroll_bar_in_dips{0};
  int32_t arrow_bitmap_width_horizontal_scroll_bar_in_dips{0};
#endif
#if BUILDFLAG(IS_OZONE)
  bool selection_clipboard_buffer_available{false};
#endif
  bool plugin_fullscreen_allowed{true};
  bool caret_browsing_enabled{false};
  std::vector<uint16_t> explicitly_allowed_network_ports;

  RendererPreferences();
  RendererPreferences(const RendererPreferences& other);
  RendererPreferences(RendererPreferences&& other);
  ~RendererPreferences();
  RendererPreferences& operator=(const RendererPreferences& other);
  RendererPreferences& operator=(RendererPreferences&& other);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_RENDERER_PREFERENCES_RENDERER_PREFERENCES_H_
