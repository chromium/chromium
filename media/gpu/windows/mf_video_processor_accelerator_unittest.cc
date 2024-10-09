// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/mf_video_processor_accelerator.h"

#include <d3d11.h>
#include <mfapi.h>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_dxgi.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/media_util.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MFVideoProcessorAcceleratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dxgi_device_man_ = DXGIDeviceManager::Create({0, 0});
    ASSERT_TRUE(dxgi_device_man_);
    d3d11_device_ = dxgi_device_man_->GetDevice();
  }

  void TearDown() override {
    ASSERT_HRESULT_SUCCEEDED(MFUnlockDXGIDeviceManager());
  }

  HRESULT CreateTexture(UINT width,
                        UINT height,
                        DXGI_FORMAT format,
                        BYTE* image_buffer,
                        ID3D11Texture2D** texture_out) {
    D3D11_TEXTURE2D_DESC desc;
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = image_buffer;
    data.SysMemPitch = width * sizeof(RGBQUAD);
    data.SysMemSlicePitch = 0;

    return d3d11_device_->CreateTexture2D(&desc, &data, texture_out);
  }

  std::vector<BYTE> CreateRGBCheckerboard(UINT width,
                                          UINT height,
                                          RGBQUAD color0,
                                          RGBQUAD color1) {
    std::vector<BYTE> image(width * height * 4);
    for (UINT y = 0; y < height; y++) {
      for (UINT x = 0; x < width; x++) {
        bool use_color0 =
            ((x % 4) < 2 && (y % 4) < 2) || ((x % 4) >= 2 && (y % 4) >= 2);
        RGBQUAD& color = use_color0 ? color0 : color1;
        image[y * width * 4 + x * 4 + 0] = color.rgbBlue;
        image[y * width * 4 + x * 4 + 1] = color.rgbGreen;
        image[y * width * 4 + x * 4 + 2] = color.rgbRed;
        image[y * width * 4 + x * 4 + 3] = color.rgbReserved;
      }
    }
    return image;
  }

  std::unique_ptr<gfx::GpuMemoryBuffer>
  TextureToGpuMemoryBuffer(ID3D11Texture2D* texture, int width, int height) {
    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    HRESULT hr = texture->QueryInterface(IID_PPV_ARGS(&dxgi_resource));
    if (FAILED(hr)) {
      return nullptr;
    }
    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &shared_handle);
    if (FAILED(hr)) {
      return nullptr;
    }
    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.dxgi_handle.Set(shared_handle);
    gmb_handle.dxgi_token = gfx::DXGIHandleToken();
    gmb_handle.type = gfx::DXGI_SHARED_HANDLE;
    std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
        gpu::GpuMemoryBufferImplDXGI::CreateFromHandle(
            std::move(gmb_handle), {width, height},
            gfx::BufferFormat::BGRA_8888, gfx::BufferUsage::GPU_READ,
            base::NullCallback(), nullptr, nullptr);
    return gmb;
  }

  template <typename F>
  void ValidateResult(ID3D11Texture2D* texture,
                      UINT width,
                      UINT height,
                      F validation_func) {
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);
    ASSERT_EQ(desc.Width, width);
    ASSERT_EQ(desc.Height, height);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ASSERT_HRESULT_SUCCEEDED(
        d3d11_device_->CreateTexture2D(&desc, nullptr, &staging_texture));
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
    d3d11_device_->GetImmediateContext(&d3d11_context);
    d3d11_context->CopyResource(staging_texture.Get(), texture);
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    ASSERT_HRESULT_SUCCEEDED(d3d11_context->Map(
        staging_texture.Get(), 0, D3D11_MAP_READ, 0, &mapped_subresource));

    BYTE* image = reinterpret_cast<BYTE*>(mapped_subresource.pData);
    validation_func(image);

    d3d11_context->Unmap(staging_texture.Get(), 0);
  }

  template <typename F>
  void ValidateResult(IMFMediaBuffer* buffer, UINT size, F validation_func) {
    MediaBufferScopedPointer scoped_buffer(buffer);
    ASSERT_EQ(scoped_buffer.current_length(), size);
    validation_func(scoped_buffer.get());
  }

  scoped_refptr<DXGIDeviceManager> dxgi_device_man_;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  UINT device_reset_token_ = 0;
  static const RGBQUAD kGreen;
  static const RGBQUAD kMagenta;
  // These values are for BT709 with 16-235 nominal range
  static const BYTE kLumaGreen = 173;
  static const BYTE kLumaMagenta = 79;
};

