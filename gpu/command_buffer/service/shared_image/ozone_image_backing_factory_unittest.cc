// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"

#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/gl_ozone_image_representation.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_gl_textures_holder.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {

namespace {

class FakeOnScreenSurface : public gl::SurfacelessEGL {
 public:
  FakeOnScreenSurface(gl::GLDisplayEGL* display, const gfx::Size& size)
      : gl::SurfacelessEGL(display, size) {}

  // gl::GLSurface:
  bool IsOffscreen() override { return false; }
  bool IsSurfaceless() const override { return false; }

 protected:
  ~FakeOnScreenSurface() override = default;
};

}  // namespace

class OzoneImageBackingFactoryTest : public SharedImageTestBase {
 public:
  OzoneImageBackingFactoryTest() = default;
  ~OzoneImageBackingFactoryTest() override = default;

  void SetUp() override {
    if (!base::FeatureList::IsEnabled(
            features::kEnablePerContextGLTextureCache)) {
      GTEST_SKIP();
    }

    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));

    backing_factory_ = std::make_unique<OzoneImageBackingFactory>(
        context_state_.get(), gpu_workarounds_, gpu_preferences_);

    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

 protected:
  bool IsEglImageSupported() const {
    bool result =
        context_state_->MakeCurrent(gl_surface_.get(), /*needs_gl=*/true);
    DCHECK(result);

    // Check the required extensions to support egl images.
    auto* egl_display = gl::GetDefaultDisplayEGL();
    if (egl_display && egl_display->ext->b_EGL_KHR_image_base &&
        egl_display->ext->b_EGL_KHR_gl_texture_2D_image &&
        gl::g_current_gl_driver->ext.b_GL_OES_EGL_image) {
      return true;
    }
    return false;
  }

  MemoryTypeTracker memory_type_tracker_{nullptr};
  SharedImageManager shared_image_manager_;

  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  std::unique_ptr<OzoneImageBackingFactory> backing_factory_;
};

TEST_F(OzoneImageBackingFactoryTest, UsesCacheForTextureHolders) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto* backing_ptr = static_cast<OzoneImageBacking*>(backing.get());

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  // Create and validate GLTexture representation.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);
  EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());

  // Verify there is only one per-context textures holder now.
  EXPECT_EQ(1u, backing_ptr->per_context_cached_textures_holders_.size());
  // Verify the context key is the current context.
  auto& cached_textures_holdes =
      *backing_ptr->per_context_cached_textures_holders_.begin();
  EXPECT_EQ(context_state_->context(), cached_textures_holdes.first);

  auto gl_representation2 =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation2);
  EXPECT_TRUE(gl_representation2->GetTexturePassthrough()->service_id());

  // Verify there is still only one per-context textures holder now.
  EXPECT_EQ(1u, backing_ptr->per_context_cached_textures_holders_.size());

  // Both representations must have the same texture id as OzoneImageBacking
  // holds a cache for GLTexture(Passthrough)OzoneImageRepresentations'
  // TextureHolder (though, if the context is different, the service_id will
  // repeat).
  EXPECT_EQ(gl_representation->GetTexturePassthrough()->service_id(),
            gl_representation2->GetTexturePassthrough()->service_id());

  auto gl_context = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                              gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  bool make_current_result = gl_context->MakeCurrent(gl_surface_.get());
  ASSERT_TRUE(make_current_result);

  auto gl_representation3 =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation3);

  // Verify there is now two per-context textures holders.
  EXPECT_EQ(2u, backing_ptr->per_context_cached_textures_holders_.size());

  auto gl_representation4 =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation4);
  EXPECT_TRUE(gl_representation4->GetTexturePassthrough()->service_id());

  // Both representations must share the same texture id.
  EXPECT_EQ(gl_representation3->GetTexturePassthrough()->service_id(),
            gl_representation4->GetTexturePassthrough()->service_id());

  // Cannot compare service_ids of the |gl_representation3/4| with
  // |gl_representation1/2| as they will be the same as the first ones because
  // these representations' texture was created for a different context.
}

// Verifies that the cache is not used for onscreen surfaces.
TEST_F(OzoneImageBackingFactoryTest, UsesCacheForTextureHolders2) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto* backing_ptr = static_cast<OzoneImageBacking*>(backing.get());

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  // Create and validate GLTexture representation.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);
  EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());

  // Verify there is only one per-context textures holder now.
  EXPECT_EQ(1u, backing_ptr->per_context_cached_textures_holders_.size());
  // Verify the context key is the current context.
  auto& cached_textures_holdes =
      *backing_ptr->per_context_cached_textures_holders_.begin();
  EXPECT_EQ(context_state_->context(), cached_textures_holdes.first);

  scoped_refptr<gl::GLSurface> fake_onscreen_gl_surface(
      new FakeOnScreenSurface(gl::GetDefaultDisplayEGL(), {100, 100}));
  ASSERT_FALSE(fake_onscreen_gl_surface->IsOffscreen());

  scoped_refptr<gl::GLContext> gl_context = gl::init::CreateGLContext(
      nullptr, fake_onscreen_gl_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  ASSERT_FALSE(gl_context->default_surface());
  bool make_current_result =
      gl_context->MakeCurrent(fake_onscreen_gl_surface.get());
  ASSERT_TRUE(make_current_result);

  auto gl_representation2 =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation2);

  // Verify there is still one per-context textures holders as the last current
  // context was created for an onscreen surface.
  EXPECT_EQ(1u, backing_ptr->per_context_cached_textures_holders_.size());
}

