// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_

#include "components/viz/common/resources/resource_format.h"

namespace gpu {

// TODO(vikassoni): In future we will need to expose the set of formats and
// constraints (e.g. max size) to the clients somehow that are available for
// certain combinations of SharedImageUsage flags (e.g. when Vulkan is on,
// SHARED_IMAGE_USAGE_GLES2 + SHARED_IMAGE_USAGE_DISPLAY implies AHB, so those
// restrictions apply, but that's decided on the service side).
// For now getting supported format is a static mechanism like this. We
// probably need something like gpu::Capabilities.texture_target_exception_list.

// Returns whether the format is supported by AHardwareBuffer.
bool AHardwareBufferSupportedFormat(viz::ResourceFormat format);

// Returns the corresponding AHardwareBuffer format.
unsigned int AHardwareBufferFormat(viz::ResourceFormat format);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_AHARDWAREBUFFER_UTILS_H_