const RGBQUAD MFVideoProcessorAcceleratorTest::kGreen = {0, 255, 0, 0};
const RGBQUAD MFVideoProcessorAcceleratorTest::kMagenta = {255, 0, 255, 0};

// This is unpleasant, but GTEST_SKIP must be run inside the actual
// test call or the test will not get skipped.
#define CheckForVideoDevice()                                                  \
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> d3d11_video_device;                \
  HRESULT hr_has_video_device = d3d11_device_.As(&d3d11_video_device);         \
  if (FAILED(hr_has_video_device)) {                                           \
    GTEST_SKIP()                                                               \
        << " gpu accelerated video processing not available on this platform"; \
  }

TEST_F(MFVideoProcessorAcceleratorTest, RGBToNV12) {
  CheckForVideoDevice();

  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<MediaFoundationVideoProcessorAccelerator> video_processor;
  video_processor = std::make_unique<MediaFoundationVideoProcessorAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  MediaFoundationVideoProcessorAccelerator::Config config;
  config.input_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.input_visible_size = {kWidth, kHeight};
  config.input_color_space = gfx::ColorSpace::CreateREC709();
  config.output_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  config.output_visible_size = {kWidth, kHeight};
  config.output_color_space = gfx::ColorSpace::CreateREC709();
  ASSERT_TRUE(video_processor->Initialize(config, dxgi_device_man_,
                                          std::make_unique<NullMediaLog>()));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth, kHeight, kGreen, kMagenta);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ASSERT_HRESULT_SUCCEEDED(CreateTexture(
      kWidth, kHeight, DXGI_FORMAT_B8G8R8A8_UNORM, image.data(), &texture));

  // Flush graphics pipeline so initial texture data is available when
  // texture is accessed through shared handle.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device_->GetImmediateContext(&d3d11_context);
  d3d11_context->Flush();

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      TextureToGpuMemoryBuffer(texture.Get(), kWidth, kHeight);
  ASSERT_TRUE(gmb);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      {kWidth, kHeight}, {kWidth, kHeight}, std::move(gmb), timestamp);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  ASSERT_HRESULT_SUCCEEDED(video_processor->Convert(frame, &sample));

  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  ASSERT_HRESULT_SUCCEEDED(sample->GetBufferByIndex(0, &media_buffer));
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  ASSERT_HRESULT_SUCCEEDED(media_buffer.As(&dxgi_buffer));
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  ASSERT_HRESULT_SUCCEEDED(
      dxgi_buffer->GetResource(IID_PPV_ARGS(&output_texture)));
  ValidateResult(output_texture.Get(), kWidth, kHeight, [](BYTE* image) {
    EXPECT_NEAR(image[0], kLumaGreen, 1);
    EXPECT_NEAR(image[2], kLumaMagenta, 1);
  });
}

