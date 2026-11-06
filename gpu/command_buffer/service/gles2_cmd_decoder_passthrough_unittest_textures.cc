// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_gl_framebuffer.h"

namespace gpu {
namespace gles2 {
namespace {

template <typename BackingType = TestImageBacking>
std::unique_ptr<BackingType> AllocateTextureAndCreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage) {
  GLuint service_id;
  glGenTextures(1, &service_id);
  glBindTexture(GL_TEXTURE_2D, service_id);
  GLFormatDesc format_desc =
      GLFormatCaps().ToGLFormatDesc(format, /*plane_index=*/0);
  glTexImage2D(GL_TEXTURE_2D, 0, format_desc.image_internal_format,
               size.width(), size.height(), 0, format_desc.data_format,
               format_desc.data_type, nullptr /* data */);
  return std::make_unique<BackingType>(
      mailbox,
      SharedImageInfo(format, size, color_space, surface_origin, alpha_type,
                      usage, "TestLabel"),
      0 /* estimated_size */, service_id);
}

// Representation which raises a GL error when access begins.
class GLErrorOnAccessRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLErrorOnAccessRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::unique_ptr<GLTexturePassthroughImageRepresentation> inner)
      : GLTexturePassthroughImageRepresentation(manager, backing, tracker),
        inner_(std::move(inner)) {}

  const scoped_refptr<TexturePassthrough>& GetTexturePassthrough(
      size_t plane_index) override {
    return inner_->GetTexturePassthrough(plane_index);
  }
  bool BeginAccess(GLenum mode) override {
    inner_access_ = inner_->BeginScopedAccess(
        mode, SharedImageRepresentation::AllowUnclearedAccess::kYes);
    if (!inner_access_) {
      return false;
    }
    glEnable(GL_NONE);
    return true;
  }
  void EndAccess() override { inner_access_.reset(); }

 private:
  std::unique_ptr<GLTexturePassthroughImageRepresentation> inner_;
  std::unique_ptr<GLTexturePassthroughImageRepresentation::ScopedAccess>
      inner_access_;
};

class GLErrorOnAccessTestImageBacking : public TestImageBacking {
 public:
  using TestImageBacking::TestImageBacking;

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    return std::make_unique<GLErrorOnAccessRepresentation>(
        manager, this, tracker,
        TestImageBacking::ProduceGLTexturePassthrough(manager, tracker));
  }
};

}  // namespace

TEST_F(GLES2DecoderPassthroughTest, CreateAndTexStorage2DSharedImageCHROMIUM) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  auto backing = AllocateTextureAndCreateSharedImage(
      mailbox, format, gfx::Size(10, 10), gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE});
  GLuint service_id = backing->service_id();
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(backing), &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Make sure the new client ID is associated with the provided service ID.
  uint32_t found_service_id = 0;
  EXPECT_TRUE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_EQ(found_service_id, service_id);
  scoped_refptr<TexturePassthrough> found_texture_passthrough;
  EXPECT_TRUE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kNewClientId, &found_texture_passthrough));
  EXPECT_EQ(found_texture_passthrough->service_id(), service_id);
  found_texture_passthrough.reset();
  EXPECT_EQ(1u, GetPassthroughResources()->texture_shared_image_map.count(
                    kNewClientId));

  // Delete the texture and make sure it is no longer accessible.
  DoDeleteTexture(kNewClientId);
  EXPECT_FALSE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_FALSE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kNewClientId, &found_texture_passthrough));
  EXPECT_EQ(0u, GetPassthroughResources()->texture_shared_image_map.count(
                    kNewClientId));

  shared_image.reset();
}

// The lazy clear of an uncleared shared image succeeds without errors.
TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMClearsImage) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto backing = AllocateTextureAndCreateSharedImage(
      mailbox, viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(10, 10),
      gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE});
  TestImageBacking* backing_ptr = backing.get();
  EXPECT_FALSE(backing_ptr->IsCleared());
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(backing), &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_TRUE(backing_ptr->IsCleared());

  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

// A GL error raised during the lazy clear of a shared image must leave the
// image uncleared, and the error must still be reported to the client.
TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMClearError) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto backing =
      AllocateTextureAndCreateSharedImage<GLErrorOnAccessTestImageBacking>(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(10, 10),
          gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
          {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE});
  TestImageBacking* backing_ptr = backing.get();
  EXPECT_FALSE(backing_ptr->IsCleared());
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(backing), &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
  EXPECT_FALSE(backing_ptr->IsCleared());

  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMInvalidMailbox) {
  // Attempt to use an invalid mailbox.
  Mailbox mailbox;
  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));

  // CreateAndTexStorage2DSharedImage should fail if the mailbox is invalid.
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Make sure the new client_id is associated with a texture id, even though
  // the command failed.
  uint32_t found_service_id = 0;
  EXPECT_TRUE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_NE(0u, found_service_id);
}

TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMPreexistingTexture) {
  MemoryTypeTracker memory_tracker(nullptr);
  // Create a texture with kNewClientId.
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          AllocateTextureAndCreateSharedImage(
              mailbox, format, gfx::Size(10, 10), gfx::ColorSpace(),
              kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
              {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE}),
          &memory_tracker);

  {
    auto& cmd = *GetImmediateAs<
        cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, mailbox.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Try to import the SharedImage a second time at the same client ID. We
  // should get a GL failure.
  {
    auto& cmd = *GetImmediateAs<
        cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, mailbox.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }

  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest, BeginEndSharedImageAccessCHROMIUM) {
  MemoryTypeTracker memory_tracker(nullptr);
  std::vector<std::unique_ptr<SharedImageRepresentationFactoryRef>>
      shared_images;
  for (int i = 0; i < 40; i++) {
    Mailbox mailbox = Mailbox::Generate();
    auto format = viz::SinglePlaneFormat::kRGBA_8888;
    std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
        GetSharedImageManager()->Register(
            AllocateTextureAndCreateSharedImage(
                mailbox, format, gfx::Size(10, 10), gfx::ColorSpace(),
                kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
                {SHARED_IMAGE_USAGE_GLES2_READ,
                 SHARED_IMAGE_USAGE_GLES2_WRITE}),
            &memory_tracker);
    shared_images.emplace_back(std::move(shared_image));

    auto& cmd = *GetImmediateAs<
        cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    auto client_id = kNewClientId + i;
    cmd.Init(client_id, mailbox.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // Begin/end read access for the created image.
    cmds::BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
    read_access_cmd.Init(client_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    cmds::EndSharedImageAccessDirectCHROMIUM read_end_cmd;
    read_end_cmd.Init(client_id);
    EXPECT_EQ(error::kNoError, ExecuteCmd(read_end_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    // Begin/end read/write access for the created image.
    cmds::BeginSharedImageAccessDirectCHROMIUM readwrite_access_cmd;
    readwrite_access_cmd.Init(client_id,
                              GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_access_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    cmds::EndSharedImageAccessDirectCHROMIUM readwrite_end_cmd;
    readwrite_end_cmd.Init(client_id);
    EXPECT_EQ(error::kNoError, ExecuteCmd(readwrite_end_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());

    DoDeleteTexture(client_id);
  }

  // Cleanup
  shared_images.clear();
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMInvalidMode) {
  // Try to begin access with an invalid mode.
  cmds::BeginSharedImageAccessDirectCHROMIUM bad_mode_access_cmd;
  bad_mode_access_cmd.Init(kClientTextureId, 0);
  EXPECT_EQ(error::kNoError, ExecuteCmd(bad_mode_access_cmd));
  EXPECT_EQ(GL_INVALID_ENUM, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to begin access with a texture that is not a shared image.
  cmds::BeginSharedImageAccessDirectCHROMIUM not_shared_image_access_cmd;
  not_shared_image_access_cmd.Init(
      kClientTextureId, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMCantBeginAccess) {
  // Create a shared image.
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  auto shared_image_backing = AllocateTextureAndCreateSharedImage(
      mailbox, format, gfx::Size(10, 10), gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      {SHARED_IMAGE_USAGE_GLES2_READ});
  // Set the shared image to fail BeginAccess.
  shared_image_backing->set_can_access(false);
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(shared_image_backing),
                                        &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Try to begin access with a shared image representation that fails
  // BeginAccess.
  cmds::BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
  read_access_cmd.Init(kNewClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // Cleanup
  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

TEST_F(GLES2DecoderPassthroughTest,
       EndSharedImageAccessDirectCHROMIUMNotSharedImage) {
  // Try to end access with a texture that is not a shared image.
  cmds::EndSharedImageAccessDirectCHROMIUM not_shared_image_end_cmd;
  not_shared_image_end_cmd.Init(kClientTextureId);
  EXPECT_EQ(error::kNoError, ExecuteCmd(not_shared_image_end_cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
}

TEST_F(GLES2DecoderPassthroughTest,
       BeginSharedImageAccessDirectCHROMIUMClearUncleared) {
  // Create an uncleared shared image.
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(
          AllocateTextureAndCreateSharedImage(
              mailbox, format, gfx::Size(10, 10), gfx::ColorSpace(),
              kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
              {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE}),
          &memory_tracker);

  // Backing should be initially uncleared.
  EXPECT_FALSE(shared_image->IsCleared());

  // Set various pieces of state to ensure the texture clear correctly restores
  // them.
  GLboolean color_mask[4] = {true, false, false, true};
  glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
  GLfloat clear_color[4] = {0.5f, 0.7f, 0.3f, 0.8f};
  glClearColor(clear_color[0], clear_color[1], clear_color[2], clear_color[3]);
  gl::ScopedGLFramebuffer dummy_fbo =
      gl::CreateScopedGLFramebuffer(gl::g_current_gl_context);
  glBindFramebufferEXT(GL_FRAMEBUFFER, dummy_fbo.get());
  GLuint dummy_texture;
  glGenTextures(1, &dummy_texture);
  glBindTexture(GL_TEXTURE_2D, dummy_texture);
  glEnable(GL_SCISSOR_TEST);

  // Create the texture from the SharedImage and Begin access. We should clear
  // the backing.
  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  {
    cmds::BeginSharedImageAccessDirectCHROMIUM read_access_cmd;
    read_access_cmd.Init(kNewClientId,
                         GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    EXPECT_EQ(error::kNoError, ExecuteCmd(read_access_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    EXPECT_TRUE(shared_image->IsCleared());
  }

  // Our state should not be modified.
  GLboolean test_color_mask[4];
  glGetBooleanv(GL_COLOR_WRITEMASK, test_color_mask);
  UNSAFE_TODO(EXPECT_TRUE(
      0 == memcmp(test_color_mask, color_mask, sizeof(test_color_mask))));
  GLfloat test_clear_color[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, test_clear_color);
  UNSAFE_TODO(EXPECT_TRUE(
      0 == memcmp(test_clear_color, clear_color, sizeof(test_clear_color))));
  GLint test_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &test_fbo);
  EXPECT_EQ(test_fbo, static_cast<GLint>(dummy_fbo.get()));
  GLint test_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &test_texture);
  EXPECT_EQ(test_texture, static_cast<GLint>(dummy_texture));
  GLboolean test_scissor;
  glGetBooleanv(GL_SCISSOR_TEST, &test_scissor);
  EXPECT_TRUE(test_scissor);

  // End access.
  {
    cmds::EndSharedImageAccessDirectCHROMIUM end_access_cmd;
    end_access_cmd.Init(kNewClientId);
    EXPECT_EQ(error::kNoError, ExecuteCmd(end_access_cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Cleanup
  glDeleteTextures(1, &dummy_texture);
  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

// With check_graphics_reset_status_after_readback enabled and no reset
// reported, the shared-image readback path must behave exactly as without the
// workaround: no context loss, no denial. The rejection leg requires a driver
// that latches a reset and is exercised on-device; ANGLE's NULL backend cannot
// forge one.
class GLES2DecoderPassthroughReadbackResetCheckTest
    : public GLES2DecoderPassthroughTest {
 public:
  GLES2DecoderPassthroughReadbackResetCheckTest() {
    workarounds_for_test_.check_graphics_reset_status_after_readback = true;
  }
};

// Tests that the shared-image ReadbackARGB path does not deny on a healthy
// driver with the workaround enabled; the gate polls the lazy shared
// context's reset status.
TEST_F(GLES2DecoderPassthroughReadbackResetCheckTest,
       ReadbackARGBImagePixelsWithResetCheckWorkaroundDoesNotDeny) {
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto shared_image = GetSharedImageManager()->Register(
      AllocateTextureAndCreateSharedImage(
          mailbox, viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(4, 4),
          gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType, SHARED_IMAGE_USAGE_GLES2_READ),
      &memory_tracker);

  // Shm layout at kSharedMemoryOffset: Result, then the mailbox, then the
  // pixel destination. color_space_offset == mailbox_offset encodes "no
  // color space".
  using Cmd = cmds::ReadbackARGBImagePixelsINTERNAL;
  constexpr uint32_t kMailboxOffset = 16;
  constexpr uint32_t kPixelsOffset = 32;
  constexpr GLuint kDstWidth = 2;
  constexpr GLuint kDstHeight = 2;
  constexpr GLuint kRowBytes = 8;

  auto* result = GetSharedMemoryAs<Cmd::Result*>();
  *result = 0;
  // SAFETY: shared memory extends past kMailboxOffset by a full mailbox.
  UNSAFE_BUFFERS(base::span(GetSharedMemoryAs<uint8_t*>() + kMailboxOffset,
                            sizeof(mailbox.name)))
      .copy_from(base::as_byte_span(mailbox.name));

  Cmd cmd;
  cmd.Init(/*src_x=*/0, /*src_y=*/0, /*plane_index=*/0, kDstWidth, kDstHeight,
           kRowBytes, kRGBA_8888_SkColorType, kPremul_SkAlphaType,
           shared_memory_id_, kSharedMemoryOffset,
           /*color_space_offset=*/kMailboxOffset, kPixelsOffset,
           kMailboxOffset);

  // The gate must not deny on a healthy (NULL-backend) driver: the command
  // may fail for unrelated backend reasons, but never with kLostContext,
  // and the context must survive.
  error::Error err = ExecuteCmd(cmd);
  EXPECT_NE(error::kLostContext, err);
  EXPECT_FALSE(GetDecoder()->WasContextLost());

  shared_image.reset();
}

namespace {

// A pbuffer surface whose OnMakeCurrent() can be made to fail, causing
// GLContext::MakeCurrent() with this surface to fail.
class FailableMakeCurrentSurface : public gl::PbufferGLSurfaceEGL {
 public:
  FailableMakeCurrentSurface()
      : gl::PbufferGLSurfaceEGL(gl::GLSurfaceEGL::GetGLDisplayEGL(),
                                gfx::Size(4, 4)) {}

  void set_fail_make_current(bool fail) { fail_make_current_ = fail; }

  bool OnMakeCurrent(gl::GLContext* context) override {
    return !fail_make_current_ &&
           gl::PbufferGLSurfaceEGL::OnMakeCurrent(context);
  }

 private:
  ~FailableMakeCurrentSurface() override = default;

  bool fail_make_current_ = false;
};

// A TestImageBacking that runs `callback` when a Skia representation is
// requested, then fails to produce one.
class CallbackOnProduceSkiaTestImageBacking : public TestImageBacking {
 public:
  CallbackOnProduceSkiaTestImageBacking(const Mailbox& mailbox,
                                        const SharedImageInfo& si_info,
                                        base::OnceClosure callback)
      : TestImageBacking(mailbox, si_info, /*estimated_size=*/0),
        callback_(std::move(callback)) {}

 protected:
  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override {
    if (callback_) {
      std::move(callback_).Run();
    }
    return nullptr;
  }

 private:
  base::OnceClosure callback_;
};

SharedImageInfo MakeTestSharedImageInfo() {
  return SharedImageInfo(
      viz::SinglePlaneFormat::kRGBA_8888, gfx::Size(10, 10), gfx::ColorSpace(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE},
      "TestLabel");
}

}  // namespace

// Commands that temporarily switch to the decoder's shared context must leave
// the decoder's context current afterwards.
TEST_F(GLES2DecoderPassthroughTest, SharedContextCommandRestoresContext) {
  gl::GLContext* decoder_context = gl::GLContext::GetCurrent();
  ASSERT_TRUE(decoder_context);

  // Unknown mailboxes; the copy fails, but only after switching contexts.
  Mailbox mailboxes[2] = {Mailbox::Generate(), Mailbox::Generate()};
  auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
  cmd.Init(0, 0, 0, 0, 1, 1, reinterpret_cast<const GLbyte*>(mailboxes));
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  EXPECT_NE(static_cast<GLenum>(GL_NO_ERROR),
            static_cast<GLenum>(GetGLError()));

  EXPECT_FALSE(GetDecoder()->WasContextLost());
  EXPECT_EQ(decoder_context, gl::GLContext::GetCurrent());
  EXPECT_TRUE(decoder_context->IsCurrent(nullptr));
}

// If the decoder's context can't be made current again after a command used
// the shared context, the decoder must lose its context.
TEST_F(GLES2DecoderPassthroughTest, ContextLostWhenSharedContextRestoreFails) {
  scoped_refptr<gl::GLContext> decoder_context = gl::GLContext::GetCurrent();
  scoped_refptr<gl::GLSurface> decoder_surface = gl::GLSurface::GetCurrent();
  ASSERT_TRUE(decoder_context);
  ASSERT_TRUE(decoder_surface);

  // Make the decoder's context current on a surface that can fail
  // OnMakeCurrent(), so the restore at the end of the command can be made to
  // fail.
  auto failable_surface = base::MakeRefCounted<FailableMakeCurrentSurface>();
  ASSERT_TRUE(failable_surface->Initialize(gl::GLSurfaceFormat()));
  decoder_context->ReleaseCurrent(decoder_surface.get());
  ASSERT_TRUE(decoder_context->MakeCurrent(failable_surface.get()));
  ASSERT_EQ(failable_surface.get(), gl::GLSurface::GetCurrent());

  bool produce_skia_called = false;
  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox src_mailbox = Mailbox::Generate();
  Mailbox dst_mailbox = Mailbox::Generate();
  std::unique_ptr<SharedImageRepresentationFactoryRef> dst_shared_image =
      GetSharedImageManager()->Register(
          std::make_unique<CallbackOnProduceSkiaTestImageBacking>(
              dst_mailbox, MakeTestSharedImageInfo(),
              base::BindLambdaForTesting([&]() {
                produce_skia_called = true;
                failable_surface->set_fail_make_current(true);
              })),
          &memory_tracker);

  Mailbox mailboxes[2] = {src_mailbox, dst_mailbox};
  auto& cmd = *GetImmediateAs<cmds::CopySharedImageINTERNALImmediate>();
  cmd.Init(0, 0, 0, 0, 1, 1, reinterpret_cast<const GLbyte*>(mailboxes));
  EXPECT_EQ(error::kLostContext, ExecuteImmediateCmd(cmd, sizeof(mailboxes)));
  EXPECT_TRUE(produce_skia_called);
  EXPECT_TRUE(GetDecoder()->WasContextLost());

  // Restore the decoder's original context and surface for teardown.
  failable_surface->set_fail_make_current(false);
  ASSERT_TRUE(decoder_context->MakeCurrent(decoder_surface.get()));
  dst_shared_image.reset();
}

namespace {

// A TestImageBacking that makes the given GL context and surface current
// while producing a GLTexturePassthrough representation.
class ContextSwitchingTestImageBacking : public TestImageBacking {
 public:
  ContextSwitchingTestImageBacking(const Mailbox& mailbox,
                                   const SharedImageInfo& si_info,
                                   GLuint texture_id,
                                   scoped_refptr<gl::GLContext> context,
                                   scoped_refptr<gl::GLSurface> surface)
      : TestImageBacking(mailbox, si_info, /*estimated_size=*/0, texture_id),
        context_(std::move(context)),
        surface_(std::move(surface)) {}

 protected:
  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override {
    auto representation =
        TestImageBacking::ProduceGLTexturePassthrough(manager, tracker);
    EXPECT_TRUE(context_->MakeCurrent(surface_.get()));
    return representation;
  }

 private:
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;
};

}  // namespace

// If a command leaves a GL context other than the decoder's current, the
// decoder must lose its context rather than continue on the wrong context.
TEST_F(GLES2DecoderPassthroughTest,
       ContextLostWhenCommandLeavesOtherContextCurrent) {
  scoped_refptr<gl::GLContext> decoder_context = gl::GLContext::GetCurrent();
  scoped_refptr<gl::GLSurface> decoder_surface = gl::GLSurface::GetCurrent();
  ASSERT_TRUE(decoder_context);
  ASSERT_TRUE(decoder_surface);

  scoped_refptr<gl::GLContext> other_context = gl::init::CreateGLContext(
      nullptr, decoder_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(other_context);

  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(10, 10);
  GLuint service_id;
  glGenTextures(1, &service_id);
  glBindTexture(GL_TEXTURE_2D, service_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  auto backing = std::make_unique<ContextSwitchingTestImageBacking>(
      mailbox,
      SharedImageInfo(
          format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType,
          {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE},
          "TestLabel"),
      service_id, other_context, decoder_surface);
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(backing), &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kLostContext,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_TRUE(GetDecoder()->WasContextLost());

  // Restore the decoder's context for teardown.
  ASSERT_TRUE(decoder_context->MakeCurrent(decoder_surface.get()));
  shared_image.reset();
}

// If a command leaves the decoder's context current but with a different
// surface, the decoder must lose its context.
TEST_F(GLES2DecoderPassthroughTest,
       ContextLostWhenCommandLeavesOtherSurfaceCurrent) {
  scoped_refptr<gl::GLContext> decoder_context = gl::GLContext::GetCurrent();
  scoped_refptr<gl::GLSurface> decoder_surface = gl::GLSurface::GetCurrent();
  ASSERT_TRUE(decoder_context);
  ASSERT_TRUE(decoder_surface);

  scoped_refptr<gl::GLSurface> other_surface =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                         gfx::Size());
  ASSERT_TRUE(other_surface);

  MemoryTypeTracker memory_tracker(nullptr);
  Mailbox mailbox = Mailbox::Generate();
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  gfx::Size size(10, 10);
  GLuint service_id;
  glGenTextures(1, &service_id);
  glBindTexture(GL_TEXTURE_2D, service_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.width(), size.height(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  auto backing = std::make_unique<ContextSwitchingTestImageBacking>(
      mailbox,
      SharedImageInfo(
          format, size, gfx::ColorSpace(), kTopLeft_GrSurfaceOrigin,
          kPremul_SkAlphaType,
          {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE},
          "TestLabel"),
      service_id, decoder_context, other_surface);
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      GetSharedImageManager()->Register(std::move(backing), &memory_tracker);

  auto& cmd = *GetImmediateAs<
      cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, mailbox.name);
  EXPECT_EQ(error::kLostContext,
            ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_TRUE(GetDecoder()->WasContextLost());

  // Restore the decoder's surface for teardown.
  ASSERT_TRUE(decoder_context->MakeCurrent(decoder_surface.get()));
  shared_image.reset();
}

// Tests that shared image textures can only be read through GL commands while
// a BeginSharedImageAccessDirectCHROMIUM access is active on them.
class GLES2DecoderPassthroughSharedImageReadAccessTest
    : public GLES2DecoderPassthroughTestBase,
      public testing::WithParamInterface<ContextType> {
 public:
  GLES2DecoderPassthroughSharedImageReadAccessTest()
      : GLES2DecoderPassthroughTestBase(GetParam()) {}

  void SetUp() override {
    GLES2DecoderPassthroughTestBase::SetUp();

    mailbox_ = Mailbox::Generate();
    shared_image_ = GetSharedImageManager()->Register(
        AllocateTextureAndCreateSharedImage(
            mailbox_, viz::SinglePlaneFormat::kRGBA_8888,
            gfx::Size(kWidth, kHeight), gfx::ColorSpace(),
            kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
            {SHARED_IMAGE_USAGE_GLES2_READ, SHARED_IMAGE_USAGE_GLES2_WRITE}),
        &memory_tracker_);
    CreateSharedImageTexture(kSharedImageClientId);

    // A regular texture used as the destination of copies.
    GenHelper<cmds::GenTexturesImmediate>(kClientTextureId);
    DoBindTexture(GL_TEXTURE_2D, kClientTextureId);
    DoTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, 0, 0);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void TearDown() override {
    DoDeleteTexture(kClientTextureId);
    DoDeleteTexture(kSharedImageClientId);
    shared_image_.reset();
    GLES2DecoderPassthroughTestBase::TearDown();
  }

 protected:
  static constexpr GLsizei kWidth = 4;
  static constexpr GLsizei kHeight = 4;
  static constexpr GLuint kSharedImageClientId = kNewClientId;
  static constexpr GLuint kSecondSharedImageClientId = kNewClientId + 1;
  static constexpr GLuint kDrawFramebufferClientId = kClientFramebufferId + 1;
  static constexpr uint8_t kSentinel = 0xEE;

  bool IsES3() const { return GetParam() == CONTEXT_TYPE_OPENGLES3; }

  void CreateSharedImageTexture(GLuint client_id) {
    auto& cmd = *GetImmediateAs<
        cmds::CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(client_id, mailbox_.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox_.name)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void BeginAccess(GLuint client_id, GLenum mode) {
    cmds::BeginSharedImageAccessDirectCHROMIUM cmd;
    cmd.Init(client_id, mode);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  void EndAccess(GLuint client_id) {
    cmds::EndSharedImageAccessDirectCHROMIUM cmd;
    cmd.Init(client_id);
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Binds a framebuffer with `texture_client_id` attached to `attachment` to
  // GL_FRAMEBUFFER.
  void AttachToFramebuffer(GLuint texture_client_id, GLenum attachment) {
    GenHelper<cmds::GenFramebuffersImmediate>(kClientFramebufferId);
    DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
    DoFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
                           texture_client_id, 0);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Issues a synchronous ReadPixels of the whole image into shared memory
  // pre-filled with kSentinel. Returns the GL error generated, and sets
  // `pixels_untouched` to whether the client's pixel buffer was left intact.
  GLenum ReadPixels(bool* pixels_untouched) {
    auto* result = GetSharedMemoryAs<cmds::ReadPixels::Result*>();
    const uint32_t pixels_shm_offset = kSharedMemoryOffset + sizeof(*result);
    const size_t pixels_size = kWidth * kHeight * 4;
    // SAFETY: the shared memory buffer is larger than the result followed by
    // the pixels.
    auto pixels_span = UNSAFE_BUFFERS(
        base::span(reinterpret_cast<uint8_t*>(&result[1]), pixels_size));
    std::ranges::fill(pixels_span, kSentinel);

    cmds::ReadPixels cmd;
    cmd.Init(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE,
             shared_memory_id_, pixels_shm_offset, shared_memory_id_,
             kSharedMemoryOffset, false);
    result->success = 0;
    EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
    // Read the results before GetGLError(), which reuses the shared memory.
    const bool success = !!result->success;
    *pixels_untouched = std::ranges::all_of(
        pixels_span, [](uint8_t value) { return value == kSentinel; });
    GLenum error = GetGLError();
    EXPECT_EQ(error == GL_NO_ERROR, success);
    return error;
  }

  MemoryTypeTracker memory_tracker_{nullptr};
  Mailbox mailbox_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         GLES2DecoderPassthroughSharedImageReadAccessTest,
                         testing::Values(CONTEXT_TYPE_OPENGLES2,
                                         CONTEXT_TYPE_OPENGLES3));

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest, ReadPixels) {
  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);

  // Reading without an access is rejected and the client buffer is untouched.
  bool pixels_untouched = false;
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);

  // Reading with a read access succeeds.
  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSharedImageClientId);

  // Reading with a read-write access succeeds.
  BeginAccess(kSharedImageClientId,
              GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSharedImageClientId);

  // Reading after the access ended is rejected again.
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       ReadPixelsFromRegularTexture) {
  AttachToFramebuffer(kClientTextureId, GL_COLOR_ATTACHMENT0);

  // Reading from a regular texture doesn't require an access, even if shared
  // image textures exist.
  bool pixels_untouched = true;
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       ReadPixelsAliasedSharedImageTexture) {
  // Import the same shared image a second time. Both client ids alias the same
  // service texture.
  CreateSharedImageTexture(kSecondSharedImageClientId);
  AttachToFramebuffer(kSecondSharedImageClientId, GL_COLOR_ATTACHMENT0);

  bool pixels_untouched = false;
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);

  // An access through either client id makes the texture readable.
  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSharedImageClientId);

  DoDeleteTexture(kSecondSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       DeleteAliasedSharedImageTexture) {
  PassthroughResources* resources = GetPassthroughResources();
  GLuint service_id = 0;
  ASSERT_TRUE(resources->texture_id_map.GetServiceID(kSharedImageClientId,
                                                     &service_id));
  ASSERT_TRUE(
      resources->shared_image_client_ids_by_service_id.contains(service_id));
  EXPECT_EQ(
      1u, resources->shared_image_client_ids_by_service_id[service_id].size());

  // Import the same shared image a second time, and delete the first import
  // while it has an active access.
  CreateSharedImageTexture(kSecondSharedImageClientId);
  EXPECT_EQ(
      2u, resources->shared_image_client_ids_by_service_id[service_id].size());
  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  DoDeleteTexture(kSharedImageClientId);
  EXPECT_EQ(
      1u, resources->shared_image_client_ids_by_service_id[service_id].size());

  // The remaining import still requires its own access.
  AttachToFramebuffer(kSecondSharedImageClientId, GL_COLOR_ATTACHMENT0);
  bool pixels_untouched = false;
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);

  BeginAccess(kSecondSharedImageClientId,
              GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSecondSharedImageClientId);

  // Deleting the last import removes the service id from the index.
  DoDeleteTexture(kSecondSharedImageClientId);
  EXPECT_FALSE(
      resources->shared_image_client_ids_by_service_id.contains(service_id));
  EXPECT_TRUE(resources->texture_shared_image_map.empty());

  // Recreate the import deleted by TearDown().
  CreateSharedImageTexture(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       KillSwitchDisablesEnforcement) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kEnforceSharedImageAccessForGLReads);

  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);
  bool pixels_untouched = true;
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);

  cmds::CopyTextureCHROMIUM cmd;
  cmd.Init(kSharedImageClientId, 0, GL_TEXTURE_2D, kClientTextureId, 0, GL_RGBA,
           GL_UNSIGNED_BYTE, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       ReadPixelsSeparateReadFramebuffer) {
  if (!IsES3() &&
      !feature_info()->feature_flags().chromium_framebuffer_multisample) {
    GTEST_SKIP() << "Separate read and draw framebuffers are not supported";
  }

  // The shared image is attached to the read framebuffer while a regular
  // texture is attached to the draw framebuffer.
  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);
  GenHelper<cmds::GenFramebuffersImmediate>(kDrawFramebufferClientId);
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, kDrawFramebufferClientId);
  DoFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, kClientTextureId, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  bool pixels_untouched = false;
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSharedImageClientId);

  // Conversely, a shared image attached only to the draw framebuffer isn't
  // read.
  DoBindFramebuffer(GL_READ_FRAMEBUFFER, kDrawFramebufferClientId);
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, kClientFramebufferId);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       ReadPixelsOnlyChecksReadBuffer) {
  if (!IsES3()) {
    GTEST_SKIP() << "glReadBuffer requires ES3";
  }

  // The regular texture is the read buffer; the shared image is attached to
  // another color attachment which isn't read.
  AttachToFramebuffer(kClientTextureId, GL_COLOR_ATTACHMENT0);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                         kSharedImageClientId, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  bool pixels_untouched = true;
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);

  // Selecting the shared image as the read buffer makes the read require an
  // access.
  cmds::ReadBuffer read_buffer_cmd;
  read_buffer_cmd.Init(GL_COLOR_ATTACHMENT1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(read_buffer_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION),
            ReadPixels(&pixels_untouched));
  EXPECT_TRUE(pixels_untouched);

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ReadPixels(&pixels_untouched));
  EXPECT_FALSE(pixels_untouched);
  EndAccess(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest, CopyTexImage2D) {
  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);

  cmds::CopyTexImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest, CopyTexSubImage2D) {
  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);
  DoBindTexture(GL_TEXTURE_2D, kClientTextureId);

  cmds::CopyTexSubImage2D cmd;
  cmd.Init(GL_TEXTURE_2D, 0, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest, CopyTexSubImage3D) {
  if (!IsES3()) {
    GTEST_SKIP() << "glCopyTexSubImage3D requires ES3";
  }

  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);

  constexpr GLuint kTexture3DClientId = kClientTextureId + 1;
  GenHelper<cmds::GenTexturesImmediate>(kTexture3DClientId);
  DoBindTexture(GL_TEXTURE_3D, kTexture3DClientId);
  cmds::TexStorage3D storage_cmd;
  storage_cmd.Init(GL_TEXTURE_3D, 1, GL_RGBA8, kWidth, kHeight, 1);
  EXPECT_EQ(error::kNoError, ExecuteCmd(storage_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::CopyTexSubImage3D cmd;
  cmd.Init(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, kWidth, kHeight);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);

  DoDeleteTexture(kTexture3DClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       BlitFramebufferCHROMIUM) {
  if (!feature_info()->feature_flags().chromium_framebuffer_multisample) {
    GTEST_SKIP() << "glBlitFramebufferCHROMIUM is not supported";
  }

  AttachToFramebuffer(kSharedImageClientId, GL_COLOR_ATTACHMENT0);
  DoBindFramebuffer(GL_READ_FRAMEBUFFER, kClientFramebufferId);
  GenHelper<cmds::GenFramebuffersImmediate>(kDrawFramebufferClientId);
  DoBindFramebuffer(GL_DRAW_FRAMEBUFFER, kDrawFramebufferClientId);
  DoFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_2D, kClientTextureId, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  cmds::BlitFramebufferCHROMIUM cmd;
  cmd.Init(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT,
           GL_NEAREST);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest, CopyTextureCHROMIUM) {
  cmds::CopyTextureCHROMIUM cmd;
  cmd.Init(kSharedImageClientId, 0, GL_TEXTURE_2D, kClientTextureId, 0, GL_RGBA,
           GL_UNSIGNED_BYTE, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);

  // Copying from a regular texture into the shared image doesn't read the
  // shared image.
  cmds::CopySubTextureCHROMIUM write_cmd;
  write_cmd.Init(kClientTextureId, 0, GL_TEXTURE_2D, kSharedImageClientId, 0, 0,
                 0, 0, 0, kWidth, kHeight, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(write_cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       CopyTextureCHROMIUMAliasedSharedImageTexture) {
  CreateSharedImageTexture(kSecondSharedImageClientId);

  cmds::CopyTextureCHROMIUM cmd;
  cmd.Init(kSecondSharedImageClientId, 0, GL_TEXTURE_2D, kClientTextureId, 0,
           GL_RGBA, GL_UNSIGNED_BYTE, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  // An access through the other client id makes the source readable.
  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);

  DoDeleteTexture(kSecondSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       CopySubTextureCHROMIUM) {
  cmds::CopySubTextureCHROMIUM cmd;
  cmd.Init(kSharedImageClientId, 0, GL_TEXTURE_2D, kClientTextureId, 0, 0, 0, 0,
           0, kWidth, kHeight, GL_FALSE, GL_FALSE, GL_FALSE);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());

  BeginAccess(kSharedImageClientId, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
  EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  EndAccess(kSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       AttachSharedImageOnlyToColorAttachments) {
  GenHelper<cmds::GenFramebuffersImmediate>(kClientFramebufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  CreateSharedImageTexture(kSecondSharedImageClientId);

  std::vector<GLenum> attachments = {GL_DEPTH_ATTACHMENT,
                                     GL_STENCIL_ATTACHMENT};
  if (IsES3()) {
    attachments.push_back(GL_DEPTH_STENCIL_ATTACHMENT);
  }
  for (GLenum attachment : attachments) {
    SCOPED_TRACE(attachment);
    // Attaching a shared image to a depth or stencil attachment is rejected,
    // for each import of the shared image.
    for (GLuint client_id :
         {kSharedImageClientId, kSecondSharedImageClientId}) {
      cmds::FramebufferTexture2D cmd;
      cmd.Init(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, client_id, 0);
      EXPECT_EQ(error::kNoError, ExecuteCmd(cmd));
      EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
    }

    // Regular textures can still be attached there, and detaching is always
    // allowed.
    DoFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D,
                           kClientTextureId, 0);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
    DoFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, 0, 0);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Shared images can be attached to any color attachment.
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         kSharedImageClientId, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
  if (IsES3()) {
    DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D,
                           kSecondSharedImageClientId, 0);
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  DoDeleteTexture(kSecondSharedImageClientId);
}

TEST_P(GLES2DecoderPassthroughSharedImageReadAccessTest,
       AttachSharedImageKillSwitch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      kRestrictSharedImageAttachmentsToColorAttachments);

  GenHelper<cmds::GenFramebuffersImmediate>(kClientFramebufferId);
  DoBindFramebuffer(GL_FRAMEBUFFER, kClientFramebufferId);
  DoFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         kSharedImageClientId, 0);
  EXPECT_EQ(GL_NO_ERROR, GetGLError());
}

}  // namespace gles2
}  // namespace gpu
