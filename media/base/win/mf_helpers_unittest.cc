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

TEST(MFHelpersTest, CreateSampleFromTextureFailsWhenKeyedMutexUnavailable) {
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                        0, D3D11_SDK_VERSION, &device, nullptr, &context);
  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
                           0, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) {
      GTEST_SKIP() << "D3D11 device creation failed";
    }
  }

  constexpr UINT kWidth = 64;
  constexpr UINT kHeight = 64;

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = kWidth;
  desc.Height = kHeight;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  hr = device->CreateTexture2D(&desc, nullptr, &input_texture);
  if (FAILED(hr)) {
    GTEST_SKIP() << "Shareable NV12 texture creation not supported";
  }

  // Simulate a producer that has released the texture under a non-zero key so
  // that the consumer must acquire the mutex before reading. The consumer only
  // ever acquires with key 0, so this makes the texture unavailable to it.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  ASSERT_HRESULT_SUCCEEDED(input_texture.As(&keyed_mutex));
  ASSERT_HRESULT_SUCCEEDED(keyed_mutex->AcquireSync(0, INFINITE));
  ASSERT_HRESULT_SUCCEEDED(keyed_mutex->ReleaseSync(1));

  gfx::Size coded_size(kWidth, kHeight);
  gfx::Rect visible_rect(coded_size);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size, visible_rect,
                              coded_size, base::TimeDelta());

  // Because the keyed mutex cannot be acquired, the copy cannot be performed
  // and CreateSampleFromTexture must fail rather than return a sample backed
  // by an unpopulated destination texture.
  Microsoft::WRL::ComPtr<IMFSample> sample = CreateSampleFromTexture(
      device, frame, input_texture, /*need_perform_copy=*/true);
  EXPECT_FALSE(sample);

  // Restore the mutex to key 0 for cleanup.
  ASSERT_HRESULT_SUCCEEDED(keyed_mutex->AcquireSync(1, INFINITE));
  ASSERT_HRESULT_SUCCEEDED(keyed_mutex->ReleaseSync(0));
}

TEST(MFHelpersTest, CreateSampleFromTextureCopiesKeyedMutexTexture) {
  Microsoft::WRL::ComPtr<ID3D11Device> device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                        0, D3D11_SDK_VERSION, &device, nullptr, &context);
  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr,
                           0, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) {
      GTEST_SKIP() << "D3D11 device creation failed";
    }
  }

  constexpr UINT kWidth = 64;
  constexpr UINT kHeight = 64;

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = kWidth;
  desc.Height = kHeight;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                   D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  hr = device->CreateTexture2D(&desc, nullptr, &input_texture);
  if (FAILED(hr)) {
    GTEST_SKIP() << "Shareable NV12 texture creation not supported";
  }

  gfx::Size coded_size(kWidth, kHeight);
  gfx::Rect visible_rect(coded_size);
  scoped_refptr<VideoFrame> frame =
      VideoFrame::CreateFrame(PIXEL_FORMAT_NV12, coded_size, visible_rect,
                              coded_size, base::TimeDelta());

  // The keyed mutex is available under key 0, so the copy should succeed.
  Microsoft::WRL::ComPtr<IMFSample> sample = CreateSampleFromTexture(
      device, frame, input_texture, /*need_perform_copy=*/true);
  ASSERT_TRUE(sample);

  // The mutex must have been released back under key 0 so that other
  // consumers can subsequently acquire it.
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  ASSERT_HRESULT_SUCCEEDED(input_texture.As(&keyed_mutex));
  EXPECT_EQ(keyed_mutex->AcquireSync(0, 0), S_OK);
  keyed_mutex->ReleaseSync(0);
}

class MFHelpersAlignmentTest
    : public ::testing::TestWithParam<VideoPixelFormat> {};

TEST_P(MFHelpersAlignmentTest,
       CreateSampleFromTextureRejectsUnalignedVisibleRect) {
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

  // Create a video frame with an unaligned visible rect.
  gfx::Size coded_size(1922, 1082);
  gfx::Rect visible_rect(1, 1, 1920, 1080);
  gfx::Size natural_size(1920, 1080);
  VideoPixelFormat format = GetParam();
  scoped_refptr<VideoFrame> frame = VideoFrame::CreateFrame(
      format, coded_size, visible_rect, natural_size, base::TimeDelta());

  if (!frame) {
    // VideoFrame::CreateFrame natively validates alignment for some formats
    // (e.g., I420) and might return null, in which case we safely skip.
    GTEST_SKIP() << "Cannot create unaligned frame natively for format "
                 << format;
  }

  // Create the sample and perform the copy.
  Microsoft::WRL::ComPtr<IMFSample> sample = CreateSampleFromTexture(
      device, frame, input_texture, /*need_perform_copy=*/true);

  // Because the visible_rect is unaligned, CreateSampleFromTexture should fail.
  EXPECT_FALSE(sample);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MFHelpersAlignmentTest,
                         ::testing::Values(PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_YV12,
                                           PIXEL_FORMAT_NV21));

}  // namespace
}  // namespace media
