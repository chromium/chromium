// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/mouse_cursor.h"

#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_MouseCursor_1_0>() {
  return PPB_MOUSECURSOR_INTERFACE_1_0;
}

}  // namespace

// static
bool MouseCursor::SetCursor(const InstanceHandle& instance,
                            PP_MouseCursor_Type type,
                            const ImageData& image,
                            const Point& hot_spot) {
  if (!has_interface<PPB_MouseCursor_1_0>())
    return false;
  return PP_ToBool(get_interface<PPB_MouseCursor_1_0>()->SetCursor(
      instance.pp_instance(), type, image.pp_resource(),
      &hot_spot.pp_point()));
}

}  // namespace pp
