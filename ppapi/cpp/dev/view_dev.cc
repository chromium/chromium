// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/view_dev.h"

#include "ppapi/c/dev/ppb_view_dev.h"
#include "ppapi/cpp/module_impl.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_View_Dev>() {
  return PPB_VIEW_DEV_INTERFACE;
}

}  // namespace

float ViewDev::GetDeviceScale() const {
  if (!has_interface<PPB_View_Dev>())
    return 1.0f;
  return get_interface<PPB_View_Dev>()->GetDeviceScale(pp_resource());
}

float ViewDev::GetCSSScale() const {
  if (!has_interface<PPB_View_Dev>())
    return 1.0f;
  return get_interface<PPB_View_Dev>()->GetCSSScale(pp_resource());
}

}  // namespace pp
