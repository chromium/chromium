// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_unittest.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
namespace gles2 {
namespace {
static const uint32_t kNewServiceId = 431;

class TestSharedImageBackingPassthrough : public SharedImageBacking {
 public:
  class TestSharedImageRepresentationPassthrough
      : public SharedImageRepresentationGLTexturePassthrough {
   public:
    TestSharedImageRepresentationPassthrough(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        scoped_refptr<TexturePassthrough>& texture_passthrough)
        : SharedImageRepresentationGLTexturePassthrough(manager, backing),
          texture_passthrough_(texture_passthrough) {}

    const scoped_refptr<TexturePassthrough>& GetTexturePassthrough() override {
      return texture_passthrough_;
    }

   private:
    const scoped_refptr<TexturePassthrough>& texture_passthrough_;
  };

  TestSharedImageBackingPassthrough(const Mailbox& mailbox,
                                    viz::ResourceFormat format,
                                    const gfx::Size& size,
                                    const gfx::ColorSpace& color_space,
                                    uint32_t usage,
                                    GLuint texture_id)
      : SharedImageBacking(mailbox, format, size, color_space, usage) {
    texture_passthrough_ =
        base::MakeRefCounted<TexturePassthrough>(texture_id, GL_TEXTURE_2D);
  }

  bool IsCleared() const override { return false; }

  void SetCleared() override {}

  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override {
    return false;
  }

  void Destroy() override { texture_passthrough_.reset(); }

  size_t EstimatedSize() const override { return 0; }

  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override {}

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager) override {
    return std::make_unique<TestSharedImageRepresentationPassthrough>(
        manager, this, texture_passthrough_);
  }

 private:
  scoped_refptr<TexturePassthrough> texture_passthrough_;
};

}  // namespace

using namespace cmds;

TEST_F(GLES2DecoderPassthroughTest, CreateAndTexStorage2DSharedImageCHROMIUM) {
  Mailbox mailbox = Mailbox::Generate();
  GetSharedImageManager()->Register(
      std::make_unique<TestSharedImageBackingPassthrough>(
          mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
          gfx::ColorSpace(), 0, kNewServiceId));

  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_RGBA, mailbox.name);
  EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
  EXPECT_EQ(GL_NO_ERROR, GetGLError());

  // Make sure the new client ID is associated with the provided service ID.
  uint32_t found_service_id = 0;
  EXPECT_TRUE(GetPassthroughResources()->texture_id_map.GetServiceID(
      kNewClientId, &found_service_id));
  EXPECT_EQ(found_service_id, kNewServiceId);
  scoped_refptr<TexturePassthrough> found_texture_passthrough;
  EXPECT_TRUE(GetPassthroughResources()->texture_object_map.GetServiceID(
      kNewClientId, &found_texture_passthrough));
  EXPECT_EQ(found_texture_passthrough->service_id(), kNewServiceId);
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

  GetSharedImageManager()->Unregister(mailbox);
}

TEST_F(GLES2DecoderPassthroughTest,
       CreateAndTexStorage2DSharedImageCHROMIUMInvalidMailbox) {
  // Attempt to use an invalid mailbox.
  Mailbox mailbox;
  CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
      *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
  cmd.Init(kNewClientId, GL_RGBA, mailbox.name);
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
  // Create a texture with kNewClientId.
  Mailbox mailbox = Mailbox::Generate();
  GetSharedImageManager()->Register(
      std::make_unique<TestSharedImageBackingPassthrough>(
          mailbox, viz::ResourceFormat::RGBA_8888, gfx::Size(10, 10),
          gfx::ColorSpace(), 0, kNewServiceId));

  {
    CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
        *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, GL_RGBA, mailbox.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
    EXPECT_EQ(GL_NO_ERROR, GetGLError());
  }

  // Try to import the SharedImage a second time at the same client ID. We
  // should get a GL failure.
  {
    CreateAndTexStorage2DSharedImageINTERNALImmediate& cmd =
        *GetImmediateAs<CreateAndTexStorage2DSharedImageINTERNALImmediate>();
    cmd.Init(kNewClientId, GL_RGBA, mailbox.name);
    EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(cmd, sizeof(mailbox.name)));
    EXPECT_EQ(GL_INVALID_OPERATION, GetGLError());
  }

  DoDeleteTexture(kNewClientId);
  GetSharedImageManager()->Unregister(mailbox);
}

}  // namespace gles2
}  // namespace gpu
