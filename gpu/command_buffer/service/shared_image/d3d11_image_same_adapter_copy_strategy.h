// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D11_IMAGE_SAME_ADAPTER_COPY_STRATEGY_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D11_IMAGE_SAME_ADAPTER_COPY_STRATEGY_H_

#include <windows.h>

#include <wrl/client.h>

#include "gpu/command_buffer/service/shared_image/shared_image_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

class ID3D11Texture2D;

namespace gpu {

// Implements a copy strategy for D3DImageBacking on different devices that
// are on the same adapter.
class D3D11ImageSameAdapterCopyStrategy : public SharedImageCopyStrategy {
 public:
  D3D11ImageSameAdapterCopyStrategy();
  ~D3D11ImageSameAdapterCopyStrategy() override;

  static bool CopyD3D11TextureOnSameAdapter(
      D3D11TextureAndArrayIndex source_texture,
      ID3D11Texture2D* dest_texture);

  // SharedImageCopyStrategy implementation.
  bool CanCopy(SharedImageBacking* source_backing,
               SharedImageBacking* dest_backing) override;
  bool Copy(SharedImageBacking* source_backing,
            SharedImageBacking* dest_backing) override;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D11_IMAGE_SAME_ADAPTER_COPY_STRATEGY_H_
