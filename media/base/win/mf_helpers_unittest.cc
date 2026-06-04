// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_helpers.h"

#include <d3d11.h>
#include <mfapi.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

TEST(MFHelpersTest, CreateSampleFromTextureDoesNotLeakUninitializedMemory) {
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                        0, D3D11_SDK_VERSION, &device, nullptr, &context);
  if (FAILED(hr)) {
    // Fallback to WARP if hardware is not available.
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
                           0, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) {
      GTEST_SKIP() << "D3D11 device creation failed";
    }
  }

  // Create a texture with a larger coded size than the visible size.
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = 1920;
  desc.Height = 1088;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  hr = device->CreateTexture2D(&desc, nullptr, &input_texture);
  ASSERT_TRUE(SUCCEEDED(hr));

  // Create a video frame with a smaller visible rect.
  gfx::Size coded_size(1920, 1088);
  gfx::Rect visible_rect(0, 0, 1920, 1080);
  gfx::Size natural_size(1920, 1080);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size, visible_rect,
                              natural_size, base::TimeDelta());

  // Create the sample and perform the copy.
  Microsoft::WRL::ComPtr<IMFSample> sample = CreateSampleFromTexture(
      device, frame, input_texture, /*need_perform_copy=*/true);
  ASSERT_TRUE(sample);

  // Get the texture from the sample.
  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  hr = sample->GetBufferByIndex(0, &buffer);
  ASSERT_TRUE(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  hr = buffer.As(&dxgi_buffer);
  ASSERT_TRUE(SUCCEEDED(hr));

  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&output_texture));
  ASSERT_TRUE(SUCCEEDED(hr));

  D3D11_TEXTURE2D_DESC output_desc;
  output_texture->GetDesc(&output_desc);

  // The copied texture should exactly match the visible rect, not the coded
  // size. This ensures no uninitialized padding is left.
  EXPECT_EQ(output_desc.Width, static_cast<UINT>(visible_rect.width()));
  EXPECT_EQ(output_desc.Height, static_cast<UINT>(visible_rect.height()));
}

}  // namespace
}  // namespace media
