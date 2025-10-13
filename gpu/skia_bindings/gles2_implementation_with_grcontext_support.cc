// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"

#include <utility>

#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"

namespace skia_bindings {

GLES2ImplementationWithGrContextSupport::
    GLES2ImplementationWithGrContextSupport(
        gpu::gles2::GLES2CmdHelper* helper,
        scoped_refptr<gpu::gles2::ShareGroup> share_group,
        gpu::TransferBufferInterface* transfer_buffer,
        bool lose_context_when_out_of_memory,
        gpu::GpuControl* gpu_control)
    : GLES2Implementation(helper,
                          std::move(share_group),
                          transfer_buffer,
                          lose_context_when_out_of_memory,
                          gpu_control) {}

GLES2ImplementationWithGrContextSupport::
    ~GLES2ImplementationWithGrContextSupport() {}

}  // namespace skia_bindings
