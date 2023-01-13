// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory_native_pixmap.h"

#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

ImageFactoryNativePixmap::ImageFactoryNativePixmap() = default;

ImageFactoryNativePixmap::~ImageFactoryNativePixmap() = default;

}  // namespace gpu
