// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_TEST_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_TEST_BASE_H_

#include <vector>

#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/vulkan/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace viz {
class VulkanInProcessContextProvider;
}

namespace gpu {

class VulkanImplementation;

// Test base that initializes SharedContextState and other classes needed to
// create SharedImageBackings + SharedImageBackingFactories.
class SharedImageTestBase : public testing::Test {
 protected:
  // Allocate a bitmap with red pixels. RED_8 will be filled with 0xFF repeating
  // and RG_88 will be filled with OxFF00 repeating. `added_stride` is a
  // multiplier that allocates bytePerPixel * added_stride extra bytes per row.
  static SkBitmap MakeRedBitmap(SkColorType color_type,
                                const gfx::Size& size,
                                size_t added_stride = 0);

  // Allocate a bitmap for each plane by calling MakeRedBitmap().
  static std::vector<SkBitmap> AllocateRedBitmaps(viz::SharedImageFormat format,
                                                  const gfx::Size& size,
                                                  size_t added_stride = 0);

  // Returns SkPixmap from each SkBitmap.
  static std::vector<SkPixmap> GetSkPixmaps(
      const std::vector<SkBitmap>& bitmaps);

  SharedImageTestBase();
  ~SharedImageTestBase() override;

  bool use_passthrough() const;
  GrDirectContext* gr_context();
  GrContextType gr_context_type();

  // Returns true if graphite/dawn is supported for running tests.
  bool IsGraphiteDawnSupported();

  // Initializes `context_state_` for `context_type`. Expected to be called as
  // part of test SetUp(). Note this function can fail with an assertion error
  // so caller should wrap call in ASSERT_NO_FATAL_FAILURE() to ensure SetUp()
  // exits on error.
  void InitializeContext(GrContextType context_type);

  // Reads back pixels for each plane and verifies that pixels match
  // corresponding bitmap from `expected_bitmaps`.
  void VerifyPixelsWithReadback(const Mailbox& mailbox,
                                const std::vector<SkBitmap>& expect_bitmaps);

  // Reads back pixels for each plane using skia ganesh and verifies that pixels
  // match corresponding bitmap from `expected_bitmaps`.
  void VerifyPixelsWithReadbackGanesh(
      const Mailbox& mailbox,
      const std::vector<SkBitmap>& expect_bitmaps);

  // Reads back pixels for each plane using skia graphite and verifies that
  // pixels match corresponding bitmap from `expected_bitmaps`.
  void VerifyPixelsWithReadbackGraphite(
      const Mailbox& mailbox,
      const std::vector<SkBitmap>& expect_bitmaps);

  GpuPreferences gpu_preferences_;
  GpuDriverBugWorkarounds gpu_workarounds_;

#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<VulkanImplementation> vulkan_implementation_;
  scoped_refptr<viz::VulkanInProcessContextProvider> vulkan_context_provider_;
#endif
#if BUILDFLAG(SKIA_USE_METAL)
  std::unique_ptr<viz::MetalContextProvider> metal_context_provider_;
#endif
#if BUILDFLAG(SKIA_USE_DAWN)
  // Subclass can customize this method to configure a specific Dawn backend
  // when InitializeContext()
  virtual wgpu::BackendType GetDawnBackendType() const;
  virtual bool DawnForceFallbackAdapter() const;
  std::unique_ptr<DawnContextProvider> dawn_context_provider_;
#endif

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<SharedContextState> context_state_;

  MemoryTypeTracker memory_type_tracker_{nullptr};
  SharedImageManager shared_image_manager_{/*thread_safe=*/false};
  SharedImageRepresentationFactory shared_image_representation_factory_{
      &shared_image_manager_, nullptr};

  // To be initialized by the test implementation.
  std::unique_ptr<SharedImageBackingFactory> backing_factory_;
};

void PrintTo(GrContextType type, std::ostream* os);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_TEST_BASE_H_
