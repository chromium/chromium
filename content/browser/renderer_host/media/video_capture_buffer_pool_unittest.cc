// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureBufferPool.

#include "media/capture/video/video_capture_buffer_pool.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/io_surface.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <dxgi1_2.h>
#include <mfapi.h>
#include "media/base/win/dxgi_device_manager.h"
#endif

namespace content {

namespace {

size_t ImageAllocationSize(const media::VideoCaptureFormat& format) {
  return media::VideoFrame::AllocationSize(format.pixel_format,
                                           format.frame_size);
}

}  // namespace

static constexpr media::VideoPixelFormat kCapturePixelFormats[] = {
    media::PIXEL_FORMAT_I420,
    media::PIXEL_FORMAT_ARGB,
    media::PIXEL_FORMAT_Y16,
};

static constexpr media::VideoCaptureBufferType kVideoCaptureBufferTypes[] = {
    media::VideoCaptureBufferType::kSharedMemory,
    media::VideoCaptureBufferType::kMailboxHolder,
    media::VideoCaptureBufferType::kGpuMemoryBuffer};

static constexpr int kTestBufferPoolSize = 3;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
static constexpr gfx::Size kDefaultTextureSize = gfx::Size(1080, 720);
static constexpr int kInvalidId = -1;
static constexpr media::VideoPixelFormat kDefaultPixelFormat =
    media::VideoPixelFormat::PIXEL_FORMAT_NV12;
static const media::VideoCaptureFormat kDefaultFormat =
    media::VideoCaptureFormat(kDefaultTextureSize, 30, kDefaultPixelFormat);
static const gfx::ColorSpace kDefaultColorSpace = gfx::ColorSpace();

#endif

// Note that this test does not exercise the class VideoCaptureBufferPool
// in isolation. The "unit under test" is an instance of VideoCaptureBufferPool
// with some context that is specific to renderer_host/media, and therefore
// this test must live here and not in media/capture/video.
class VideoCaptureBufferPoolTest
    : public testing::TestWithParam<
          std::tuple<media::VideoPixelFormat, media::VideoCaptureBufferType>> {
 public:
  VideoCaptureBufferPoolTest(const VideoCaptureBufferPoolTest&) = delete;
  VideoCaptureBufferPoolTest& operator=(const VideoCaptureBufferPoolTest&) =
      delete;

 protected:
  // This is a generic Buffer tracker
  class Buffer {
   public:
    Buffer(const scoped_refptr<media::VideoCaptureBufferPool> pool,
           std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle,
           int id)
        : id_(id), pool_(pool), buffer_handle_(std::move(buffer_handle)) {}
    ~Buffer() { pool_->RelinquishProducerReservation(id()); }
    int id() const { return id_; }
    size_t mapped_size() { return buffer_handle_->mapped_size(); }
    void* data() { return buffer_handle_->data(); }

   private:
    const int id_;
    const scoped_refptr<media::VideoCaptureBufferPool> pool_;
    const std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle_;
  };

  VideoCaptureBufferPoolTest() : expected_dropped_id_(0) {
#if BUILDFLAG(IS_WIN)
    auto dxgi_device_manager =
        media::DXGIDeviceManager::Create(CHROME_LUID{0, 0});
    DCHECK(dxgi_device_manager);
    d3d11_device_ = dxgi_device_manager->GetDevice().Get();
    DCHECK(d3d11_device_);
    pool_ = new media::VideoCaptureBufferPoolImpl(
        GetBufferType(), kTestBufferPoolSize,
        std::make_unique<media::VideoCaptureBufferTrackerFactoryImpl>(
            std::move(dxgi_device_manager)));
#else
    pool_ = new media::VideoCaptureBufferPoolImpl(
        media::VideoCaptureBufferType::kSharedMemory, kTestBufferPoolSize);
#endif
  }

  void ExpectDroppedId(int expected_dropped_id) {
    expected_dropped_id_ = expected_dropped_id;
  }

  std::unique_ptr<Buffer> ReserveBuffer(const gfx::Size& dimensions,
                                        media::VideoPixelFormat pixel_format) {
    // To verify that ReserveBuffer always sets |buffer_id_to_drop|,
    // initialize it to something different than the expected value.
    int buffer_id_to_drop = ~expected_dropped_id_;
    DVLOG(1) << media::VideoPixelFormatToString(pixel_format) << " "
             << dimensions.ToString();
    const int arbitrary_frame_feedback_id = 0;
    int buffer_id = media::VideoCaptureBufferPool::kInvalidId;
    const auto reserve_result = pool_->ReserveForProducer(
        dimensions, pixel_format, nullptr, arbitrary_frame_feedback_id,
        &buffer_id, &buffer_id_to_drop);
    if (reserve_result !=
        media::VideoCaptureDevice::Client::ReserveResult::kSucceeded) {
      return nullptr;
    }
    EXPECT_EQ(expected_dropped_id_, buffer_id_to_drop);

    std::unique_ptr<media::VideoCaptureBufferHandle> buffer_handle =
        pool_->GetHandleForInProcessAccess(buffer_id);
    return std::make_unique<Buffer>(pool_, std::move(buffer_handle), buffer_id);
  }
  media::VideoPixelFormat GetPixelFormat() { return std::get<0>(GetParam()); }
  media::VideoCaptureBufferType GetBufferType() {
    return std::get<1>(GetParam());
  }

#if BUILDFLAG(IS_WIN)
  raw_ptr<ID3D11Device> d3d11_device_ = nullptr;
#endif
  base::test::SingleThreadTaskEnvironment task_environment_;
  int expected_dropped_id_;
  scoped_refptr<media::VideoCaptureBufferPool> pool_;
};

TEST_P(VideoCaptureBufferPoolTest, BufferPool) {
  media::VideoPixelFormat pixel_format = GetPixelFormat();

  const gfx::Size size_lo = gfx::Size(10, 10);
  const gfx::Size size_hi = gfx::Size(21, 33);
  const media::VideoCaptureFormat format_lo(size_lo, 0.0, pixel_format);
  const media::VideoCaptureFormat format_hi(size_hi, 0.0, pixel_format);

  // Reallocation won't happen for the first part of the test.
  ExpectDroppedId(media::VideoCaptureBufferPool::kInvalidId);

  // The buffer pool should have zero utilization before any buffers have been
  // reserved.
  ASSERT_EQ(0.0, pool_->GetBufferPoolUtilization());

  std::unique_ptr<Buffer> buffer1 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer1.get());
  ASSERT_EQ(1.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer2 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer3 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer3.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());

