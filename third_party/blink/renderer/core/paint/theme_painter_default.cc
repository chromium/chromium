/*
 * Copyright (C) 2007 Apple Inc.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2008, 2009 Google Inc.
 * Copyright (C) 2009 Kenneth Rohde Christiansen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/paint/theme_painter_default.h"

#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/platform/web_theme_engine.h"
#include "third_party/blink/public/resources/grit/blink_image_resources.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/slider_thumb_element.h"
#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_progress.h"
#include "third_party/blink/renderer/core/layout/layout_theme_default.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_helper.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

namespace {

bool IsIndeterminate(const Element& element) {
  if (const auto* input = DynamicTo<HTMLInputElement>(element))
    return input->ShouldAppearIndeterminate();
  return false;
}

bool IsChecked(const Element& element) {
  if (const auto* input = DynamicTo<HTMLInputElement>(element))
    return input->ShouldAppearChecked();
  return false;
}

WebThemeEngine::State GetWebThemeState(const Element& element) {
  if (element.IsDisabledFormControl())
    return WebThemeEngine::kStateDisabled;
  if (element.IsActive())
    return WebThemeEngine::kStatePressed;
  if (element.IsHovered())
    return WebThemeEngine::kStateHover;

  return WebThemeEngine::kStateNormal;
}

SkColor GetContrastingColorFor(const Element& element,
                               const mojom::ColorScheme color_scheme,
                               WebThemeEngine::Part part) {
  WebThemeEngine::State state = GetWebThemeState(element);

  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);

  const bool is_disabled = (state == WebThemeEngine::kStateDisabled);
  switch (part) {
    case WebThemeEngine::kPartCheckbox:
    case WebThemeEngine::kPartRadio:
      return is_disabled ? color_provider->GetColor(
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
      NOTREACHED();
  }
}

mojom::ColorScheme CalculateColorSchemeForAccentColor(
    std::optional<SkColor> accent_color,
    mojom::ColorScheme color_scheme,
    SkColor light_contrasting_color,
    SkColor dark_contrasting_color) {
  if (!accent_color) {
    return color_scheme;
  }

  const float contrast_with_light =
      color_utils::GetContrastRatio(*accent_color, light_contrasting_color);
  const float contrast_with_dark =
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

mojom::blink::ColorScheme GetColorSchemeForAccentColor(
    const Element& element,
    const mojom::blink::ColorScheme color_scheme,
    const std::optional<SkColor> accent_color,
    WebThemeEngine::Part part) {
  return CalculateColorSchemeForAccentColor(
      accent_color, color_scheme,
      GetContrastingColorFor(element, mojom::blink::ColorScheme::kLight, part),
      GetContrastingColorFor(element, mojom::blink::ColorScheme::kDark, part));
}

class DirectionFlippingScope {
  STACK_ALLOCATED();

 public:
  DirectionFlippingScope(const LayoutObject&,
                         const PaintInfo&,
                         const gfx::Rect&);
  ~DirectionFlippingScope();

 private:
  bool needs_horizontal_flipping_;
  bool needs_vertical_flipping_;
  const PaintInfo& paint_info_;
};

DirectionFlippingScope::DirectionFlippingScope(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const gfx::Rect& rect)
    : paint_info_(paint_info) {
  PhysicalDirection inline_end =
      layout_object.StyleRef().GetWritingDirection().InlineEnd();
  needs_horizontal_flipping_ = inline_end == PhysicalDirection::kLeft;
  needs_vertical_flipping_ = inline_end == PhysicalDirection::kUp;
  if (needs_horizontal_flipping_) {
    paint_info_.context.Save();
    paint_info_.context.Translate(2 * rect.x() + rect.width(), 0);
    paint_info_.context.Scale(-1, 1);
  } else if (needs_vertical_flipping_) {
    paint_info_.context.Save();
    paint_info_.context.Translate(0, 2 * rect.y() + rect.height());
    paint_info_.context.Scale(1, -1);
  }
}

DirectionFlippingScope::~DirectionFlippingScope() {
  if (!needs_horizontal_flipping_ && !needs_vertical_flipping_) {
    return;
  }
  paint_info_.context.Restore();
}

gfx::Rect DeterminateProgressValueRectFor(const LayoutProgress& layout_progress,
                                          const gfx::Rect& rect) {
  int dx = rect.width();
  int dy = rect.height();
  if (layout_progress.IsHorizontalWritingMode()) {
    dx *= layout_progress.GetPosition();
  } else {
    dy *= layout_progress.GetPosition();
  }
  return gfx::Rect(rect.x(), rect.y(), dx, dy);
}

gfx::Rect IndeterminateProgressValueRectFor(
    const LayoutProgress& layout_progress,
    const gfx::Rect& rect) {
  // Value comes from default of GTK+.
  static const int kProgressActivityBlocks = 5;

  int x = rect.x();
  int y = rect.y();
  int value_width = rect.width();
  int value_height = rect.height();
  double progress = layout_progress.AnimationProgress();

  if (layout_progress.IsHorizontalWritingMode()) {
    value_width = value_width / kProgressActivityBlocks;
    int movable_width = rect.width() - value_width;
    if (movable_width <= 0)
      return gfx::Rect();
    x = progress < 0.5 ? x + progress * 2 * movable_width
                       : rect.x() + (1.0 - progress) * 2 * movable_width;
  } else {
    value_height = value_height / kProgressActivityBlocks;
    int movable_height = rect.height() - value_height;
    if (movable_height <= 0)
      return gfx::Rect();
    y = progress < 0.5 ? y + progress * 2 * movable_height
                       : rect.y() + (1.0 - progress) * 2 * movable_height;
  }

  return gfx::Rect(x, y, value_width, value_height);
}

gfx::Rect ProgressValueRectFor(const LayoutProgress& layout_progress,
                               const gfx::Rect& rect) {
  return layout_progress.IsDeterminate()
             ? DeterminateProgressValueRectFor(layout_progress, rect)
             : IndeterminateProgressValueRectFor(layout_progress, rect);
}

gfx::Rect ConvertToPaintingRect(const LayoutObject& input_layout_object,
                                const LayoutObject& part_layout_object,
                                PhysicalRect part_rect,
                                const gfx::Rect& local_offset) {
  // Compute an offset between the partLayoutObject and the inputLayoutObject.
  PhysicalOffset offset_from_input_layout_object =
      -part_layout_object.OffsetFromAncestor(&input_layout_object);
  // Move the rect into partLayoutObject's coords.
  part_rect.Move(offset_from_input_layout_object);
  // Account for the local drawing offset.
  part_rect.Move(PhysicalOffset(local_offset.origin()));

  return ToPixelSnappedRect(part_rect);
}

std::optional<SkColor> GetAccentColor(const ComputedStyle& style,
                                      const Document& document) {
  std::optional<Color> css_accent_color = style.AccentColorResolved();
  if (css_accent_color)
    return css_accent_color->Rgb();

  // We should not allow the system accent color to be rendered in image
  // contexts because it could be read back by the page and used for
  // fingerprinting.
  if (!document.GetPage()->GetChromeClient().IsIsolatedSVGChromeClient()) {
    mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
    LayoutTheme& layout_theme = LayoutTheme::GetTheme();
    if (!document.InForcedColorsMode() &&
        layout_theme.IsAccentColorCustomized(color_scheme)) {
      return layout_theme.GetSystemAccentColor(color_scheme).Rgb();
    }
  }

  return std::nullopt;
}

}  // namespace

ThemePainterDefault::ThemePainterDefault(LayoutThemeDefault& theme)
    : ThemePainter(), theme_(theme) {}

bool ThemePainterDefault::PaintCheckbox(const Element& element,
                                        const Document& document,
                                        const ComputedStyle& style,
                                        const PaintInfo& paint_info,
                                        const gfx::Rect& rect) {
  WebThemeEngine::ButtonExtraParams button;
  button.checked = IsChecked(element);
  button.indeterminate = IsIndeterminate(element);

  float zoom_level = style.EffectiveZoom();
  button.zoom = zoom_level;
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  gfx::Rect unzoomed_rect =
      ApplyZoomToRect(rect, paint_info, state_saver, zoom_level);
  WebThemeEngine::ExtraParams extra_params(button);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();

  // This is used for `kPartCheckbox`, which gets drawn adjacent to
  // `accent_color`. In order to guarantee contrast between `kPartCheckbox` and
  // `accent_color`, we choose the `color_scheme` here based on the two possible
  // color values for `kPartCheckbox`.
  bool accent_color_affects_color_scheme =
      button.checked &&
      GetWebThemeState(element) != WebThemeEngine::kStateDisabled;
  if (accent_color_affects_color_scheme) {
    color_scheme = GetColorSchemeForAccentColor(element, color_scheme,
                                                GetAccentColor(style, document),
                                                WebThemeEngine::kPartCheckbox);
  }

  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartCheckbox,
      GetWebThemeState(element), unzoomed_rect, &extra_params, color_scheme,
      document.InForcedColorsMode(), color_provider,
      GetAccentColor(style, document));
  return false;
}

bool ThemePainterDefault::PaintRadio(const Element& element,
                                     const Document& document,
                                     const ComputedStyle& style,
                                     const PaintInfo& paint_info,
                                     const gfx::Rect& rect) {
  WebThemeEngine::ButtonExtraParams button;
  button.checked = IsChecked(element);

  float zoom_level = style.EffectiveZoom();
  button.zoom = zoom_level;
  WebThemeEngine::ExtraParams extra_params(button);
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  gfx::Rect unzoomed_rect =
      ApplyZoomToRect(rect, paint_info, state_saver, zoom_level);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();

  // This is used for `kPartRadio`, which gets drawn adjacent to `accent_color`.
  // In order to guarantee contrast between `kPartRadio` and `accent_color`, we
  // choose the `color_scheme` here based on the two possible color values for
  // `kPartRadio`.
  bool accent_color_affects_color_scheme =
      button.checked &&
      GetWebThemeState(element) != WebThemeEngine::kStateDisabled;
  if (accent_color_affects_color_scheme) {
    color_scheme = GetColorSchemeForAccentColor(element, color_scheme,
                                                GetAccentColor(style, document),
                                                WebThemeEngine::kPartRadio);
  }

  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartRadio,
      GetWebThemeState(element), unzoomed_rect, &extra_params, color_scheme,
      document.InForcedColorsMode(), color_provider,
      GetAccentColor(style, document));
  return false;
}

bool ThemePainterDefault::PaintButton(const Element& element,
                                      const Document& document,
                                      const ComputedStyle& style,
                                      const PaintInfo& paint_info,
                                      const gfx::Rect& rect) {
  WebThemeEngine::ButtonExtraParams button;
  button.has_border = true;
  button.zoom = style.EffectiveZoom();
  WebThemeEngine::ExtraParams extra_params(button);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartButton,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      document.InForcedColorsMode(), color_provider,
      GetAccentColor(style, document));
  return false;
}

bool ThemePainterDefault::PaintTextField(const Element& element,
                                         const ComputedStyle& style,
                                         const PaintInfo& paint_info,
                                         const gfx::Rect& rect) {
  // WebThemeEngine does not handle border rounded corner and background image
  // so return true to draw CSS border and background.
  if (style.HasBorderRadius() || style.HasBackgroundImage())
    return true;

  ControlPart part = style.EffectiveAppearance();

  WebThemeEngine::TextFieldExtraParams text_field;
  text_field.is_text_area = part == kTextAreaPart;
  text_field.is_listbox = part == kListboxPart;
  text_field.has_border = true;
  text_field.zoom = style.EffectiveZoom();

  Color background_color =
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  text_field.background_color = background_color.Rgb();
  text_field.auto_complete_active =
      DynamicTo<HTMLFormControlElement>(element)->IsAutofilled() ||
      DynamicTo<HTMLFormControlElement>(element)->IsPreviewed();

  WebThemeEngine::ExtraParams extra_params(text_field);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartTextField,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      element.GetDocument().InForcedColorsMode(), color_provider,
      GetAccentColor(style, element.GetDocument()));
  return false;
}

bool ThemePainterDefault::PaintMenuList(const Element& element,
                                        const Document& document,
                                        const ComputedStyle& style,
                                        const PaintInfo& paint_info,
                                        const gfx::Rect& rect) {
  WebThemeEngine::MenuListExtraParams menu_list;
  // Match Chromium Win behaviour of showing all borders if any are shown.
  menu_list.has_border = style.HasBorder();
  menu_list.has_border_radius = style.HasBorderRadius();
  menu_list.zoom = style.EffectiveZoom();
  // Fallback to transparent if the specified color object is invalid.
  Color background_color(Color::kTransparent);
  if (style.HasBackground()) {
    background_color =
        style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  }
  menu_list.background_color = background_color.Rgb();

  // If we have a background image, don't fill the content area to expose the
  // parent's background. Also, we shouldn't fill the content area if the
  // alpha of the color is 0. The API of Windows GDI ignores the alpha.
  // FIXME: the normal Aura theme doesn't care about this, so we should
  // investigate if we really need fillContentArea.
  menu_list.fill_content_area =
      !style.HasBackgroundImage() && !background_color.IsFullyTransparent();

  WebThemeEngine::ExtraParams extra_params(menu_list);

  SetupMenuListArrow(document, style, rect, extra_params);

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartMenuList,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      document.InForcedColorsMode(), color_provider,
      GetAccentColor(style, document));
  return false;
}

bool ThemePainterDefault::PaintMenuListButton(const Element& element,
                                              const Document& document,
                                              const ComputedStyle& style,
                                              const PaintInfo& paint_info,
                                              const gfx::Rect& rect) {
  WebThemeEngine::MenuListExtraParams menu_list;
  menu_list.has_border = false;
  menu_list.has_border_radius = style.HasBorderRadius();
  menu_list.background_color = SK_ColorTRANSPARENT;
  menu_list.fill_content_area = false;
  WebThemeEngine::ExtraParams extra_params(menu_list);
  SetupMenuListArrow(document, style, rect, extra_params);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const ui::ColorProvider* color_provider =
      document.GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartMenuList,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      document.InForcedColorsMode(), color_provider,
      GetAccentColor(style, document));
  return false;
}

void ThemePainterDefault::SetupMenuListArrow(
    const Document& document,
    const ComputedStyle& style,
    const gfx::Rect& rect,
    WebThemeEngine::ExtraParams& extra_params) {
  auto& menu_list =
      absl::get<WebThemeEngine::MenuListExtraParams>(extra_params);
  WritingDirectionMode writing_direction = style.GetWritingDirection();
  PhysicalDirection block_end = writing_direction.BlockEnd();
  if (block_end == PhysicalDirection::kDown) {
    menu_list.arrow_direction = WebThemeEngine::ArrowDirection::kDown;
    const int left = rect.x() + floorf(style.BorderLeftWidth());
    const int right =
        rect.x() + rect.width() - floorf(style.BorderRightWidth());
    const int middle = rect.y() + rect.height() / 2;

    menu_list.arrow_y = middle;
    float arrow_box_width =
        theme_.ClampedMenuListArrowPaddingSize(document.GetFrame(), style);
    float arrow_scale_factor =
        arrow_box_width / theme_.MenuListArrowWidthInDIP();
    // TODO(tkent): This should be 7.0 to match scroll bar buttons.
    float arrow_size = 8.0 * arrow_scale_factor;
    // Put the arrow at the center of paddingForArrow area.
    // |arrow_x| is the left position for Aura theme engine.
    menu_list.arrow_x =
        (writing_direction.InlineEnd() == PhysicalDirection::kLeft)
            ? left + (arrow_box_width - arrow_size) / 2
            : right - (arrow_box_width + arrow_size) / 2;
    menu_list.arrow_size = arrow_size;
  } else {
    if (block_end == PhysicalDirection::kRight) {
      menu_list.arrow_direction = WebThemeEngine::ArrowDirection::kRight;
    } else {
      menu_list.arrow_direction = WebThemeEngine::ArrowDirection::kLeft;
    }
    const int bottom = rect.y() + floorf(style.BorderBottomWidth());
    const int top = rect.y() + rect.height() - floorf(style.BorderTopWidth());
    const int middle = rect.x() + rect.width() / 2;

    menu_list.arrow_x = middle;
    float arrow_box_height =
        theme_.ClampedMenuListArrowPaddingSize(document.GetFrame(), style);
    float arrow_scale_factor =
        arrow_box_height / theme_.MenuListArrowWidthInDIP();
    // TODO(tkent): This should be 7.0 to match scroll bar buttons.
    float arrow_size = 8.0 * arrow_scale_factor;
    // Put the arrow at the center of paddingForArrow area.
    // |arrow_y| is the bottom position for Aura theme engine.
    menu_list.arrow_y =
        (writing_direction.InlineEnd() == PhysicalDirection::kUp)
            ? bottom + (arrow_box_height - arrow_size) / 2
            : top - (arrow_box_height + arrow_size) / 2;
    menu_list.arrow_size = arrow_size;
  }

  // TODO: (https://crbug.com/1227305)This color still does not support forced
  // dark mode
  menu_list.arrow_color =
      style.VisitedDependentColor(GetCSSPropertyColor()).Rgb();
}

bool ThemePainterDefault::PaintSliderTrack(const Element& element,
                                           const LayoutObject& layout_object,
                                           const PaintInfo& paint_info,
                                           const gfx::Rect& rect,
                                           const ComputedStyle& style) {
  WebThemeEngine::SliderExtraParams slider;
  bool is_slider_vertical =
      RuntimeEnabledFeatures::
          NonStandardAppearanceValueSliderVerticalEnabled() &&
      style.EffectiveAppearance() == kSliderVerticalPart;
  const WritingMode writing_mode = style.GetWritingMode();
  bool is_writing_mode_vertical = !IsHorizontalWritingMode(writing_mode);
  slider.vertical = is_writing_mode_vertical || is_slider_vertical;
  slider.in_drag = false;

  PaintSliderTicks(layout_object, paint_info, rect);

  slider.zoom = style.EffectiveZoom();
  slider.thumb_x = 0;
  slider.thumb_y = 0;
  // If we do not allow direction support for vertical writing-mode or the
  // slider is vertical by computed appearance slider-vertical, then it should
  // behave like it has direction rtl and its value should be rendered
  // bottom-to-top.
  slider.right_to_left =
      (IsHorizontalWritingMode(writing_mode) && !is_slider_vertical) ||
              is_writing_mode_vertical
          ? !style.IsLeftToRightDirection()
          : true;
  if (writing_mode == WritingMode::kSidewaysLr) {
    slider.right_to_left = !slider.right_to_left;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(element)) {
    Element* thumb_element = input->UserAgentShadowRoot()
                                 ? input->UserAgentShadowRoot()->getElementById(
                                       shadow_element_names::kIdSliderThumb)
                                 : nullptr;
    LayoutBox* thumb = thumb_element ? thumb_element->GetLayoutBox() : nullptr;
    LayoutBox* input_box = input->GetLayoutBox();
    if (thumb) {
      gfx::Rect thumb_rect = ToPixelSnappedRect(
          PhysicalRect(thumb->PhysicalLocation(), thumb->Size()));
      slider.thumb_x = thumb_rect.x() + input_box->PaddingLeft().ToInt() +
                       input_box->BorderLeft().ToInt();
      slider.thumb_y = thumb_rect.y() + input_box->PaddingTop().ToInt() +
                       input_box->BorderTop().ToInt();
    }
  }
  WebThemeEngine::ExtraParams extra_params(slider);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();

  // This is used for `kPartSliderTrack`, which gets drawn adjacent to
  // `accent_color`. In order to guarantee contrast between `kPartSliderTrack`
  // and `accent_color`, we choose the `color_scheme` here based on the two
  // possible color values for `kPartSliderTrack`.
  bool accent_color_affects_color_scheme =
      GetWebThemeState(element) != WebThemeEngine::kStateDisabled;
  if (accent_color_affects_color_scheme) {
    color_scheme = GetColorSchemeForAccentColor(
        element, color_scheme, GetAccentColor(style, element.GetDocument()),
        WebThemeEngine::kPartSliderTrack);
  }

  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartSliderTrack,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      element.GetDocument().InForcedColorsMode(), color_provider,
      GetAccentColor(style, element.GetDocument()));
  return false;
}

bool ThemePainterDefault::PaintSliderThumb(const Element& element,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const gfx::Rect& rect) {
  WebThemeEngine::SliderExtraParams slider;
  slider.vertical = !style.IsHorizontalWritingMode() ||
                    (RuntimeEnabledFeatures::
                         NonStandardAppearanceValueSliderVerticalEnabled() &&
                     style.EffectiveAppearance() == kSliderThumbVerticalPart);
  slider.in_drag = element.IsActive();
  slider.zoom = style.EffectiveZoom();

  // The element passed in is inside the user agent shadow DOM of the input
  // element, so we have to access the parent input element in order to get the
  // accent-color style set by the page.
  const SliderThumbElement* slider_element =
      DynamicTo<SliderThumbElement>(&element);
  DCHECK(slider_element);  // PaintSliderThumb should always be passed a
                           // SliderThumbElement
  std::optional<SkColor> accent_color =
      GetAccentColor(*slider_element->HostInput()->EnsureComputedStyle(),
                     element.GetDocument());
  WebThemeEngine::ExtraParams extra_params(slider);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();

  // This is used for `kPartSliderThumb`, which gets drawn adjacent to
  // `accent_color`. In order to guarantee contrast between `kPartSliderThumb`
  // and `accent_color`, we choose the `color_scheme` here based on the two
  // possible color values for `kPartSliderThumb`.
  bool accent_color_affects_color_scheme =
      GetWebThemeState(element) != WebThemeEngine::kStateDisabled;
  if (accent_color_affects_color_scheme) {
    color_scheme = GetColorSchemeForAccentColor(
        element, color_scheme, GetAccentColor(style, element.GetDocument()),
        WebThemeEngine::kPartSliderThumb);
  }

  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartSliderThumb,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      element.GetDocument().InForcedColorsMode(), color_provider, accent_color);
  return false;
}

bool ThemePainterDefault::PaintInnerSpinButton(const Element& element,
                                               const ComputedStyle& style,
                                               const PaintInfo& paint_info,
                                               const gfx::Rect& rect) {
  WebThemeEngine::InnerSpinButtonExtraParams inner_spin;

  bool spin_up = false;
  if (const auto* spin_buttom = DynamicTo<SpinButtonElement>(element)) {
    if (spin_buttom->GetUpDownState() == SpinButtonElement::kUp)
      spin_up = element.IsHovered() || element.IsActive();
  }

  bool read_only = false;
  if (const auto* control = DynamicTo<HTMLFormControlElement>(element))
    read_only = control->IsReadOnly();

  inner_spin.spin_up = spin_up;
  inner_spin.read_only = read_only;
  inner_spin.spin_arrows_direction =
      style.IsHorizontalWritingMode()
          ? WebThemeEngine::SpinArrowsDirection::kUpDown
          : WebThemeEngine::SpinArrowsDirection::kLeftRight;

  WebThemeEngine::ExtraParams extra_params(inner_spin);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);

  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartInnerSpinButton,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      element.GetDocument().InForcedColorsMode(), color_provider,
      GetAccentColor(style, element.GetDocument()));
  return false;
}

bool ThemePainterDefault::PaintProgressBar(const Element& element,
                                           const LayoutObject& layout_object,
                                           const PaintInfo& paint_info,
                                           const gfx::Rect& rect,
                                           const ComputedStyle& style) {
  const auto* layout_progress = DynamicTo<LayoutProgress>(layout_object);
  if (!layout_progress)
    return true;

  gfx::Rect value_rect = ProgressValueRectFor(*layout_progress, rect);

  WebThemeEngine::ProgressBarExtraParams progress_bar;
  progress_bar.determinate = layout_progress->IsDeterminate();
  progress_bar.value_rect_x = value_rect.x();
  progress_bar.value_rect_y = value_rect.y();
  progress_bar.value_rect_width = value_rect.width();
  progress_bar.value_rect_height = value_rect.height();
  progress_bar.zoom = style.EffectiveZoom();
  progress_bar.is_horizontal = layout_progress->IsHorizontalWritingMode();
  WebThemeEngine::ExtraParams extra_params(progress_bar);
  DirectionFlippingScope scope(layout_object, paint_info, rect);
  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();

  // This is used for `kPartProgressBar`, which gets drawn adjacent to
  // `accent_color`. In order to guarantee contrast between `kPartProgressBar`
  // and `accent_color`, we choose the `color_scheme` here based on the two
  // possible color values for `kPartProgressBar`.
  color_scheme = GetColorSchemeForAccentColor(
      element, color_scheme, GetAccentColor(style, element.GetDocument()),
      WebThemeEngine::kPartProgressBar);

  const ui::ColorProvider* color_provider =
      element.GetDocument().GetColorProviderForPainting(color_scheme);
  WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
      paint_info.context.Canvas(), WebThemeEngine::kPartProgressBar,
      GetWebThemeState(element), rect, &extra_params, color_scheme,
      element.GetDocument().InForcedColorsMode(), color_provider,
      GetAccentColor(style, element.GetDocument()));
  return false;
}

bool ThemePainterDefault::PaintTextArea(const Element& element,
                                        const ComputedStyle& style,
                                        const PaintInfo& paint_info,
                                        const gfx::Rect& rect) {
  return PaintTextField(element, style, paint_info, rect);
}

bool ThemePainterDefault::PaintSearchField(const Element& element,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const gfx::Rect& rect) {
  return PaintTextField(element, style, paint_info, rect);
}

bool ThemePainterDefault::PaintSearchFieldCancelButton(
    const LayoutObject& cancel_button_object,
    const PaintInfo& paint_info,
    const gfx::Rect& r) {
  // Get the layoutObject of <input> element.
  Node* input = cancel_button_object.GetNode()->OwnerShadowHost();
  const LayoutObject& base_layout_object = input && input->GetLayoutObject()
                                               ? *input->GetLayoutObject()
                                               : cancel_button_object;
  if (!base_layout_object.IsBox())
    return false;
  const auto& input_layout_box = To<LayoutBox>(base_layout_object);
  PhysicalRect input_content_box = input_layout_box.PhysicalContentBoxRect();

  // Make sure the scaled button stays square and will fit in its parent's box.
  LayoutUnit cancel_button_size =
      std::min(input_content_box.size.width,
               std::min(input_content_box.size.height, LayoutUnit(r.height())));
  // Calculate cancel button's coordinates relative to the input element.
  // Center the button inline.  Round up though, so if it has to be one
  // pixel off-center, it will be one pixel closer to the bottom of the field.
  // This tends to look better with the text.
  const bool is_horizontal = cancel_button_object.IsHorizontalWritingMode();
  const LayoutUnit cancel_button_rect_left =
      is_horizontal
          ? cancel_button_object.OffsetFromAncestor(&input_layout_box).left
          : input_content_box.X() +
                (input_content_box.Width() - cancel_button_size + 1) / 2;
  const LayoutUnit cancel_button_rect_top =
      is_horizontal
          ? input_content_box.Y() +
                (input_content_box.Height() - cancel_button_size + 1) / 2
          : cancel_button_object.OffsetFromAncestor(&input_layout_box).top;
  PhysicalRect cancel_button_rect(cancel_button_rect_left,
                                  cancel_button_rect_top, cancel_button_size,
                                  cancel_button_size);
  gfx::Rect painting_rect = ConvertToPaintingRect(
      input_layout_box, cancel_button_object, cancel_button_rect, r);
  DEFINE_STATIC_REF(Image, cancel_image,
                    (Image::LoadPlatformResource(IDR_SEARCH_CANCEL)));
  DEFINE_STATIC_REF(Image, cancel_pressed_image,
                    (Image::LoadPlatformResource(IDR_SEARCH_CANCEL_PRESSED)));
  DEFINE_STATIC_REF(Image, cancel_image_dark_mode,
                    (Image::LoadPlatformResource(IDR_SEARCH_CANCEL_DARK_MODE)));
  DEFINE_STATIC_REF(
      Image, cancel_pressed_image_dark_mode,
      (Image::LoadPlatformResource(IDR_SEARCH_CANCEL_PRESSED_DARK_MODE)));
  DEFINE_STATIC_REF(
      Image, cancel_image_hc_light_mode,
      (Image::LoadPlatformResource(IDR_SEARCH_CANCEL_HC_LIGHT_MODE)));
  DEFINE_STATIC_REF(
      Image, cancel_pressed_image_hc_light_mode,
      (Image::LoadPlatformResource(IDR_SEARCH_CANCEL_PRESSED_HC_LIGHT_MODE)));
  Image* color_scheme_adjusted_cancel_image;
  Image* color_scheme_adjusted_cancel_pressed_image;
  if (ui::NativeTheme::GetInstanceForWeb()->UserHasContrastPreference()) {
    // TODO(crbug.com/1159597): Ideally we want the cancel button to be the same
    // color as search field text. Since the cancel button is currently painted
    // with a .png, it can't be colored dynamically so currently our only
    // choices are black and white.
    Color search_field_text_color =
        cancel_button_object.StyleRef().VisitedDependentColor(
            GetCSSPropertyColor());
    bool text_is_dark = color_utils::GetRelativeLuminance4f(
                            search_field_text_color.toSkColor4f()) < 0.5;
    color_scheme_adjusted_cancel_image =
        text_is_dark ? cancel_image_hc_light_mode : cancel_image_dark_mode;
    color_scheme_adjusted_cancel_pressed_image =
        color_scheme_adjusted_cancel_image =
            text_is_dark ? cancel_pressed_image_hc_light_mode
                         : cancel_pressed_image_dark_mode;
  } else {
    mojom::blink::ColorScheme color_scheme =
        cancel_button_object.StyleRef().UsedColorScheme();
    color_scheme_adjusted_cancel_image =
        color_scheme == mojom::blink::ColorScheme::kLight
            ? cancel_image
            : cancel_image_dark_mode;
    color_scheme_adjusted_cancel_pressed_image =
        color_scheme == mojom::blink::ColorScheme::kLight
            ? cancel_pressed_image
            : cancel_pressed_image_dark_mode;
  }
  Image& target_image = To<Element>(cancel_button_object.GetNode())->IsActive()
                            ? *color_scheme_adjusted_cancel_pressed_image
                            : *color_scheme_adjusted_cancel_image;
  paint_info.context.DrawImage(
      target_image, Image::kSyncDecode, ImageAutoDarkMode::Disabled(),
      ImagePaintTimingInfo(), gfx::RectF(painting_rect));
  return false;
}

gfx::Rect ThemePainterDefault::ApplyZoomToRect(
    const gfx::Rect& rect,
    const PaintInfo& paint_info,
    GraphicsContextStateSaver& state_saver,
    float zoom_level) {
  gfx::Rect unzoomed_rect = rect;
  if (zoom_level != 1) {
    state_saver.Save();
    unzoomed_rect.set_width(unzoomed_rect.width() / zoom_level);
    unzoomed_rect.set_height(unzoomed_rect.height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.x(), unzoomed_rect.y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.x(), -unzoomed_rect.y());
  }

  return unzoomed_rect;
}

}  // namespace blink
