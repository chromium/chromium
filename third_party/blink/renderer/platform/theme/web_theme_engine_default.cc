// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_default.h"

#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"
#include "ui/color/color_provider_utils.h"
#include "ui/native_theme/native_theme.h"
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

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  if (!extra_params)
    return;

  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack:
      native_theme_extra_params->scrollbar_track.is_upper =
          extra_params->scrollbar_track.is_back;
      native_theme_extra_params->scrollbar_track.track_x =
          extra_params->scrollbar_track.track_x;
      native_theme_extra_params->scrollbar_track.track_y =
          extra_params->scrollbar_track.track_y;
      native_theme_extra_params->scrollbar_track.track_width =
          extra_params->scrollbar_track.track_width;
      native_theme_extra_params->scrollbar_track.track_height =
          extra_params->scrollbar_track.track_height;
      break;
    case WebThemeEngine::kPartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      native_theme_extra_params->button.zoom = extra_params->button.zoom;
      break;
    case WebThemeEngine::kPartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebThemeEngine::kPartButton:
      native_theme_extra_params->button.has_border =
          extra_params->button.has_border;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.background_color;
      native_theme_extra_params->button.zoom = extra_params->button.zoom;
      break;
    case WebThemeEngine::kPartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->text_field.is_text_area;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->text_field.is_listbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->text_field.background_color;
      native_theme_extra_params->text_field.has_border =
          extra_params->text_field.has_border;
      native_theme_extra_params->text_field.auto_complete_active =
          extra_params->text_field.auto_complete_active;
      native_theme_extra_params->text_field.zoom =
          extra_params->text_field.zoom;
      break;
    case WebThemeEngine::kPartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menu_list.has_border;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menu_list.has_border_radius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menu_list.arrow_x;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menu_list.arrow_y;
      native_theme_extra_params->menu_list.arrow_size =
          extra_params->menu_list.arrow_size;
      native_theme_extra_params->menu_list.arrow_color =
          extra_params->menu_list.arrow_color;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menu_list.background_color;
      native_theme_extra_params->menu_list.zoom = extra_params->menu_list.zoom;
      break;
    case WebThemeEngine::kPartSliderTrack:
      native_theme_extra_params->slider.thumb_x = extra_params->slider.thumb_x;
      native_theme_extra_params->slider.thumb_y = extra_params->slider.thumb_y;
      native_theme_extra_params->slider.zoom = extra_params->slider.zoom;
      native_theme_extra_params->slider.right_to_left =
          extra_params->slider.right_to_left;
      [[fallthrough]];
      // vertical and in_drag properties are used by both slider track and
      // slider thumb.
    case WebThemeEngine::kPartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.in_drag;
      break;
    case WebThemeEngine::kPartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->inner_spin.spin_up;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->inner_spin.read_only;
      break;
    case WebThemeEngine::kPartProgressBar:
      native_theme_extra_params->progress_bar.determinate =
          extra_params->progress_bar.determinate;
      native_theme_extra_params->progress_bar.value_rect_x =
          extra_params->progress_bar.value_rect_x;
      native_theme_extra_params->progress_bar.value_rect_y =
          extra_params->progress_bar.value_rect_y;
      native_theme_extra_params->progress_bar.value_rect_width =
          extra_params->progress_bar.value_rect_width;
      native_theme_extra_params->progress_bar.value_rect_height =
          extra_params->progress_bar.value_rect_height;
      native_theme_extra_params->progress_bar.zoom =
          extra_params->progress_bar.zoom;
      native_theme_extra_params->progress_bar.is_horizontal =
          extra_params->progress_bar.is_horizontal;
      break;
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb:
      native_theme_extra_params->scrollbar_thumb.scrollbar_theme =
          NativeThemeScrollbarOverlayColorTheme(
              extra_params->scrollbar_thumb.scrollbar_theme);
      break;
    case WebThemeEngine::kPartScrollbarDownArrow:
    case WebThemeEngine::kPartScrollbarLeftArrow:
    case WebThemeEngine::kPartScrollbarRightArrow:
    case WebThemeEngine::kPartScrollbarUpArrow:
      native_theme_extra_params->scrollbar_arrow.zoom =
          extra_params->scrollbar_button.zoom;
      native_theme_extra_params->scrollbar_arrow.right_to_left =
          extra_params->scrollbar_button.right_to_left;
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

WebThemeEngineDefault::WebThemeEngineDefault() {
  light_color_provider_.GenerateColorMap();
  dark_color_provider_.GenerateColorMap();
  emulated_forced_colors_provider_.GenerateColorMap();
}

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
    const absl::optional<SkColor>& accent_color) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(part, state, extra_params,
                            &native_theme_extra_params);
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, GetColorProviderForPainting(color_scheme), NativeThemePart(part),
      NativeThemeState(state), rect, native_theme_extra_params,
      NativeColorScheme(color_scheme), accent_color);
}

void WebThemeEngineDefault::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  style->fade_out_delay = ui::kOverlayScrollbarFadeDelay;
  style->fade_out_duration = ui::kOverlayScrollbarFadeDuration;
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

