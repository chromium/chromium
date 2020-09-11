// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_WEB_PREFERENCES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_WEB_PREFERENCES_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/nqe/effective_connection_type.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/css/preferred_color_scheme.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"
#include "ui/base/pointer/pointer_device.h"
#include "url/gurl.h"

namespace blink {

class WebView;

namespace web_pref {

// Map of ISO 15924 four-letter script code to font family.  For example,
// "Arab" to "My Arabic Font".
typedef std::map<std::string, base::string16> ScriptFontFamilyMap;

enum EditingBehavior {
  EDITING_BEHAVIOR_MAC,
  EDITING_BEHAVIOR_WIN,
  EDITING_BEHAVIOR_UNIX,
  EDITING_BEHAVIOR_ANDROID,
  EDITING_BEHAVIOR_CHROMEOS,
  EDITING_BEHAVIOR_LAST = EDITING_BEHAVIOR_CHROMEOS
};

// ImageAnimationPolicy is used for controlling image animation
// when image frame is rendered for animation.
// See third_party/WebKit/Source/platform/graphics/ImageAnimationPolicy.h
// for information on the options.
enum ImageAnimationPolicy {
  IMAGE_ANIMATION_POLICY_ALLOWED,
  IMAGE_ANIMATION_POLICY_ANIMATION_ONCE,
  IMAGE_ANIMATION_POLICY_NO_ANIMATION
};

enum class ViewportStyle { DEFAULT, MOBILE, TELEVISION, LAST = TELEVISION };

// Defines the autoplay policy to be used. Should match the class in
// WebSettings.h.
enum class AutoplayPolicy {
  kNoUserGestureRequired,
  kUserGestureRequired,
  kDocumentUserActivationRequired,
};

// The ISO 15924 script code for undetermined script aka Common. It's the
// default used on WebKit's side to get/set a font setting when no script is
// specified.
BLINK_COMMON_EXPORT extern const char kCommonScript[];

// A struct for managing blink's settings.
//
// Adding new values to this class probably involves updating
// blink::WebSettings, content/common/view_messages.h,
// browser/profiles/profile.cc, and
// content/public/common/common_param_traits_macros.h
struct BLINK_COMMON_EXPORT WebPreferences {
  ScriptFontFamilyMap standard_font_family_map;
  ScriptFontFamilyMap fixed_font_family_map;
  ScriptFontFamilyMap serif_font_family_map;
  ScriptFontFamilyMap sans_serif_font_family_map;
  ScriptFontFamilyMap cursive_font_family_map;
  ScriptFontFamilyMap fantasy_font_family_map;
  ScriptFontFamilyMap pictograph_font_family_map;
  int default_font_size;
  int default_fixed_font_size;
  int minimum_font_size;
  int minimum_logical_font_size;
  std::string default_encoding;
  bool context_menu_on_mouse_up;
  bool javascript_enabled;
  bool web_security_enabled;
  bool loads_images_automatically;
  bool images_enabled;
  bool plugins_enabled;
  bool dom_paste_enabled;
  bool shrinks_standalone_images_to_fit;
  bool text_areas_are_resizable;
  bool allow_scripts_to_close_windows;
  bool remote_fonts_enabled;
  bool javascript_can_access_clipboard;
  bool xslt_enabled;
  // We don't use dns_prefetching_enabled to disable DNS prefetching.  Instead,
  // we disable the feature at a lower layer so that we catch non-WebKit uses
  // of DNS prefetch as well.
  bool dns_prefetching_enabled;
  // Preference to save data. When enabled, requests will contain the header
  // 'Save-Data: on'.
  bool data_saver_enabled;
  // Whether data saver holdback for Web APIs is enabled. If enabled, data saver
  // appears as disabled to the web consumers even if it has been actually
  // enabled by the user.
  bool data_saver_holdback_web_api_enabled;
  bool local_storage_enabled;
  bool databases_enabled;
  bool application_cache_enabled;
  bool tabs_to_links;
  bool disable_ipc_flooding_protection;
  bool hyperlink_auditing_enabled;
  bool allow_universal_access_from_file_urls;
  bool allow_file_access_from_file_urls;
  bool webgl1_enabled;
  bool webgl2_enabled;
  bool pepper_3d_enabled;
  bool flash_3d_enabled;
  bool flash_stage3d_enabled;
  bool flash_stage3d_baseline_enabled;
  bool privileged_webgl_extensions_enabled;
  bool webgl_errors_to_console_enabled;
  bool hide_scrollbars;
  bool accelerated_2d_canvas_enabled;
  bool new_canvas_2d_api_enabled;
  bool antialiased_2d_canvas_disabled;
  bool antialiased_clips_2d_canvas_enabled;
  bool accelerated_filters_enabled;
  bool deferred_filters_enabled;
  bool container_culling_enabled;
  bool allow_running_insecure_content;
  // If true, taints all <canvas> elements, regardless of origin.
  bool disable_reading_from_canvas;
  // Strict mixed content checking disables both displaying and running insecure
  // mixed content, and disables embedder notifications that such content was
  // requested (thereby preventing user override).
  bool strict_mixed_content_checking;
  // Strict powerful feature restrictions block insecure usage of powerful
  // features (like device orientation) that we haven't yet disabled for the web
  // at large.
  bool strict_powerful_feature_restrictions;
  // TODO(jww): Remove when WebView no longer needs this exception.
  bool allow_geolocation_on_insecure_origins;
  // Disallow user opt-in for blockable mixed content.
  bool strictly_block_blockable_mixed_content;
  bool block_mixed_plugin_content;
  bool password_echo_enabled;
  bool should_print_backgrounds;
  bool should_clear_document_background;
  bool enable_scroll_animator;
  bool prefers_reduced_motion;
  bool touch_event_feature_detection_enabled;
  int pointer_events_max_touch_points;
  int available_pointer_types;
  ui::PointerType primary_pointer_type;
  int available_hover_types;
  ui::HoverType primary_hover_type;
  bool dont_send_key_events_to_javascript;
  bool barrel_button_for_drag_enabled = false;
  bool sync_xhr_in_documents_enabled;
  int number_of_cpu_cores;
  EditingBehavior editing_behavior;
  bool supports_multiple_windows;
  bool viewport_enabled;
  bool viewport_meta_enabled;

