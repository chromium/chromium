// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfidl.h>

#include <dxgi1_2.h>
#include <mfapi.h>
#include <mferror.h>
#include <wrl.h>
#include <wrl/client.h>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "media/capture/video/win/d3d_capture_test_utils.h"
#include "media/capture/video/win/gpu_memory_buffer_tracker_win.h"
#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Pointee;

namespace media {

namespace {

class MockDXGIDeviceManager : public DXGIDeviceManager {
 public:
  MockDXGIDeviceManager()
      : DXGIDeviceManager(nullptr, 0, CHROME_LUID{.LowPart = 0, .HighPart = 0}),
        mock_d3d_device_(new MockD3D11Device()) {}

  // Associates a new D3D device with the DXGI Device Manager
  // returns it in the parameter, which can't be nullptr.
  HRESULT ResetDevice(
      Microsoft::WRL::ComPtr<ID3D11Device>& d3d_device) override {
    mock_d3d_device_ = new MockD3D11Device();
    d3d_device = mock_d3d_device_;
    return S_OK;
  }

  // Directly access D3D device stored in DXGI device manager
  Microsoft::WRL::ComPtr<ID3D11Device> GetDevice() override {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    mock_d3d_device_.As(&device);
    return device;
  }

  Microsoft::WRL::ComPtr<MockD3D11Device> GetMockDevice() {
    return mock_d3d_device_;
  }

 protected:
  ~MockDXGIDeviceManager() override {}
  Microsoft::WRL::ComPtr<MockD3D11Device> mock_d3d_device_;
};

}  // namespace

class GpuMemoryBufferTrackerWinTest : public ::testing::Test {
 protected:
  GpuMemoryBufferTrackerWinTest() = default;

  void SetUp() override {
    if (!VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {
      GTEST_SKIP()
          << "Media foundation is not supported by the current platform.";
    }

    dxgi_device_manager_ =
        scoped_refptr<MockDXGIDeviceManager>(new MockDXGIDeviceManager());
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<MockDXGIDeviceManager> dxgi_device_manager_;
};

TEST_F(GpuMemoryBufferTrackerWinTest, TextureCreation) {
  // Verify that GpuMemoryBufferTrackerWin creates a D3D11 texture with the
  // correct properties
  const gfx::Size expected_buffer_size = {1920, 1080};
  const DXGI_FORMAT expected_buffer_format = DXGI_FORMAT_NV12;
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnCreateTexture2D(
                  Pointee(AllOf(Field(&D3D11_TEXTURE2D_DESC::Format,
                                      expected_buffer_format),
                                Field(&D3D11_TEXTURE2D_DESC::Width,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.width())),
                                Field(&D3D11_TEXTURE2D_DESC::Height,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.height())))),
                  _, _));
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTrackerWin>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
}

TEST_F(GpuMemoryBufferTrackerWinTest, InvalidateOnDeviceLoss) {
  // Verify that GpuMemoryBufferTrackerWin recreates a D3D11 texture with the
  // correct properties when there is a device loss
  const gfx::Size expected_buffer_size = {1920, 1080};
  const DXGI_FORMAT expected_buffer_format = DXGI_FORMAT_NV12;
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  // Expect two texture creation calls (the second occurs on device loss
  // recovery)
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnCreateTexture2D(
                  Pointee(AllOf(Field(&D3D11_TEXTURE2D_DESC::Format,
                                      expected_buffer_format),
                                Field(&D3D11_TEXTURE2D_DESC::Width,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.width())),
                                Field(&D3D11_TEXTURE2D_DESC::Height,
                                      static_cast<const unsigned int>(
                                          expected_buffer_size.height())))),
                  _, _))
      .Times(1);
  // Mock device loss.
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnGetDeviceRemovedReason())
      .WillOnce(Invoke([]() { return DXGI_ERROR_DEVICE_REMOVED; }));
  // Create and init tracker (causes initial texture creation)
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTrackerWin>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
  // The tracker now should be invalid.
  EXPECT_FALSE(tracker->IsReusableForFormat(expected_buffer_size,
                                            PIXEL_FORMAT_NV12, nullptr));
  gfx::GpuMemoryBufferHandle gmb = tracker->GetGpuMemoryBufferHandle();
  EXPECT_FALSE(gmb.dxgi_handle.IsValid());
}

