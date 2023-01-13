// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_

#include "gpu/gpu_export.h"

namespace gpu {
namespace gles2 {
class GLES2DecoderImpl;
}

class GPU_EXPORT ImageFactory {
 public:
  virtual ~ImageFactory();

 protected:
  ImageFactory();

 private:
  // This class is used by validating command decoder for NaCL swapchain.
  friend class gles2::GLES2DecoderImpl;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_IMAGE_FACTORY_H_