TEST_F(MFVideoProcessorAcceleratorTest, RGBResize) {
  CheckForVideoDevice();

  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<MediaFoundationVideoProcessorAccelerator> video_processor;
  video_processor = std::make_unique<MediaFoundationVideoProcessorAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  MediaFoundationVideoProcessorAccelerator::Config config;
  config.input_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.input_visible_size = {kWidth, kHeight};
  config.input_color_space = gfx::ColorSpace::CreateREC709();
  config.output_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.output_visible_size = {kWidth / 2, kHeight / 2};
  config.output_color_space = gfx::ColorSpace::CreateREC709();
  ASSERT_TRUE(video_processor->Initialize(config, dxgi_device_man_,
                                          std::make_unique<NullMediaLog>()));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth, kHeight, kGreen, kMagenta);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ASSERT_HRESULT_SUCCEEDED(CreateTexture(
      kWidth, kHeight, DXGI_FORMAT_B8G8R8A8_UNORM, image.data(), &texture));

  // Flush graphics pipeline so initial texture data is available when
  // texture is accessed through shared handle.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device_->GetImmediateContext(&d3d11_context);
  d3d11_context->Flush();

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      TextureToGpuMemoryBuffer(texture.Get(), kWidth, kHeight);
  ASSERT_TRUE(gmb);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      {kWidth, kHeight}, {kWidth, kHeight}, std::move(gmb), timestamp);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  ASSERT_HRESULT_SUCCEEDED(video_processor->Convert(frame, &sample));

  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  ASSERT_HRESULT_SUCCEEDED(sample->GetBufferByIndex(0, &media_buffer));
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  ASSERT_HRESULT_SUCCEEDED(media_buffer.As(&dxgi_buffer));
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  ASSERT_HRESULT_SUCCEEDED(
      dxgi_buffer->GetResource(IID_PPV_ARGS(&output_texture)));
  ValidateResult(output_texture.Get(), kWidth / 2, kHeight / 2,
                 [](BYTE* image) {
                   EXPECT_EQ(image[0], 0);
                   EXPECT_EQ(image[1], 255);
                   EXPECT_EQ(image[2], 0);
                   EXPECT_EQ(image[4], 255);
                   EXPECT_EQ(image[5], 0);
                   EXPECT_EQ(image[6], 255);
                 });
}

TEST_F(MFVideoProcessorAcceleratorTest, RGBToNV12Resize) {
  CheckForVideoDevice();

  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<MediaFoundationVideoProcessorAccelerator> video_processor;
  video_processor = std::make_unique<MediaFoundationVideoProcessorAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  MediaFoundationVideoProcessorAccelerator::Config config;
  config.input_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.input_visible_size = {kWidth, kHeight};
  config.input_color_space = gfx::ColorSpace::CreateREC709();
  config.output_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  config.output_visible_size = {kWidth / 2, kHeight / 2};
  config.output_color_space = gfx::ColorSpace::CreateREC709();
  ;
  ASSERT_TRUE(video_processor->Initialize(config, dxgi_device_man_,
                                          std::make_unique<NullMediaLog>()));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth, kHeight, kGreen, kMagenta);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ASSERT_HRESULT_SUCCEEDED(CreateTexture(
      kWidth, kHeight, DXGI_FORMAT_B8G8R8A8_UNORM, image.data(), &texture));

  // Flush graphics pipeline so initial texture data is available when
  // texture is accessed through shared handle.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device_->GetImmediateContext(&d3d11_context);
  d3d11_context->Flush();

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      TextureToGpuMemoryBuffer(texture.Get(), kWidth, kHeight);
  ASSERT_TRUE(gmb);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      {kWidth, kHeight}, {kWidth, kHeight}, std::move(gmb), timestamp);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  ASSERT_HRESULT_SUCCEEDED(video_processor->Convert(frame, &sample));

  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  ASSERT_HRESULT_SUCCEEDED(sample->GetBufferByIndex(0, &media_buffer));
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  ASSERT_HRESULT_SUCCEEDED(media_buffer.As(&dxgi_buffer));
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  ASSERT_HRESULT_SUCCEEDED(
      dxgi_buffer->GetResource(IID_PPV_ARGS(&output_texture)));
  ValidateResult(output_texture.Get(), kWidth / 2, kHeight / 2,
                 [](BYTE* image) {
                   // This tolerance is large, due to variance in how hardware
                   // handles this color convert and resize operation with
                   // strong edges.  Most hardware is good with a tolerance of 1
                   // (likely using bilinear interpolation) but other hardware
                   // appears to be using more advanced scaling algorithms using
                   // more than just neighboring pixels. Due to this tolerance,
                   // this test may miss issues with nominal range. Previous
                   // tests in this group -- color space conversion tests like
                   // RGBToNV12 -- have a low tolerance and will catch if the
                   // wrong nominal range is used.
                   EXPECT_NEAR(image[0], kLumaGreen, 16);
                   EXPECT_NE(image[1], kLumaGreen);
                 });
}

