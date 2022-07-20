// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/surface_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"

namespace gpu {
namespace {

class TestSharedImageBackingFactory : public SharedImageBackingFactory {
 public:
  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe) override {
    return std::make_unique<TestImageBacking>(mailbox, format, size,
                                              color_space, surface_origin,
                                              alpha_type, usage, 0);
  }
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::span<const uint8_t> pixel_data) override {
    return nullptr;
  }
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      int client_id,
      gfx::GpuMemoryBufferHandle handle,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      SurfaceHandle surface_handle,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage) override {
    return nullptr;
  }
  bool IsSupported(uint32_t usage,
                   viz::ResourceFormat format,
                   bool thread_safe,
                   gfx::GpuMemoryBufferType gmb_type,
                   GrContextType gr_context_type,
                   bool* allow_legacy_mailbox,
                   bool is_pixel_used) override {
    return true;
  }
};

}  // namespace

class CompoundImageBackingTest : public testing::Test {
 public:
  TestImageBacking* GetGpuBacking(CompoundImageBacking* backing) {
    auto* gpu_backing = backing->gpu_backing_.get();
    DCHECK_EQ(gpu_backing->GetType(), SharedImageBackingType::kTest);
    return static_cast<TestImageBacking*>(gpu_backing);
  }

  // Create a compound backing containing shared memory + GPU backing.
  std::unique_ptr<SharedImageBacking> CreateCompoundBacking() {
    constexpr gfx::Size size(100, 100);
    constexpr gfx::BufferFormat buffer_format = gfx::BufferFormat::RGBA_8888;
    constexpr gfx::BufferUsage buffer_usage =
        gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

    gfx::GpuMemoryBufferHandle handle =
        GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
            static_cast<gfx::GpuMemoryBufferId>(1), size, buffer_format,
            buffer_usage);

    return CompoundImageBacking::CreateSharedMemory(
        &test_factory_, Mailbox::GenerateForSharedImage(), std::move(handle),
        buffer_format, gfx::BufferPlane::DEFAULT, kNullSurfaceHandle, size,
        gfx::ColorSpace(), kBottomLeft_GrSurfaceOrigin, kOpaque_SkAlphaType,
        SHARED_IMAGE_USAGE_DISPLAY);
  }

 protected:
  SharedImageManager manager_;
  TestSharedImageBackingFactory test_factory_;
  gles2::MockMemoryTracker mock_memory_tracker_;
  MemoryTypeTracker tracker_{&mock_memory_tracker_};
};

TEST_F(CompoundImageBackingTest, References) {
  auto backing = CreateCompoundBacking();

  auto* compound_backing = static_cast<CompoundImageBacking*>(backing.get());
  EXPECT_NE(compound_backing, nullptr);

  auto* gpu_backing = GetGpuBacking(compound_backing);

  // The compound backing hasn't been added to the manager yet so it should
  // have zero references.
  EXPECT_FALSE(compound_backing->HasAnyRefs());
  EXPECT_FALSE(gpu_backing->HasAnyRefs());

  auto factory_rep = manager_.Register(std::move(backing), &tracker_);

  // After register compound backing it should have a reference. The GPU
  // backing should never have any reference as it's owned by the compound
  // backing and isn't reference counted.
  EXPECT_TRUE(compound_backing->HasAnyRefs());
  EXPECT_FALSE(gpu_backing->HasAnyRefs());

  auto overlay_rep =
      manager_.ProduceOverlay(compound_backing->mailbox(), &tracker_);

  // GPU backing still shouldn't have any references after a wrapped
  // representation is created.
  EXPECT_TRUE(compound_backing->HasAnyRefs());
  EXPECT_FALSE(gpu_backing->HasAnyRefs());

  overlay_rep.reset();
  EXPECT_TRUE(compound_backing->HasAnyRefs());

  // All the backings will be destroyed after this point.
  factory_rep.reset();
}

TEST_F(CompoundImageBackingTest, UploadOnAccess) {
  auto backing = CreateCompoundBacking();
  auto* compound_backing = static_cast<CompoundImageBacking*>(backing.get());
  auto* gpu_backing = GetGpuBacking(compound_backing);

  auto factory_rep = manager_.Register(std::move(backing), &tracker_);

  // The compound backing hasn't been accessed yet.
  EXPECT_FALSE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  auto overlay_rep =
      manager_.ProduceOverlay(compound_backing->mailbox(), &tracker_);

  // First access should trigger upload from memory to GPU.
  overlay_rep->BeginScopedReadAccess(false);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  // Second access shouldn't trigger upload since no shared memory updates
  // happened.
  overlay_rep->BeginScopedReadAccess(false);
  EXPECT_FALSE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  // Notify compound backing of shared memory update. Next access should
  // trigger a new upload.
  compound_backing->Update(nullptr);
  overlay_rep->BeginScopedReadAccess(false);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  // Test that GLTexturePassthrough access causes upload.
  auto gl_passthrough_rep = manager_.ProduceGLTexturePassthrough(
      compound_backing->mailbox(), &tracker_);
  compound_backing->Update(nullptr);
  gl_passthrough_rep->BeginScopedAccess(
      0, SharedImageRepresentation::AllowUnclearedAccess::kNo);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  // Test that GLTexture access causes upload.
  auto gl_rep =
      manager_.ProduceGLTexture(compound_backing->mailbox(), &tracker_);
  compound_backing->Update(nullptr);
  gl_rep->BeginScopedAccess(
      0, SharedImageRepresentation::AllowUnclearedAccess::kNo);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  // Test that both read and write access by Skia triggers upload.
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto skia_rep =
      manager_.ProduceSkia(compound_backing->mailbox(), &tracker_, nullptr);

  compound_backing->Update(nullptr);
  skia_rep->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());

  compound_backing->Update(nullptr);
  skia_rep->BeginScopedReadAccess(&begin_semaphores, &end_semaphores);
  EXPECT_TRUE(gpu_backing->GetUploadFromMemoryCalledAndReset());
}

}  // namespace gpu
