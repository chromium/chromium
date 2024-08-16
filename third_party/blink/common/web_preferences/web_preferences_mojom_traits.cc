// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_preferences/web_preferences_mojom_traits.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::WebPreferencesDataView,
                  blink::web_pref::WebPreferences>::
    Read(blink::mojom::WebPreferencesDataView data,
         blink::web_pref::WebPreferences* out) {
  if (!data.ReadStandardFontFamilyMap(&out->standard_font_family_map) ||
      !data.ReadFixedFontFamilyMap(&out->fixed_font_family_map) ||
      !data.ReadSerifFontFamilyMap(&out->serif_font_family_map) ||
      !data.ReadSansSerifFontFamilyMap(&out->sans_serif_font_family_map) ||
      !data.ReadCursiveFontFamilyMap(&out->cursive_font_family_map) ||
      !data.ReadFantasyFontFamilyMap(&out->fantasy_font_family_map) ||
      !data.ReadMathFontFamilyMap(&out->math_font_family_map) ||
      !data.ReadDefaultEncoding(&out->default_encoding) ||
      !data.ReadTextTrackBackgroundColor(&out->text_track_background_color) ||
      !data.ReadTextTrackTextColor(&out->text_track_text_color) ||
      !data.ReadTextTrackTextSize(&out->text_track_text_size) ||
      !data.ReadTextTrackTextShadow(&out->text_track_text_shadow) ||
      !data.ReadTextTrackFontFamily(&out->text_track_font_family) ||
      !data.ReadTextTrackFontStyle(&out->text_track_font_style) ||
      !data.ReadTextTrackFontVariant(&out->text_track_font_variant) ||
      !data.ReadTextTrackWindowColor(&out->text_track_window_color) ||
      !data.ReadTextTrackWindowRadius(&out->text_track_window_radius) ||
      !data.ReadPrimaryPointerType(&out->primary_pointer_type) ||
      !data.ReadOutputDeviceUpdateAbilityType(
          &out->output_device_update_ability_type) ||
      !data.ReadPrimaryHoverType(&out->primary_hover_type) ||
      !data.ReadViewportStyle(&out->viewport_style) ||
      !data.ReadAnimationPolicy(&out->animation_policy) ||
      !data.ReadLowPriorityIframesThreshold(
          &out->low_priority_iframes_threshold) ||
      !data.ReadNetworkQualityEstimatorWebHoldback(
          &out->network_quality_estimator_web_holdback) ||
      !data.ReadWebAppScope(&out->web_app_scope)
#if BUILDFLAG(IS_ANDROID)
      || !data.ReadDefaultVideoPosterUrl(&out->default_video_poster_url)
#endif
  )
    return false;

  out->default_font_size = data.default_font_size();
  out->default_fixed_font_size = data.default_fixed_font_size();
  out->minimum_font_size = data.minimum_font_size();
  out->minimum_logical_font_size = data.minimum_logical_font_size();
  out->context_menu_on_mouse_up = data.context_menu_on_mouse_up();
  out->javascript_enabled = data.javascript_enabled();
  out->web_security_enabled = data.web_security_enabled();
  out->loads_images_automatically = data.loads_images_automatically();
  out->images_enabled = data.images_enabled();
  out->plugins_enabled = data.plugins_enabled();
  out->dom_paste_enabled = data.dom_paste_enabled();
  out->shrinks_standalone_images_to_fit =
      data.shrinks_standalone_images_to_fit();
  out->text_areas_are_resizable = data.text_areas_are_resizable();
  out->allow_scripts_to_close_windows = data.allow_scripts_to_close_windows();
  out->remote_fonts_enabled = data.remote_fonts_enabled();
  out->javascript_can_access_clipboard = data.javascript_can_access_clipboard();
  out->dns_prefetching_enabled = data.dns_prefetching_enabled();
  out->data_saver_enabled = data.data_saver_enabled();
  out->local_storage_enabled = data.local_storage_enabled();
  out->databases_enabled = data.databases_enabled();
  out->tabs_to_links = data.tabs_to_links();
  out->disable_ipc_flooding_protection = data.disable_ipc_flooding_protection();
  out->hyperlink_auditing_enabled = data.hyperlink_auditing_enabled();
  out->allow_universal_access_from_file_urls =
      data.allow_universal_access_from_file_urls();
  out->allow_file_access_from_file_urls =
      data.allow_file_access_from_file_urls();
  out->webgl1_enabled = data.webgl1_enabled();
  out->webgl2_enabled = data.webgl2_enabled();
  out->pepper_3d_enabled = data.pepper_3d_enabled();
  out->privileged_webgl_extensions_enabled =
      data.privileged_webgl_extensions_enabled();
  out->webgl_errors_to_console_enabled = data.webgl_errors_to_console_enabled();
  out->hide_scrollbars = data.hide_scrollbars();
  out->prefers_default_scrollbar_styles =
      data.prefers_default_scrollbar_styles();
  out->accelerated_2d_canvas_enabled = data.accelerated_2d_canvas_enabled();
  out->canvas_2d_layers_enabled = data.canvas_2d_layers_enabled();
  out->antialiased_2d_canvas_disabled = data.antialiased_2d_canvas_disabled();
  out->antialiased_clips_2d_canvas_enabled =
      data.antialiased_clips_2d_canvas_enabled();
  out->accelerated_filters_enabled = data.accelerated_filters_enabled();
  out->deferred_filters_enabled = data.deferred_filters_enabled();
  out->container_culling_enabled = data.container_culling_enabled();
  out->allow_running_insecure_content = data.allow_running_insecure_content();
  out->disable_reading_from_canvas = data.disable_reading_from_canvas();
  out->strict_mixed_content_checking = data.strict_mixed_content_checking();
  out->strict_powerful_feature_restrictions =
      data.strict_powerful_feature_restrictions();
  out->allow_geolocation_on_insecure_origins =
      data.allow_geolocation_on_insecure_origins();
  out->strictly_block_blockable_mixed_content =
      data.strictly_block_blockable_mixed_content();
  out->block_mixed_plugin_content = data.block_mixed_plugin_content();
  out->password_echo_enabled = data.password_echo_enabled();
  out->disable_reading_from_canvas = data.disable_reading_from_canvas();
  out->should_clear_document_background =
      data.should_clear_document_background();
  out->enable_scroll_animator = data.enable_scroll_animator();
  out->prefers_reduced_motion = data.prefers_reduced_motion();
  out->prefers_reduced_transparency = data.prefers_reduced_transparency();
  out->inverted_colors = data.inverted_colors();
  out->touch_event_feature_detection_enabled =
      data.touch_event_feature_detection_enabled();
  out->pointer_events_max_touch_points = data.pointer_events_max_touch_points();
  out->available_pointer_types = data.available_pointer_types();
  out->available_hover_types = data.available_hover_types();
  out->output_device_update_ability_type =
      data.output_device_update_ability_type();
  out->dont_send_key_events_to_javascript =
      data.dont_send_key_events_to_javascript();
  out->barrel_button_for_drag_enabled = data.barrel_button_for_drag_enabled();
  out->sync_xhr_in_documents_enabled = data.sync_xhr_in_documents_enabled();
  out->target_blank_implies_no_opener_enabled_will_be_removed =
      data.target_blank_implies_no_opener_enabled_will_be_removed();
  out->allow_non_empty_navigator_plugins =
      data.allow_non_empty_navigator_plugins();
  out->number_of_cpu_cores = data.number_of_cpu_cores();
  out->editing_behavior = data.editing_behavior();
  out->supports_multiple_windows = data.supports_multiple_windows();
  out->viewport_enabled = data.viewport_enabled();
  out->viewport_meta_enabled = data.viewport_meta_enabled();
  out->auto_zoom_focused_editable_to_legible_scale =
      data.auto_zoom_focused_editable_to_legible_scale();
  out->shrinks_viewport_contents_to_fit =
      data.shrinks_viewport_contents_to_fit();
  out->smooth_scroll_for_find_enabled = data.smooth_scroll_for_find_enabled();
  out->main_frame_resizes_are_orientation_changes =
      data.main_frame_resizes_are_orientation_changes();
  out->initialize_at_minimum_page_scale =
      data.initialize_at_minimum_page_scale();
  out->smart_insert_delete_enabled = data.smart_insert_delete_enabled();
  out->spatial_navigation_enabled = data.spatial_navigation_enabled();
  out->v8_cache_options = data.v8_cache_options();
  out->record_whole_document = data.record_whole_document();
  out->stylus_handwriting_enabled = data.stylus_handwriting_enabled();
  out->cookie_enabled = data.cookie_enabled();
  out->accelerated_video_decode_enabled =
      data.accelerated_video_decode_enabled();
  out->user_gesture_required_for_presentation =
      data.user_gesture_required_for_presentation();
  out->text_tracks_enabled = data.text_tracks_enabled();
  out->text_track_margin_percentage = data.text_track_margin_percentage();
  out->immersive_mode_enabled = data.immersive_mode_enabled();
  out->double_tap_to_zoom_enabled = data.double_tap_to_zoom_enabled();
  out->fullscreen_supported = data.fullscreen_supported();
  out->text_autosizing_enabled = data.text_autosizing_enabled();
#if BUILDFLAG(IS_ANDROID)
  out->font_scale_factor = data.font_scale_factor();
  out->font_weight_adjustment = data.font_weight_adjustment();
  out->text_size_contrast_factor = data.text_size_contrast_factor();
  out->device_scale_adjustment = data.device_scale_adjustment();
  out->force_enable_zoom = data.force_enable_zoom();
  out->support_deprecated_target_density_dpi =
      data.support_deprecated_target_density_dpi();
  out->wide_viewport_quirk = data.wide_viewport_quirk();
  out->use_wide_viewport = data.use_wide_viewport();
  out->force_zero_layout_height = data.force_zero_layout_height();
  out->viewport_meta_merge_content_quirk =
      data.viewport_meta_merge_content_quirk();
  out->viewport_meta_non_user_scalable_quirk =
      data.viewport_meta_non_user_scalable_quirk();
  out->viewport_meta_zero_values_quirk = data.viewport_meta_zero_values_quirk();
  out->clobber_user_agent_initial_scale_quirk =
      data.clobber_user_agent_initial_scale_quirk();
  out->ignore_main_frame_overflow_hidden_quirk =
      data.ignore_main_frame_overflow_hidden_quirk();
  out->report_screen_size_in_physical_pixels_quirk =
      data.report_screen_size_in_physical_pixels_quirk();
  out->reuse_global_for_unowned_main_frame =
      data.reuse_global_for_unowned_main_frame();
  out->spellcheck_enabled_by_default = data.spellcheck_enabled_by_default();
  out->video_fullscreen_orientation_lock_enabled =
      data.video_fullscreen_orientation_lock_enabled();
  out->video_rotate_to_fullscreen_enabled =
      data.video_rotate_to_fullscreen_enabled();
  out->embedded_media_experience_enabled =
      data.embedded_media_experience_enabled();
  out->css_hex_alpha_color_enabled = data.css_hex_alpha_color_enabled();
  out->scroll_top_left_interop_enabled = data.scroll_top_left_interop_enabled();
  out->disable_accelerated_small_canvases =
      data.disable_accelerated_small_canvases();
  out->long_press_link_select_text = data.long_press_link_select_text();
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)
  out->disable_webauthn = data.disable_webauthn();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

  out->force_dark_mode_enabled = data.force_dark_mode_enabled();
  out->default_minimum_page_scale_factor =
      data.default_minimum_page_scale_factor();
  out->default_maximum_page_scale_factor =
      data.default_maximum_page_scale_factor();
  out->hide_download_ui = data.hide_download_ui();
  out->presentation_receiver = data.presentation_receiver();
  out->media_controls_enabled = data.media_controls_enabled();
  out->do_not_update_selection_on_mutating_selection_range =
      data.do_not_update_selection_on_mutating_selection_range();
  out->autoplay_policy = data.autoplay_policy();
  out->require_transient_activation_for_get_display_media =
      data.require_transient_activation_for_get_display_media();
  out->require_transient_activation_for_show_file_or_directory_picker =
      data.require_transient_activation_for_show_file_or_directory_picker();
  out->in_forced_colors = data.in_forced_colors();
  out->is_forced_colors_disabled = data.is_forced_colors_disabled();
  out->preferred_root_scrollbar_color_scheme =
      data.preferred_root_scrollbar_color_scheme();
  out->preferred_color_scheme = data.preferred_color_scheme();
  out->preferred_contrast = data.preferred_contrast();
  out->picture_in_picture_enabled = data.picture_in_picture_enabled();
  out->translate_service_available = data.translate_service_available();
  out->lazy_load_enabled = data.lazy_load_enabled();
  out->allow_mixed_content_upgrades = data.allow_mixed_content_upgrades();
  out->always_show_focus = data.always_show_focus();
  out->touch_drag_drop_enabled = data.touch_drag_drop_enabled();
  out->webxr_immersive_ar_allowed = data.webxr_immersive_ar_allowed();
  out->renderer_wide_named_frame_lookup =
      data.renderer_wide_named_frame_lookup();
  out->modal_context_menu = data.modal_context_menu();
  out->subapps_apis_require_user_gesture_and_authorization =
      data.require_transient_activation_and_user_confirmation_for_subapps_api();
  return true;
}

}  // namespace mojo
