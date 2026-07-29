// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/media_foundation_video_encode_accelerator_win.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_handle.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/encoder_status.h"
#include "media/base/media_log.h"
#include "media/base/media_util.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/win/d3d11_mocks.h"
#include "media/base/win/dxgi_device_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

using ::testing::_;
using ::testing::Property;
using ::testing::Return;

namespace media {

namespace {

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

  EncoderStatus GetLastStatus() const { return status_; }

 private:
  EncoderStatus status_ = EncoderStatus::Codes::kOk;
};

class MockD3D11Texture2D
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ID3D11Texture2D> {
 public:
  MockD3D11Texture2D() = default;
  explicit MockD3D11Texture2D(Microsoft::WRL::ComPtr<ID3D11Device> device)
      : device_(std::move(device)) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void** ppvObject) override {
    if (riid == __uuidof(IDXGIKeyedMutex) && keyed_mutex_) {
      return keyed_mutex_.CopyTo(
          reinterpret_cast<IDXGIKeyedMutex**>(ppvObject));
    }
    return RuntimeClass::QueryInterface(riid, ppvObject);
  }

  void SetKeyedMutex(Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex) {
    keyed_mutex_ = std::move(keyed_mutex);
  }

  MOCK_METHOD(void,
              GetDesc,
              (D3D11_TEXTURE2D_DESC * pDesc),
              (override, Calltype(STDMETHODCALLTYPE)));

  // Dummy implementations for ID3D11Texture2D pure virtuals.
  void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) override {
    if (device_) {
      device_.CopyTo(ppDevice);
    }
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid,
                                           UINT* pDataSize,
                                           void* pData) override {
    return E_NOTIMPL;
  }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid,
                                           UINT DataSize,
                                           const void* pData) override {
    return S_OK;
  }
  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override {
    return E_NOTIMPL;
  }
  void STDMETHODCALLTYPE
  GetType(D3D11_RESOURCE_DIMENSION* pResourceDimension) override {}
  void STDMETHODCALLTYPE SetEvictionPriority(UINT EvictionPriority) override {}
  UINT STDMETHODCALLTYPE GetEvictionPriority() override { return 0; }

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  Microsoft::WRL::ComPtr<ID3D11Device> device_;
};

class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MOCK_METHOD(void,
              RequireBitstreamBuffers,
              (unsigned int, const gfx::Size&, size_t),
              (override));
  MOCK_METHOD(void,
              BitstreamBufferReady,
              (int32_t, const BitstreamBufferMetadata&),
              (override));
  MOCK_METHOD(void, NotifyErrorStatus, (const EncoderStatus&), (override));
  MOCK_METHOD(void,
              NotifyEncoderInfoChange,
              (const VideoEncoderInfo&),
              (override));
};

class TestDXGIDeviceManager : public DXGIDeviceManager {
 public:
  explicit TestDXGIDeviceManager(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d_device)
      : DXGIDeviceManager(nullptr, 0, CHROME_LUID{0, 0}),
        d3d_device_(std::move(d3d_device)) {}

  Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() override {
    return d3d_device_;
  }

 protected:
  ~TestDXGIDeviceManager() override = default;

 private:
  Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
};

class TestMediaFoundationVideoEncodeAccelerator
    : public MediaFoundationVideoEncodeAccelerator {
 public:
  using MediaFoundationVideoEncodeAccelerator::InitializeForTesting;

  TestMediaFoundationVideoEncodeAccelerator(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      CHROME_LUID luid)
      : MediaFoundationVideoEncodeAccelerator(gpu_preferences,
                                              gpu_workarounds,
                                              luid) {}
  ~TestMediaFoundationVideoEncodeAccelerator() override = default;

  void SetupForTesting(Client* client,
                       scoped_refptr<DXGIDeviceManager> dxgi_device_manager) {
    InitializeForTesting(client, std::make_unique<NullMediaLog>(),
                         gfx::Size(1280, 720), std::move(dxgi_device_manager));
  }
};

}  // namespace

