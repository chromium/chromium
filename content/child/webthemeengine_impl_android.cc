// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webthemeengine_impl_android.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "ui/native_theme/native_theme.h"

using blink::WebRect;
using blink::WebThemeEngine;

namespace content {

namespace {
  const int kVersionLollipop = 5;

  int getMajorVersion() {
    int major, minor, bugfix;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    return major;
  }
}

static ui::NativeTheme::Part NativeThemePart(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarDownArrow:
      return ui::NativeTheme::kScrollbarDownArrow;
    case WebThemeEngine::kPartScrollbarLeftArrow:
      return ui::NativeTheme::kScrollbarLeftArrow;
    case WebThemeEngine::kPartScrollbarRightArrow:
      return ui::NativeTheme::kScrollbarRightArrow;
    case WebThemeEngine::kPartScrollbarUpArrow:
      return ui::NativeTheme::kScrollbarUpArrow;
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
      return ui::NativeTheme::kScrollbarHorizontalThumb;
    case WebThemeEngine::kPartScrollbarVerticalThumb:
      return ui::NativeTheme::kScrollbarVerticalThumb;
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
      return ui::NativeTheme::kScrollbarHorizontalTrack;
    case WebThemeEngine::kPartScrollbarVerticalTrack:
      return ui::NativeTheme::kScrollbarVerticalTrack;
    case WebThemeEngine::kPartScrollbarCorner:
      return ui::NativeTheme::kScrollbarCorner;
    case WebThemeEngine::kPartCheckbox:
      return ui::NativeTheme::kCheckbox;
    case WebThemeEngine::kPartRadio:
      return ui::NativeTheme::kRadio;
    case WebThemeEngine::kPartButton:
      return ui::NativeTheme::kPushButton;
    case WebThemeEngine::kPartTextField:
      return ui::NativeTheme::kTextField;
    case WebThemeEngine::kPartMenuList:
      return ui::NativeTheme::kMenuList;
    case WebThemeEngine::kPartSliderTrack:
      return ui::NativeTheme::kSliderTrack;
    case WebThemeEngine::kPartSliderThumb:
      return ui::NativeTheme::kSliderThumb;
    case WebThemeEngine::kPartInnerSpinButton:
      return ui::NativeTheme::kInnerSpinButton;
    case WebThemeEngine::kPartProgressBar:
      return ui::NativeTheme::kProgressBar;
    default:
      return ui::NativeTheme::kScrollbarDownArrow;
  }
}

static ui::NativeTheme::State NativeThemeState(WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::kStateDisabled:
      return ui::NativeTheme::kDisabled;
    case WebThemeEngine::kStateHover:
      return ui::NativeTheme::kHovered;
    case WebThemeEngine::kStateNormal:
      return ui::NativeTheme::kNormal;
    case WebThemeEngine::kStatePressed:
      return ui::NativeTheme::kPressed;
    default:
      return ui::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      break;
    case WebThemeEngine::kPartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebThemeEngine::kPartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebThemeEngine::kPartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.is_default;
      native_theme_extra_params->button.has_border =
          extra_params->button.has_border;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.background_color;
      break;
    case WebThemeEngine::kPartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->text_field.is_text_area;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->text_field.is_listbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->text_field.background_color;
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
      break;
    case WebThemeEngine::kPartSliderTrack:
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
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

blink::WebSize WebThemeEngineImpl::GetSize(WebThemeEngine::Part part) {
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

void WebThemeEngineImpl::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  // TODO(bokan): Android scrollbars on non-composited scrollers don't
  // currently fade out so the fadeOutDuration and Delay  Now that this has
  // been added into Blink for other platforms we should plumb that through for
  // Android as well.
  style->fade_out_delay = base::TimeDelta();
  style->fade_out_duration = base::TimeDelta();
  if (getMajorVersion() >= kVersionLollipop) {
    style->thumb_thickness = 4;
    style->scrollbar_margin = 0;
    style->color = SkColorSetARGB(128, 64, 64, 64);
  } else {
    style->thumb_thickness = 3;
    style->scrollbar_margin = 3;
    style->color = SkColorSetARGB(128, 128, 128, 128);
  }
}

void WebThemeEngineImpl::Paint(
    cc::PaintCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const blink::WebRect& rect,
    const WebThemeEngine::ExtraParams* extra_params) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, NativeThemePart(part), NativeThemeState(state), gfx::Rect(rect),
      native_theme_extra_params);
}
}  // namespace content
