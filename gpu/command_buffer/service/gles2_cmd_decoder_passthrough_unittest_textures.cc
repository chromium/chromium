// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/test_image_backing.h"

namespace gpu {
namespace gles2 {
namespace {

std::unique_ptr<TestImageBacking> AllocateTextureAndCreateSharedImage(
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
  return std::make_unique<TestImageBacking>(mailbox, format, size, color_space,
                                            surface_origin, alpha_type, usage,
                                            0 /* estimated_size */, service_id);
}

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
  GLuint dummy_fbo;
  glGenFramebuffersEXT(1, &dummy_fbo);
  glBindFramebufferEXT(GL_FRAMEBUFFER, dummy_fbo);
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
  EXPECT_TRUE(0 ==
              memcmp(test_color_mask, color_mask, sizeof(test_color_mask)));
  GLfloat test_clear_color[4];
  glGetFloatv(GL_COLOR_CLEAR_VALUE, test_clear_color);
  EXPECT_TRUE(0 ==
              memcmp(test_clear_color, clear_color, sizeof(test_clear_color)));
  GLint test_fbo;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &test_fbo);
  EXPECT_EQ(test_fbo, static_cast<GLint>(dummy_fbo));
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
  glDeleteFramebuffersEXT(1, &dummy_fbo);
  glDeleteTextures(1, &dummy_texture);
  DoDeleteTexture(kNewClientId);
  shared_image.reset();
}

}  // namespace gles2
}  // namespace gpu
