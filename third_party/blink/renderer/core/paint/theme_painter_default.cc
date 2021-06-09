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

#include "third_party/blink/public/platform/platform.h"
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
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

namespace {

const unsigned kDefaultButtonBackgroundColor = 0xffdddddd;

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

class DirectionFlippingScope {
  STACK_ALLOCATED();

 public:
  DirectionFlippingScope(const LayoutObject&, const PaintInfo&, const IntRect&);
  ~DirectionFlippingScope();

 private:
  bool needs_flipping_;
  const PaintInfo& paint_info_;
};

DirectionFlippingScope::DirectionFlippingScope(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const IntRect& rect)
    : needs_flipping_(!layout_object.StyleRef().IsLeftToRightDirection()),
      paint_info_(paint_info) {
  if (!needs_flipping_)
    return;
  paint_info_.context.Save();
  paint_info_.context.Translate(2 * rect.X() + rect.Width(), 0);
  paint_info_.context.Scale(-1, 1);
}

DirectionFlippingScope::~DirectionFlippingScope() {
  if (!needs_flipping_)
    return;
  paint_info_.context.Restore();
}

IntRect DeterminateProgressValueRectFor(const LayoutProgress& layout_progress,
                                        const IntRect& rect) {
  int dx = rect.Width() * layout_progress.GetPosition();
  return IntRect(rect.X(), rect.Y(), dx, rect.Height());
}

IntRect IndeterminateProgressValueRectFor(const LayoutProgress& layout_progress,
                                          const IntRect& rect) {
  // Value comes from default of GTK+.
  static const int kProgressActivityBlocks = 5;

  int value_width = rect.Width() / kProgressActivityBlocks;
  int movable_width = rect.Width() - value_width;
  if (movable_width <= 0)
    return IntRect();

  double progress = layout_progress.AnimationProgress();
  if (progress < 0.5)
    return IntRect(rect.X() + progress * 2 * movable_width, rect.Y(),
                   value_width, rect.Height());
  return IntRect(rect.X() + (1.0 - progress) * 2 * movable_width, rect.Y(),
                 value_width, rect.Height());
}

IntRect ProgressValueRectFor(const LayoutProgress& layout_progress,
                             const IntRect& rect) {
  return layout_progress.IsDeterminate()
             ? DeterminateProgressValueRectFor(layout_progress, rect)
             : IndeterminateProgressValueRectFor(layout_progress, rect);
}

IntRect ConvertToPaintingRect(const LayoutObject& input_layout_object,
                              const LayoutObject& part_layout_object,
                              PhysicalRect part_rect,
                              const IntRect& local_offset) {
  // Compute an offset between the partLayoutObject and the inputLayoutObject.
  PhysicalOffset offset_from_input_layout_object =
      -part_layout_object.OffsetFromAncestor(&input_layout_object);
  // Move the rect into partLayoutObject's coords.
  part_rect.Move(offset_from_input_layout_object);
  // Account for the local drawing offset.
  part_rect.Move(PhysicalOffset(local_offset.Location()));

  return PixelSnappedIntRect(part_rect);
}

absl::optional<SkColor> GetAccentColor(const ComputedStyle& style) {
  if (!RuntimeEnabledFeatures::CSSAccentColorEnabled())
    return absl::nullopt;

  absl::optional<Color> css_accent_color = style.AccentColorResolved();
  if (css_accent_color)
    return css_accent_color->Rgb();

  mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
  LayoutTheme& layout_theme = LayoutTheme::GetTheme();
  if (layout_theme.IsAccentColorCustomized(color_scheme)) {
    return layout_theme.GetAccentColor(color_scheme).Rgb();
  }

  return absl::nullopt;
}

}  // namespace

ThemePainterDefault::ThemePainterDefault(LayoutThemeDefault& theme)
    : ThemePainter(), theme_(theme) {}

