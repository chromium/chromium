// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_PRESENTATION_FEEDBACK_UTILS_H_
#define GPU_COMMAND_BUFFER_COMMON_PRESENTATION_FEEDBACK_UTILS_H_

#include <cstdint>

#include "gpu/gpu_export.h"

namespace gfx {
struct PresentationFeedback;
}

namespace gpu {

// Returns true if command buffer should update vsync timing paramters based on
// presentation feedback.
GPU_EXPORT bool ShouldUpdateVsyncParams(
    const gfx::PresentationFeedback& feedback);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_PRESENTATION_FEEDBACK_UTILS_H_
