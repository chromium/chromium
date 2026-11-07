// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/test/bind.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/cpu_readback_upload_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/dawn_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dawn_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if BUILDFLAG(SKIA_USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

#include "gpu/command_buffer/service/dawn_context_provider.h"
#endif

namespace gpu {
namespace {

class AsyncReadbackTestImageBacking : public TestImageBacking {
 public:
  using TestImageBacking::TestImageBacking;

  void ReadbackToMemoryAsync(const std::vector<SkPixmap>& pixmaps,
                             base::OnceCallback<void(bool)> callback) override {
    pending_pixmaps_ = pixmaps;
    pending_callback_ = std::move(callback);
  }

  bool has_pending_readback() const { return !pending_callback_.is_null(); }

  void CompletePendingReadback(bool success, uint32_t pixel_value) {
    ASSERT_TRUE(pending_callback_);
    if (success) {
      for (auto& pixmap : pending_pixmaps_) {
        *pixmap.writable_addr32(0, 0) = pixel_value;
      }
    }
    pending_pixmaps_.clear();
    std::move(pending_callback_).Run(success);
  }

 private:
  std::vector<SkPixmap> pending_pixmaps_;
  base::OnceCallback<void(bool)> pending_callback_;
};

}  // namespace

class SharedImageCopyManagerTest : public SharedImageTestBase {
 public:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));
    copy_manager_ = base::MakeRefCounted<SharedImageCopyManager>();
  }

 protected:
  scoped_refptr<SharedImageCopyManager> copy_manager_;
};

TEST_F(SharedImageCopyManagerTest, CopyUsingCpuFallbackStrategy) {
  copy_manager_->AddStrategy(std::make_unique<CPUReadbackUploadCopyStrategy>());

  auto src_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(
          viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(100, 100),
          gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType,
          SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
          "TestLabel"),
      1024);
  auto dst_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(
          viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(100, 100),
          gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType,
          SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
          "TestLabel"),
      1024);

  EXPECT_TRUE(copy_manager_->CopyImage(src_backing.get(), dst_backing.get()));

  EXPECT_TRUE(src_backing->GetReadbackToMemoryCalledAndReset());
  EXPECT_TRUE(dst_backing->GetUploadFromMemoryCalledAndReset());
}

TEST_F(SharedImageCopyManagerTest, CopySharedMemoryDirectViaCpuStrategy) {
  auto strategy = std::make_unique<CPUReadbackUploadCopyStrategy>();
  CPUReadbackUploadCopyStrategy* strategy_ptr = strategy.get();
  copy_manager_->AddStrategy(std::move(strategy));

  constexpr viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  constexpr gfx::Size size(100, 100);
  constexpr auto usage =
      SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;

  // Create two shared memory backings.
  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, usage, "TestLabel"},
      kNullSurfaceHandle,
      /*is_thread_safe=*/false, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  auto other_shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, usage, "OtherShmLabel"},
      kNullSurfaceHandle,
      /*is_thread_safe=*/false, gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);

  // Create a generic test backing.
  auto test_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(format, size, gfx::ColorSpace::CreateSRGB(),
                      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
                      "TestLabel"),
      1024);

  // Copy from SharedMemory to Test backing uploads directly in 1 hop.
  EXPECT_TRUE(copy_manager_->CopyImage(shm_backing.get(), test_backing.get()));
  EXPECT_TRUE(test_backing->GetUploadFromMemoryCalledAndReset());
  EXPECT_FALSE(test_backing->GetReadbackToMemoryCalledAndReset());

  // Copy from Test to SharedMemory backing reads back directly in 1 hop.
  EXPECT_TRUE(copy_manager_->CopyImage(test_backing.get(), shm_backing.get()));
  EXPECT_TRUE(test_backing->GetReadbackToMemoryCalledAndReset());
  EXPECT_FALSE(test_backing->GetUploadFromMemoryCalledAndReset());

  // SharedMemory-to-SharedMemory is unsupported and fails cleanly without
  // invoking SharedImageBacking::ReadbackToMemory().
  EXPECT_FALSE(
      strategy_ptr->CanCopy(shm_backing.get(), other_shm_backing.get()));
  EXPECT_FALSE(strategy_ptr->Copy(shm_backing.get(), other_shm_backing.get()));
  EXPECT_FALSE(
      copy_manager_->CopyImage(shm_backing.get(), other_shm_backing.get()));
}

