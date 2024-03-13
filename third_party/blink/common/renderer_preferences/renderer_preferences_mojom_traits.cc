// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/renderer_preferences/renderer_preferences_mojom_traits.h"

#include <string>

#include "build/build_config.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom-shared.h"

namespace mojo {

bool StructTraits<blink::mojom::RendererPreferencesDataView,
                  ::blink::RendererPreferences>::
    Read(blink::mojom::RendererPreferencesDataView data,
         ::blink::RendererPreferences* out) {
  out->can_accept_load_drops = data.can_accept_load_drops();
  out->should_antialias_text = data.should_antialias_text();

  if (!data.ReadHinting(&out->hinting))
    return false;
  out->use_autohinter = data.use_autohinter();

  out->use_bitmaps = data.use_bitmaps();

  if (!data.ReadSubpixelRendering(&out->subpixel_rendering))
    return false;
  out->use_subpixel_positioning = data.use_subpixel_positioning();

#if BUILDFLAG(IS_WIN)
  out->text_contrast = data.text_contrast();
  out->text_gamma = data.text_gamma();
#endif  // BUILDFLAG(IS_WIN)

  out->focus_ring_color = data.focus_ring_color();
  out->active_selection_bg_color = data.active_selection_bg_color();
  out->active_selection_fg_color = data.active_selection_fg_color();
  out->inactive_selection_bg_color = data.inactive_selection_bg_color();
  out->inactive_selection_fg_color = data.inactive_selection_fg_color();

  out->browser_handles_all_top_level_requests =
      data.browser_handles_all_top_level_requests();

  if (!data.ReadCaretBlinkInterval(&out->caret_blink_interval))
    return false;

  out->use_custom_colors = data.use_custom_colors();
  out->enable_referrers = data.enable_referrers();
  out->allow_cross_origin_auth_prompt = data.allow_cross_origin_auth_prompt();
  out->enable_do_not_track = data.enable_do_not_track();
  out->enable_encrypted_media = data.enable_encrypted_media();

  if (!data.ReadWebrtcIpHandlingPolicy(&out->webrtc_ip_handling_policy))
    return false;

  out->webrtc_udp_min_port = data.webrtc_udp_min_port();
  out->webrtc_udp_max_port = data.webrtc_udp_max_port();

  if (!data.ReadWebrtcLocalIpsAllowedUrls(&out->webrtc_local_ips_allowed_urls))
    return false;

  if (!data.ReadUserAgentOverride(&out->user_agent_override))
    return false;

  if (!data.ReadAcceptLanguages(&out->accept_languages))
    return false;

  out->send_subresource_notification = data.send_subresource_notification();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (!data.ReadSystemFontFamilyName(&out->system_font_family_name))
    return false;
#endif
#if BUILDFLAG(IS_WIN)
  if (!data.ReadCaptionFontFamilyName(&out->caption_font_family_name))
    return false;
  out->caption_font_height = data.caption_font_height();

  if (!data.ReadSmallCaptionFontFamilyName(
          &out->small_caption_font_family_name))
    return false;
  out->small_caption_font_height = data.small_caption_font_height();

  if (!data.ReadMenuFontFamilyName(&out->menu_font_family_name))
    return false;
  out->menu_font_height = data.menu_font_height();

  if (!data.ReadStatusFontFamilyName(&out->status_font_family_name))
    return false;
  out->status_font_height = data.status_font_height();

  if (!data.ReadMessageFontFamilyName(&out->message_font_family_name))
    return false;
  out->message_font_height = data.message_font_height();

  out->vertical_scroll_bar_width_in_dips =
      data.vertical_scroll_bar_width_in_dips();
  out->horizontal_scroll_bar_height_in_dips =
      data.horizontal_scroll_bar_height_in_dips();
  out->arrow_bitmap_height_vertical_scroll_bar_in_dips =
      data.arrow_bitmap_height_vertical_scroll_bar_in_dips();
  out->arrow_bitmap_width_horizontal_scroll_bar_in_dips =
      data.arrow_bitmap_width_horizontal_scroll_bar_in_dips();
#endif
#if BUILDFLAG(IS_OZONE)
  out->selection_clipboard_buffer_available =
      data.selection_clipboard_buffer_available();
#endif
  out->plugin_fullscreen_allowed = data.plugin_fullscreen_allowed();
  out->caret_browsing_enabled = data.caret_browsing_enabled();

  if (!data.ReadExplicitlyAllowedNetworkPorts(
          &out->explicitly_allowed_network_ports)) {
    return false;
  }

  out->prefixed_fullscreen_video_api_availability =
      data.prefixed_fullscreen_video_api_availability();

  return true;
}

}  // namespace mojo
