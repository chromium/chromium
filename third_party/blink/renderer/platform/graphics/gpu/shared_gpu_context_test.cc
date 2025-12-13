// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/null_task_runner.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/khronos/GLES2/gl2ext.h"

using testing::_;
using testing::Test;

namespace blink {

namespace {

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

// Test fixure that simulate a graphics context creation failure, when using gpu
// compositing.
class BadSharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    accelerated_compositing_scope_ = std::make_unique<
        ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();
    task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);
    auto factory = []() -> std::unique_ptr<WebGraphicsContext3DProvider> {
      return nullptr;
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        BindRepeating(factory));
  }

  void TearDown() override {
    handle_.reset();
    task_runner_.reset();
    SharedGpuContext::Reset();
    accelerated_compositing_scope_ = nullptr;
  }

 private:
  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle> handle_;
  std::unique_ptr<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>
      accelerated_compositing_scope_;
};

// Test fixure that simulate not using gpu compositing.
class SoftwareCompositingTest : public Test {
 public:
  void SetUp() override {
    auto factory = [](FakeGLES2Interface* gl)
        -> std::unique_ptr<WebGraphicsContext3DProvider> {
      // Return a context anyway, to ensure that's not what the class checks
      // to determine compositing mode.
      gl->SetIsContextLost(false);
      return std::make_unique<FakeWebGraphicsContext3DProvider>(gl);
    };
    SharedGpuContext::SetContextProviderFactoryForTesting(
        BindRepeating(factory, Unretained(&gl_)));
  }

  void TearDown() override { SharedGpuContext::Reset(); }

  FakeGLES2Interface gl_;
};

class SharedGpuContextTest : public Test {
 public:
  void SetUp() override {
    accelerated_compositing_scope_ = std::make_unique<
        ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();
    task_runner_ = base::MakeRefCounted<base::NullTaskRunner>();
    handle_ =
        std::make_unique<base::SingleThreadTaskRunner::CurrentDefaultHandle>(
            task_runner_);
    test_context_provider_ = viz::TestContextProvider::CreateRaster();

    InitializeSharedGpuContextRaster(test_context_provider_.get(),
                                     /*cache = */ nullptr,
                                     SetIsContextLost::kSetToFalse);
  }

  void TearDown() override {
    handle_.reset();
    task_runner_.reset();
    SharedGpuContext::Reset();
  }
  scoped_refptr<base::NullTaskRunner> task_runner_;
  std::unique_ptr<base::SingleThreadTaskRunner::CurrentDefaultHandle> handle_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
  std::unique_ptr<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>
      accelerated_compositing_scope_;
};

TEST_F(SharedGpuContextTest, contextLossAutoRecovery) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context =
      SharedGpuContext::ContextProviderWrapper();
  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoringForTesting());
  EXPECT_TRUE(!!context);

  // Context recreation results in old provider being discarded.
  EXPECT_TRUE(!!SharedGpuContext::ContextProviderWrapper());
  EXPECT_FALSE(!!context);
}

TEST_F(SharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_NE(SharedGpuContext::ContextProviderWrapper(), nullptr);
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoringForTesting());
}

TEST_F(BadSharedGpuContextTest, IsValidWithoutRestoring) {
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoringForTesting());
}

TEST_F(BadSharedGpuContextTest, AllowSoftwareToAcceleratedCanvasUpgrade) {
  EXPECT_FALSE(SharedGpuContext::AllowSoftwareToAcceleratedCanvasUpgrade());
}

TEST_F(BadSharedGpuContextTest, AccelerateImageBufferSurfaceCreationFails) {
  // With a bad shared context, AccelerateImageBufferSurface should fail and
  // return a nullptr provider
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
          gfx::ColorSpace::CreateSRGB(),
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          gpu::SharedImageUsageSet());
  EXPECT_FALSE(resource_provider);
}

TEST_F(SharedGpuContextTest, CompositingMode) {
  EXPECT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(BadSharedGpuContextTest, CompositingMode) {
  EXPECT_TRUE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(SoftwareCompositingTest, CompositingMode) {
  EXPECT_FALSE(SharedGpuContext::IsGpuCompositingEnabled());
}

TEST_F(SharedGpuContextTest, AccelerateImageBufferSurfaceAutoRecovery) {
  // Verifies that after a context loss, attempting to allocate an
  // AcceleratedImageBufferSurface will restore the context and succeed
  test_context_provider_->GetTestRasterInterface()->set_context_lost(true);
  EXPECT_FALSE(SharedGpuContext::IsValidWithoutRestoringForTesting());
  std::unique_ptr<CanvasResourceProvider> resource_provider =
      CanvasResourceProvider::CreateSharedImageProvider(
          gfx::Size(10, 10), GetN32FormatForCanvas(), kPremul_SkAlphaType,
          gfx::ColorSpace::CreateSRGB(),
          CanvasResourceProvider::ShouldInitialize::kNo,
          SharedGpuContext::ContextProviderWrapper(), RasterMode::kGPU,
          gpu::SharedImageUsageSet());
  EXPECT_TRUE(resource_provider && resource_provider->IsValid());
  EXPECT_TRUE(resource_provider->IsAccelerated());
  EXPECT_TRUE(SharedGpuContext::IsValidWithoutRestoringForTesting());
}

}  // unnamed namespace

}  // namespace blink
