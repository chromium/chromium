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
      native_scrollbar_thumb.scrollbar_theme =
          NativeThemeScrollbarOverlayColorTheme(
              scrollbar_thumb.scrollbar_theme);
      native_scrollbar_thumb.thumb_color = scrollbar_thumb.thumb_color;
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

WebThemeEngineDefault::WebThemeEngineDefault() {
  light_color_provider_.GenerateColorMap();
  dark_color_provider_.GenerateColorMap();
  emulated_forced_colors_provider_.GenerateColorMap();
  forced_colors_provider_.GenerateColorMap();
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
  ui::NativeTheme::ExtraParams native_theme_extra_params =
      GetNativeThemeExtraParams(part, state, extra_params);

  if (ShouldPartBeAffectedByAccentColor(part, state, extra_params)) {
    // This is used for `part`, which gets drawn adjacent to `accent_color`. In
    // order to guarantee contrast between `part` and `accent_color`, we choose
    // the `color_scheme` here based on the two possible color values for
    // `part`.
    color_scheme = CalculateColorSchemeForAccentColor(
        accent_color, color_scheme,
        GetContrastingColorFor(mojom::ColorScheme::kLight, part, state),
        GetContrastingColorFor(mojom::ColorScheme::kDark, part, state));
  }

  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, GetColorProviderForPainting(color_scheme), NativeThemePart(part),
      NativeThemeState(state), rect, native_theme_extra_params,
      NativeColorScheme(color_scheme), accent_color);
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

bool WebThemeEngineDefault::IsFluentOverlayScrollbarEnabled() const {
  return ui::IsFluentOverlayScrollbarEnabled();
}

int WebThemeEngineDefault::GetPaintedScrollbarTrackInset() const {
  return ui::NativeTheme::GetInstanceForWeb()->GetPaintedScrollbarTrackInset();
}

absl::optional<SkColor> WebThemeEngineDefault::GetSystemColor(
    WebThemeEngine::SystemThemeColor system_theme_color) const {
  return ui::NativeTheme::GetInstanceForWeb()->GetSystemThemeColor(
      NativeSystemThemeColor(system_theme_color));
}

absl::optional<SkColor> WebThemeEngineDefault::GetAccentColor() const {
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
  EmulateForcedColors(is_dark_theme, /*is_web_test=*/false);
  ui::NativeTheme::GetInstanceForWeb()->UpdateSystemColorInfo(
      false, true, is_dark_theme ? dark_theme : light_theme);
}

void WebThemeEngineDefault::EmulateForcedColors(bool is_dark_theme,
                                                bool is_web_test) {
  SetEmulateForcedColors(true);
  emulated_forced_colors_provider_ =
      is_web_test ? ui::CreateEmulatedForcedColorsColorProviderForWebTests()
                  : ui::CreateEmulatedForcedColorsColorProvider(is_dark_theme);
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
    const ui::RendererColorMap& dark_colors,
    const ui::RendererColorMap& forced_colors_map) {
  if (WebTestSupport::IsRunningWebTest() &&
      GetForcedColors() == ForcedColors::kActive) {
    // Web tests use a different set of colors when determining which system
    // colors to render in forced colors mode.
    EmulateForcedColors(/*is_dark_theme=*/false, /*is_web_test=*/true);
  }

  // Do not create new ColorProviders if the renderer color maps match the
  // existing ColorProviders.
  bool did_color_provider_update = false;
  if (!IsRendererColorMappingEquivalent(light_color_provider_, light_colors)) {
    light_color_provider_ =
        ui::CreateColorProviderFromRendererColorMap(light_colors);
    did_color_provider_update = true;
  }
  if (!IsRendererColorMappingEquivalent(dark_color_provider_, dark_colors)) {
    dark_color_provider_ =
        ui::CreateColorProviderFromRendererColorMap(dark_colors);
    did_color_provider_update = true;
  }
  if (!IsRendererColorMappingEquivalent(forced_colors_provider_,
                                        forced_colors_map)) {
    forced_colors_provider_ =
        ui::CreateColorProviderFromRendererColorMap(forced_colors_map);
    did_color_provider_update = true;
  }

  return did_color_provider_update;
}

