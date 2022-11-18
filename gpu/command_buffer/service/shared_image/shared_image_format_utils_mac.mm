// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"

#include "components/viz/common/resources/resource_format_utils.h"

namespace gpu {

// TODO (hitawala): Add support for multiplanar formats.
unsigned int ToMTLPixelFormat(viz::SharedImageFormat format) {
  return viz::ToMTLPixelFormat(format.resource_format());
}

}  // namespace gpu