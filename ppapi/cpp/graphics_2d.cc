// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/graphics_2d.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_graphics_2d.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance_handle.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Graphics2D_1_0>() {
  return PPB_GRAPHICS_2D_INTERFACE_1_0;
}

template <> const char* interface_name<PPB_Graphics2D_1_1>() {
  return PPB_GRAPHICS_2D_INTERFACE_1_1;
}

template <> const char* interface_name<PPB_Graphics2D_1_2>() {
    return PPB_GRAPHICS_2D_INTERFACE_1_2;
}


}  // namespace

Graphics2D::Graphics2D() : Resource() {
}

Graphics2D::Graphics2D(const Graphics2D& other)
    : Resource(other),
      size_(other.size_) {
}

Graphics2D::Graphics2D(const InstanceHandle& instance,
                       const Size& size,
                       bool is_always_opaque)
    : Resource() {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics2D_1_1>()->Create(
        instance.pp_instance(),
        &size.pp_size(),
        PP_FromBool(is_always_opaque)));
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    PassRefFromConstructor(get_interface<PPB_Graphics2D_1_0>()->Create(
        instance.pp_instance(),
        &size.pp_size(),
        PP_FromBool(is_always_opaque)));
  } else {
    return;
  }
  if (!is_null()) {
    // Only save the size if allocation succeeded.
    size_ = size;
  }
}

Graphics2D::~Graphics2D() {
}

Graphics2D& Graphics2D::operator=(const Graphics2D& other) {
  Resource::operator=(other);
  size_ = other.size_;
  return *this;
}

void Graphics2D::PaintImageData(const ImageData& image,
                                const Point& top_left) {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    get_interface<PPB_Graphics2D_1_1>()->PaintImageData(pp_resource(),
                                                        image.pp_resource(),
                                                        &top_left.pp_point(),
                                                        NULL);
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    get_interface<PPB_Graphics2D_1_0>()->PaintImageData(pp_resource(),
                                                        image.pp_resource(),
                                                        &top_left.pp_point(),
                                                        NULL);
  }
}

void Graphics2D::PaintImageData(const ImageData& image,
                                const Point& top_left,
                                const Rect& src_rect) {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    get_interface<PPB_Graphics2D_1_1>()->PaintImageData(pp_resource(),
                                                        image.pp_resource(),
                                                        &top_left.pp_point(),
                                                        &src_rect.pp_rect());
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    get_interface<PPB_Graphics2D_1_0>()->PaintImageData(pp_resource(),
                                                        image.pp_resource(),
                                                        &top_left.pp_point(),
                                                        &src_rect.pp_rect());
  }
}

void Graphics2D::Scroll(const Rect& clip, const Point& amount) {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    get_interface<PPB_Graphics2D_1_1>()->Scroll(pp_resource(),
                                                &clip.pp_rect(),
                                                &amount.pp_point());
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    get_interface<PPB_Graphics2D_1_0>()->Scroll(pp_resource(),
                                                &clip.pp_rect(),
                                                &amount.pp_point());
  }
}

void Graphics2D::ReplaceContents(ImageData* image) {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    get_interface<PPB_Graphics2D_1_1>()->ReplaceContents(pp_resource(),
                                                         image->pp_resource());
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    get_interface<PPB_Graphics2D_1_0>()->ReplaceContents(pp_resource(),
                                                         image->pp_resource());
  } else {
    return;
  }

  // On success, reset the image data. This is to help prevent people
  // from continuing to use the resource which will result in artifacts.
  *image = ImageData();
}

int32_t Graphics2D::Flush(const CompletionCallback& cc) {
  if (has_interface<PPB_Graphics2D_1_1>()) {
    return get_interface<PPB_Graphics2D_1_1>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  } else if (has_interface<PPB_Graphics2D_1_0>()) {
    return get_interface<PPB_Graphics2D_1_0>()->Flush(
        pp_resource(), cc.pp_completion_callback());
  } else {
    return cc.MayForce(PP_ERROR_NOINTERFACE);
  }
}

bool Graphics2D::SetScale(float scale) {
  if (!has_interface<PPB_Graphics2D_1_1>())
    return false;
  return PP_ToBool(get_interface<PPB_Graphics2D_1_1>()->SetScale(pp_resource(),
                                                                 scale));
}

float Graphics2D::GetScale() {
  if (!has_interface<PPB_Graphics2D_1_1>())
    return 1.0f;
  return get_interface<PPB_Graphics2D_1_1>()->GetScale(pp_resource());
}

bool Graphics2D::SetLayerTransform(float scale,
                                   const Point& origin,
                                   const Point& translate) {
  if (!has_interface<PPB_Graphics2D_1_2>())
    return false;
  return PP_ToBool(get_interface<PPB_Graphics2D_1_2>()->SetLayerTransform(
      pp_resource(), scale, &origin.pp_point(), &translate.pp_point()));
}


}  // namespace pp
