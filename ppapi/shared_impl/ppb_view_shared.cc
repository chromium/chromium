// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_view_shared.h"

#include <string.h>

namespace {

bool IsRectNonempty(const PP_Rect& rect) {
  return rect.size.width > 0 && rect.size.height > 0;
}

}  // namespace

namespace ppapi {

ViewData::ViewData() {
  // Assume POD.
  memset(this, 0, sizeof(ViewData));

  device_scale = 1.0f;
  css_scale = 1.0f;
}

ViewData::~ViewData() {}

bool ViewData::Equals(const ViewData& other) const {
  return rect.point.x == other.rect.point.x &&
         rect.point.y == other.rect.point.y &&
         rect.size.width == other.rect.size.width &&
         rect.size.height == other.rect.size.height &&
         is_fullscreen == other.is_fullscreen &&
         is_page_visible == other.is_page_visible &&
         clip_rect.point.x == other.clip_rect.point.x &&
         clip_rect.point.y == other.clip_rect.point.y &&
         clip_rect.size.width == other.clip_rect.size.width &&
         clip_rect.size.height == other.clip_rect.size.height &&
         device_scale == other.device_scale && css_scale == other.css_scale &&
         scroll_offset.x == other.scroll_offset.x &&
         scroll_offset.y == other.scroll_offset.y;
}

PPB_View_Shared::PPB_View_Shared(ResourceObjectType type,
                                 PP_Instance instance,
                                 const ViewData& data)
    : Resource(type, instance), data_(data) {}

PPB_View_Shared::~PPB_View_Shared() {}

thunk::PPB_View_API* PPB_View_Shared::AsPPB_View_API() { return this; }

const ViewData& PPB_View_Shared::GetData() const { return data_; }

PP_Bool PPB_View_Shared::GetRect(PP_Rect* viewport) const {
  if (!viewport)
    return PP_FALSE;
  *viewport = data_.rect;
  return PP_TRUE;
}

PP_Bool PPB_View_Shared::IsFullscreen() const {
  return PP_FromBool(data_.is_fullscreen);
}

PP_Bool PPB_View_Shared::IsVisible() const {
  return PP_FromBool(data_.is_page_visible && IsRectNonempty(data_.clip_rect));
}

PP_Bool PPB_View_Shared::IsPageVisible() const {
  return PP_FromBool(data_.is_page_visible);
}

PP_Bool PPB_View_Shared::GetClipRect(PP_Rect* clip) const {
  if (!clip)
    return PP_FALSE;
  *clip = data_.clip_rect;
  return PP_TRUE;
}

float PPB_View_Shared::GetDeviceScale() const { return data_.device_scale; }

float PPB_View_Shared::GetCSSScale() const { return data_.css_scale; }

PP_Bool PPB_View_Shared::GetScrollOffset(PP_Point* scroll_offset) const {
  if (!scroll_offset)
    return PP_FALSE;
  *scroll_offset = data_.scroll_offset;
  return PP_TRUE;
}

}  // namespace ppapi
