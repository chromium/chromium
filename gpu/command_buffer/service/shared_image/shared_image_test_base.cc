// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"

#include <vector>

#include "base/command_line.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_in_process_context_provider.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif

namespace gpu {

// static
SkBitmap SharedImageTestBase::MakeRedBitmap(SkColorType color_type,
                                            const gfx::Size& size,
                                            size_t added_stride) {
  SkImageInfo info = SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kOpaque_SkAlphaType);
  const size_t stride =
      info.minRowBytes() + added_stride * info.bytesPerPixel();

  SkBitmap bitmap;
  bitmap.allocPixels(info, stride);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

// static
std::vector<SkBitmap> SharedImageTestBase::AllocateRedBitmaps(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    size_t added_stride) {
  int num_planes = format.NumberOfPlanes();
  std::vector<SkBitmap> bitmaps(num_planes);

  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType color_type = ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    bitmaps[plane] = MakeRedBitmap(color_type, plane_size, added_stride);
  }
  return bitmaps;
}

// static
std::vector<SkPixmap> SharedImageTestBase::GetSkPixmaps(
    const std::vector<SkBitmap>& bitmaps) {
  std::vector<SkPixmap> pixmaps;
  for (auto& bitmap : bitmaps) {
    pixmaps.push_back(bitmap.pixmap());
  }
  return pixmaps;
}

SharedImageTestBase::SharedImageTestBase() {
  gpu_preferences_.use_passthrough_cmd_decoder =
      gles2::UsePassthroughCommandDecoder(
          base::CommandLine::ForCurrentProcess()) &&
      gles2::PassthroughCommandDecoderSupported();
}

SharedImageTestBase::~SharedImageTestBase() {
  if (context_state_) {
    // |context_state_| must be destroyed while current.
    context_state_->MakeCurrent(gl_surface_.get(), /*needs_gl=*/true);
  }
}

bool SharedImageTestBase::use_passthrough() const {
  return gpu_preferences_.use_passthrough_cmd_decoder;
}

GrDirectContext* SharedImageTestBase::gr_context() {
  return context_state_->gr_context();
}

void SharedImageTestBase::InitializeContext(GrContextType context_type) {
  if (context_type == GrContextType::kVulkan) {
#if BUILDFLAG(ENABLE_VULKAN)
    vulkan_implementation_ = gpu::CreateVulkanImplementation();
    ASSERT_TRUE(vulkan_implementation_);
    ASSERT_TRUE(vulkan_implementation_->InitializeVulkanInstance());
    vulkan_context_provider_ = viz::VulkanInProcessContextProvider::Create(
        vulkan_implementation_.get());
    ASSERT_TRUE(vulkan_context_provider_);
#else
    FAIL() << "Vulkan not available";
#endif
  }

  // Set up a GL context. Even if the GrContext is Vulkan it's still needed.
  gl_surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                                   gfx::Size());
  ASSERT_TRUE(gl_surface_);
  gl_context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                          gl::GLContextAttribs());
  ASSERT_TRUE(gl_context_);
  bool make_current_result = gl_context_->MakeCurrent(gl_surface_.get());
  ASSERT_TRUE(make_current_result);

  context_state_ = base::MakeRefCounted<SharedContextState>(
      base::MakeRefCounted<gl::GLShareGroup>(), gl_surface_, gl_context_,
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(), context_type
#if BUILDFLAG(ENABLE_VULKAN)
      ,
      vulkan_context_provider_.get()
#endif
  );

  bool initialize_gl = context_state_->InitializeGL(
      gpu_preferences_, base::MakeRefCounted<gles2::FeatureInfo>(
                            gpu_workarounds_, GpuFeatureInfo()));
  ASSERT_TRUE(initialize_gl);

  bool initialize_skia =
      context_state_->InitializeSkia(gpu_preferences_, gpu_workarounds_);
  ASSERT_TRUE(initialize_skia);
}

void SharedImageTestBase::VerifyPixelsWithReadback(
    const Mailbox& mailbox,
    const std::vector<SkBitmap>& expected_bitmaps) {
  // Create Skia representation to readback from.
  auto skia_representation =
      shared_image_representation_factory_.ProduceSkia(mailbox, context_state_);
  ASSERT_TRUE(skia_representation);

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  ASSERT_TRUE(scoped_read_access);

  // If this function is used with a backing that produces semaphores or end
  // state then code here needs to be updated to handle them.
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());

  viz::SharedImageFormat format = skia_representation->format();
  gfx::Size size = skia_representation->size();

  int num_planes = format.NumberOfPlanes();
  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType plane_color_type =
        viz::ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    SkImageInfo dst_info =
        SkImageInfo::Make(plane_size.width(), plane_size.height(),
                          plane_color_type, kOpaque_SkAlphaType);
    SkBitmap dst_bitmap;
    dst_bitmap.allocPixels(dst_info);

    auto* promise_texture = scoped_read_access->promise_image_texture(plane);
    ASSERT_TRUE(promise_texture) << "plane_index=" << plane;

    auto sk_image = SkImages::BorrowTextureFrom(
        gr_context(), promise_texture->backendTexture(),
        skia_representation->surface_origin(), plane_color_type,
        skia_representation->alpha_type(), nullptr);
    ASSERT_TRUE(sk_image) << "plane_index=" << plane;

    ASSERT_TRUE(sk_image->readPixels(dst_info, dst_bitmap.getPixels(),
                                     dst_bitmap.rowBytes(), /*srcX=*/0,
                                     /*srcY=*/0))
        << "plane_index=" << plane;

    EXPECT_TRUE(cc::MatchesBitmap(dst_bitmap, expected_bitmaps[plane],
                                  cc::ExactPixelComparator()))
        << "plane_index=" << plane;
  }
  scoped_read_access->ApplyBackendSurfaceEndState();
}

}  // namespace gpu
