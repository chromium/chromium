/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
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
 */

#include "third_party/blink/renderer/core/paint/theme_painter.h"

#include "build/build_config.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/paint_auto_dark_mode.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

using mojom::blink::FormControlType;

namespace {

bool IsMultipleFieldsTemporalInput(FormControlType type) {
#if !BUILDFLAG(IS_ANDROID)
  return type == FormControlType::kInputDate ||
         type == FormControlType::kInputDatetimeLocal ||
         type == FormControlType::kInputMonth ||
         type == FormControlType::kInputTime ||
         type == FormControlType::kInputWeek;
#else
  return false;
#endif
}

}  // anonymous namespace

ThemePainter::ThemePainter() = default;

#define COUNT_APPEARANCE(doc, feature) \
  doc.CountUse(WebFeature::kCSSValueAppearance##feature##Rendered)

void CountAppearanceTextFieldPart(const Element& element) {
  if (auto* input = DynamicTo<HTMLInputElement>(element)) {
    FormControlType type = input->FormControlType();
    if (type == FormControlType::kInputSearch) {
      UseCounter::Count(element.GetDocument(),
                        WebFeature::kCSSValueAppearanceTextFieldForSearch);
    } else if (input->IsTextField()) {
      UseCounter::Count(element.GetDocument(),
                        WebFeature::kCSSValueAppearanceTextFieldForTextField);
    } else if (IsMultipleFieldsTemporalInput(type)) {
      UseCounter::Count(
          element.GetDocument(),
          WebFeature::kCSSValueAppearanceTextFieldForTemporalRendered);
    }
  }
}

// Returns true; Needs CSS painting and/or PaintBorderOnly().
bool ThemePainter::Paint(const LayoutObject& o,
                         const PaintInfo& paint_info,
                         const gfx::Rect& r) {
  Document& doc = o.GetDocument();
  const ComputedStyle& style = o.StyleRef();
  AppearanceValue appearance = o.StyleRef().EffectiveAppearance();
  // LayoutTheme::AdjustAppearanceWithElementType() ensures |node| is a
  // non-null Element.
  DCHECK(o.GetNode());
  DCHECK_NE(appearance, AppearanceValue::kNone);
  const Element& element = *To<Element>(o.GetNode());

  if (appearance == AppearanceValue::kButton) {
    if (IsA<HTMLButtonElement>(element)) {
      UseCounter::Count(doc, WebFeature::kCSSValueAppearanceButtonForButton);
    } else if (auto* input_element = DynamicTo<HTMLInputElement>(element);
               input_element && input_element->IsTextButton()) {
      // Text buttons (type=button, reset, submit) has
      // -webkit-appearance:push-button by default.
      UseCounter::Count(doc,
                        WebFeature::kCSSValueAppearanceButtonForOtherButtons);
    }
    //  'button' for input[type=color], of which default appearance is
    // 'square-button', is not deprecated or for ::scroll-button.
  }

  // Call the appropriate paint method based off the appearance value.
  switch (appearance) {
    case AppearanceValue::kCheckbox: {
      COUNT_APPEARANCE(doc, Checkbox);
      return PaintCheckbox(element, o.GetDocument(), style, paint_info, r);
    }
    case AppearanceValue::kRadio: {
      COUNT_APPEARANCE(doc, Radio);
      return PaintRadio(element, o.GetDocument(), style, paint_info, r);
    }
    case AppearanceValue::kPushButton: {
      COUNT_APPEARANCE(doc, PushButton);
      return PaintButton(element, o.GetDocument(), style, paint_info, r);
    }
    case AppearanceValue::kSquareButton: {
      COUNT_APPEARANCE(doc, SquareButton);
      return PaintButton(element, o.GetDocument(), style, paint_info, r);
    }
    case AppearanceValue::kButton:
      // UseCounter for this is handled at the beginning of the function.
      return PaintButton(element, o.GetDocument(), style, paint_info, r);
    case AppearanceValue::kInnerSpinButton: {
      COUNT_APPEARANCE(doc, InnerSpinButton);
      return PaintInnerSpinButton(element, style, paint_info, r);
    }
    case AppearanceValue::kMenulist:
      COUNT_APPEARANCE(doc, MenuList);
      return PaintMenuList(element, o.GetDocument(), style, paint_info, r);
    case AppearanceValue::kMeter:
      return true;
    case AppearanceValue::kProgressBar:
      COUNT_APPEARANCE(doc, ProgressBar);
      // Note that |-webkit-appearance: progress-bar| works only for <progress>.
      return PaintProgressBar(element, o, paint_info, r, style);
    case AppearanceValue::kSliderHorizontal: {
      COUNT_APPEARANCE(doc, SliderHorizontal);
      return PaintSliderTrack(element, o, paint_info, r, style);
    }
    case AppearanceValue::kSliderVertical: {
      COUNT_APPEARANCE(doc, SliderVertical);
      return PaintSliderTrack(element, o, paint_info, r, style);
    }
    case AppearanceValue::kSliderThumbHorizontal: {
      COUNT_APPEARANCE(doc, SliderThumbHorizontal);
      return PaintSliderThumb(element, style, paint_info, r);
    }
    case AppearanceValue::kSliderThumbVertical: {
      COUNT_APPEARANCE(doc, SliderThumbVertical);
      return PaintSliderThumb(element, style, paint_info, r);
    }
    case AppearanceValue::kMediaSlider:
      COUNT_APPEARANCE(doc, MediaSlider);
      return true;
    case AppearanceValue::kMediaSliderThumb:
      COUNT_APPEARANCE(doc, MediaSliderThumb);
      return true;
    case AppearanceValue::kMediaVolumeSlider:
      COUNT_APPEARANCE(doc, MediaVolumeSlider);
      return true;
    case AppearanceValue::kMediaVolumeSliderThumb:
      COUNT_APPEARANCE(doc, MediaVolumeSliderThumb);
      return true;
    case AppearanceValue::kMenulistButton:
      return true;
    case AppearanceValue::kTextField:
      CountAppearanceTextFieldPart(element);
      return PaintTextField(element, style, paint_info, r);
    case AppearanceValue::kTextArea:
      COUNT_APPEARANCE(doc, TextArea);
      return PaintTextArea(element, style, paint_info, r);
    case AppearanceValue::kSearchField: {
      COUNT_APPEARANCE(doc, SearchField);
      return PaintSearchField(element, style, paint_info, r);
    }
    case AppearanceValue::kSearchFieldCancelButton: {
      COUNT_APPEARANCE(doc, SearchCancel);
      return PaintSearchFieldCancelButton(o, paint_info, r);
    }
    case AppearanceValue::kListbox:
      return true;
    default:
      break;
  }

  // We don't support the appearance, so let the normal background/border paint.
  return true;
}

// Returns true; Needs CSS border painting.
bool ThemePainter::PaintBorderOnly(const Node* node,
                                   const ComputedStyle& style,
                                   const PaintInfo& paint_info,
                                   const gfx::Rect& r) {
  DCHECK(style.HasEffectiveAppearance());
  DCHECK(node);
  const Element& element = *To<Element>(node);
  // Call the appropriate paint method based off the appearance value.
  switch (style.EffectiveAppearance()) {
    case AppearanceValue::kTextField:
    case AppearanceValue::kTextArea:
      return false;
    case AppearanceValue::kMenulistButton:
    case AppearanceValue::kSearchField:
    case AppearanceValue::kListbox:
      return true;
    case AppearanceValue::kButton:
    case AppearanceValue::kCheckbox:
    case AppearanceValue::kInnerSpinButton:
    case AppearanceValue::kMenulist:
    case AppearanceValue::kProgressBar:
    case AppearanceValue::kPushButton:
    case AppearanceValue::kRadio:
    case AppearanceValue::kSearchFieldCancelButton:
    case AppearanceValue::kSliderHorizontal:
    case AppearanceValue::kSliderThumbHorizontal:
    case AppearanceValue::kSliderThumbVertical:
    case AppearanceValue::kSliderVertical:
    case AppearanceValue::kSquareButton:
      // Supported appearance values don't need CSS border painting.
      return false;
    case AppearanceValue::kBaseSelect:
    case AppearanceValue::kBase:
      return true;
    case AppearanceValue::kNone:
    case AppearanceValue::kAuto:
      // AppearanceValue::kNone isn't possible because callers should only
      // call this function when HasEffectiveAppearance is true. kAutoPart isn't
      // possible because it can't be an effective appearance.
      NOTREACHED();
    // TODO(dbaron): The following values were previously covered by a
    // default: case and should be classified correctly:
    case AppearanceValue::kMediaControl:
    case AppearanceValue::kMeter:
    case AppearanceValue::kMediaSlider:
    case AppearanceValue::kMediaSliderThumb:
    case AppearanceValue::kMediaVolumeSlider:
    case AppearanceValue::kMediaVolumeSliderThumb:
      UseCounter::Count(
          element.GetDocument(),
          WebFeature::kCSSValueAppearanceNoImplementationSkipBorder);
      // TODO(tkent): Should do CSS border painting for non-supported
      // appearance values.
      return false;
  }
}

bool ThemePainter::PaintDecorations(const Node* node,
                                    const Document& document,
                                    const ComputedStyle& style,
                                    const PaintInfo& paint_info,
                                    const gfx::Rect& r) {
  DCHECK(node);
  // Call the appropriate paint method based off the appearance value.
  switch (style.EffectiveAppearance()) {
    case AppearanceValue::kMenulistButton:
      COUNT_APPEARANCE(document, MenuListButton);
      return PaintMenuListButton(*To<Element>(node), document, style,
                                 paint_info, r);
    case AppearanceValue::kTextField:
    case AppearanceValue::kTextArea:
    case AppearanceValue::kCheckbox:
    case AppearanceValue::kRadio:
    case AppearanceValue::kPushButton:
    case AppearanceValue::kSquareButton:
    case AppearanceValue::kButton:
    case AppearanceValue::kMenulist:
    case AppearanceValue::kMeter:
    case AppearanceValue::kProgressBar:
    case AppearanceValue::kSliderHorizontal:
    case AppearanceValue::kSliderVertical:
    case AppearanceValue::kSliderThumbHorizontal:
    case AppearanceValue::kSliderThumbVertical:
    case AppearanceValue::kSearchField:
    case AppearanceValue::kSearchFieldCancelButton:
    default:
      break;
  }

  return false;
}

#undef COUNT_APPEARANCE

void ThemePainter::PaintSliderTicks(const LayoutObject& o,
                                    const PaintInfo& paint_info,
                                    const gfx::Rect& rect) {
  auto* input = DynamicTo<HTMLInputElement>(o.GetNode());
  if (!input)
    return;

  if (input->FormControlType() != FormControlType::kInputRange ||
      !input->UserAgentShadowRoot()->HasChildren()) {
    return;
  }

  HTMLDataListElement* data_list = input->DataList();
  if (!data_list)
    return;

  double min = input->Minimum();
  double max = input->Maximum();
  if (min >= max)
    return;

  const ComputedStyle& style = o.StyleRef();
  AppearanceValue appearance = style.EffectiveAppearance();
  // We don't support ticks on alternate sliders like MediaVolumeSliders.
  bool is_slider_vertical =
      RuntimeEnabledFeatures::
          NonStandardAppearanceValueSliderVerticalEnabled() &&
      appearance == AppearanceValue::kSliderVertical;
  bool is_writing_mode_vertical = !style.IsHorizontalWritingMode();
  if (!(appearance == AppearanceValue::kSliderHorizontal ||
        is_slider_vertical)) {
    return;
  }
  bool is_horizontal = !is_writing_mode_vertical && !is_slider_vertical;

  gfx::Size thumb_size;
  LayoutObject* thumb_layout_object =
      input->UserAgentShadowRoot()
          ->getElementById(shadow_element_names::kIdSliderThumb)
          ->GetLayoutObject();
  if (thumb_layout_object && thumb_layout_object->IsBox()) {
    thumb_size =
        ToFlooredSize(To<LayoutBox>(thumb_layout_object)->StitchedSize());
  }

  gfx::Size tick_size = LayoutTheme::GetTheme().SliderTickSize();
  float zoom_factor = style.EffectiveZoom();
  gfx::RectF tick_rect;
  int tick_region_side_margin = 0;
  int tick_region_width = 0;
  gfx::Rect track_bounds;
  LayoutObject* track_layout_object =
      input->UserAgentShadowRoot()
          ->getElementById(shadow_element_names::kIdSliderTrack)
          ->GetLayoutObject();
  if (track_layout_object && track_layout_object->IsBox()) {
    track_bounds = gfx::Rect(
        ToCeiledPoint(track_layout_object->FirstFragment().PaintOffset()),
        ToFlooredSize(To<LayoutBox>(track_layout_object)->StitchedSize()));
  }

  const float tick_offset_from_center =
      LayoutTheme::GetTheme().SliderTickOffsetFromTrackCenter() * zoom_factor;
  const float tick_inline_size = tick_size.width() * zoom_factor;
  const float tick_block_size = tick_size.height() * zoom_factor;
  const auto writing_direction = style.GetWritingDirection();
  if (is_horizontal) {
    tick_rect.set_size({floor(tick_inline_size), floor(tick_block_size)});
    tick_rect.set_y(
        floor(rect.y() + rect.height() / 2.0 + tick_offset_from_center));
    tick_region_side_margin =
        track_bounds.x() + (thumb_size.width() - tick_inline_size) / 2.0;
    tick_region_width = track_bounds.width() - thumb_size.width();
  } else {
    tick_rect.set_size({floor(tick_block_size), floor(tick_inline_size)});
    const float slider_center = rect.x() + rect.width() / 2.0;
    const float tick_x =
        (style.IsHorizontalTypographicMode() &&
         writing_direction.LineUnder() == PhysicalDirection::kLeft)
            ? (slider_center - tick_offset_from_center - tick_block_size)
            : (slider_center + tick_offset_from_center);
    tick_rect.set_x(floor(tick_x));
    tick_region_side_margin =
        track_bounds.y() + (thumb_size.height() - tick_inline_size) / 2.0;
    tick_region_width = track_bounds.height() - thumb_size.height();
  }
  HTMLDataListOptionsCollection* options = data_list->options();
  bool flip_tick_direction = true;
  if (is_horizontal || is_writing_mode_vertical) {
    PhysicalDirection inline_end = writing_direction.InlineEnd();
    flip_tick_direction = inline_end == PhysicalDirection::kLeft ||
                          inline_end == PhysicalDirection::kUp;
  }
  for (unsigned i = 0; HTMLOptionElement* option_element = options->Item(i);
       i++) {
    String value = option_element->value();
    if (option_element->IsDisabledFormControl() || value.empty())
      continue;
    if (!input->IsValidValue(value))
      continue;
    double parsed_value =
        ParseToDoubleForNumberType(input->SanitizeValue(value));
    double tick_fraction = (parsed_value - min) / (max - min);
    double tick_ratio =
        flip_tick_direction ? 1.0 - tick_fraction : tick_fraction;
    double tick_position =
        round(tick_region_side_margin + tick_region_width * tick_ratio);
    if (is_horizontal)
      tick_rect.set_x(tick_position);
    else
      tick_rect.set_y(tick_position);
    paint_info.context.FillRect(
        tick_rect, o.ResolveColor(GetCSSPropertyColor()),
        PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground));
  }
}

}  // namespace blink
