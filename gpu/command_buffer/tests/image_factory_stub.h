// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_IMAGE_FACTORY_STUB_H_
#define GPU_COMMAND_BUFFER_TESTS_IMAGE_FACTORY_STUB_H_

#include "gpu/command_buffer/service/image_factory.h"

namespace gpu {

// Stub implementation of ImageFactory for tests.
class ImageFactoryStub : public gpu::ImageFactory {
 public:
  unsigned RequiredTextureType() override;
  bool SupportsFormatRGB() override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_IMAGE_FACTORY_STUB_H_