class MediaFoundationVideoEncodeAcceleratorTest : public ::testing::Test {
 protected:
  void SetUpFakeEncoder(TestMediaFoundationVideoEncodeAccelerator* encoder,
                        VideoEncodeAccelerator::Client* client,
                        Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device) {
    encoder->InitializeForTesting(
        client, std::make_unique<NullMediaLog>(), gfx::Size(1920, 1080),
        DXGIDeviceManager::Create(CHROME_LUID{0, 0}, d3d11_device.Get()));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
};

class MediaFoundationVideoEncodeAcceleratorAlignmentTest
    : public MediaFoundationVideoEncodeAcceleratorTest,
      public ::testing::WithParamInterface<VideoPixelFormat> {};

TEST_P(MediaFoundationVideoEncodeAcceleratorAlignmentTest,
       RejectUnalignedVisibleRect) {
  // Step 1: Initialize a D3D11 device. Try hardware first, then fallback to
  // WARP (software) if unavailable.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device;
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr,
                        0, D3D11_SDK_VERSION, &d3d11_device, nullptr, &context);
  if (FAILED(hr)) {
    hr =
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                          D3D11_SDK_VERSION, &d3d11_device, nullptr, &context);
    if (FAILED(hr)) {
      GTEST_SKIP() << "D3D11 device creation failed";
    }
  }

  // Step 2: Set up a fake MediaFoundationVideoEncodeAccelerator with a mock
  // client.
  auto encoder = base::WrapUnique(new TestMediaFoundationVideoEncodeAccelerator(
      gpu::GpuPreferences(), gpu::GpuDriverBugWorkarounds(),
      CHROME_LUID{0, 0}));

  MockEncoderClient client;
  SetUpFakeEncoder(encoder.get(), &client, d3d11_device);

  // Step 3: Define an intentionally unaligned visible_rect (odd
  // coordinates/dimensions) for a 4:2:0 format.
  gfx::Rect visible_rect(1, 1, 1919, 1079);
  gfx::Size natural_size(1919, 1079);
  VideoPixelFormat format = GetParam();
  scoped_refptr<VideoFrame> frame;

  if (format == PIXEL_FORMAT_NV12) {
    // Step 4a: For NV12, simulate a GPU-backed frame. Create a shared D3D11
    // texture and wrap it in a SharedImage and VideoFrame.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = 1920;
    desc.Height = 1080;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                     D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = d3d11_device->CreateTexture2D(&desc, nullptr, &texture);
    ASSERT_HRESULT_SUCCEEDED(hr);

    Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
    hr = texture.As(&dxgi_resource);
    ASSERT_HRESULT_SUCCEEDED(hr);

    HANDLE shared_handle;
    hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                           nullptr, &shared_handle);
    ASSERT_HRESULT_SUCCEEDED(hr);

    gfx::GpuMemoryBufferHandle gmb_handle{
        gfx::DXGIHandle(base::win::ScopedHandle(shared_handle))};
    gmb_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;

    auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    auto shared_image = test_sii->CreateSharedImage(
        {viz::MultiPlaneFormat::kNV12, gfx::Size(1920, 1080), gfx::ColorSpace(),
         gpu::SHARED_IMAGE_USAGE_DISPLAY_READ,
         "MediaFoundationVideoEncodeAcceleratorTest"},
        gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
        std::move(gmb_handle));

    frame = VideoFrame::WrapMappableSharedImage(
        std::move(shared_image), test_sii->GenVerifiedSyncToken(),
        base::DoNothing(), visible_rect, natural_size, base::TimeDelta());
  } else {
    // Step 4b: For memory-backed frames (I420, YV12, NV21), use
    // CreateZeroInitializedFrame instead of D3D texture mocks.
    frame = VideoFrame::CreateZeroInitializedFrame(
        format, gfx::Size(1920, 1080), visible_rect, natural_size,
        base::TimeDelta());
  }

  if (!frame) {
    // Step 5: Check if the frame was successfully created.
    // VideoFrame::CreateZeroInitializedFrame natively validates alignment and
    // might return null, in which case we safely skip.
    GTEST_SKIP() << "Cannot create unaligned frame natively for format "
                 << format;
  }

  encoder->Encode(frame, false);
  task_environment_.RunUntilIdle();

  // Step 6: Attempt to encode the frame. Because the visible_rect is unaligned,
  // the encoder should immediately reject the frame and safely report
  // kInvalidInputFrame via the client.
  EXPECT_EQ(client.GetLastStatus().code(),
            EncoderStatus::Codes::kInvalidInputFrame);
}

