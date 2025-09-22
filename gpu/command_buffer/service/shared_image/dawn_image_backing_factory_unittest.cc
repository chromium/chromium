// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_image_backing_factory.h"

#include <dawn/webgpu_cpp.h>

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/dawn_context_provider.h"
#include "gpu/command_buffer/service/shared_image/dawn_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "ui/gfx/color_space.h"

namespace gpu {

class DawnImageBackingFactoryTest : public SharedImageTestBase {
 public:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGraphiteDawn));
    backing_factory_ = std::make_unique<DawnImageBackingFactory>();

    // Find a Dawn Vulkan adapter
    wgpu::RequestAdapterOptions adapter_options;
    adapter_options.backendType = wgpu::BackendType::Vulkan;
    std::vector<dawn::native::Adapter> adapters =
        dawn_instance_.EnumerateAdapters(&adapter_options);
    ASSERT_GT(adapters.size(), 0u);

    // We need to request internal usage to be able to do operations with
    // internal methods that would need specific usages.
    wgpu::FeatureName dawn_internal_usage =
        wgpu::FeatureName::DawnInternalUsages;
    wgpu::DeviceDescriptor device_descriptor;
    device_descriptor.requiredFeatureCount = 1;
    device_descriptor.requiredFeatures = &dawn_internal_usage;

    dawn_device_ =
        wgpu::Device::Acquire(adapters[0].CreateDevice(&device_descriptor));
    ASSERT_TRUE(dawn_device_) << "Failed to create Dawn device";
  }

  void TearDown() override { dawn_device_ = wgpu::Device(); }

 protected:
  static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
  static constexpr wgpu::InstanceDescriptor dawn_instance_desc_ = {
      .requiredFeatureCount = 1,
      .requiredFeatures = &kTimedWaitAny,
  };
  dawn::native::Instance dawn_instance_ =
      dawn::native::Instance(&dawn_instance_desc_);
  wgpu::Device dawn_device_;
};

#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
TEST_F(DawnImageBackingFactoryTest, MAYBE_Basic) {
  auto mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SharedImageUsageSet usage = SHARED_IMAGE_USAGE_WEBGPU_READ;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGraphiteDawn, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, kNullSurfaceHandle, size, color_space, surface_origin,
      alpha_type, usage, "DawnImageBackingFactoryTest",
      /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  static_cast<DawnImageBacking*>(backing.get())
      ->InitializeForTesting(dawn_device_);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  auto dawn_representation = shared_image_representation_factory_.ProduceDawn(
      mailbox, dawn_device_, wgpu::BackendType::Null, {}, context_state_);
  EXPECT_TRUE(dawn_representation);

  auto scoped_access = dawn_representation->BeginScopedAccess(
      wgpu::TextureUsage::TextureBinding,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  ASSERT_TRUE(scoped_access);
  EXPECT_TRUE(scoped_access->texture());

  scoped_access.reset();
  dawn_representation.reset();
  shared_image.reset();
}

}  // namespace gpu
