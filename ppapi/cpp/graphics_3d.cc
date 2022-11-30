// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/graphics_3d.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/var.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics3D_1_0>() {
  return PPB_GRAPHICS_3D_INTERFACE_1_0;
}

}  // namespace

Graphics3D::Graphics3D() {
}

Graphics3D::Graphics3D(const InstanceHandle& instance,
                       const int32_t attrib_list[]) {
  if (has_interface<PPB_Graphics3D_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics3D_1_0>()->Create(
        instance.pp_instance(), 0, attrib_list));
  }
}

Graphics3D::Graphics3D(const InstanceHandle& instance,
                       const Graphics3D& share_context,
                       const int32_t attrib_list[]) {
  if (has_interface<PPB_Graphics3D_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics3D_1_0>()->Create(
        instance.pp_instance(),
        share_context.pp_resource(),
        attrib_list));
  }
}

Graphics3D::~Graphics3D() {
}

int32_t Graphics3D::GetAttribs(int32_t attrib_list[]) const {
  if (!has_interface<PPB_Graphics3D_1_0>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_1_0>()->GetAttribs(
      pp_resource(),
      attrib_list);
}

int32_t Graphics3D::SetAttribs(const int32_t attrib_list[]) {
  if (!has_interface<PPB_Graphics3D_1_0>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_1_0>()->SetAttribs(
      pp_resource(),
      attrib_list);
}

int32_t Graphics3D::ResizeBuffers(int32_t width, int32_t height) {
  if (!has_interface<PPB_Graphics3D_1_0>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_1_0>()->ResizeBuffers(
      pp_resource(), width, height);
}

int32_t Graphics3D::SwapBuffers(const CompletionCallback& cc) {
  if (!has_interface<PPB_Graphics3D_1_0>())
    return PP_ERROR_NOINTERFACE;

  return get_interface<PPB_Graphics3D_1_0>()->SwapBuffers(
      pp_resource(),
      cc.pp_completion_callback());
}

}  // namespace pp