bool ThemePainterDefault::PaintCheckbox(const Element& element,
                                        const Document&,
                                        const ComputedStyle& style,
                                        const PaintInfo& paint_info,
                                        const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = paint_info.context.Canvas();
  extra_params.button = WebThemeEngine::ButtonExtraParams();
  extra_params.button.checked = IsChecked(element);
  extra_params.button.indeterminate = IsIndeterminate(element);

  float zoom_level = style.EffectiveZoom();
  extra_params.button.zoom = zoom_level;
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  IntRect unzoomed_rect =
      ApplyZoomToRect(rect, paint_info, state_saver, zoom_level);

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartCheckbox, GetWebThemeState(element),
      gfx::Rect(unzoomed_rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintRadio(const Element& element,
                                     const Document&,
                                     const ComputedStyle& style,
                                     const PaintInfo& paint_info,
                                     const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = paint_info.context.Canvas();
  extra_params.button = WebThemeEngine::ButtonExtraParams();
  extra_params.button.checked = IsChecked(element);

  float zoom_level = style.EffectiveZoom();
  extra_params.button.zoom = zoom_level;
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  IntRect unzoomed_rect =
      ApplyZoomToRect(rect, paint_info, state_saver, zoom_level);

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartRadio, GetWebThemeState(element),
      gfx::Rect(unzoomed_rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintButton(const Element& element,
                                      const Document&,
                                      const ComputedStyle& style,
                                      const PaintInfo& paint_info,
                                      const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = paint_info.context.Canvas();
  extra_params.button = WebThemeEngine::ButtonExtraParams();
  extra_params.button.has_border = true;
  extra_params.button.background_color = kDefaultButtonBackgroundColor;
  extra_params.button.zoom = style.EffectiveZoom();
  if (style.HasBackground()) {
    extra_params.button.background_color =
        style.VisitedDependentColor(GetCSSPropertyBackgroundColor()).Rgb();
  }
  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartButton, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintTextField(const Element& element,
                                         const ComputedStyle& style,
                                         const PaintInfo& paint_info,
                                         const IntRect& rect) {
  // WebThemeEngine does not handle border rounded corner and background image
  // so return true to draw CSS border and background.
  if (style.HasBorderRadius() || style.HasBackgroundImage())
    return true;

  // Don't use the theme painter if dark mode is enabled. It has a separate
  // graphics pipeline that doesn't go through GraphicsContext and so does not
  // currently know how to handle Dark Mode, causing elements to be rendered
  // incorrectly (e.g. https://crbug.com/937872).
  // TODO(gilmanmh): Implement a more permanent solution that allows use of
  // native dark themes.
  if (paint_info.context.IsDarkModeEnabled())
    return true;

  ControlPart part = style.EffectiveAppearance();

  WebThemeEngine::ExtraParams extra_params;
  extra_params.text_field.is_text_area = part == kTextAreaPart;
  extra_params.text_field.is_listbox = part == kListboxPart;
  extra_params.text_field.has_border = true;
  extra_params.text_field.zoom = style.EffectiveZoom();

  cc::PaintCanvas* canvas = paint_info.context.Canvas();

  Color background_color =
      style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  extra_params.text_field.background_color = background_color.Rgb();
  extra_params.text_field.auto_complete_active =
      DynamicTo<HTMLFormControlElement>(element)->IsAutofilled();

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartTextField, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintMenuList(const Element& element,
                                        const Document& document,
                                        const ComputedStyle& style,
                                        const PaintInfo& i,
                                        const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  // Match Chromium Win behaviour of showing all borders if any are shown.
  extra_params.menu_list.has_border = style.HasBorder();
  extra_params.menu_list.has_border_radius = style.HasBorderRadius();
  extra_params.menu_list.zoom = style.EffectiveZoom();
  // Fallback to transparent if the specified color object is invalid.
  Color background_color(Color::kTransparent);
  if (style.HasBackground()) {
    background_color =
        style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  }
  extra_params.menu_list.background_color = background_color.Rgb();

  // If we have a background image, don't fill the content area to expose the
  // parent's background. Also, we shouldn't fill the content area if the
  // alpha of the color is 0. The API of Windows GDI ignores the alpha.
  // FIXME: the normal Aura theme doesn't care about this, so we should
  // investigate if we really need fillContentArea.
  extra_params.menu_list.fill_content_area =
      !style.HasBackgroundImage() && background_color.Alpha();

  SetupMenuListArrow(document, style, rect, extra_params);

  cc::PaintCanvas* canvas = i.context.Canvas();
  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartMenuList, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintMenuListButton(const Element& element,
                                              const Document& document,
                                              const ComputedStyle& style,
                                              const PaintInfo& paint_info,
                                              const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  extra_params.menu_list.has_border = false;
  extra_params.menu_list.has_border_radius = style.HasBorderRadius();
  extra_params.menu_list.background_color = Color::kTransparent;
  extra_params.menu_list.fill_content_area = false;
  SetupMenuListArrow(document, style, rect, extra_params);

  cc::PaintCanvas* canvas = paint_info.context.Canvas();
  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartMenuList, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

void ThemePainterDefault::SetupMenuListArrow(
    const Document& document,
    const ComputedStyle& style,
    const IntRect& rect,
    WebThemeEngine::ExtraParams& extra_params) {
  const int left = rect.X() + floorf(style.BorderLeftWidth());
  const int right = rect.X() + rect.Width() - floorf(style.BorderRightWidth());
  const int middle = rect.Y() + rect.Height() / 2;

  extra_params.menu_list.arrow_y = middle;
  float arrow_box_width =
      theme_.ClampedMenuListArrowPaddingSize(document.GetFrame(), style);
  float arrow_scale_factor = arrow_box_width / theme_.MenuListArrowWidthInDIP();
  // TODO(tkent): This should be 7.0 to match scroll bar buttons.
  float arrow_size = 8.0 * arrow_scale_factor;
  // Put the arrow at the center of paddingForArrow area.
  // |arrowX| is the left position for Aura theme engine.
  extra_params.menu_list.arrow_x =
      (style.Direction() == TextDirection::kRtl)
          ? left + (arrow_box_width - arrow_size) / 2
          : right - (arrow_box_width + arrow_size) / 2;
  extra_params.menu_list.arrow_size = arrow_size;
  extra_params.menu_list.arrow_color =
      style.VisitedDependentColor(GetCSSPropertyColor()).Rgb();
}

bool ThemePainterDefault::PaintSliderTrack(const Element& element,
                                           const LayoutObject& o,
                                           const PaintInfo& i,
                                           const IntRect& rect,
                                           const ComputedStyle& style) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = i.context.Canvas();
  extra_params.slider.vertical =
      o.StyleRef().EffectiveAppearance() == kSliderVerticalPart;
  extra_params.slider.in_drag = false;

  PaintSliderTicks(o, i, rect);

  float zoom_level = o.StyleRef().EffectiveZoom();
  extra_params.slider.zoom = zoom_level;
  GraphicsContextStateSaver state_saver(i.context, false);
  IntRect unzoomed_rect = rect;

  auto* input = DynamicTo<HTMLInputElement>(element);
  extra_params.slider.thumb_x = 0;
  extra_params.slider.thumb_y = 0;
  extra_params.slider.right_to_left = !o.StyleRef().IsLeftToRightDirection();
  if (input) {
    Element* thumb_element = input->UserAgentShadowRoot()
                                 ? input->UserAgentShadowRoot()->getElementById(
                                       shadow_element_names::kIdSliderThumb)
                                 : nullptr;
    LayoutBox* thumb = thumb_element ? thumb_element->GetLayoutBox() : nullptr;
    LayoutBox* input_box = input->GetLayoutBox();
    if (thumb) {
      IntRect thumb_rect = PixelSnappedIntRect(thumb->FrameRect());
      extra_params.slider.thumb_x = thumb_rect.X() +
                                    input_box->PaddingLeft().ToInt() +
                                    input_box->BorderLeft().ToInt();
      extra_params.slider.thumb_y = thumb_rect.Y() +
                                    input_box->PaddingTop().ToInt() +
                                    input_box->BorderTop().ToInt();
    }
  }

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartSliderTrack, GetWebThemeState(element),
      gfx::Rect(unzoomed_rect), &extra_params, o.StyleRef().UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintSliderThumb(const Element& element,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = paint_info.context.Canvas();
  extra_params.slider.vertical =
      style.EffectiveAppearance() == kSliderThumbVerticalPart;
  extra_params.slider.in_drag = element.IsActive();

  float zoom_level = style.EffectiveZoom();
  extra_params.slider.zoom = zoom_level;
  GraphicsContextStateSaver state_saver(paint_info.context, false);
  IntRect unzoomed_rect = rect;

  // The element passed in is inside the user agent shadowdom of the input
  // element, so we have to access the parent input element in order to get the
  // accent-color style set by the page.
  const SliderThumbElement* slider_element =
      DynamicTo<SliderThumbElement>(&element);
  DCHECK(slider_element);  // PaintSliderThumb should always be passed a
                           // SliderThumbElement
  absl::optional<SkColor> accent_color =
      GetAccentColor(*slider_element->HostInput()->EnsureComputedStyle());

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartSliderThumb, GetWebThemeState(element),
      gfx::Rect(unzoomed_rect), &extra_params, style.UsedColorScheme(),
      accent_color);
  return false;
}

bool ThemePainterDefault::PaintInnerSpinButton(const Element& element,
                                               const ComputedStyle& style,
                                               const PaintInfo& paint_info,
                                               const IntRect& rect) {
  WebThemeEngine::ExtraParams extra_params;
  cc::PaintCanvas* canvas = paint_info.context.Canvas();

  bool spin_up = false;
  if (const auto* spin_buttom = DynamicTo<SpinButtonElement>(element)) {
    if (spin_buttom->GetUpDownState() == SpinButtonElement::kUp)
      spin_up = element.IsHovered() || element.IsActive();
  }

  bool read_only = false;
  if (const auto* control = DynamicTo<HTMLFormControlElement>(element))
    read_only = control->IsReadOnly();

  extra_params.inner_spin.spin_up = spin_up;
  extra_params.inner_spin.read_only = read_only;

  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartInnerSpinButton, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, style.UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintProgressBar(const Element& element,
                                           const LayoutObject& o,
                                           const PaintInfo& i,
                                           const IntRect& rect,
                                           const ComputedStyle& style) {
  if (!o.IsProgress())
    return true;

  const auto& layout_progress = To<LayoutProgress>(o);
  IntRect value_rect = ProgressValueRectFor(layout_progress, rect);

  WebThemeEngine::ExtraParams extra_params;
  extra_params.progress_bar.determinate = layout_progress.IsDeterminate();
  extra_params.progress_bar.value_rect_x = value_rect.X();
  extra_params.progress_bar.value_rect_y = value_rect.Y();
  extra_params.progress_bar.value_rect_width = value_rect.Width();
  extra_params.progress_bar.value_rect_height = value_rect.Height();
  extra_params.progress_bar.zoom = o.StyleRef().EffectiveZoom();

  DirectionFlippingScope scope(o, i, rect);
  cc::PaintCanvas* canvas = i.context.Canvas();
  Platform::Current()->ThemeEngine()->Paint(
      canvas, WebThemeEngine::kPartProgressBar, GetWebThemeState(element),
      gfx::Rect(rect), &extra_params, o.StyleRef().UsedColorScheme(),
      GetAccentColor(style));
  return false;
}

bool ThemePainterDefault::PaintTextArea(const Element& element,
                                        const ComputedStyle& style,
                                        const PaintInfo& paint_info,
                                        const IntRect& rect) {
  return PaintTextField(element, style, paint_info, rect);
}

bool ThemePainterDefault::PaintSearchField(const Element& element,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const IntRect& rect) {
  return PaintTextField(element, style, paint_info, rect);
}

bool ThemePainterDefault::PaintSearchFieldCancelButton(
    const LayoutObject& cancel_button_object,
    const PaintInfo& paint_info,
    const IntRect& r) {
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
               std::min(input_content_box.size.height, LayoutUnit(r.Height())));
  // Calculate cancel button's coordinates relative to the input element.
  // Center the button vertically.  Round up though, so if it has to be one
  // pixel off-center, it will be one pixel closer to the bottom of the field.
  // This tends to look better with the text.
  PhysicalRect cancel_button_rect(
      cancel_button_object.OffsetFromAncestor(&input_layout_box).left,
      input_content_box.Y() +
          (input_content_box.Height() - cancel_button_size + 1) / 2,
      cancel_button_size, cancel_button_size);
  IntRect painting_rect = ConvertToPaintingRect(
      input_layout_box, cancel_button_object, cancel_button_rect, r);
  mojom::blink::ColorScheme color_scheme =
      cancel_button_object.StyleRef().UsedColorScheme();
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
    bool text_is_dark = color_utils::GetRelativeLuminance(
                            SkColor(search_field_text_color)) < 0.5;
    color_scheme_adjusted_cancel_image =
        text_is_dark ? cancel_image_hc_light_mode : cancel_image_dark_mode;
    color_scheme_adjusted_cancel_pressed_image =
        color_scheme_adjusted_cancel_image =
            text_is_dark ? cancel_pressed_image_hc_light_mode
                         : cancel_pressed_image_dark_mode;
  } else {
    color_scheme_adjusted_cancel_image =
        color_scheme == mojom::blink::ColorScheme::kLight
            ? cancel_image
            : cancel_image_dark_mode;
    color_scheme_adjusted_cancel_pressed_image =
        color_scheme == mojom::blink::ColorScheme::kLight
            ? cancel_pressed_image
            : cancel_pressed_image_dark_mode;
  }
  paint_info.context.DrawImage(
      To<Element>(cancel_button_object.GetNode())->IsActive()
          ? color_scheme_adjusted_cancel_pressed_image
          : color_scheme_adjusted_cancel_image,
      Image::kSyncDecode, FloatRect(painting_rect));
  return false;
}

IntRect ThemePainterDefault::ApplyZoomToRect(
    const IntRect& rect,
    const PaintInfo& paint_info,
    GraphicsContextStateSaver& state_saver,
    float zoom_level) {
  IntRect unzoomed_rect = rect;
  if (zoom_level != 1) {
    state_saver.Save();
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  return unzoomed_rect;
}

}  // namespace blink
