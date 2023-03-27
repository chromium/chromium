// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"

#include "base/command_line.h"
#include "gpu/command_buffer/service/service_utils.h"
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
                                            const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kOpaque_SkAlphaType));

  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
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
      /*use_virtualized_gl_contexts=*/false, base::DoNothing(), context_type,
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_context_provider_.get()
#endif
  );

  bool initialize_gl = context_state_->InitializeGL(
      gpu_preferences_, base::MakeRefCounted<gles2::FeatureInfo>(
                            gpu_workarounds_, GpuFeatureInfo()));
  ASSERT_TRUE(initialize_gl);

  bool initialize_gr = context_state_->InitializeGrContext(
      gpu_preferences_, gpu_workarounds_, nullptr);
  ASSERT_TRUE(initialize_gr);
}

}  // namespace gpu
