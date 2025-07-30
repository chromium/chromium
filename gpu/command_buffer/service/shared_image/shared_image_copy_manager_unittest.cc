// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"

#include "gpu/command_buffer/service/shared_image/cpu_readback_upload_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace gpu {

class SharedImageCopyManagerTest : public SharedImageTestBase {
 public:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));
  }
};

TEST_F(SharedImageCopyManagerTest, CopyUsingCpuFallbackStrategy) {
  SharedImageCopyManager copy_manager;
  copy_manager.AddStrategy(std::make_unique<CPUReadbackUploadCopyStrategy>());

  auto src_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(), viz::SinglePlaneFormat::kRGBA_8888,
      gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, 1024);
  auto dst_backing = std::make_unique<TestImageBacking>(
      Mailbox::Generate(), viz::SinglePlaneFormat::kRGBA_8888,
      gfx::Size(100, 100), gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      SHARED_IMAGE_USAGE_CPU_READ | SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, 1024);

  EXPECT_TRUE(copy_manager.CopyImage(src_backing.get(), dst_backing.get()));

  EXPECT_TRUE(src_backing->GetReadbackToMemoryCalledAndReset());
  EXPECT_TRUE(dst_backing->GetUploadFromMemoryCalledAndReset());
}

}  // namespace gpu
