// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include "base/run_loop.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/test/fake_web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_memory_buffer_test_platform.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/test_webgraphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

namespace {

class AcceleratedCompositingTestPlatform
    : public blink::TestingPlatformSupport {
 public:
  bool IsGpuCompositingDisabled() const override { return false; }
};

}  // namespace

TEST(CanvasResourceTest, PrepareTransferableResource_Software) {
  test::TaskEnvironment task_environment;
  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      test_web_shared_image_interface_provider =
          TestWebGraphicsSharedImageInterfaceProvider::Create();
  auto shared_image_interface_provider =
      test_web_shared_image_interface_provider->GetWeakPtr();

  scoped_refptr<CanvasResource> canvas_resource =
      CanvasResourceSharedImage::CreateSoftware(
          gfx::Size(10, 10), viz::SinglePlaneFormat::kBGRA_8888,
          kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(),
          /*CanvasResourceProvider=*/nullptr, shared_image_interface_provider);
  EXPECT_TRUE(!!canvas_resource);
  viz::TransferableResource resource;
  CanvasResource::ReleaseCallback release_callback;
  bool success = canvas_resource->PrepareTransferableResource(
      &resource, &release_callback, /*needs_verified_synctoken=*/false);

  EXPECT_TRUE(success);
  EXPECT_TRUE(resource.GetIsSoftware());

  std::move(release_callback)
      .Run(std::move(canvas_resource), gpu::SyncToken(), false);
}

TEST(CanvasResourceTest, PrepareTransferableResource_PreservesAlphaType) {
  test::TaskEnvironment task_environment;
  auto accelerated_compositing_platform = std::make_unique<
      ScopedTestingPlatformSupport<AcceleratedCompositingTestPlatform>>();
  auto test_context_provider = viz::TestContextProvider::CreateRaster();
  InitializeSharedGpuContextRaster(test_context_provider.get());

  viz::TransferableResource resource;
  CanvasResource::ReleaseCallback release_callback;

  scoped_refptr<CanvasResource> premul_canvas_resource =
      CanvasResourceSharedImage::Create(
          gfx::Size(10, 10), viz::SinglePlaneFormat::kRGBA_8888,
          kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(),
          SharedGpuContext::ContextProviderWrapper(),
          /*CanvasResourceProvider=*/nullptr,
          /*is_accelerated=*/false, gpu::SHARED_IMAGE_USAGE_DISPLAY_READ);

  ASSERT_TRUE(premul_canvas_resource->PrepareTransferableResource(
      &resource, &release_callback, /*needs_verified_synctoken=*/false));
  EXPECT_EQ(resource.GetAlphaType(), kPremul_SkAlphaType);

  scoped_refptr<CanvasResource> unpremul_canvas_resource =
      CanvasResourceSharedImage::Create(
          gfx::Size(10, 10), viz::SinglePlaneFormat::kRGBA_8888,
          kUnpremul_SkAlphaType, gfx::ColorSpace::CreateSRGB(),
          SharedGpuContext::ContextProviderWrapper(),
          /*CanvasResourceProvider=*/nullptr,
          /*is_accelerated=*/false,
          gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
              gpu::SHARED_IMAGE_USAGE_RASTER_WRITE);

  ASSERT_TRUE(unpremul_canvas_resource->PrepareTransferableResource(
      &resource, &release_callback, /*needs_verified_synctoken=*/false));
  EXPECT_EQ(resource.GetAlphaType(), kUnpremul_SkAlphaType);

  // InitializeSharedGpuContextRaster() requires SharedGpuContext::Reset()
  // at TearDown().
  SharedGpuContext::Reset();
}

}  // namespace blink