bool WebThemeEngineDefault::ShouldPartBeAffectedByAccentColor(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params) const {
  switch (part) {
    case WebThemeEngine::kPartCheckbox:
    case WebThemeEngine::kPartRadio: {
      const auto& button =
          absl::get<WebThemeEngine::ButtonExtraParams>(*extra_params);
      return button.checked && state != WebThemeEngine::kStateDisabled;
    }

    case WebThemeEngine::kPartSliderTrack:
    case WebThemeEngine::kPartSliderThumb:
      return state != WebThemeEngine::kStateDisabled;
    case WebThemeEngine::kPartProgressBar:
      return true;
    default:
      return false;
  }
}

SkColor WebThemeEngineDefault::GetContrastingColorFor(
    mojom::ColorScheme color_scheme,
    WebThemeEngine::Part part,
    WebThemeEngine::State state) const {
  const ui::ColorProvider* color_provider =
      color_scheme == mojom::ColorScheme::kLight ? &light_color_provider_
                                                 : &dark_color_provider_;
  bool isDisabled = (state == WebThemeEngine::kStateDisabled);
  switch (part) {
    case WebThemeEngine::kPartCheckbox:
    case WebThemeEngine::kPartRadio:
      return isDisabled ? color_provider->GetColor(
                              ui::kColorWebNativeControlBackgroundDisabled)
                        : color_provider->GetColor(
                              ui::kColorWebNativeControlBackground);
    case WebThemeEngine::kPartSliderTrack:
    case WebThemeEngine::kPartSliderThumb:
    case WebThemeEngine::kPartProgressBar:
      // We use `kStateNormal` here because the user hovering or clicking on the
      // slider will change the state to something else, and we don't want the
      // color-scheme to flicker back and forth when the user interacts with it.
      return color_provider->GetColor(ui::kColorWebNativeControlFill);
    default:
      NOTREACHED_NORETURN();
  }
}

mojom::ColorScheme WebThemeEngineDefault::CalculateColorSchemeForAccentColor(
    absl::optional<SkColor> accent_color,
    mojom::ColorScheme color_scheme,
    SkColor light_contrasting_color,
    SkColor dark_contrasting_color) const {
  if (!accent_color) {
    return color_scheme;
  }

  float contrast_with_light =
      color_utils::GetContrastRatio(*accent_color, light_contrasting_color);
  float contrast_with_dark =
      color_utils::GetContrastRatio(*accent_color, dark_contrasting_color);

  // If there is enough contrast between `accent_color` and `color_scheme`, then
  // let's keep it the same. Otherwise, flip the `color_scheme` to guarantee
  // contrast.
  if (color_scheme == mojom::ColorScheme::kDark) {
    if (contrast_with_dark < color_utils::kMinimumVisibleContrastRatio &&
        contrast_with_dark < contrast_with_light) {
      // TODO(crbug.com/1216137): what if `contrast_with_light` is less than
      // `kMinimumContrast`? Should we modify `accent_color`...?
      return mojom::ColorScheme::kLight;
    }
  } else {
    if (contrast_with_light < color_utils::kMinimumVisibleContrastRatio &&
        contrast_with_light < contrast_with_dark) {
      return mojom::ColorScheme::kDark;
    }
  }

  return color_scheme;
}

const ui::ColorProvider* WebThemeEngineDefault::GetColorProviderForPainting(
    mojom::ColorScheme color_scheme) const {
  if (GetForcedColors() == ForcedColors::kActive) {
    if (emulate_forced_colors_) {
      return &emulated_forced_colors_provider_;
    }
    return &forced_colors_provider_;
  }
  return color_scheme == mojom::ColorScheme::kLight ? &light_color_provider_
                                                    : &dark_color_provider_;
}

}  // namespace blink