TEST_F(MFVideoProcessorAcceleratorTest, RGBToNV12SizeChange) {
  CheckForVideoDevice();

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  ASSERT_HRESULT_SUCCEEDED(d3d11_device_.As(&dxgi_device));
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  ASSERT_HRESULT_SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter));
  DXGI_ADAPTER_DESC dxgi_adapter_desc;
  ASSERT_HRESULT_SUCCEEDED(dxgi_adapter->GetDesc(&dxgi_adapter_desc));
  constexpr UINT kVendorID_Qualcomm = 0x5143;
  constexpr UINT kVendorID_Qualcomm_ACPI = 0x4D4F4351;
  if (dxgi_adapter_desc.VendorId == kVendorID_Qualcomm ||
      dxgi_adapter_desc.VendorId == kVendorID_Qualcomm_ACPI) {
    // This test crashes inside the graphics driver on the
    // win11-arm64-dbg-tests run.  Until the graphics driver
    // is updated for this run, disable this test.
    GTEST_SKIP()
        << " Test crashes on win11-arm64-dbg-tests run on older drivers";
  }

  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<MediaFoundationVideoProcessorAccelerator> video_processor;
  video_processor = std::make_unique<MediaFoundationVideoProcessorAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  MediaFoundationVideoProcessorAccelerator::Config config;
  config.input_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.input_visible_size = {kWidth, kHeight};
  config.input_color_space = gfx::ColorSpace::CreateREC709();
  config.output_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  config.output_visible_size = {kWidth, kHeight};
  config.output_color_space = gfx::ColorSpace::CreateREC709();
  ASSERT_TRUE(video_processor->Initialize(config, dxgi_device_man_,
                                          std::make_unique<NullMediaLog>()));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth * 2, kHeight * 2, kGreen, kMagenta);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ASSERT_HRESULT_SUCCEEDED(CreateTexture(kWidth * 2, kHeight * 2,
                                         DXGI_FORMAT_B8G8R8A8_UNORM,
                                         image.data(), &texture));

  // Flush graphics pipeline so initial texture data is available when
  // texture is accessed through shared handle.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device_->GetImmediateContext(&d3d11_context);
  d3d11_context->Flush();

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      TextureToGpuMemoryBuffer(texture.Get(), kWidth * 2, kHeight * 2);
  ASSERT_TRUE(gmb);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      {kWidth * 2, kHeight * 2}, {kWidth * 2, kHeight * 2}, std::move(gmb),
      timestamp);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  ASSERT_HRESULT_SUCCEEDED(video_processor->Convert(frame, &sample));

  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  ASSERT_HRESULT_SUCCEEDED(sample->GetBufferByIndex(0, &media_buffer));
  Microsoft::WRL::ComPtr<IMFDXGIBuffer> dxgi_buffer;
  ASSERT_HRESULT_SUCCEEDED(media_buffer.As(&dxgi_buffer));
  Microsoft::WRL::ComPtr<ID3D11Texture2D> output_texture;
  ASSERT_HRESULT_SUCCEEDED(
      dxgi_buffer->GetResource(IID_PPV_ARGS(&output_texture)));
  ValidateResult(output_texture.Get(), kWidth, kHeight, [](BYTE* image) {
    // This test is affected by the same tolerance issues as RGBToNV12Resize.
    EXPECT_NEAR(image[0], kLumaGreen, 16);
    EXPECT_NEAR(image[1], kLumaMagenta, 16);
  });
}

