// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/view.h"

#include "ppapi/c/ppb_view.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_View_1_0>() {
  return PPB_VIEW_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_View_1_1>() {
  return PPB_VIEW_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_View_1_2>() {
  return PPB_VIEW_INTERFACE_1_2;
}

}  // namespace

View::View() : Resource() {
}

View::View(PP_Resource view_resource) : Resource(view_resource) {
}

Rect View::GetRect() const {
  PP_Rect out;
  if (has_interface<PPB_View_1_2>()) {
    if (PP_ToBool(get_interface<PPB_View_1_2>()->GetRect(pp_resource(), &out)))
      return Rect(out);
  } else if (has_interface<PPB_View_1_1>()) {
    if (PP_ToBool(get_interface<PPB_View_1_1>()->GetRect(pp_resource(), &out)))
      return Rect(out);
  } else if (has_interface<PPB_View_1_0>()) {
    if (PP_ToBool(get_interface<PPB_View_1_0>()->GetRect(pp_resource(), &out)))
      return Rect(out);
  }
  return Rect();
}

bool View::IsFullscreen() const {
  if (has_interface<PPB_View_1_2>()) {
    return PP_ToBool(get_interface<PPB_View_1_2>()->IsFullscreen(
        pp_resource()));
  } else if (has_interface<PPB_View_1_1>()) {
    return PP_ToBool(get_interface<PPB_View_1_1>()->IsFullscreen(
        pp_resource()));
  } else if (has_interface<PPB_View_1_0>()) {
    return PP_ToBool(get_interface<PPB_View_1_0>()->IsFullscreen(
        pp_resource()));
  }
  return false;
}

bool View::IsVisible() const {
  if (has_interface<PPB_View_1_2>())
    return PP_ToBool(get_interface<PPB_View_1_2>()->IsVisible(pp_resource()));
  else if (has_interface<PPB_View_1_1>())
    return PP_ToBool(get_interface<PPB_View_1_1>()->IsVisible(pp_resource()));
  else if (has_interface<PPB_View_1_0>())
    return PP_ToBool(get_interface<PPB_View_1_0>()->IsVisible(pp_resource()));
  return false;
}

bool View::IsPageVisible() const {
  if (has_interface<PPB_View_1_2>()) {
    return PP_ToBool(get_interface<PPB_View_1_2>()->IsPageVisible(
        pp_resource()));
  } else if (has_interface<PPB_View_1_1>()) {
    return PP_ToBool(get_interface<PPB_View_1_1>()->IsPageVisible(
        pp_resource()));
  } else if (has_interface<PPB_View_1_0>()) {
    return PP_ToBool(get_interface<PPB_View_1_0>()->IsPageVisible(
        pp_resource()));
  }
  return true;
}

Rect View::GetClipRect() const {
  PP_Rect out;
  if (has_interface<PPB_View_1_2>()) {
    if (PP_ToBool(get_interface<PPB_View_1_2>()->GetClipRect(pp_resource(),
                                                             &out)))
      return Rect(out);
  } else if (has_interface<PPB_View_1_1>()) {
    if (PP_ToBool(get_interface<PPB_View_1_1>()->GetClipRect(pp_resource(),
                                                             &out)))
      return Rect(out);
  } else if (has_interface<PPB_View_1_0>()) {
    if (PP_ToBool(get_interface<PPB_View_1_0>()->GetClipRect(pp_resource(),
                                                             &out)))
      return Rect(out);
  }
  return Rect();
}

float View::GetDeviceScale() const {
  if (has_interface<PPB_View_1_2>())
    return get_interface<PPB_View_1_2>()->GetDeviceScale(pp_resource());
  else if (has_interface<PPB_View_1_1>())
    return get_interface<PPB_View_1_1>()->GetDeviceScale(pp_resource());
  return 1.0f;
}

float View::GetCSSScale() const {
  if (has_interface<PPB_View_1_2>())
    return get_interface<PPB_View_1_2>()->GetCSSScale(pp_resource());
  else if (has_interface<PPB_View_1_1>())
    return get_interface<PPB_View_1_1>()->GetCSSScale(pp_resource());
  return 1.0f;
}

Point View::GetScrollOffset() const {
  PP_Point out;
  if (has_interface<PPB_View_1_2>()) {
    if (PP_ToBool(get_interface<PPB_View_1_2>()->GetScrollOffset(pp_resource(),
                                                                 &out))) {
      return Point(out);
    }
  }
  return Point();
}

}  // namespace pp
