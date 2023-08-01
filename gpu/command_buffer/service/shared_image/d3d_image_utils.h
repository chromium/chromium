// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <dawn/native/D3DBackend.h>
#include <webgpu/webgpu_cpp.h>

#include "base/containers/span.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gpu {

bool ClearD3D11TextureToColor(
    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& d3d11_texture,
    const SkColor4f& color);

std::unique_ptr<dawn::native::d3d::ExternalImageDXGI>
CreateDawnExternalImageDXGI(
    const wgpu::Device& device,
    uint32_t shared_image_usage,
    const D3D11_TEXTURE2D_DESC& d3d11_texture_desc,
    absl::variant<HANDLE, Microsoft::WRL::ComPtr<ID3D11Texture2D>>
        handle_or_texture,
    base::span<wgpu::TextureFormat> view_formats);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_D3D_IMAGE_UTILS_H_
