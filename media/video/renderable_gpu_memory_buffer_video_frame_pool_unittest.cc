// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/renderable_gpu_memory_buffer_video_frame_pool.h"

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "media/base/video_frame.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace media {

namespace {

class FakeContext : public RenderableGpuMemoryBufferVideoFramePool::Context {
 public:
  FakeContext() : weak_factory_(this) {}
  ~FakeContext() override = default;

  std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    DoCreateGpuMemoryBuffer(size, format);
    return std::make_unique<FakeGpuMemoryBuffer>(size, format);
  }
  void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                         gfx::BufferPlane plane,
                         const gfx::ColorSpace& color_space,
                         GrSurfaceOrigin surface_origin,
                         SkAlphaType alpha_type,
                         uint32_t usage,
                         gpu::Mailbox& mailbox,
                         gpu::SyncToken& sync_token) override {
    DoCreateSharedImage(gpu_memory_buffer, plane, color_space, surface_origin,
                        alpha_type, usage);
    mailbox = gpu::Mailbox::Generate();
  }

  MOCK_METHOD2(DoCreateGpuMemoryBuffer,
               void(const gfx::Size& size, gfx::BufferFormat format));
  MOCK_METHOD6(DoCreateSharedImage,
               void(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                    gfx::BufferPlane plane,
                    const gfx::ColorSpace& color_space,
                    GrSurfaceOrigin surface_origin,
                    SkAlphaType alpha_type,
                    uint32_t usage));
  MOCK_METHOD2(DestroySharedImage,
               void(const gpu::SyncToken& sync_token,
                    const gpu::Mailbox& mailbox));

  base::WeakPtr<FakeContext> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  base::WeakPtrFactory<FakeContext> weak_factory_;
};

TEST(RenderableGpuMemoryBufferVideoFramePool, SimpleLifetimes) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const gfx::BufferFormat format = gfx::BufferFormat::YUV_420_BIPLANAR;
  const gfx::Size size0(128, 256);

  base::WeakPtr<FakeContext> context;
  std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool> pool;
  {
    auto context_strong = std::make_unique<FakeContext>();
    context = context_strong->GetWeakPtr();
    pool = RenderableGpuMemoryBufferVideoFramePool::Create(
        std::move(context_strong));
  }

  // Create a new frame.
  EXPECT_CALL(*context, DoCreateGpuMemoryBuffer(size0, format));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::Y, _, _, _, _));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::UV, _, _, _, _));
  auto video_frame0 = pool->MaybeCreateVideoFrame(size0);
  video_frame0 = nullptr;
  task_environment.RunUntilIdle();

  // Expect the frame to be reused.
  EXPECT_CALL(*context, DoCreateGpuMemoryBuffer(size0, format)).Times(0);
  EXPECT_CALL(*context, DoCreateSharedImage(_, _, _, _, _, _)).Times(0);
  auto video_frame1 = pool->MaybeCreateVideoFrame(size0);

  // Expect a new frame to be created.
  EXPECT_CALL(*context, DoCreateGpuMemoryBuffer(size0, format));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::Y, _, _, _, _));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::UV, _, _, _, _));
  auto video_frame2 = pool->MaybeCreateVideoFrame(size0);

  // Expect a new frame to be created.
  EXPECT_CALL(*context, DoCreateGpuMemoryBuffer(size0, format));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::Y, _, _, _, _));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::UV, _, _, _, _));
  auto video_frame3 = pool->MaybeCreateVideoFrame(size0);

  // Freeing two frames will not result in any frames being destroyed, because
  // we allow unused 2 frames to exist.
  video_frame1 = nullptr;
  video_frame2 = nullptr;
  task_environment.RunUntilIdle();

  // Freeing the third frame will result in one of the frames being destroyed.
  EXPECT_CALL(*context, DestroySharedImage(_, _)).Times(2);
  video_frame3 = nullptr;
  task_environment.RunUntilIdle();

  // Destroying the pool will result in the remaining two frames being
  // destroyed.
  EXPECT_TRUE(!!context);
  EXPECT_CALL(*context, DestroySharedImage(_, _)).Times(4);
  pool.reset();
  task_environment.RunUntilIdle();
  EXPECT_FALSE(!!context);
}

TEST(RenderableGpuMemoryBufferVideoFramePool, FrameFreedAfterPool) {
  base::test::SingleThreadTaskEnvironment task_environment;
  const gfx::BufferFormat format = gfx::BufferFormat::YUV_420_BIPLANAR;
  const gfx::Size size0(128, 256);

  base::WeakPtr<FakeContext> context;
  std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool> pool;
  {
    auto context_strong = std::make_unique<FakeContext>();
    context = context_strong->GetWeakPtr();
    pool = RenderableGpuMemoryBufferVideoFramePool::Create(
        std::move(context_strong));
  }

  // Create a new frame.
  EXPECT_CALL(*context, DoCreateGpuMemoryBuffer(size0, format));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::Y, _, _, _, _));
  EXPECT_CALL(*context,
              DoCreateSharedImage(_, gfx::BufferPlane::UV, _, _, _, _));
  auto video_frame0 = pool->MaybeCreateVideoFrame(size0);
  task_environment.RunUntilIdle();

  // If the pool is destroyed, but a frame still exists, the context will not
  // be destroyed.
  pool.reset();
  task_environment.RunUntilIdle();
  EXPECT_TRUE(context);

  // Destroy the frame. Still nothing will happen, because its destruction will
  // happen after a posted task is run.
  video_frame0 = nullptr;

  // The shared images will be destroyed once the posted task is run.
  EXPECT_CALL(*context, DestroySharedImage(_, _)).Times(2);
  task_environment.RunUntilIdle();
  EXPECT_FALSE(!!context);
}

}  // namespace

}  // namespace media