TEST_F(SharedImageCopyManagerTest, CopyImageAsyncToSharedMemoryAndFallback) {
  copy_manager_->AddStrategy(std::make_unique<CPUReadbackUploadCopyStrategy>());

  constexpr viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  constexpr gfx::Size size(16, 16);
  constexpr auto usage =
      SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY;

  auto shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, usage, "ShmLabel"},
      kNullSurfaceHandle, /*is_thread_safe=*/false,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  auto other_shm_backing = SharedMemoryImageBackingFactory().CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, usage, "OtherShmLabel"},
      kNullSurfaceHandle, /*is_thread_safe=*/false,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  auto async_src = std::make_unique<AsyncReadbackTestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(format, size, gfx::ColorSpace::CreateSRGB(),
                      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
                      "AsyncSrc"),
      1024);
  auto sync_dst = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(format, size, gfx::ColorSpace::CreateSRGB(),
                      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, usage,
                      "SyncDst"),
      1024);

  // 1. GPU -> SharedMemory delegates to ReadbackToMemoryAsync() into the
  //    destination SharedMemoryImageBacking's pixmaps without blocking.
  std::optional<bool> async_result;
  copy_manager_->CopyImageAsync(async_src.get(), shm_backing.get(),
                                base::BindLambdaForTesting([&](bool success) {
                                  async_result = success;
                                }));
  EXPECT_FALSE(async_result.has_value());
  EXPECT_FALSE(async_src->GetReadbackToMemoryCalledAndReset());
  ASSERT_TRUE(async_src->has_pending_readback());

  async_src->CompletePendingReadback(/*success=*/true,
                                     /*pixel_value=*/0x12345678u);
  ASSERT_TRUE(async_result.has_value());
  EXPECT_TRUE(*async_result);
  const auto& shm_pixmaps =
      static_cast<SharedMemoryImageBacking*>(shm_backing.get())->pixmaps();
  EXPECT_EQ(*shm_pixmaps[0].addr32(0, 0), 0x12345678u);

  // 2. SharedMemory -> GPU falls back to synchronous UploadFromMemory().
  std::optional<bool> upload_result;
  copy_manager_->CopyImageAsync(shm_backing.get(), sync_dst.get(),
                                base::BindLambdaForTesting([&](bool success) {
                                  upload_result = success;
                                }));
  ASSERT_TRUE(upload_result.has_value());
  EXPECT_TRUE(*upload_result);
  EXPECT_TRUE(sync_dst->GetUploadFromMemoryCalledAndReset());

  // 3. Unsupported pairs invoke `callback` with false.
  std::optional<bool> fail_result;
  copy_manager_->CopyImageAsync(
      shm_backing.get(), other_shm_backing.get(),
      base::BindLambdaForTesting([&](bool success) { fail_result = success; }));
  ASSERT_TRUE(fail_result.has_value());
  EXPECT_FALSE(*fail_result);
}

TEST(SharedImageCopyManagerUnitTest, GetEffectiveCopyFormat) {
  EXPECT_EQ(GetEffectiveCopyFormat(viz::SinglePlaneFormat::kRGBA_8888),
            viz::SinglePlaneFormat::kRGBA_8888);
  EXPECT_EQ(GetEffectiveCopyFormat(viz::MultiPlaneFormat::kNV12),
            viz::MultiPlaneFormat::kNV12);
#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_ANDROID)
  viz::SharedImageFormat nv12_external = viz::MultiPlaneFormat::kNV12;
  nv12_external.SetPrefersExternalSampler();
  EXPECT_EQ(GetEffectiveCopyFormat(nv12_external),
            viz::SinglePlaneFormat::kRGBA_8888);
  viz::SharedImageFormat p010_external = viz::MultiPlaneFormat::kP010;
  p010_external.SetPrefersExternalSampler();
  EXPECT_EQ(GetEffectiveCopyFormat(p010_external),
            viz::SinglePlaneFormat::kRGBA_1010102);
#endif
}

#if BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_ANDROID)
TEST(SharedImageCopyManagerUnitTest,
     CpuFallbackStrategySupportsExternalSamplerSourceOnly) {
  CPUReadbackUploadCopyStrategy strategy;
  viz::SharedImageFormat nv12_external = viz::MultiPlaneFormat::kNV12;
  nv12_external.SetPrefersExternalSampler();

  auto ext_src = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(nv12_external, gfx::Size(64, 64),
                      gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
                      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_DISPLAY_READ,
                      "ext_src"),
      1024);
  auto rgba_dst = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(64, 64),
                      gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
                      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2_READ,
                      "rgba_dst"),
      1024);

  EXPECT_TRUE(strategy.CanCopy(ext_src.get(), rgba_dst.get()));
  EXPECT_FALSE(strategy.CanCopy(rgba_dst.get(), ext_src.get()));
  EXPECT_FALSE(strategy.CanCopy(ext_src.get(), ext_src.get()));

  EXPECT_TRUE(strategy.Copy(ext_src.get(), rgba_dst.get()));
  EXPECT_TRUE(ext_src->GetReadbackToMemoryCalledAndReset());
  EXPECT_TRUE(rgba_dst->GetUploadFromMemoryCalledAndReset());

  auto shm_rgba = SharedMemoryImageBackingFactory().CreateSharedImage(
      Mailbox::Generate(),
      {viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(64, 64),
       gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType,
       SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
       "shm_rgba"},
      kNullSurfaceHandle, /*is_thread_safe=*/false,
      gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  EXPECT_TRUE(strategy.CanCopy(ext_src.get(), shm_rgba.get()));
  EXPECT_FALSE(strategy.CanCopy(shm_rgba.get(), ext_src.get()));
  EXPECT_TRUE(strategy.Copy(ext_src.get(), shm_rgba.get()));
  EXPECT_TRUE(ext_src->GetReadbackToMemoryCalledAndReset());
}
#endif  // BUILDFLAG(IS_OZONE) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(SKIA_USE_DAWN)
class SharedImageCopyManagerTestWithGraphiteDawn : public SharedImageTestBase {
 public:
  void SetUp() override {
    // This will set up Dawn and Graphite.
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGraphiteDawn));
    copy_manager_ = base::MakeRefCounted<SharedImageCopyManager>();
  }

 protected:
  scoped_refptr<SharedImageCopyManager> copy_manager_;
};