  ASSERT_LE(ImageAllocationSize(format_lo), buffer1->mapped_size());
  ASSERT_LE(ImageAllocationSize(format_lo), buffer2->mapped_size());
  ASSERT_LE(ImageAllocationSize(format_lo), buffer3->mapped_size());

  ASSERT_NE(nullptr, buffer1->data());
  ASSERT_NE(nullptr, buffer2->data());
  ASSERT_NE(nullptr, buffer3->data());

  // Touch the memory.
  if (buffer1->data() != nullptr)
    memset(buffer1->data(), 0x11, buffer1->mapped_size());
  if (buffer2->data() != nullptr)
    memset(buffer2->data(), 0x44, buffer2->mapped_size());
  if (buffer3->data() != nullptr)
    memset(buffer3->data(), 0x77, buffer3->mapped_size());

  // Fourth buffer should fail.  Buffer pool utilization should be at 100%.
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Release 1st buffer and retry; this should succeed.
  buffer1.reset();
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  std::unique_ptr<Buffer> buffer4 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer4.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());

  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_hi, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Validate the IDs
  int buffer_id2 = buffer2->id();
  ASSERT_EQ(1, buffer_id2);
  const int buffer_id3 = buffer3->id();
  ASSERT_EQ(2, buffer_id3);
  const int buffer_id4 = buffer4->id();
  ASSERT_EQ(0, buffer_id4);
  void* const memory_pointer3 = buffer3->data();

  // Deliver a buffer.
  pool_->HoldForConsumers(buffer_id3, 2);

  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  buffer3.reset();  // Old producer releases buffer. Should be a noop.
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_hi, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  buffer2.reset();  // Active producer releases buffer. Should free a buffer.

  buffer1 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer1.get());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // First consumer finishes.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Second consumer finishes. This should free that buffer.
  pool_->RelinquishConsumerHold(buffer_id3, 1);
  buffer3 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer3.get());
  ASSERT_EQ(buffer_id3, buffer3->id()) << "Buffer ID should be reused.";
  ASSERT_EQ(memory_pointer3, buffer3->data());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Now deliver & consume buffer1, but don't release the buffer.
  int buffer_id1 = buffer1->id();
  ASSERT_EQ(1, buffer_id1);
  pool_->HoldForConsumers(buffer_id1, 5);
  pool_->RelinquishConsumerHold(buffer_id1, 5);

  // Even though the consumer is done with the buffer at |buffer_id1|, it cannot
  // be re-allocated to the producer, because |buffer1| still references it. But
  // when |buffer1| goes away, we should be able to re-reserve the buffer (and
  // the ID ought to be the same).
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());
  buffer1.reset();  // Should free the buffer.
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_lo, pixel_format);
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(buffer_id1, buffer2->id());
  buffer_id2 = buffer_id1;
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Now try reallocation with different resolutions. We expect reallocation
  // to occur only when the old buffer is too small.
  buffer2.reset();
  ExpectDroppedId(buffer_id2);
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_hi, pixel_format);
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_LE(ImageAllocationSize(format_hi), buffer2->mapped_size());
  ASSERT_EQ(3, buffer2->id());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  void* const memory_pointer_hi = buffer2->data();
  buffer2.reset();  // Frees it.
  ExpectDroppedId(media::VideoCaptureBufferPool::kInvalidId);
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  buffer2 = ReserveBuffer(size_lo, pixel_format);
  void* const memory_pointer_lo = buffer2->data();
  ASSERT_EQ(memory_pointer_hi, memory_pointer_lo)
      << "Decrease in resolution should not reallocate buffer";
  ASSERT_NE(nullptr, buffer2.get());
  ASSERT_EQ(3, buffer2->id());
  ASSERT_LE(ImageAllocationSize(format_lo), buffer2->mapped_size());
  ASSERT_EQ(3.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  ASSERT_FALSE(ReserveBuffer(size_lo, pixel_format)) << "Pool should be empty";
  ASSERT_EQ(1.0, pool_->GetBufferPoolUtilization());

  // Tear down the pool_, writing into the buffers. The buffer should preserve
  // the lifetime of the underlying memory.
  buffer3.reset();
  ASSERT_EQ(2.0 / kTestBufferPoolSize, pool_->GetBufferPoolUtilization());
  pool_ = nullptr;

  // Touch the memory.
  if (buffer2->data() != nullptr)
    memset(buffer2->data(), 0x22, buffer2->mapped_size());
  if (buffer4->data() != nullptr)
    memset(buffer4->data(), 0x55, buffer4->mapped_size());
  buffer2.reset();

  if (buffer4->data() != nullptr)
    memset(buffer4->data(), 0x77, buffer4->mapped_size());
  buffer4.reset();
}

