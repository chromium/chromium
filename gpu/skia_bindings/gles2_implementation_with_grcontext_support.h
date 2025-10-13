// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/client/gles2_implementation.h"

#ifndef GPU_SKIA_BINDINGS_GLES2_IMPLEMENTATION_WITH_GRCONTEXT_SUPPORT_H_
#define GPU_SKIA_BINDINGS_GLES2_IMPLEMENTATION_WITH_GRCONTEXT_SUPPORT_H_

namespace skia_bindings {

class GLES2ImplementationWithGrContextSupport
    : public gpu::gles2::GLES2Implementation {
 public:
  GLES2ImplementationWithGrContextSupport(
      gpu::gles2::GLES2CmdHelper* helper,
      scoped_refptr<gpu::gles2::ShareGroup> share_group,
      gpu::TransferBufferInterface* transfer_buffer,
      bool lose_context_when_out_of_memory,
      gpu::GpuControl* gpu_control);

  ~GLES2ImplementationWithGrContextSupport() override;

  typedef gpu::gles2::GLES2Implementation BaseClass;
};

}  // namespace skia_bindings

#endif  // GPU_SKIA_BINDINGS_GLES2_IMPLEMENTATION_WITH_GRCONTEXT_SUPPORT_H_