TEST_F(SharedImageCopyManagerTestWithGraphiteDawn, CopyUsingDawnStrategy) {
  copy_manager_->AddStrategy(std::make_unique<DawnCopyStrategy>());

  constexpr viz::SharedImageFormat format = viz::SinglePlaneFormat::kRGBA_8888;
  constexpr gfx::Size size(100, 100);
  constexpr auto dawn_backing_usage = SharedImageUsageSet(
      SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE);
  constexpr auto graphite_backing_usage = SharedImageUsageSet(
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE);

  // Create a Dawn backing.
  DawnImageBackingFactory dawn_factory;
  auto dawn_backing = dawn_factory.CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, dawn_backing_usage, "dawn_backing"},
      kNullSurfaceHandle, /*is_thread_safe=*/false);
  ASSERT_TRUE(dawn_backing);

  static_cast<DawnImageBacking*>(dawn_backing.get())
      ->InitializeForTesting(dawn_context_provider_->GetDevice());

  // Create a Graphite backing.
  WrappedSkImageBackingFactory sk_image_factory(context_state_);
  auto graphite_backing = sk_image_factory.CreateSharedImage(
      Mailbox::Generate(),
      {format, size, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
       kPremul_SkAlphaType, graphite_backing_usage, "graphite_backing"},
      kNullSurfaceHandle, /*is_thread_safe=*/false);
  ASSERT_TRUE(graphite_backing);

  // Test copy from Graphite to Dawn.
  EXPECT_TRUE(
      copy_manager_->CopyImage(graphite_backing.get(), dawn_backing.get()));

  // Test copy from Dawn to Graphite.
  EXPECT_TRUE(
      copy_manager_->CopyImage(dawn_backing.get(), graphite_backing.get()));

  // Test copy from Dawn texture to CPU memory.
  SkBitmap readback_bitmap;
  ASSERT_TRUE(readback_bitmap.tryAllocPixels(dawn_backing->AsSkImageInfo()));
  EXPECT_TRUE(DawnCopyStrategy::CopyFromTextureToMemory(
      static_cast<DawnImageBacking*>(dawn_backing.get())->GetTexture(), format,
      size, dawn_context_provider_->GetDevice(), {readback_bitmap.pixmap()}));
}

class DawnCopyStrategyStackUARTest : public testing::Test {
 public:
  void SetUp() override {
    dawnProcSetProcs(&dawn::native::GetProcs());

    // Create instance WITHOUT TimedWaitAny
    dawn_instance_ = std::make_unique<dawn::native::Instance>();

    wgpu::RequestAdapterOptions adapter_options;
    adapter_options.backendType = wgpu::BackendType::Vulkan;
    std::vector<dawn::native::Adapter> adapters =
        dawn_instance_->EnumerateAdapters(&adapter_options);
    ASSERT_GT(adapters.size(), 0u);

    wgpu::FeatureName dawn_internal_usage =
        wgpu::FeatureName::DawnInternalUsages;
    wgpu::DeviceDescriptor device_descriptor;
    device_descriptor.requiredFeatureCount = 1;
    device_descriptor.requiredFeatures = &dawn_internal_usage;

    dawn_device_ =
        wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));
    ASSERT_TRUE(dawn_device_) << "Failed to create Dawn device";
  }

  void TearDown() override {
    dawn_device_ = wgpu::Device();
    dawn_instance_.reset();
  }

 protected:
  std::unique_ptr<dawn::native::Instance> dawn_instance_;
  wgpu::Device dawn_device_;
};

TEST_F(DawnCopyStrategyStackUARTest,
       MapAsyncCallbackOutlivesStackOnWaitAnyFailure) {
  wgpu::TextureDescriptor texture_desc;
  texture_desc.size = {4, 4, 1};
  texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
  texture_desc.usage =
      wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::RenderAttachment;
  wgpu::Texture src_texture = dawn_device_.CreateTexture(&texture_desc);
  ASSERT_TRUE(src_texture);

  auto dst_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(),
      SharedImageInfo(
          viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(4, 4),
          gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType,
          SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
          "TestLabel"),
      1024);

  EXPECT_FALSE(DawnCopyStrategy::CopyFromTextureToBacking(
      src_texture, dst_backing.get(), dawn_device_));
}

#endif  // BUILDFLAG(SKIA_USE_DAWN)

}  // namespace gpu
