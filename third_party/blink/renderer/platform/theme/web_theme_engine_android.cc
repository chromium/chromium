// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_android.h"

#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

static ui::NativeTheme::ExtraParams GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack: {
      // Android doesn't draw scrollbars.
      NOTREACHED_IN_MIGRATION();
      ui::NativeTheme::ExtraParams native_theme_extra_params;
      return native_theme_extra_params;
    }
    case WebThemeEngine::kPartCheckbox: {
      ui::NativeTheme::ButtonExtraParams native_button;
      const auto& button =
          absl::get<WebThemeEngine::ButtonExtraParams>(*extra_params);
      native_button.checked = button.checked;
      native_button.indeterminate = button.indeterminate;
      native_button.zoom = button.zoom;
      return ui::NativeTheme::ExtraParams(native_button);
    }
    case WebThemeEngine::kPartRadio: {
      ui::NativeTheme::ButtonExtraParams native_button;
      const auto& button =
          absl::get<WebThemeEngine::ButtonExtraParams>(*extra_params);
      native_button.checked = button.checked;
      return ui::NativeTheme::ExtraParams(native_button);
    }
    case WebThemeEngine::kPartButton: {
      ui::NativeTheme::ButtonExtraParams native_button;
      const auto& button =
          absl::get<WebThemeEngine::ButtonExtraParams>(*extra_params);
      native_button.has_border = button.has_border;
      // Native buttons have a different focus style.
      native_button.is_focused = false;
      native_button.background_color = button.background_color;
      native_button.zoom = button.zoom;
      return ui::NativeTheme::ExtraParams(native_button);
    }
    case WebThemeEngine::kPartTextField: {
      ui::NativeTheme::TextFieldExtraParams native_text_field;
      const auto& text_field =
          absl::get<WebThemeEngine::TextFieldExtraParams>(*extra_params);
      native_text_field.is_text_area = text_field.is_text_area;
      native_text_field.is_listbox = text_field.is_listbox;
      native_text_field.background_color = text_field.background_color;
      native_text_field.has_border = text_field.has_border;
      native_text_field.auto_complete_active = text_field.auto_complete_active;
      native_text_field.zoom = text_field.zoom;
      return ui::NativeTheme::ExtraParams(native_text_field);
    }
    case WebThemeEngine::kPartMenuList: {
      ui::NativeTheme::MenuListExtraParams native_menu_list;
      const auto& menu_list =
          absl::get<WebThemeEngine::MenuListExtraParams>(*extra_params);
      native_menu_list.has_border = menu_list.has_border;
      native_menu_list.has_border_radius = menu_list.has_border_radius;
      native_menu_list.arrow_x = menu_list.arrow_x;
      native_menu_list.arrow_y = menu_list.arrow_y;
      native_menu_list.arrow_size = menu_list.arrow_size;
      //  Need to explicit cast so we can assign enum to enum.
      ui::NativeTheme::ArrowDirection dir =
          ui::NativeTheme::ArrowDirection(menu_list.arrow_direction);
      native_menu_list.arrow_direction = dir;
      native_menu_list.arrow_color = menu_list.arrow_color;
      native_menu_list.background_color = menu_list.background_color;
      native_menu_list.zoom = menu_list.zoom;
      return ui::NativeTheme::ExtraParams(native_menu_list);
    }
    case WebThemeEngine::kPartSliderTrack: {
      ui::NativeTheme::SliderExtraParams native_slider_track;
      const auto& slider_track =
          absl::get<WebThemeEngine::SliderExtraParams>(*extra_params);
      native_slider_track.thumb_x = slider_track.thumb_x;
      native_slider_track.thumb_y = slider_track.thumb_y;
      native_slider_track.zoom = slider_track.zoom;
      native_slider_track.right_to_left = slider_track.right_to_left;
      native_slider_track.vertical = slider_track.vertical;
      native_slider_track.in_drag = slider_track.in_drag;
      return ui::NativeTheme::ExtraParams(native_slider_track);
    }
    case WebThemeEngine::kPartSliderThumb: {
      ui::NativeTheme::SliderExtraParams native_slider_thumb;
      const auto& slider_thumb =
          absl::get<WebThemeEngine::SliderExtraParams>(*extra_params);
      native_slider_thumb.vertical = slider_thumb.vertical;
      native_slider_thumb.in_drag = slider_thumb.in_drag;
      return ui::NativeTheme::ExtraParams(native_slider_thumb);
    }
    case WebThemeEngine::kPartInnerSpinButton: {
      ui::NativeTheme::InnerSpinButtonExtraParams native_inner_spin;
      const auto& inner_spin =
          absl::get<WebThemeEngine::InnerSpinButtonExtraParams>(*extra_params);
      native_inner_spin.spin_up = inner_spin.spin_up;
      native_inner_spin.read_only = inner_spin.read_only;
      //  Need to explicit cast so we can assign enum to enum.
      ui::NativeTheme::SpinArrowsDirection dir =
          ui::NativeTheme::SpinArrowsDirection(
              inner_spin.spin_arrows_direction);
      native_inner_spin.spin_arrows_direction = dir;
      return ui::NativeTheme::ExtraParams(native_inner_spin);
    }
    case WebThemeEngine::kPartProgressBar: {
      ui::NativeTheme::ProgressBarExtraParams native_progress_bar;
      const auto& progress_bar =
          absl::get<WebThemeEngine::ProgressBarExtraParams>(*extra_params);
      native_progress_bar.determinate = progress_bar.determinate;
      native_progress_bar.value_rect_x = progress_bar.value_rect_x;
      native_progress_bar.value_rect_y = progress_bar.value_rect_y;
      native_progress_bar.value_rect_width = progress_bar.value_rect_width;
      native_progress_bar.value_rect_height = progress_bar.value_rect_height;
      native_progress_bar.zoom = progress_bar.zoom;
      native_progress_bar.is_horizontal = progress_bar.is_horizontal;
      return ui::NativeTheme::ExtraParams(native_progress_bar);
    }
    default: {
      ui::NativeTheme::ExtraParams native_theme_extra_params;
      return native_theme_extra_params;  // Parts that have no extra params get
                                         // here.
    }
  }
}

