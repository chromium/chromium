// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_

#include <memory>

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
namespace raster {

struct RasterDecoderContextState;

class GPU_GLES2_EXPORT WrappedSkImageFactory
    : public gpu::SharedImageBackingFactory {
 public:
  explicit WrappedSkImageFactory(RasterDecoderContextState* context_state);
  ~WrappedSkImageFactory() override;

  // SharedImageBackingFactory implementation:
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;

 private:
  RasterDecoderContextState* const context_state_;

  DISALLOW_COPY_AND_ASSIGN(WrappedSkImageFactory);
};

}  // namespace raster
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_WRAPPED_SK_IMAGE_H_
