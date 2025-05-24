// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_HOLDER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/gpu/graphite/BackendTexture.h"

namespace gpu {

class SharedContextState;

// Holds Skia Graphite allocated BackendTextures if RefCountedThreadSafe class.
// This holder can be used to extend BackendTexture's lifetime beyond that of
// the backing that holds it.
class WrappedGraphiteTextureHolder
    : public SkiaImageRepresentation::GraphiteTextureHolder {
 public:
  WrappedGraphiteTextureHolder(
      skgpu::graphite::BackendTexture backend_texture,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

 private:
  ~WrappedGraphiteTextureHolder() override;

  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_WRAPPED_GRAPHITE_TEXTURE_HOLDER_H_
