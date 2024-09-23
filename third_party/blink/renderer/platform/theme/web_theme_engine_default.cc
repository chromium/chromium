// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_default.h"

#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/color_palette.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace blink {

using mojom::ColorScheme;

namespace {

#if BUILDFLAG(IS_WIN)
// The width of a vertical scroll bar in dips.
int32_t g_vertical_scroll_bar_width;

// The height of a horizontal scroll bar in dips.
int32_t g_horizontal_scroll_bar_height;

// The height of the arrow bitmap on a vertical scroll bar in dips.
int32_t g_vertical_arrow_bitmap_height;

// The width of the arrow bitmap on a horizontal scroll bar in dips.
int32_t g_horizontal_arrow_bitmap_width;
#endif

}  // namespace

static ui::NativeTheme::ExtraParams GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params) {
  if (!extra_params) {
    ui::NativeTheme::ExtraParams native_theme_extra_params;
    return native_theme_extra_params;
  }

  switch (part) {
    case WebThemeEngine::kPartScrollbarCorner:
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack: {
      ui::NativeTheme::ScrollbarTrackExtraParams native_scrollbar_track;
      const auto& scrollbar_track =
          absl::get<WebThemeEngine::ScrollbarTrackExtraParams>(*extra_params);
      native_scrollbar_track.is_upper = scrollbar_track.is_back;
      native_scrollbar_track.track_x = scrollbar_track.track_x;
      native_scrollbar_track.track_y = scrollbar_track.track_y;
      native_scrollbar_track.track_width = scrollbar_track.track_width;
      native_scrollbar_track.track_height = scrollbar_track.track_height;
      native_scrollbar_track.track_color = scrollbar_track.track_color;
      return ui::NativeTheme::ExtraParams(native_scrollbar_track);
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
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb: {
      ui::NativeTheme::ScrollbarThumbExtraParams native_scrollbar_thumb;
      const auto& scrollbar_thumb =
          absl::get<WebThemeEngine::ScrollbarThumbExtraParams>(*extra_params);
      native_scrollbar_thumb.thumb_color = scrollbar_thumb.thumb_color;
      native_scrollbar_thumb.is_thumb_minimal_mode =
          scrollbar_thumb.is_thumb_minimal_mode;
      native_scrollbar_thumb.is_web_test = scrollbar_thumb.is_web_test;
      return ui::NativeTheme::ExtraParams(native_scrollbar_thumb);
    }
    case WebThemeEngine::kPartScrollbarDownArrow:
    case WebThemeEngine::kPartScrollbarLeftArrow:
    case WebThemeEngine::kPartScrollbarRightArrow:
    case WebThemeEngine::kPartScrollbarUpArrow: {
      ui::NativeTheme::ScrollbarArrowExtraParams native_scrollbar_arrow;
      const auto& scrollbar_button =
          absl::get<WebThemeEngine::ScrollbarButtonExtraParams>(*extra_params);
      native_scrollbar_arrow.zoom = scrollbar_button.zoom;
      native_scrollbar_arrow.needs_rounded_corner =
          scrollbar_button.needs_rounded_corner;
      native_scrollbar_arrow.right_to_left = scrollbar_button.right_to_left;
      native_scrollbar_arrow.thumb_color = scrollbar_button.thumb_color;
      native_scrollbar_arrow.track_color = scrollbar_button.track_color;
      return ui::NativeTheme::ExtraParams(native_scrollbar_arrow);
    }
    default: {
      ui::NativeTheme::ExtraParams native_theme_extra_params;
      return native_theme_extra_params;  // Parts that have no extra params get
                                         // here.
    }
  }
}

WebThemeEngineDefault::WebThemeEngineDefault() = default;

WebThemeEngineDefault::~WebThemeEngineDefault() = default;

gfx::Size WebThemeEngineDefault::GetSize(WebThemeEngine::Part part) {
  ui::NativeTheme::ExtraParams extra;
  ui::NativeTheme::Part native_theme_part = NativeThemePart(part);
#if BUILDFLAG(IS_WIN)
  if (!ScrollbarThemeSettings::FluentScrollbarsEnabled()) {
    switch (native_theme_part) {
      case ui::NativeTheme::kScrollbarDownArrow:
      case ui::NativeTheme::kScrollbarLeftArrow:
      case ui::NativeTheme::kScrollbarRightArrow:
      case ui::NativeTheme::kScrollbarUpArrow:
      case ui::NativeTheme::kScrollbarHorizontalThumb:
      case ui::NativeTheme::kScrollbarVerticalThumb:
      case ui::NativeTheme::kScrollbarHorizontalTrack:
      case ui::NativeTheme::kScrollbarVerticalTrack: {
        return gfx::Size(g_vertical_scroll_bar_width,
                         g_vertical_scroll_bar_width);
      }

      default:
        break;
    }
  }
#endif
  return ui::NativeTheme::GetInstanceForWeb()->GetPartSize(
      native_theme_part, ui::NativeTheme::kNormal, extra);
}

void WebThemeEngineDefault::Paint(
    cc::PaintCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const gfx::Rect& rect,
    const WebThemeEngine::ExtraParams* extra_params,
    mojom::ColorScheme color_scheme,
    bool in_forced_colors,
    const ui::ColorProvider* color_provider,
    const std::optional<SkColor>& accent_color) {
  ui::NativeTheme::ExtraParams native_theme_extra_params =
      GetNativeThemeExtraParams(part, state, extra_params);
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, color_provider, NativeThemePart(part), NativeThemeState(state),
      rect, native_theme_extra_params, NativeColorScheme(color_scheme),
      in_forced_colors, accent_color);
}

