// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/win/d3d11_shared_image_buffer_tracker.h"

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "media/capture/video/video_capture_gpu_channel_host.h"
#include "media/capture/video/win/d3d_capture_test_utils.h"
#include "media/capture/video/win/video_capture_device_factory_win.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Pointee;

namespace media {

namespace {

// A SharedImageInterface whose mappable-buffer overload drops the buffer
// handle, yielding a shared image with no client-side mappable buffer. This
// models a GPU process that cannot wrap the slot texture: the premapped region
// would never reach consumers, so Init() is expected to reject the slot.
class NonMappableSharedImageInterface : public gpu::TestSharedImageInterface {
 public:
  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override {
    return gpu::TestSharedImageInterface::CreateSharedImage(
        si_info, surface_handle, /*pool_id=*/std::nullopt);
  }

 private:
  ~NonMappableSharedImageInterface() override = default;
};

}  // namespace

class D3D11SharedImageBufferTrackerTest : public ::testing::Test {
 protected:
  D3D11SharedImageBufferTrackerTest() = default;

  void SetUp() override {
    if (!VideoCaptureDeviceFactoryWin::PlatformSupportsMediaFoundation()) {
      GTEST_SKIP()
          << "Media foundation is not supported by the current platform.";
    }

    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
        test_sii_);
    dxgi_device_manager_ = base::MakeRefCounted<FakeDXGIDeviceManager>();
    dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();
  }

  void TearDown() override {
    VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(nullptr);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
  scoped_refptr<FakeDXGIDeviceManager> dxgi_device_manager_;
};

TEST_F(D3D11SharedImageBufferTrackerTest, InitCreatesReusableSlot) {
  // Verify that the tracker creates an NV12 D3D11 texture, imports it into a
  // shared image, and is reusable for matching format and dimensions.
  const gfx::Size expected_buffer_size = {1920, 1080};
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(AllOf(Field(&D3D11_TEXTURE2D_DESC::Format, DXGI_FORMAT_NV12),
                        Field(&D3D11_TEXTURE2D_DESC::Width,
                              static_cast<const unsigned int>(
                                  expected_buffer_size.width())),
                        Field(&D3D11_TEXTURE2D_DESC::Height,
                              static_cast<const unsigned int>(
                                  expected_buffer_size.height())))),
          _, _))
      .Times(1);

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  EXPECT_EQ(tracker->GetBufferType(), VideoCaptureBufferType::kSharedImage);
  EXPECT_TRUE(tracker->IsReusableForFormat(
      expected_buffer_size, PIXEL_FORMAT_NV12, nullptr, gfx::ColorSpace()));
  EXPECT_FALSE(tracker->IsReusableForFormat(
      gfx::Size(640, 480), PIXEL_FORMAT_NV12, nullptr, gfx::ColorSpace()));
  EXPECT_FALSE(tracker->IsReusableForFormat(
      expected_buffer_size, PIXEL_FORMAT_I420, nullptr, gfx::ColorSpace()));
}

TEST_F(D3D11SharedImageBufferTrackerTest,
       IsReusableForFormatMatchesColorSpace) {
  // The slot's shared image is created with a fixed color space (which
  // VideoFrame::set_color_space() enforces downstream), so slots must only be
  // reused for frames carrying the same color space.
  const gfx::Size buffer_size = {1920, 1080};
  const gfx::ColorSpace kRec709 = gfx::ColorSpace::CreateREC709();
  const gfx::ColorSpace kRec601 = gfx::ColorSpace::CreateREC601();

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr, kRec709));

  EXPECT_TRUE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                           nullptr, kRec709));
  EXPECT_FALSE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                            nullptr, kRec601));
}

TEST_F(D3D11SharedImageBufferTrackerTest, GetGpuMemoryBufferHandle) {
  // The producer copies captured frames through the DXGI handle returned here.
  const gfx::Size expected_buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  gfx::GpuMemoryBufferHandle handle = tracker->GetGpuMemoryBufferHandle();
  EXPECT_EQ(handle.type, gfx::DXGI_SHARED_HANDLE);
  EXPECT_TRUE(handle.dxgi_handle().IsValid());
}

TEST_F(D3D11SharedImageBufferTrackerTest, GetVideoBufferHandle) {
  // Consumers receive the slot as an exported shared image handle.
  const gfx::Size expected_buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  media::mojom::VideoBufferHandlePtr handle = tracker->GetVideoBufferHandle();
  ASSERT_TRUE(handle);
  ASSERT_TRUE(handle->is_shared_image_handle());
  // The premapped region is delivered inside the exported image's buffer
  // handle; a consumer importing the image (as Blink does) sees it attached
  // to the DXGI handle, so software reads need no GPU roundtrip.
  auto imported_shared_image = gpu::ClientSharedImage::ImportUnowned(
      std::move(handle->get_shared_image_handle()->shared_image));
  ASSERT_TRUE(imported_shared_image);
  EXPECT_TRUE(imported_shared_image->CloneGpuMemoryBufferHandle()
                  .dxgi_handle()
                  .region()
                  .IsValid());
}