#if BUILDFLAG(IS_WIN)
namespace {

gfx::GpuMemoryBufferHandle CreateHandle(ID3D11Device* d3d11_device) {
  EXPECT_TRUE(d3d11_device != nullptr);

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = kDefaultTextureSize.width();
  desc.Height = kDefaultTextureSize.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.SampleDesc.Count = 1;
  desc.BindFlags = 0;
  desc.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

  ID3D11Texture2D* texture = nullptr;
  HRESULT hr = d3d11_device->CreateTexture2D(&desc, nullptr, &texture);
  EXPECT_HRESULT_SUCCEEDED(hr);

  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = texture->QueryInterface(IID_PPV_ARGS(&dxgi_resource));
  EXPECT_HRESULT_SUCCEEDED(hr);

  HANDLE texture_handle;
  hr = dxgi_resource->CreateSharedHandle(
      nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
      &texture_handle);
  EXPECT_HRESULT_SUCCEEDED(hr);

  gfx::GpuMemoryBufferHandle result;
  result.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
  result.dxgi_handle.Set(texture_handle);
  result.dxgi_token = gfx::DXGIHandleToken();
  return result;
}

}  // namespace

TEST_P(VideoCaptureBufferPoolTest, BufferPoolExternalWin) {
  auto handle0 = CreateHandle(d3d11_device_);
  auto handle1 = CreateHandle(d3d11_device_);
  auto handle2 = CreateHandle(d3d11_device_);
  int buffer_id_to_drop;
  int buffer_id0 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle0), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id0),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id0, kInvalidId);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id0, 1);
  pool_->RelinquishProducerReservation(buffer_id0);
  // We should get a new buffer for handle1.
  int buffer_id1 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle1), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id1, kInvalidId);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1, 1);
  pool_->RelinquishProducerReservation(buffer_id1);
  pool_->RelinquishConsumerHold(buffer_id1, 1);
  // We should reuse handle1's buffer.
  int buffer_id1_reuse = kInvalidId;

  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id1), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1_reuse),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_EQ(buffer_id1, buffer_id1_reuse);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1_reuse, 1);
  pool_->RelinquishProducerReservation(buffer_id1_reuse);
  // If we leave buffer_id1 held for a consumer, then we create a new buffer id
  // for it.
  int buffer_id1_new = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id1), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1_new),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id1, buffer_id1_new);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1_new, 1);
  pool_->RelinquishProducerReservation(buffer_id1_new);
  pool_->RelinquishConsumerHold(buffer_id1_new, 1);
  // We have now reached kTestBufferPoolSize buffers. So our next allocation
  // will return the LRU buffer, which is buffer_id1_new.
  pool_->RelinquishConsumerHold(buffer_id1_reuse, 1);
  int buffer_id2 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle2), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id2),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id0, buffer_id2);
  EXPECT_NE(buffer_id1, buffer_id2);
  EXPECT_NE(buffer_id1_new, buffer_id2);
  EXPECT_EQ(buffer_id_to_drop, buffer_id1_new);
  EXPECT_NE(buffer_id2, kInvalidId);
  // Finally, let's reuse handle0.
  pool_->RelinquishConsumerHold(buffer_id0, 1);
  int buffer_id0_reuse = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id0), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id0_reuse),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_EQ(buffer_id0, buffer_id0_reuse);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
}