gfx::Insets WebThemeEngineDefault::GetScrollbarSolidColorThumbInsets(
    Part part) const {
  return ui::NativeTheme::GetInstanceForWeb()
      ->GetScrollbarSolidColorThumbInsets(NativeThemePart(part));
}

SkColor4f WebThemeEngineDefault::GetScrollbarThumbColor(
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    const ui::ColorProvider* color_provider) const {
  const ui::NativeTheme::ScrollbarThumbExtraParams native_theme_extra_params =
      absl::get<ui::NativeTheme::ScrollbarThumbExtraParams>(
          GetNativeThemeExtraParams(
              /*part=*/WebThemeEngine::kPartScrollbarVerticalThumb, state,
              extra_params));

  return ui::NativeTheme::GetInstanceForWeb()->GetScrollbarThumbColor(
      *color_provider, NativeThemeState(state), native_theme_extra_params);
}

void WebThemeEngineDefault::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  if (IsFluentOverlayScrollbarEnabled()) {
    style->fade_out_delay = ui::kFluentOverlayScrollbarFadeDelay;
    style->fade_out_duration = ui::kFluentOverlayScrollbarFadeDuration;
  } else {
    style->fade_out_delay = ui::kOverlayScrollbarFadeDelay;
    style->fade_out_duration = ui::kOverlayScrollbarFadeDuration;
  }
  style->idle_thickness_scale = ui::kOverlayScrollbarIdleThicknessScale;
  // The other fields in this struct are used only on Android to draw solid
  // color scrollbars. On other platforms the scrollbars are painted in
  // NativeTheme so these fields are unused.
}

bool WebThemeEngineDefault::SupportsNinePatch(Part part) const {
  return ui::NativeTheme::GetInstanceForWeb()->SupportsNinePatch(
      NativeThemePart(part));
}

gfx::Size WebThemeEngineDefault::NinePatchCanvasSize(Part part) const {
  return ui::NativeTheme::GetInstanceForWeb()->GetNinePatchCanvasSize(
      NativeThemePart(part));
}

gfx::Rect WebThemeEngineDefault::NinePatchAperture(Part part) const {
  return ui::NativeTheme::GetInstanceForWeb()->GetNinePatchAperture(
      NativeThemePart(part));
}

bool WebThemeEngineDefault::IsFluentScrollbarEnabled() const {
  return ui::IsFluentScrollbarEnabled();
}

bool WebThemeEngineDefault::IsFluentOverlayScrollbarEnabled() const {
  return ui::IsFluentOverlayScrollbarEnabled();
}

int WebThemeEngineDefault::GetPaintedScrollbarTrackInset() const {
  return ui::NativeTheme::GetInstanceForWeb()->GetPaintedScrollbarTrackInset();
}

std::optional<SkColor> WebThemeEngineDefault::GetAccentColor() const {
  return ui::NativeTheme::GetInstanceForWeb()->user_color();
}

#if BUILDFLAG(IS_WIN)
// static
void WebThemeEngineDefault::cacheScrollBarMetrics(
    int32_t vertical_scroll_bar_width,
    int32_t horizontal_scroll_bar_height,
    int32_t vertical_arrow_bitmap_height,
    int32_t horizontal_arrow_bitmap_width) {
  g_vertical_scroll_bar_width = vertical_scroll_bar_width;
  g_horizontal_scroll_bar_height = horizontal_scroll_bar_height;
  g_vertical_arrow_bitmap_height = vertical_arrow_bitmap_height;
  g_horizontal_arrow_bitmap_width = horizontal_arrow_bitmap_width;
}
#endif

}  // namespace blink