TEST_F(D3D11SharedImageBufferTrackerTest, GetMemorySizeInBytes) {
  const gfx::Size expected_buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  // A slot costs both the NV12 texture and the equally-sized premapped region
  // it carries for software consumers; the pool ranks eviction candidates by
  // this value, so both must be reported.
  const uint32_t nv12_size_in_bytes =
      (expected_buffer_size.width() * expected_buffer_size.height() * 3) / 2;
  EXPECT_GE(tracker->GetMemorySizeInBytes(), 2 * nv12_size_in_bytes);
}

TEST_F(D3D11SharedImageBufferTrackerTest, InitFailsWhenSharedImageNotMappable) {
  // A slot whose shared image has no mappable buffer cannot deliver the
  // premapped region, and the renderer CHECKs when it later asks to use it, so
  // the slot must be rejected at creation instead.
  VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(
      base::MakeRefCounted<NonMappableSharedImageInterface>());
  const gfx::Size buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_FALSE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr,
                             gfx::ColorSpace()));

  // The rejected slot must not report itself as reusable.
  EXPECT_FALSE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                            nullptr, gfx::ColorSpace()));
}

TEST_F(D3D11SharedImageBufferTrackerTest, InitFailsForUnsupportedFormat) {
  const gfx::Size buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_FALSE(tracker->Init(buffer_size, PIXEL_FORMAT_I420, nullptr,
                             gfx::ColorSpace()));
}

TEST_F(D3D11SharedImageBufferTrackerTest,
       InitFailsWhenSharedImageInterfaceUnavailable) {
  VideoCaptureGpuChannelHost::GetInstance().SetSharedImageInterface(nullptr);
  const gfx::Size buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_FALSE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr,
                             gfx::ColorSpace()));
}

TEST_F(D3D11SharedImageBufferTrackerTest, OnContextLostInvalidatesSlot) {
  const gfx::Size buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  tracker->OnContextLost();
  EXPECT_FALSE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                            nullptr, gfx::ColorSpace()));
}

TEST_F(D3D11SharedImageBufferTrackerTest, DuplicateAsUnsafeRegion) {
  // Verify that DuplicateAsUnsafeRegion copies the slot's D3D11 texture to
  // shared memory, so software consumers can read premapped frames.
  const gfx::Size expected_buffer_size = {1920, 1080};

  // Create the mock texture used to satisfy the shared resource open.
  D3D11_TEXTURE2D_DESC mock_desc = {};
  mock_desc.Format = DXGI_FORMAT_NV12;
  mock_desc.Width = expected_buffer_size.width();
  mock_desc.Height = expected_buffer_size.height();

  Microsoft::WRL::ComPtr<ID3D11Texture2D> mock_dxgi_texture;
  Microsoft::WRL::ComPtr<MockD3D11Texture2D> dxgi_texture =
      MakeComPtrFromRefCounted<MockD3D11Texture2D>(
          mock_desc, dxgi_device_manager_->GetMockDevice().Get());
  EXPECT_TRUE(SUCCEEDED(dxgi_texture.CopyTo(IID_PPV_ARGS(&mock_dxgi_texture))));

  // One call for creating the slot texture by the tracker.
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(
              AllOf(Field(&D3D11_TEXTURE2D_DESC::Format, DXGI_FORMAT_NV12),
                    Field(&D3D11_TEXTURE2D_DESC::Usage, D3D11_USAGE_DEFAULT))),
          _, _))
      .Times(1);

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_TRUE(tracker->Init(expected_buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  // The DXGI texture should be opened as a shared resource for the copy.
  EXPECT_CALL(*(dxgi_device_manager_->GetMockDevice().Get()),
              DoOpenSharedResource1)
      .WillOnce([&dxgi_texture](HANDLE resource, REFIID returned_interface,
                                void** resource_out) {
        return dxgi_texture.CopyTo(returned_interface, resource_out);
      });

  // Expect creation of a staging texture.
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(
              AllOf(Field(&D3D11_TEXTURE2D_DESC::Format, DXGI_FORMAT_NV12),
                    Field(&D3D11_TEXTURE2D_DESC::Width,
                          static_cast<const unsigned int>(
                              expected_buffer_size.width())),
                    Field(&D3D11_TEXTURE2D_DESC::Height,
                          static_cast<const unsigned int>(
                              expected_buffer_size.height())),
                    Field(&D3D11_TEXTURE2D_DESC::Usage, D3D11_USAGE_STAGING))),
          _, _))
      .Times(1);

  // The slot texture opened as a shared resource should be copied to the
  // staging texture.
  ID3D11Resource* expected_source =
      static_cast<ID3D11Resource*>(mock_dxgi_texture.Get());
  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnCopySubresourceRegion(_, _, _, _, _, expected_source, _, _))
      .Times(1);

  // Stand in for the staging texture's contents, filled with a marker so the
  // copy into the premapped region can be verified below.
  constexpr uint8_t kFrameDataMarker = 0x5a;
  std::vector<uint8_t> staging_contents(
      expected_buffer_size.width() * expected_buffer_size.height() * 3 / 2,
      kFrameDataMarker);
  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnMap(_, _, _, _, _))
      .WillOnce([&staging_contents, &expected_buffer_size](
                    ID3D11Resource* resource, UINT subresource,
                    D3D11_MAP MapType, UINT MapFlags,
                    D3D11_MAPPED_SUBRESOURCE* mapped_resource) {
        mapped_resource->pData = staging_contents.data();
        mapped_resource->RowPitch = expected_buffer_size.width();
        mapped_resource->DepthPitch = staging_contents.size();
        return S_OK;
      });
  EXPECT_CALL(
      *dxgi_device_manager_->GetMockDevice()->mock_immediate_context_.Get(),
      OnUnmap(_, _))
      .Times(1);

  auto memory_region = tracker->DuplicateAsUnsafeRegion();
  ASSERT_TRUE(memory_region.IsValid());

  // The region a software consumer maps must actually hold the frame, not just
  // be the right size.
  base::WritableSharedMemoryMapping mapping = memory_region.Map();
  ASSERT_TRUE(mapping.IsValid());
  const base::span<const uint8_t> premapped_frame =
      mapping.GetMemoryAsSpan<const uint8_t>().first(staging_contents.size());
  EXPECT_THAT(premapped_frame, ::testing::Each(kFrameDataMarker));
}