TEST_F(MFVideoProcessorAcceleratorTest, RGBToNV12CPU) {
  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<MediaFoundationVideoProcessorAccelerator> video_processor;
  video_processor = std::make_unique<MediaFoundationVideoProcessorAccelerator>(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds());
  MediaFoundationVideoProcessorAccelerator::Config config;
  config.input_format = VideoPixelFormat::PIXEL_FORMAT_XRGB;
  config.input_visible_size = {kWidth, kHeight};
  config.input_color_space = gfx::ColorSpace::CreateREC709();
  config.output_format = VideoPixelFormat::PIXEL_FORMAT_NV12;
  config.output_visible_size = {kWidth, kHeight};
  config.output_color_space = gfx::ColorSpace::CreateREC709();
  ASSERT_TRUE(video_processor->Initialize(config, nullptr,
                                          std::make_unique<NullMediaLog>()));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth, kHeight, kGreen, kMagenta);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalData(
      VideoPixelFormat::PIXEL_FORMAT_XRGB, {kWidth, kHeight},
      gfx::Rect(0, 0, kWidth, kHeight), {kWidth, kHeight}, image.data(),
      image.size(), timestamp);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  ASSERT_HRESULT_SUCCEEDED(video_processor->Convert(frame, &sample));

  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer;
  ASSERT_HRESULT_SUCCEEDED(sample->GetBufferByIndex(0, &media_buffer));
  ValidateResult(media_buffer.Get(), kWidth * kHeight * 3 / 2, [](BYTE* image) {
    EXPECT_NEAR(image[0], kLumaGreen, 1);
    EXPECT_NEAR(image[2], kLumaMagenta, 1);
  });
}

class MockEncoderClient : public VideoEncodeAccelerator::Client {
 public:
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override {}
  void BitstreamBufferReady(int32_t bitstream_buffer_id,
                            const BitstreamBufferMetadata& metadata) override {}
  void NotifyErrorStatus(const EncoderStatus& status) override {
    status_ = status;
  }
  void NotifyEncoderInfoChange(const VideoEncoderInfo& info) override {}
  EncoderStatus GetLastStatus() { return status_; }

 private:
  EncoderStatus status_ = EncoderStatus::Codes::kOk;
};

#ifdef USE_WITH_ENCODER_TEST
TEST_F(MFVideoProcessorAcceleratorTest, RGBToH264) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const UINT kWidth = 128;
  const UINT kHeight = 128;

  std::unique_ptr<VideoEncodeAccelerator> video_encoder;
  video_encoder = base::WrapUnique<VideoEncodeAccelerator>(
      new MediaFoundationVideoEncodeAccelerator(
          gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(), {0}));
  VideoEncodeAccelerator::Config config(
      VideoPixelFormat::PIXEL_FORMAT_XRGB, {kWidth, kHeight}, H264PROFILE_MAIN,
      Bitrate::ConstantBitrate(4000000u), 30,
      VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer,
      VideoEncodeAccelerator::Config::ContentType::kDisplay);
  MockEncoderClient client;
  ASSERT_TRUE(video_encoder->Initialize(config, &client,
                                        std::make_unique<NullMediaLog>()));
  const int32_t kBitstreamBufferId = 1;
  const int32_t kShMemSize = 16384;
  auto shmem = base::UnsafeSharedMemoryRegion::Create(kShMemSize);
  BitstreamBuffer buffer(kBitstreamBufferId, std::move(shmem), kShMemSize,
                         0 /* offset */, base::TimeDelta());
  video_encoder->UseOutputBitstreamBuffer(std::move(buffer));

  std::vector<BYTE> image =
      CreateRGBCheckerboard(kWidth, kHeight, kGreen, kMagenta);
  Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
  ASSERT_HRESULT_SUCCEEDED(CreateTexture(kWidth * 2, kHeight * 2,
                                         DXGI_FORMAT_B8G8R8A8_UNORM,
                                         image.data(), &texture));

  // Flush graphics pipeline so initial texture data is available when
  // texture is accessed through shared handle.
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
  d3d11_device_->GetImmediateContext(&d3d11_context);
  d3d11_context->Flush();

  std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
      TextureToGpuMemoryBuffer(texture.Get(), kWidth, kHeight);
  ASSERT_TRUE(gmb);
  auto timestamp = base::Milliseconds(0);
  auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
      {kWidth, kHeight}, {kWidth, kHeight}, std::move(gmb), timestamp);
  video_encoder->Encode(frame, false);
  video_encoder = nullptr;

  ASSERT_EQ(client.GetLastStatus(), EncoderStatus::Codes::kOk);
}
#endif

}  // namespace media