TEST_F(OzoneImageBackingFactoryTest, MarksContextLostOnContextLost) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto* backing_ptr = static_cast<OzoneImageBacking*>(backing.get());

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  // Create another context and produce a glTexture. if the context is marked as
  // lost, the image must notify the texture holders as well.
  auto gl_context = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                              gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  bool make_current_result = gl_context->MakeCurrent(gl_surface_.get());
  ASSERT_TRUE(make_current_result);

  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);

  // Make own reference of the texture holders as the SI will remove its
  // reference.
  auto it =
      backing_ptr->per_context_cached_textures_holders_.find(gl_context.get());
  ASSERT_TRUE(it != backing_ptr->per_context_cached_textures_holders_.end());
  auto textures_holder_ref = it->second;

  backing_ptr->OnGLContextLost(gl_context.get());

  ASSERT_TRUE(backing_ptr->per_context_cached_textures_holders_.empty());

  EXPECT_TRUE(textures_holder_ref->WasContextLost());

  // The holder must have already been marked as a context lost. However, the
  // representation should be marked as a context lost as well so that it can
  // exercise the DCHECK that verifies the texture holders have already been
  // marked as context lost.
  gl_representation->OnContextLost();
  gl_representation.reset();

  // Manually destroy the glTexture to avoid leaking it.
  EXPECT_EQ(1u, textures_holder_ref->GetNumberOfTextures());
  const GLuint service_id =
      textures_holder_ref->texture_passthrough(/*plane_index=*/0)->service_id();
  glDeleteTextures(1, &service_id);
}

// Same as above, but with an onscreen surface.
TEST_F(OzoneImageBackingFactoryTest, MarksContextLostOnContextLost2) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto* backing_ptr = static_cast<OzoneImageBacking*>(backing.get());

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  scoped_refptr<gl::GLSurface> fake_onscreen_gl_surface(
      new FakeOnScreenSurface(gl::GetDefaultDisplayEGL(), {100, 100}));
  ASSERT_FALSE(fake_onscreen_gl_surface->IsOffscreen());

  scoped_refptr<gl::GLContext> gl_context = gl::init::CreateGLContext(
      nullptr, fake_onscreen_gl_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  ASSERT_FALSE(gl_context->default_surface());
  bool make_current_result =
      gl_context->MakeCurrent(fake_onscreen_gl_surface.get());
  ASSERT_TRUE(make_current_result);

  {
    // gles2::Texture
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(0u, backing_ptr->per_context_cached_textures_holders_.size());

    auto* ozone_reprensentation =
        static_cast<GLTextureOzoneImageRepresentation*>(
            gl_representation.get());
    auto textures_holder_ref = ozone_reprensentation->textures_holder_;

    gl_representation->OnContextLost();
    gl_representation.reset();

    EXPECT_TRUE(textures_holder_ref->WasContextLost());

    // Manually destroy the glTexture to avoid leaking it.
    EXPECT_EQ(1u, textures_holder_ref->GetNumberOfTextures());
    const GLuint service_id =
        textures_holder_ref->texture(/*plane_index=*/0)->service_id();
    glDeleteTextures(1, &service_id);
  }

  {
    // gles2::TexturePassthrough
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(0u, backing_ptr->per_context_cached_textures_holders_.size());

    auto* ozone_reprensentation =
        static_cast<GLTexturePassthroughOzoneImageRepresentation*>(
            gl_representation.get());
    auto textures_holder_ref = ozone_reprensentation->textures_holder_;

    gl_representation->OnContextLost();
    gl_representation.reset();

    EXPECT_TRUE(textures_holder_ref->WasContextLost());

    // Manually destroy the glTexture to avoid leaking it.
    EXPECT_EQ(1u, textures_holder_ref->GetNumberOfTextures());
    const GLuint service_id =
        textures_holder_ref->texture_passthrough(/*plane_index=*/0)
            ->service_id();
    glDeleteTextures(1, &service_id);
  }
}

TEST_F(OzoneImageBackingFactoryTest, RemovesTextureHoldersOnContextDestroy) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto* backing_ptr = static_cast<OzoneImageBacking*>(backing.get());

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  auto gl_context = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                              gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  bool make_current_result = gl_context->MakeCurrent(gl_surface_.get());
  ASSERT_TRUE(make_current_result);

  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);

  // Remove the reference here so that when SI removes its own, the texture is
  // actually safely destroyed.
  gl_representation.reset();

  gl_context.reset();

  ASSERT_TRUE(backing_ptr->per_context_cached_textures_holders_.empty());
}

// If textures are created for different contexts, the SI must restore a
// previous current context upon destruction a texture from a different context.
TEST_F(OzoneImageBackingFactoryTest, RestoresContextOnAnotherContextDestroy) {
  if (!IsEglImageSupported()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  const Mailbox mailbox = Mailbox::GenerateForSharedImage();
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gpu::kNullSurfaceHandle,
      {100, 100}, gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2, "TestLabel", false);
  EXPECT_TRUE(backing);

  auto shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);

  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation);

  auto gl_context = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                              gl::GLContextAttribs());
  ASSERT_TRUE(gl_context);
  EXPECT_TRUE(gl_context->MakeCurrent(gl_surface_.get()));

  auto gl_representation2 =
      shared_image_representation_factory_->ProduceGLTexturePassthrough(
          mailbox);
  EXPECT_TRUE(gl_representation2);

  EXPECT_TRUE(context_state_->MakeCurrent(context_state_->surface(),
                                          true /* needs_gl*/));

  // Remove the reference here so that when SI removes its own, the texture is
  // actually safely destroyed.
  gl_representation2.reset();
  gl_context.reset();

  EXPECT_TRUE(context_state_->context()->IsCurrent(gl_surface_.get()));
}

}  // namespace gpu