TEST_F(D3D11SharedImageBufferTrackerTest, ReusesSlotAcrossFrames) {
  // The defining behaviour of this tracker: one texture and one shared image
  // serve every frame the slot carries, unlike SharedImageBufferTracker which
  // allocates per frame.
  const gfx::Size buffer_size = {1920, 1080};

  // Exactly one slot texture, however many frames pass through it.
  EXPECT_CALL(
      *(dxgi_device_manager_->GetMockDevice().Get()),
      OnCreateTexture2D(
          Pointee(
              AllOf(Field(&D3D11_TEXTURE2D_DESC::Format, DXGI_FORMAT_NV12),
                    Field(&D3D11_TEXTURE2D_DESC::Usage, D3D11_USAGE_DEFAULT))),
          _, _))
      .Times(1);

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  ASSERT_TRUE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));

  // Frame 1: the producer takes the slot's handle, then the pool hands the
  // slot to consumers and takes it back.
  const gfx::GpuMemoryBufferHandle first_handle =
      tracker->GetGpuMemoryBufferHandle();
  ASSERT_TRUE(first_handle.dxgi_handle().IsValid());
  tracker->SetHeldByProducer(true);
  tracker->AddConsumerHolds(1);
  tracker->RemoveConsumerHolds(1);
  tracker->SetHeldByProducer(false);

  // The slot is still a match, so the pool reserves it again rather than
  // allocating a replacement.
  ASSERT_TRUE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                           nullptr, gfx::ColorSpace()));

  // Frame 2 lands in the same texture: the handle is duplicated per call, but
  // the token identifies the same underlying resource.
  const gfx::GpuMemoryBufferHandle second_handle =
      tracker->GetGpuMemoryBufferHandle();
  ASSERT_TRUE(second_handle.dxgi_handle().IsValid());
  EXPECT_EQ(first_handle.dxgi_handle().token(),
            second_handle.dxgi_handle().token());
  EXPECT_EQ(first_handle.stride, second_handle.stride);
}

TEST_F(D3D11SharedImageBufferTrackerTest, DeviceLossInvalidatesSlot) {
  // A slot's texture belongs to the device it was created on, so once that
  // device is replaced the pool must stop reusing the slot, and the producer
  // must get an invalid handle rather than one for a dead texture.
  const gfx::Size buffer_size = {1920, 1080};

  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  ASSERT_TRUE(tracker->Init(buffer_size, PIXEL_FORMAT_NV12, nullptr,
                            gfx::ColorSpace()));
  ASSERT_TRUE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                           nullptr, gfx::ColorSpace()));

  // Swap in a new device, as device-removal recovery does.
  Microsoft::WRL::ComPtr<ID3D11Device> replacement_device;
  ASSERT_EQ(dxgi_device_manager_->ResetDevice(replacement_device), S_OK);
  dxgi_device_manager_->GetMockDevice()->SetupDefaultMocks();

  EXPECT_FALSE(tracker->IsReusableForFormat(buffer_size, PIXEL_FORMAT_NV12,
                                            nullptr, gfx::ColorSpace()));
  // Note this must be checked with is_null(): dxgi_handle() CHECKs that the
  // handle is of DXGI type, which an empty handle is not.
  EXPECT_TRUE(tracker->GetGpuMemoryBufferHandle().is_null());
}

TEST_F(D3D11SharedImageBufferTrackerTest, GetMemorySizeInBytesBeforeInit) {
  // The pool queries this while ranking eviction candidates, including for
  // trackers whose Init() never ran.
  auto tracker =
      std::make_unique<D3D11SharedImageBufferTracker>(dxgi_device_manager_);
  EXPECT_EQ(tracker->GetMemorySizeInBytes(), 0u);
}

}  // namespace media
