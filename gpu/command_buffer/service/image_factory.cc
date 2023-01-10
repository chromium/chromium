// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory.h"

#include "base/notreached.h"

namespace gpu {

ImageFactory::ImageFactory() = default;

ImageFactory::~ImageFactory() = default;

unsigned ImageFactory::RequiredTextureType() {
  NOTIMPLEMENTED();
  return 0;
}

bool ImageFactory::SupportsFormatRGB() {
  return true;
}

ImageFactoryNativePixmap* ImageFactory::AsImageFactoryNativePixmap() {
  return nullptr;
}

}  // namespace gpu