  // If true - Blink will clamp the minimum scale factor to the content width,
  // preventing zoom beyond the visible content. This is really only needed if
  // viewport_enabled is on.
  bool shrinks_viewport_contents_to_fit;

  ViewportStyle viewport_style;
  bool always_show_context_menu_on_touch;
  bool smooth_scroll_for_find_enabled;
  bool main_frame_resizes_are_orientation_changes;
  bool initialize_at_minimum_page_scale;
  bool smart_insert_delete_enabled;
  bool spatial_navigation_enabled;
  bool navigate_on_drag_drop;
  blink::mojom::V8CacheOptions v8_cache_options;
  bool record_whole_document;

  // This flags corresponds to a Page's Settings' setCookieEnabled state. It
  // only controls whether or not the "document.cookie" field is properly
  // connected to the backing store, for instance if you wanted to be able to
  // define custom getters and setters from within a unique security content
  // without raising a DOM security exception.
  bool cookie_enabled;

  // This flag indicates whether H/W accelerated video decode is enabled.
  // Defaults to false.
  bool accelerated_video_decode_enabled;

  ImageAnimationPolicy animation_policy;

  bool user_gesture_required_for_presentation;

  bool text_tracks_enabled;

  // These fields specify the foreground and background color for WebVTT text
  // tracks. Their values can be any legal CSS color descriptor.
  std::string text_track_background_color;
  std::string text_track_text_color;

  // These fields specify values for CSS properties used to style WebVTT text
  // tracks.
  // Specifies CSS font-size property in percentage.
  std::string text_track_text_size;
  std::string text_track_text_shadow;
  std::string text_track_font_family;
  std::string text_track_font_style;
  // Specifies the value for CSS font-variant property.
  std::string text_track_font_variant;

  // These fields specify values for CSS properties used to style the window
  // around WebVTT text tracks.
  // Window color can be any legal CSS color descriptor.
  std::string text_track_window_color;
  // Window padding is in em.
  std::string text_track_window_padding;
  // Window radius is in pixels.
  std::string text_track_window_radius;

  // Specifies the margin for WebVTT text tracks as a percentage of media
  // element height/width (for horizontal/vertical text respectively).
  // Cues will not be placed in this margin area.
  float text_track_margin_percentage;

  bool immersive_mode_enabled;

  bool double_tap_to_zoom_enabled;

  bool fullscreen_supported;

  bool text_autosizing_enabled;

  // Representation of the Web App Manifest scope if any.
  GURL web_app_scope;

#if defined(OS_ANDROID)
  float font_scale_factor;
  float device_scale_adjustment;
  bool force_enable_zoom;
  GURL default_video_poster_url;
  bool support_deprecated_target_density_dpi;
  bool use_legacy_background_size_shorthand_behavior;
  bool wide_viewport_quirk;
  bool use_wide_viewport;
  bool force_zero_layout_height;
  bool viewport_meta_merge_content_quirk;
  bool viewport_meta_non_user_scalable_quirk;
  bool viewport_meta_zero_values_quirk;
  bool clobber_user_agent_initial_scale_quirk;
  bool ignore_main_frame_overflow_hidden_quirk;
  bool report_screen_size_in_physical_pixels_quirk;
  // Used by Android_WebView only to support legacy apps that inject script into
  // a top-level initial empty document and expect it to persist on navigation.
  bool reuse_global_for_unowned_main_frame;
  // Specifies default setting for spellcheck when the spellcheck attribute is
  // not explicitly specified.
  bool spellcheck_enabled_by_default;
  // If enabled, when a video goes fullscreen, the orientation should be locked.
  bool video_fullscreen_orientation_lock_enabled;
  // If enabled, fullscreen should be entered/exited when the device is rotated
  // to/from the orientation of the video.
  bool video_rotate_to_fullscreen_enabled;
  bool embedded_media_experience_enabled;
  // Enable 8 (#RRGGBBAA) and 4 (#RGBA) value hex colors in CSS Android
  // WebView quirk (http://crbug.com/618472).
  bool css_hex_alpha_color_enabled;
  // Enable support for document.scrollingElement
  // WebView sets this to false to retain old documentElement behaviour
  // (http://crbug.com/761016).
  bool scroll_top_left_interop_enabled;
  // Disable features such as offscreen canvas that depend on the viz
  // architecture of surface embedding. Android WebView does not support this
  // architecture yet.
  bool disable_features_depending_on_viz;
  // Don't accelerate small canvases to avoid crashes TODO(crbug.com/1004304)
  bool disable_accelerated_small_canvases;
  // Re-enable Web Components v0 on Webview, temporarily. This should get
  // removed when crbug.com/1021631 gets fixed.
  bool reenable_web_components_v0;
#endif  // defined(OS_ANDROID)

