// Copyright 2021 The Chromium Authors. All rights reserved.
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
#include "base/win/windows_version.h"
#include "media/capture/video/win/d3d_capture_test_utils.h"
#include "media/capture/video/win/gpu_memory_buffer_tracker.h"
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
      : DXGIDeviceManager(nullptr, 0),
        mock_d3d_device_(new MockD3D11Device()) {}

  // Associates a new D3D device with the DXGI Device Manager
  HRESULT ResetDevice() override { return S_OK; }

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

class GpuMemoryBufferTrackerTest : public ::testing::Test {
 protected:
  GpuMemoryBufferTrackerTest()
      : media_foundation_supported_(
            VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {}

  bool ShouldSkipTest() {
    if (!media_foundation_supported_) {
      DVLOG(1) << "Media foundation is not supported by the current platform. "
                  "Skipping test.";
      return true;
    }
    // D3D11 is only supported with Media Foundation on Windows 8 or later
    if (base::win::GetVersion() < base::win::Version::WIN8) {
      DVLOG(1) << "D3D11 with Media foundation is not supported by the current "
                  "platform. "
                  "Skipping test.";
      return true;
    }
    return false;
  }

  void SetUp() override {
    if (ShouldSkipTest()) {
      GTEST_SKIP();
    }

    dxgi_device_manager_ =
        scoped_refptr<MockDXGIDeviceManager>(new MockDXGIDeviceManager());
  }

  base::test::TaskEnvironment task_environment_;
  const bool media_foundation_supported_;
  scoped_refptr<MockDXGIDeviceManager> dxgi_device_manager_;
};

TEST_F(GpuMemoryBufferTrackerTest, TextureCreation) {
  // Verify that GpuMemoryBufferTracker creates a D3D11 texture with the correct
  // properties
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
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
}

TEST_F(GpuMemoryBufferTrackerTest, TextureRecreationOnDeviceLoss) {
  // Verify that GpuMemoryBufferTracker recreates a D3D11 texture with the
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
      .Times(2);
  // Mock device loss
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              OnGetDeviceRemovedReason())
      .WillOnce(Invoke([]() { return DXGI_ERROR_DEVICE_REMOVED; }));
  // Create and init tracker (causes initial texture creation)
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);
  // Get GpuMemoryBufferHandle (should trigger device/texture recreation)
  gfx::GpuMemoryBufferHandle gmb = tracker->GetGpuMemoryBufferHandle();
}

TEST_F(GpuMemoryBufferTrackerTest, GetMemorySizeInBytes) {
  // Verify that GpuMemoryBufferTracker returns an expected value from
  // GetMemorySizeInBytes
  const gfx::Size expected_buffer_size = {1920, 1080};
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  std::unique_ptr<VideoCaptureBufferTracker> tracker =
      std::make_unique<GpuMemoryBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr),
            true);

  const uint32_t expectedSizeInBytes =
      (expected_buffer_size.width() * expected_buffer_size.height() * 3) / 2;
  EXPECT_EQ(tracker->GetMemorySizeInBytes(), expectedSizeInBytes);
}

}  // namespace media