WebThemeEngineAndroid::~WebThemeEngineAndroid() = default;

gfx::Size WebThemeEngineAndroid::GetSize(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb: {
      // Minimum length for scrollbar thumb is the scrollbar thickness.
      ScrollbarStyle style;
      GetOverlayScrollbarStyle(&style);
      int scrollbarThickness = style.thumb_thickness + style.scrollbar_margin;
      return gfx::Size(scrollbarThickness, scrollbarThickness);
    }
    default: {
      ui::NativeTheme::ExtraParams extra;
      return ui::NativeTheme::GetInstanceForWeb()->GetPartSize(
          NativeThemePart(part), ui::NativeTheme::kNormal, extra);
    }
  }
}

void WebThemeEngineAndroid::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  *style = WebThemeEngineHelper::AndroidScrollbarStyle();
}

void WebThemeEngineAndroid::Paint(
    cc::PaintCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const gfx::Rect& rect,
    const WebThemeEngine::ExtraParams* extra_params,
    blink::mojom::ColorScheme color_scheme,
    bool in_forced_colors,
    const ui::ColorProvider* color_provider,
    const std::optional<SkColor>& accent_color) {
  ui::NativeTheme::ExtraParams native_theme_extra_params =
      GetNativeThemeExtraParams(part, state, extra_params);
  // ColorProviders are not supported on android and there are no controls that
  // require ColorProvider colors on the platform.
  const ui::ColorProvider* color_provider_android = nullptr;
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, color_provider_android, NativeThemePart(part),
      NativeThemeState(state), rect, native_theme_extra_params,
      NativeColorScheme(color_scheme), in_forced_colors, accent_color);
}

}  // namespace blink
