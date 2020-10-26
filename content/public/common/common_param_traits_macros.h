// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or Multiply-included shared traits file depending on circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
#define CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "content/public/common/browser_controls_state.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/referrer.h"
#include "content/public/common/web_preferences.h"
#include "content/public/common/webplugininfo_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/cpp/network_ipc_param_traits.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/common/page/web_drag_operation.h"
#include "third_party/blink/public/common/security/security_style.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "third_party/blink/public/platform/web_history_scroll_restoration_type.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "ui/accessibility/ax_param_traits.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/transform.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(content::BrowserControlsState,
                          content::BROWSER_CONTROLS_STATE_LAST)

IPC_ENUM_TRAITS_VALIDATE(ui::PageTransition,
                         ((value &
                           ui::PageTransition::PAGE_TRANSITION_CORE_MASK) <=
                          ui::PageTransition::PAGE_TRANSITION_LAST_CORE))
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::ConsoleMessageLevel,
                          blink::mojom::ConsoleMessageLevel::kError)
IPC_ENUM_TRAITS_MAX_VALUE(network::mojom::ReferrerPolicy,
                          network::mojom::ReferrerPolicy::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebHistoryScrollRestorationType,
                          blink::kWebHistoryScrollRestorationManual)
IPC_ENUM_TRAITS_MAX_VALUE(blink::SecurityStyle, blink::SecurityStyle::kMaxValue)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::PermissionStatus,
                          blink::mojom::PermissionStatus::LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::EditingBehavior,
                          content::EDITING_BEHAVIOR_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(WindowOpenDisposition,
                          WindowOpenDisposition::MAX_VALUE)
IPC_ENUM_TRAITS_MAX_VALUE(blink::mojom::V8CacheOptions,
                          blink::mojom::V8CacheOptions::kMaxValue)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::PointerType,
                              ui::POINTER_TYPE_FIRST,
                              ui::POINTER_TYPE_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(ui::HoverType,
                              ui::HOVER_TYPE_FIRST,
                              ui::HOVER_TYPE_LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::ImageAnimationPolicy,
                              content::IMAGE_ANIMATION_POLICY_ALLOWED,
                              content::IMAGE_ANIMATION_POLICY_NO_ANIMATION)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::ViewportStyle,
                              content::ViewportStyle::DEFAULT,
                              content::ViewportStyle::LAST)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(
    content::AutoplayPolicy,
    content::AutoplayPolicy::kNoUserGestureRequired,
    content::AutoplayPolicy::kDocumentUserActivationRequired)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::PreferredColorScheme,
                              blink::PreferredColorScheme::kDark,
                              blink::PreferredColorScheme::kMaxValue)