class MediaFoundationVideoEncodeAcceleratorKeyedMutexTimeoutTest
    : public ::testing::Test {
 public:
  MediaFoundationVideoEncodeAcceleratorKeyedMutexTimeoutTest() = default;

  void SetUp() override {
    mock_d3d11_device_ = Microsoft::WRL::Make<D3D11DeviceMock>();
    mock_d3d11_device_context_ = Microsoft::WRL::Make<D3D11DeviceContextMock>();
    mock_d3d11_video_device_ = Microsoft::WRL::Make<D3D11VideoDeviceMock>();
    mock_d3d11_video_context_ = Microsoft::WRL::Make<D3D11VideoContextMock>();

    ON_CALL(*mock_d3d11_device_.Get(), QueryInterface)
        .WillByDefault([this](REFIID riid, void** ppv) {
          if (riid == __uuidof(ID3D11Device1)) {
            mock_d3d11_device_->AddRef();
            *ppv = static_cast<ID3D11Device*>(mock_d3d11_device_.Get());
            return S_OK;
          } else if (riid == __uuidof(ID3D11VideoDevice1)) {
            mock_d3d11_video_device_->AddRef();
            *ppv = static_cast<ID3D11VideoDevice1*>(
                mock_d3d11_video_device_.Get());
            return S_OK;
          }
          return mock_d3d11_device_->RuntimeClass::QueryInterface(riid, ppv);
        });
    ON_CALL(*mock_d3d11_device_.Get(), CreateTexture2D)
        .WillByDefault([](const D3D11_TEXTURE2D_DESC*,
                          const D3D11_SUBRESOURCE_DATA*,
                          ID3D11Texture2D** ppTexture2D) {
          if (ppTexture2D) {
            auto texture = Microsoft::WRL::Make<D3D11Texture2DMock>();
            ON_CALL(*texture.Get(), QueryInterface)
                .WillByDefault([texture_ptr = texture.Get()](REFIID riid,
                                                             void** ppv) {
                  return texture_ptr->RuntimeClass::QueryInterface(riid, ppv);
                });
            *ppTexture2D = texture.Detach();
          }
          return S_OK;
        });
    ON_CALL(*mock_d3d11_device_.Get(), GetImmediateContext)
        .WillByDefault([this](ID3D11DeviceContext** ppImmediateContext) {
          if (ppImmediateContext) {
            mock_d3d11_device_context_.CopyTo(ppImmediateContext);
          }
        });
    ON_CALL(*mock_d3d11_device_context_.Get(), QueryInterface)
        .WillByDefault([this](REFIID riid, void** ppv) {
          if (riid == __uuidof(ID3D11VideoContext1)) {
            mock_d3d11_video_context_->AddRef();
            *ppv = static_cast<ID3D11VideoContext1*>(
                mock_d3d11_video_context_.Get());
            return S_OK;
          }
          return mock_d3d11_device_context_->RuntimeClass::QueryInterface(riid,
                                                                          ppv);
        });
    gpu::GpuPreferences gpu_preferences;
    gpu::GpuDriverBugWorkarounds gpu_workarounds;
    encoder_ = std::make_unique<TestMediaFoundationVideoEncodeAccelerator>(
        gpu_preferences, gpu_workarounds, CHROME_LUID{0, 0});

    encoder_->SetupForTesting(
        &client_,
        base::MakeRefCounted<TestDXGIDeviceManager>(mock_d3d11_device_));
  }

  void TearDown() override {
    if (encoder_) {
      encoder_.release()->Destroy();
    }
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  MockVideoEncodeAcceleratorClient client_;
  Microsoft::WRL::ComPtr<D3D11DeviceMock> mock_d3d11_device_;
  Microsoft::WRL::ComPtr<D3D11DeviceContextMock> mock_d3d11_device_context_;
  Microsoft::WRL::ComPtr<D3D11VideoDeviceMock> mock_d3d11_video_device_;
  Microsoft::WRL::ComPtr<D3D11VideoContextMock> mock_d3d11_video_context_;
  std::unique_ptr<TestMediaFoundationVideoEncodeAccelerator> encoder_;
};

// Tests that if the keyed mutex cannot be acquired (e.g. returns WAIT_TIMEOUT),
// the encoder drops the frame and signals an error instead of silently
// proceeding and encoding uninitialized GPU VRAM.
TEST_F(MediaFoundationVideoEncodeAcceleratorKeyedMutexTimeoutTest,
       RejectFrameOnKeyedMutexTimeout) {
  // 1. Set up a mock keyed mutex that simulates a WAIT_TIMEOUT scenario.
  auto mock_keyed_mutex = Microsoft::WRL::Make<DXGIKeyedMutexMock>();
  EXPECT_CALL(*mock_keyed_mutex.Get(), AcquireSync(0, _))
      .WillOnce(Return(WAIT_TIMEOUT));

  // 2. Set up a mock texture that returns the mock keyed mutex when queried.
  auto mock_texture =
      Microsoft::WRL::Make<MockD3D11Texture2D>(mock_d3d11_device_);
  mock_texture->SetKeyedMutex(mock_keyed_mutex);

  // Give the texture a valid description so the encoder doesn't fail early.
  D3D11_TEXTURE2D_DESC desc = {};
  gfx::Size input_visible_size(1280, 720);
  desc.Width = input_visible_size.width();
  desc.Height = input_visible_size.height();
  desc.Format = DXGI_FORMAT_NV12;
  EXPECT_CALL(*mock_texture.Get(), GetDesc(_))
      .WillRepeatedly([desc](D3D11_TEXTURE2D_DESC* pDesc) { *pDesc = desc; });

  // 3. Setup the mock D3D11 device to return our mock texture when opening the
  // shared resource.
  EXPECT_CALL(*mock_d3d11_device_.Get(), OpenSharedResource1(_, _, _))
      .WillOnce([&mock_texture](HANDLE, REFIID riid, void** ppv) {
        return mock_texture.CopyTo(riid, ppv);
      });

  // 4. Create a dummy Native Texture backed VideoFrame.
  auto test_sii = base::MakeRefCounted<gpu::TestSharedImageInterface>();
  gfx::GpuMemoryBufferHandle gmb_handle{gfx::DXGIHandle::CreateFakeForTest()};
  const auto si_usage = gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY |
                        gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;

  auto shared_image = test_sii->CreateSharedImage(
      {viz::MultiPlaneFormat::kNV12, input_visible_size, gfx::ColorSpace(),
       gpu::SharedImageUsageSet(si_usage),
       "MediaFoundationVideoEncodeAcceleratorTest"},
      gpu::kNullSurfaceHandle, gfx::BufferUsage::GPU_READ,
      std::move(gmb_handle));

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapMappableSharedImage(
      std::move(shared_image), test_sii->GenVerifiedSyncToken(),
      base::NullCallback(), gfx::Rect(input_visible_size), input_visible_size,
      base::TimeDelta());

  VideoEncoder::EncodeOptions options(false);

  // 5. We expect that encoding the frame drops the frame because the texture
  // couldn't be safely acquired, calling BitstreamBufferReady with
  // dropped_frame == true.
  EXPECT_CALL(client_, NotifyErrorStatus(_)).Times(0);
  EXPECT_CALL(client_,
              BitstreamBufferReady(
                  1, Property(&BitstreamBufferMetadata::dropped_frame, true)));

  EXPECT_CALL(*mock_d3d11_device_context_.Get(), CopySubresourceRegion)
      .Times(0);
  EXPECT_CALL(*mock_d3d11_video_device_.Get(), CreateVideoProcessorInputView)
      .Times(0);
  EXPECT_CALL(*mock_d3d11_video_device_.Get(), CreateVideoProcessorOutputView)
      .Times(0);
  EXPECT_CALL(*mock_d3d11_video_context_.Get(), VideoProcessorBlt).Times(0);

  // Provide a bitstream buffer for the encoder to return the dropped frame.
  auto shmem = base::UnsafeSharedMemoryRegion::Create(100);
  encoder_->UseOutputBitstreamBuffer(BitstreamBuffer(1, std::move(shmem), 100));

  // 6. Act: Attempt to encode.
  encoder_->Encode(frame, options);
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         MediaFoundationVideoEncodeAcceleratorAlignmentTest,
                         ::testing::Values(PIXEL_FORMAT_NV12,
                                           PIXEL_FORMAT_I420,
                                           PIXEL_FORMAT_YV12,
                                           PIXEL_FORMAT_NV21));

}  // namespace media