  // Enable forcibly modifying content rendering to result in a light on dark
  // color scheme.
  bool force_dark_mode_enabled = false;

  // Default (used if the page or UA doesn't override these) values for page
  // scale limits. These are set directly on the WebView so there's no analogue
  // in WebSettings.
  float default_minimum_page_scale_factor;
  float default_maximum_page_scale_factor;

  // Whether download UI should be hidden on this page.
  bool hide_download_ui;

  // Whether it is a presentation receiver.
  bool presentation_receiver;

  // If disabled, media controls should never be used.
  bool media_controls_enabled;

  // Whether we want to disable updating selection on mutating selection range.
  // This is to work around Samsung's email app issue. See
  // https://crbug.com/699943 for details.
  // TODO(changwan): remove this once we no longer support Android N.
  bool do_not_update_selection_on_mutating_selection_range;

  // Defines the current autoplay policy.
  AutoplayPolicy autoplay_policy;

  // The preferred color scheme for the web content. The scheme is used to
  // evaluate the prefers-color-scheme media query and resolve UA color scheme
  // to be used based on the supported-color-schemes META tag and CSS property.
  blink::PreferredColorScheme preferred_color_scheme =
      blink::PreferredColorScheme::kLight;

  // Network quality threshold below which resources from iframes are assigned
  // either kVeryLow or kVeryLow Blink priority.
  net::EffectiveConnectionType low_priority_iframes_threshold;

  // Whether Picture-in-Picture is enabled.
  bool picture_in_picture_enabled;

  // Whether a translate service is available.
  // blink's hrefTranslate attribute existence relies on the result.
  // See https://github.com/dtapuska/html-translate
  bool translate_service_available;

  // A value other than net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN implies that the
  // network quality estimate related Web APIs are in the holdback mode. When
  // the holdback is enabled, the related Web APIs return network quality
  // estimate corresponding to |network_quality_estimator_web_holdback|
  // regardless of the actual quality.
  net::EffectiveConnectionType network_quality_estimator_web_holdback;

  // Whether lazy loading of frames and images is enabled.
  bool lazy_load_enabled = true;

  // Specifies how close a lazily loaded iframe or image should be from the
  // viewport before it should start being loaded in, depending on the effective
  // connection type of the current network. Blink will use the default distance
  // threshold for effective connection types that aren't specified here.
  std::map<net::EffectiveConnectionType, int>
      lazy_frame_loading_distance_thresholds_px;
  std::map<net::EffectiveConnectionType, int>
      lazy_image_loading_distance_thresholds_px;
  std::map<net::EffectiveConnectionType, int> lazy_image_first_k_fully_load;

  // Setting to false disables upgrades to HTTPS for HTTP resources in HTTPS
  // sites.
  bool allow_mixed_content_upgrades;

  // Whether the focused element should always be indicated (for example, by
  // forcing :focus-visible to match regardless of focus method).
  bool always_show_focus;

  // Whether touch input can trigger HTML drag-and-drop operations. The
  // default value depends on the platform.
  bool touch_drag_drop_enabled;

  // Whether the end of a drag fires a contextmenu event and possibly shows a
  // context-menu (depends on how the event is handled).  Currently touch-drags
  // cannot show context menus, see crbug.com/1096189.
  bool touch_dragend_context_menu = false;

  // We try to keep the default values the same as the default values in
  // chrome, except for the cases where it would require lots of extra work for
  // the embedder to use the same default value.
  WebPreferences();
  WebPreferences(const WebPreferences& other);
  WebPreferences(WebPreferences&& other);
  ~WebPreferences();
  WebPreferences& operator=(const WebPreferences& other);
  WebPreferences& operator=(WebPreferences&& other);
};

}  // namespace web_pref

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEB_PREFERENCES_WEB_PREFERENCES_H_