IPC_STRUCT_TRAITS_BEGIN(blink::WebRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::Referrer)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(policy)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::WebPreferences)
  IPC_STRUCT_TRAITS_MEMBER(standard_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fixed_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(sans_serif_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(cursive_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(fantasy_font_family_map)
  IPC_STRUCT_TRAITS_MEMBER(default_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_fixed_font_size)
  IPC_STRUCT_TRAITS_MEMBER(minimum_font_size)
  IPC_STRUCT_TRAITS_MEMBER(minimum_logical_font_size)
  IPC_STRUCT_TRAITS_MEMBER(default_encoding)
  IPC_STRUCT_TRAITS_MEMBER(context_menu_on_mouse_up)
  IPC_STRUCT_TRAITS_MEMBER(javascript_enabled)
  IPC_STRUCT_TRAITS_MEMBER(web_security_enabled)
  IPC_STRUCT_TRAITS_MEMBER(loads_images_automatically)
  IPC_STRUCT_TRAITS_MEMBER(images_enabled)
  IPC_STRUCT_TRAITS_MEMBER(plugins_enabled)
  IPC_STRUCT_TRAITS_MEMBER(dom_paste_enabled)
  IPC_STRUCT_TRAITS_MEMBER(shrinks_standalone_images_to_fit)
  IPC_STRUCT_TRAITS_MEMBER(text_areas_are_resizable)
  IPC_STRUCT_TRAITS_MEMBER(allow_scripts_to_close_windows)
  IPC_STRUCT_TRAITS_MEMBER(remote_fonts_enabled)
  IPC_STRUCT_TRAITS_MEMBER(javascript_can_access_clipboard)
  IPC_STRUCT_TRAITS_MEMBER(xslt_enabled)
  IPC_STRUCT_TRAITS_MEMBER(dns_prefetching_enabled)
  IPC_STRUCT_TRAITS_MEMBER(data_saver_enabled)
  IPC_STRUCT_TRAITS_MEMBER(data_saver_holdback_web_api_enabled)
  IPC_STRUCT_TRAITS_MEMBER(local_storage_enabled)
  IPC_STRUCT_TRAITS_MEMBER(databases_enabled)
  IPC_STRUCT_TRAITS_MEMBER(application_cache_enabled)
  IPC_STRUCT_TRAITS_MEMBER(tabs_to_links)
  IPC_STRUCT_TRAITS_MEMBER(disable_ipc_flooding_protection)
  IPC_STRUCT_TRAITS_MEMBER(hyperlink_auditing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_universal_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(allow_file_access_from_file_urls)
  IPC_STRUCT_TRAITS_MEMBER(webgl1_enabled)
  IPC_STRUCT_TRAITS_MEMBER(webgl2_enabled)
  IPC_STRUCT_TRAITS_MEMBER(pepper_3d_enabled)
  IPC_STRUCT_TRAITS_MEMBER(record_whole_document)
  IPC_STRUCT_TRAITS_MEMBER(flash_3d_enabled)
  IPC_STRUCT_TRAITS_MEMBER(flash_stage3d_enabled)
  IPC_STRUCT_TRAITS_MEMBER(flash_stage3d_baseline_enabled)
  IPC_STRUCT_TRAITS_MEMBER(privileged_webgl_extensions_enabled)
  IPC_STRUCT_TRAITS_MEMBER(webgl_errors_to_console_enabled)
  IPC_STRUCT_TRAITS_MEMBER(hide_scrollbars)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_2d_canvas_enabled)
  IPC_STRUCT_TRAITS_MEMBER(new_canvas_2d_api_enabled)
  IPC_STRUCT_TRAITS_MEMBER(antialiased_2d_canvas_disabled)
  IPC_STRUCT_TRAITS_MEMBER(antialiased_clips_2d_canvas_enabled)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_filters_enabled)
  IPC_STRUCT_TRAITS_MEMBER(deferred_filters_enabled)
  IPC_STRUCT_TRAITS_MEMBER(container_culling_enabled)
  IPC_STRUCT_TRAITS_MEMBER(allow_running_insecure_content)
  IPC_STRUCT_TRAITS_MEMBER(disable_reading_from_canvas)
  IPC_STRUCT_TRAITS_MEMBER(strict_mixed_content_checking)
  IPC_STRUCT_TRAITS_MEMBER(strict_powerful_feature_restrictions)
  IPC_STRUCT_TRAITS_MEMBER(allow_geolocation_on_insecure_origins)
  IPC_STRUCT_TRAITS_MEMBER(strictly_block_blockable_mixed_content)
  IPC_STRUCT_TRAITS_MEMBER(block_mixed_plugin_content)
  IPC_STRUCT_TRAITS_MEMBER(enable_scroll_animator)
  IPC_STRUCT_TRAITS_MEMBER(prefers_reduced_motion)
  IPC_STRUCT_TRAITS_MEMBER(password_echo_enabled)
  IPC_STRUCT_TRAITS_MEMBER(should_clear_document_background)
  IPC_STRUCT_TRAITS_MEMBER(touch_event_feature_detection_enabled)
  IPC_STRUCT_TRAITS_MEMBER(pointer_events_max_touch_points)
  IPC_STRUCT_TRAITS_MEMBER(available_pointer_types)
  IPC_STRUCT_TRAITS_MEMBER(primary_pointer_type)
  IPC_STRUCT_TRAITS_MEMBER(available_hover_types)
  IPC_STRUCT_TRAITS_MEMBER(primary_hover_type)
  IPC_STRUCT_TRAITS_MEMBER(barrel_button_for_drag_enabled)
  IPC_STRUCT_TRAITS_MEMBER(sync_xhr_in_documents_enabled)
  IPC_STRUCT_TRAITS_MEMBER(number_of_cpu_cores)
  IPC_STRUCT_TRAITS_MEMBER(editing_behavior)
  IPC_STRUCT_TRAITS_MEMBER(supports_multiple_windows)
  IPC_STRUCT_TRAITS_MEMBER(viewport_enabled)
  IPC_STRUCT_TRAITS_MEMBER(viewport_meta_enabled)
  IPC_STRUCT_TRAITS_MEMBER(shrinks_viewport_contents_to_fit)
  IPC_STRUCT_TRAITS_MEMBER(viewport_style)
  IPC_STRUCT_TRAITS_MEMBER(smooth_scroll_for_find_enabled)
  IPC_STRUCT_TRAITS_MEMBER(main_frame_resizes_are_orientation_changes)
  IPC_STRUCT_TRAITS_MEMBER(initialize_at_minimum_page_scale)
  IPC_STRUCT_TRAITS_MEMBER(smart_insert_delete_enabled)
  IPC_STRUCT_TRAITS_MEMBER(cookie_enabled)
  IPC_STRUCT_TRAITS_MEMBER(navigate_on_drag_drop)
  IPC_STRUCT_TRAITS_MEMBER(spatial_navigation_enabled)
  IPC_STRUCT_TRAITS_MEMBER(v8_cache_options)
  IPC_STRUCT_TRAITS_MEMBER(accelerated_video_decode_enabled)
  IPC_STRUCT_TRAITS_MEMBER(animation_policy)
  IPC_STRUCT_TRAITS_MEMBER(user_gesture_required_for_presentation)
  IPC_STRUCT_TRAITS_MEMBER(text_tracks_enabled)
  IPC_STRUCT_TRAITS_MEMBER(text_track_background_color)
  IPC_STRUCT_TRAITS_MEMBER(text_track_margin_percentage)
  IPC_STRUCT_TRAITS_MEMBER(text_track_text_color)
  IPC_STRUCT_TRAITS_MEMBER(text_track_text_size)
  IPC_STRUCT_TRAITS_MEMBER(text_track_text_shadow)
  IPC_STRUCT_TRAITS_MEMBER(text_track_font_family)
  IPC_STRUCT_TRAITS_MEMBER(text_track_font_style)
  IPC_STRUCT_TRAITS_MEMBER(text_track_font_variant)
  IPC_STRUCT_TRAITS_MEMBER(text_track_window_color)
  IPC_STRUCT_TRAITS_MEMBER(text_track_window_padding)
  IPC_STRUCT_TRAITS_MEMBER(text_track_window_radius)
  IPC_STRUCT_TRAITS_MEMBER(text_autosizing_enabled)
  IPC_STRUCT_TRAITS_MEMBER(double_tap_to_zoom_enabled)
  IPC_STRUCT_TRAITS_MEMBER(dont_send_key_events_to_javascript)
  IPC_STRUCT_TRAITS_MEMBER(web_app_scope)
  IPC_STRUCT_TRAITS_MEMBER(fullscreen_supported)
#if defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(font_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_adjustment)
  IPC_STRUCT_TRAITS_MEMBER(force_enable_zoom)
  IPC_STRUCT_TRAITS_MEMBER(default_video_poster_url)
  IPC_STRUCT_TRAITS_MEMBER(support_deprecated_target_density_dpi)
  IPC_STRUCT_TRAITS_MEMBER(use_legacy_background_size_shorthand_behavior)
  IPC_STRUCT_TRAITS_MEMBER(wide_viewport_quirk)
  IPC_STRUCT_TRAITS_MEMBER(use_wide_viewport)
  IPC_STRUCT_TRAITS_MEMBER(force_zero_layout_height)
  IPC_STRUCT_TRAITS_MEMBER(viewport_meta_merge_content_quirk)
  IPC_STRUCT_TRAITS_MEMBER(viewport_meta_non_user_scalable_quirk)
  IPC_STRUCT_TRAITS_MEMBER(viewport_meta_zero_values_quirk)
  IPC_STRUCT_TRAITS_MEMBER(clobber_user_agent_initial_scale_quirk)
  IPC_STRUCT_TRAITS_MEMBER(ignore_main_frame_overflow_hidden_quirk)
  IPC_STRUCT_TRAITS_MEMBER(report_screen_size_in_physical_pixels_quirk)
  IPC_STRUCT_TRAITS_MEMBER(reuse_global_for_unowned_main_frame)
  IPC_STRUCT_TRAITS_MEMBER(spellcheck_enabled_by_default)
  IPC_STRUCT_TRAITS_MEMBER(video_fullscreen_orientation_lock_enabled)
  IPC_STRUCT_TRAITS_MEMBER(video_rotate_to_fullscreen_enabled)
  IPC_STRUCT_TRAITS_MEMBER(embedded_media_experience_enabled)
  IPC_STRUCT_TRAITS_MEMBER(immersive_mode_enabled)
  IPC_STRUCT_TRAITS_MEMBER(css_hex_alpha_color_enabled)
  IPC_STRUCT_TRAITS_MEMBER(scroll_top_left_interop_enabled)
  IPC_STRUCT_TRAITS_MEMBER(disable_features_depending_on_viz)
  IPC_STRUCT_TRAITS_MEMBER(disable_accelerated_small_canvases)
  IPC_STRUCT_TRAITS_MEMBER(reenable_web_components_v0)
#endif  // defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(force_dark_mode_enabled)
  IPC_STRUCT_TRAITS_MEMBER(default_minimum_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(default_maximum_page_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(hide_download_ui)
  IPC_STRUCT_TRAITS_MEMBER(presentation_receiver)
  IPC_STRUCT_TRAITS_MEMBER(media_controls_enabled)
  IPC_STRUCT_TRAITS_MEMBER(do_not_update_selection_on_mutating_selection_range)
  IPC_STRUCT_TRAITS_MEMBER(autoplay_policy)
  IPC_STRUCT_TRAITS_MEMBER(preferred_color_scheme)
  IPC_STRUCT_TRAITS_MEMBER(low_priority_iframes_threshold)
  IPC_STRUCT_TRAITS_MEMBER(picture_in_picture_enabled)
  IPC_STRUCT_TRAITS_MEMBER(translate_service_available)
  IPC_STRUCT_TRAITS_MEMBER(network_quality_estimator_web_holdback)
  IPC_STRUCT_TRAITS_MEMBER(lazy_frame_loading_distance_thresholds_px)
  IPC_STRUCT_TRAITS_MEMBER(lazy_image_loading_distance_thresholds_px)
  IPC_STRUCT_TRAITS_MEMBER(lazy_image_first_k_fully_load)
  IPC_STRUCT_TRAITS_MEMBER(allow_mixed_content_upgrades)
  IPC_STRUCT_TRAITS_MEMBER(always_show_focus)
  IPC_STRUCT_TRAITS_MEMBER(touch_drag_drop_enabled)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::mojom::WindowFeatures)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(has_x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(has_y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(has_width)
  IPC_STRUCT_TRAITS_MEMBER(height)
  IPC_STRUCT_TRAITS_MEMBER(has_height)
  IPC_STRUCT_TRAITS_MEMBER(menu_bar_visible)
  IPC_STRUCT_TRAITS_MEMBER(status_bar_visible)
  IPC_STRUCT_TRAITS_MEMBER(tool_bar_visible)
  IPC_STRUCT_TRAITS_MEMBER(scrollbars_visible)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::Hinting,
                          gfx::FontRenderParams::HINTING_MAX)
IPC_ENUM_TRAITS_MAX_VALUE(gfx::FontRenderParams::SubpixelRendering,
                          gfx::FontRenderParams::SUBPIXEL_RENDERING_MAX)

IPC_STRUCT_TRAITS_BEGIN(blink::UserAgentBrandVersion)
  IPC_STRUCT_TRAITS_MEMBER(brand)
  IPC_STRUCT_TRAITS_MEMBER(major_version)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::UserAgentMetadata)
  IPC_STRUCT_TRAITS_MEMBER(brand_version_list)
  IPC_STRUCT_TRAITS_MEMBER(full_version)
  IPC_STRUCT_TRAITS_MEMBER(platform)
  IPC_STRUCT_TRAITS_MEMBER(platform_version)
  IPC_STRUCT_TRAITS_MEMBER(architecture)
  IPC_STRUCT_TRAITS_MEMBER(model)
  IPC_STRUCT_TRAITS_MEMBER(mobile)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::UserAgentOverride)
  IPC_STRUCT_TRAITS_MEMBER(ua_string_override)
  IPC_STRUCT_TRAITS_MEMBER(ua_metadata_override)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::mojom::RendererPreferences)
  IPC_STRUCT_TRAITS_MEMBER(can_accept_load_drops)
  IPC_STRUCT_TRAITS_MEMBER(should_antialias_text)
  IPC_STRUCT_TRAITS_MEMBER(hinting)
  IPC_STRUCT_TRAITS_MEMBER(use_autohinter)
  IPC_STRUCT_TRAITS_MEMBER(use_bitmaps)
  IPC_STRUCT_TRAITS_MEMBER(subpixel_rendering)
  IPC_STRUCT_TRAITS_MEMBER(use_subpixel_positioning)
  IPC_STRUCT_TRAITS_MEMBER(focus_ring_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(active_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_bg_color)
  IPC_STRUCT_TRAITS_MEMBER(inactive_selection_fg_color)
  IPC_STRUCT_TRAITS_MEMBER(browser_handles_all_top_level_requests)
  IPC_STRUCT_TRAITS_MEMBER(caret_blink_interval)
  IPC_STRUCT_TRAITS_MEMBER(use_custom_colors)
  IPC_STRUCT_TRAITS_MEMBER(enable_referrers)
  IPC_STRUCT_TRAITS_MEMBER(enable_do_not_track)
  IPC_STRUCT_TRAITS_MEMBER(enable_encrypted_media)
  IPC_STRUCT_TRAITS_MEMBER(webrtc_ip_handling_policy)
  IPC_STRUCT_TRAITS_MEMBER(webrtc_udp_min_port)
  IPC_STRUCT_TRAITS_MEMBER(webrtc_udp_max_port)
  IPC_STRUCT_TRAITS_MEMBER(user_agent_override)
  IPC_STRUCT_TRAITS_MEMBER(accept_languages)
  IPC_STRUCT_TRAITS_MEMBER(disable_client_blocked_error_page)
  IPC_STRUCT_TRAITS_MEMBER(plugin_fullscreen_allowed)
  IPC_STRUCT_TRAITS_MEMBER(caret_browsing_enabled)
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  IPC_STRUCT_TRAITS_MEMBER(system_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(selection_clipboard_buffer_available)
#endif
#if defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(small_caption_font_height)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(menu_font_height)
  IPC_STRUCT_TRAITS_MEMBER(status_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(status_font_height)
  IPC_STRUCT_TRAITS_MEMBER(message_font_family_name)
  IPC_STRUCT_TRAITS_MEMBER(message_font_height)
  IPC_STRUCT_TRAITS_MEMBER(vertical_scroll_bar_width_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(horizontal_scroll_bar_height_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_height_vertical_scroll_bar_in_dips)
  IPC_STRUCT_TRAITS_MEMBER(arrow_bitmap_width_horizontal_scroll_bar_in_dips)
#endif
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS(blink::WebDragOperation)  // Bitmask.
IPC_ENUM_TRAITS_MAX_VALUE(content::DropData::Kind,
                          content::DropData::Kind::LAST)

IPC_STRUCT_TRAITS_BEGIN(ui::FileInfo)
  IPC_STRUCT_TRAITS_MEMBER(path)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(url_title)
  IPC_STRUCT_TRAITS_MEMBER(download_metadata)
  IPC_STRUCT_TRAITS_MEMBER(referrer_policy)
  IPC_STRUCT_TRAITS_MEMBER(filenames)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
  IPC_STRUCT_TRAITS_MEMBER(file_system_files)
  IPC_STRUCT_TRAITS_MEMBER(text)
  IPC_STRUCT_TRAITS_MEMBER(html)
  IPC_STRUCT_TRAITS_MEMBER(html_base_url)
  IPC_STRUCT_TRAITS_MEMBER(file_contents)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_source_url)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_filename_extension)
  IPC_STRUCT_TRAITS_MEMBER(file_contents_content_disposition)
  IPC_STRUCT_TRAITS_MEMBER(custom_data)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData::FileSystemFileInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(filesystem_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DropData::Metadata)
  IPC_STRUCT_TRAITS_MEMBER(kind)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(filename)
  IPC_STRUCT_TRAITS_MEMBER(file_system_url)
IPC_STRUCT_TRAITS_END()

#endif  // CONTENT_PUBLIC_COMMON_COMMON_PARAM_TRAITS_MACROS_H_
