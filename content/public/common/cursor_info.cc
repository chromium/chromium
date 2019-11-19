// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/cursor_info.h"

#include "ui/gfx/skia_util.h"

namespace content {

CursorInfo::CursorInfo(ui::CursorType cursor) : type(cursor) {}

CursorInfo::CursorInfo(const blink::WebCursorInfo& info)
    : type(info.type),
      custom_image(info.custom_image),
      hotspot(info.hot_spot),
      image_scale_factor(info.image_scale_factor) {}

bool CursorInfo::operator==(const CursorInfo& other) const {
  return type == other.type && hotspot == other.hotspot &&
         image_scale_factor == other.image_scale_factor &&
         gfx::BitmapsAreEqual(custom_image, other.custom_image);
}

blink::WebCursorInfo CursorInfo::GetWebCursorInfo() const {
  blink::WebCursorInfo info;
  info.type = type;
  info.hot_spot = hotspot;
  info.custom_image = custom_image;
  info.image_scale_factor = image_scale_factor;
  return info;
}

}  // namespace content