absl::optional<SkColor> WebThemeEngineDefault::GetSystemColor(
    WebThemeEngine::SystemThemeColor system_theme_color) const {
  return ui::NativeTheme::GetInstanceForWeb()->GetSystemThemeColor(
      NativeSystemThemeColor(system_theme_color));
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

ForcedColors WebThemeEngineDefault::GetForcedColors() const {
  return ui::NativeTheme::GetInstanceForWeb()->InForcedColorsMode()
             ? ForcedColors::kActive
             : ForcedColors::kNone;
}

void WebThemeEngineDefault::OverrideForcedColorsTheme(bool is_dark_theme) {
  // Colors were chosen based on Windows 10 default light and dark high contrast
  // themes.
  const base::flat_map<ui::NativeTheme::SystemThemeColor, uint32_t> dark_theme{
      {ui::NativeTheme::SystemThemeColor::kButtonFace, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kButtonText, 0xFFFFFFFF},
      {ui::NativeTheme::SystemThemeColor::kGrayText, 0xFF3FF23F},
      {ui::NativeTheme::SystemThemeColor::kHighlight, 0xFF1AEBFF},
      {ui::NativeTheme::SystemThemeColor::kHighlightText, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kHotlight, 0xFFFFFF00},
      {ui::NativeTheme::SystemThemeColor::kMenuHighlight, 0xFF800080},
      {ui::NativeTheme::SystemThemeColor::kScrollbar, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kWindow, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kWindowText, 0xFFFFFFFF},
  };
  const base::flat_map<ui::NativeTheme::SystemThemeColor, uint32_t> light_theme{
      {ui::NativeTheme::SystemThemeColor::kButtonFace, 0xFFFFFFFF},
      {ui::NativeTheme::SystemThemeColor::kButtonText, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kGrayText, 0xFF600000},
      {ui::NativeTheme::SystemThemeColor::kHighlight, 0xFF37006E},
      {ui::NativeTheme::SystemThemeColor::kHighlightText, 0xFFFFFFFF},
      {ui::NativeTheme::SystemThemeColor::kHotlight, 0xFF00009F},
      {ui::NativeTheme::SystemThemeColor::kMenuHighlight, 0xFF000000},
      {ui::NativeTheme::SystemThemeColor::kScrollbar, 0xFFFFFFFF},
      {ui::NativeTheme::SystemThemeColor::kWindow, 0xFFFFFFFF},
      {ui::NativeTheme::SystemThemeColor::kWindowText, 0xFF000000},
  };
  emulated_forced_colors_provider_ =
      ui::CreateEmulatedForcedColorsColorProvider(is_dark_theme);
  SetEmulateForcedColors(true);
  ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
      false, true, is_dark_theme ? dark_theme : light_theme);
}

void WebThemeEngineDefault::SetForcedColors(const ForcedColors forced_colors) {
  ui::NativeTheme::GetInstanceForWeb()->set_forced_colors(
      forced_colors == ForcedColors::kActive);
}

void WebThemeEngineDefault::ResetToSystemColors(
    SystemColorInfoState system_color_info_state) {
  base::flat_map<ui::NativeTheme::SystemThemeColor, uint32_t> colors;

  for (const auto& color : system_color_info_state.colors) {
    colors.insert({NativeSystemThemeColor(color.first), color.second});
  }

  ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
      system_color_info_state.is_dark_mode,
      system_color_info_state.forced_colors, colors);

  SetEmulateForcedColors(false);
}

WebThemeEngine::SystemColorInfoState
WebThemeEngineDefault::GetSystemColorInfo() {
  WebThemeEngine::SystemColorInfoState state;
  state.is_dark_mode =
      ui::NativeTheme::GetInstanceForWeb()->ShouldUseDarkColors();
  state.forced_colors =
      ui::NativeTheme::GetInstanceForWeb()->InForcedColorsMode();

  std::map<SystemThemeColor, uint32_t> colors;
  auto native_theme_colors =
      ui::NativeTheme::GetInstanceForWeb()->GetSystemColors();
  for (const auto& color : native_theme_colors) {
    colors.insert({WebThemeSystemThemeColor(color.first), color.second});
  }
  state.colors = colors;

  return state;
}

bool WebThemeEngineDefault::UpdateColorProviders(
    const ui::RendererColorMap& light_colors,
    const ui::RendererColorMap& dark_colors) {
  // Do not create new ColorProviders if the renderer color maps match the
  // existing ColorProviders.
  if (IsRendererColorMappingEquivalent(light_color_provider_, light_colors) &&
      IsRendererColorMappingEquivalent(dark_color_provider_, dark_colors)) {
    return false;
  }

  light_color_provider_ =
      ui::CreateColorProviderFromRendererColorMap(light_colors);
  dark_color_provider_ =
      ui::CreateColorProviderFromRendererColorMap(dark_colors);
  return true;
}

const ui::ColorProvider* WebThemeEngineDefault::GetColorProviderForPainting(
    mojom::ColorScheme color_scheme) const {
  if (emulate_forced_colors_ && GetForcedColors() == ForcedColors::kActive) {
    return &emulated_forced_colors_provider_;
  }
  return color_scheme == mojom::ColorScheme::kLight ? &light_color_provider_
                                                    : &dark_color_provider_;
}

}  // namespace blink
