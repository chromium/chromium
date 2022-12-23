// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/image_factory_stub.h"

#include <GLES2/gl2extchromium.h>

namespace gpu {

unsigned ImageFactoryStub::RequiredTextureType() {
  return GL_TEXTURE_RECTANGLE_ARB;
}

bool ImageFactoryStub::SupportsFormatRGB() {
  return false;
}

}  // namespace gpu