TEST_F(GpuMemoryBufferTrackerWinTest, GetMemorySizeInBytes) {
  // Verify that GpuMemoryBufferTrackerWin returns an expected value from
  // GetMemorySizeInBytes
  const gfx::Size expected_buffer_size = {1920, 1080};
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTrackerWin>(dxgi_device_manager_);

  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);

  const uint32_t expectedSizeInBytes =
      (expected_buffer_size.width() * expected_buffer_size.height() * 3) / 2;
  EXPECT_EQ(tracker->GetMemorySizeInBytes(), expectedSizeInBytes);
}

TEST_F(GpuMemoryBufferTrackerWinTest, DuplicateAsUnsafeRegion) {
  // Verify that GpuMemoryBufferTrackerWin copies a D3D11 texture
  // to shared memory.
  const gfx::Size expected_buffer_size = {1920, 1080};
  const DXGI_FORMAT expected_buffer_format = DXGI_FORMAT_NV12;
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();

  // Create mock texture to be used by the tracker.
  D3D11_TEXTURE2D_DESC mock_desc = {};
  mock_desc.Format = expected_buffer_format;
  mock_desc.Width = expected_buffer_size.width();
  mock_desc.Height = expected_buffer_size.height();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> mock_dxgi_texture;
  Microsoft::WRL::ComPtr<MockD3D11Texture2D> dxgi_texture(
      new MockD3D11Texture2D(mock_desc,
                             dxgi_device_manager_->GetMockDevice().Get()));
  EXPECT_TRUE(SUCCEEDED(dxgi_texture.CopyTo(IID_PPV_ARGS(&mock_dxgi_texture))));

  // One call for creating DXGI texture by the tracker.
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(AllOf(
              Field(&D3D11_TEXTURE2D_DESC::Format, expected_buffer_format),
              Field(&D3D11_TEXTURE2D_DESC::Width,
                    static_cast<const unsigned int>(
                        expected_buffer_size.width())),
              Field(&D3D11_TEXTURE2D_DESC::Height,
                    static_cast<const unsigned int>(
                        expected_buffer_size.height())),
              Field(&D3D11_TEXTURE2D_DESC::Usage, D3D11_USAGE_DEFAULT))),
          _, _))
      .Times(1);

  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTrackerWin>(dxgi_device_manager_);

  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr));

  // DXGI texture should be opened as a shared resource.
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              DoOpenSharedResource1)
      .WillOnce(
          Invoke([&dxgi_texture](HANDLE resource, REFIID returned_interface,
                                 void** resource_out) {
            return dxgi_texture.CopyTo(returned_interface, resource_out);
          }));

  // Expect creation of a staging texture.
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(AllOf(
              Field(&D3D11_TEXTURE2D_DESC::Format, expected_buffer_format),
              Field(&D3D11_TEXTURE2D_DESC::Width,
                    static_cast<const unsigned int>(
                        expected_buffer_size.width())),
              Field(&D3D11_TEXTURE2D_DESC::Height,
                    static_cast<const unsigned int>(
                        expected_buffer_size.height())),
              Field(&D3D11_TEXTURE2D_DESC::Usage, D3D11_USAGE_STAGING))),
          _, _))
      .Times(1);

  // dxgi_texture opened as a shared resource should be copied to the staging
  // texture.
  ID3D11Resource* expected_source =
      static_cast<ID3D11Resource*>(mock_dxgi_texture.Get());

  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnCopySubresourceRegion(_, _, _, _, _, expected_source, _, _))
      .Times(1);

  uint8_t* tmp_mapped_resource;

  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnMap(_, _, _, _, _))
      .WillOnce(Invoke(
          [&tmp_mapped_resource](ID3D11Resource* resource, UINT subresource,
                                 D3D11_MAP MapType, UINT MapFlags,
                                 D3D11_MAPPED_SUBRESOURCE* mapped_resource) {
            const size_t buffer_size = 1920 * 1080 * 3 / 2;
            tmp_mapped_resource = new uint8_t[buffer_size];
            mapped_resource->pData = tmp_mapped_resource;
            mapped_resource->RowPitch = 1920;
            mapped_resource->DepthPitch = buffer_size;
            return S_OK;
          }));
  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnUnmap(_, _))
      .WillOnce(Invoke(
          [&tmp_mapped_resource](ID3D11Resource* resource, UINT subresource) {
            delete[] tmp_mapped_resource;
          }));

  auto memory_region = tracker->DuplicateAsUnsafeRegion();
  EXPECT_TRUE(memory_region.IsValid());
}

}  // namespace media