#endif

#if BUILDFLAG(IS_MAC)
namespace {

gfx::GpuMemoryBufferHandle CreateIOSurfaceHandle() {
  gfx::GpuMemoryBufferHandle result;
  result.type = gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
  result.id = gfx::GpuMemoryBufferHandle::kInvalidId;
  result.io_surface =
      gfx::CreateIOSurface(kDefaultTextureSize, gfx::BufferFormat::BGRA_8888);
  return result;
}

}  // namespace

TEST_P(VideoCaptureBufferPoolTest, BufferPoolExternal) {
  auto handle0 = CreateIOSurfaceHandle();
  auto handle1 = CreateIOSurfaceHandle();
  auto handle2 = CreateIOSurfaceHandle();

  int buffer_id_to_drop;
  int buffer_id0 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle0), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id0),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id0, kInvalidId);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  EXPECT_FALSE(IOSurfaceIsInUse(
      pool_->GetGpuMemoryBufferHandle(buffer_id0).io_surface.get()));
  pool_->HoldForConsumers(buffer_id0, 1);
  EXPECT_TRUE(IOSurfaceIsInUse(
      pool_->GetGpuMemoryBufferHandle(buffer_id0).io_surface.get()));
  pool_->RelinquishProducerReservation(buffer_id0);
  // We should get a new buffer for handle1.
  int buffer_id1 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle1), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id1, kInvalidId);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1, 1);
  pool_->RelinquishProducerReservation(buffer_id1);
  pool_->RelinquishConsumerHold(buffer_id1, 1);
  // We should reuse handle1's buffer.
  int buffer_id1_reuse = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id1), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1_reuse),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_EQ(buffer_id1, buffer_id1_reuse);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1_reuse, 1);
  pool_->RelinquishProducerReservation(buffer_id1_reuse);
  // If we leave buffer_id1 held for a consumer, then we create a new buffer id
  // for it.
  int buffer_id1_new = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id1), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id1_new),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id1, buffer_id1_new);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
  pool_->HoldForConsumers(buffer_id1_new, 1);
  pool_->RelinquishProducerReservation(buffer_id1_new);
  pool_->RelinquishConsumerHold(buffer_id1_new, 1);
  // We have now reached kTestBufferPoolSize buffers. So our next allocation
  // will return the LRU buffer, which is buffer_id1_new.
  pool_->RelinquishConsumerHold(buffer_id1_reuse, 1);
  int buffer_id2 = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    std::move(handle2), kDefaultFormat, kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id2),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_NE(buffer_id0, buffer_id2);
  EXPECT_NE(buffer_id1, buffer_id2);
  EXPECT_NE(buffer_id1_new, buffer_id2);
  EXPECT_NE(buffer_id2, kInvalidId);
  EXPECT_EQ(buffer_id_to_drop, buffer_id1_new);
  // Finally, let's reuse handle0.
  pool_->RelinquishConsumerHold(buffer_id0, 1);
  int buffer_id0_reuse = kInvalidId;
  EXPECT_EQ(pool_->ReserveIdForExternalBuffer(
                media::CapturedExternalVideoBuffer(
                    pool_->GetGpuMemoryBufferHandle(buffer_id0), kDefaultFormat,
                    kDefaultColorSpace),
                kDefaultTextureSize, &buffer_id_to_drop, &buffer_id0_reuse),
            media::VideoCaptureDevice::Client::ReserveResult::kSucceeded);
  EXPECT_EQ(buffer_id0, buffer_id0_reuse);
  EXPECT_EQ(buffer_id_to_drop, kInvalidId);
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    VideoCaptureBufferPoolTest,
    testing::Combine(testing::ValuesIn(kCapturePixelFormats),
                     testing::ValuesIn(kVideoCaptureBufferTypes)));

}  // namespace content
