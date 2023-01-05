// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {
namespace gles2 {

static const SyncToken g_sync_token(gpu::CommandBufferNamespace::GPU_IO,
                                    gpu::CommandBufferId::FromUnsafeValue(123),
                                    0);

class MailboxManagerTest : public GpuServiceTest {
 public:
  MailboxManagerTest() = default;

  MailboxManagerTest(const MailboxManagerTest&) = delete;
  MailboxManagerTest& operator=(const MailboxManagerTest&) = delete;

  ~MailboxManagerTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    feature_info_ = new FeatureInfo;
    manager_ = std::make_unique<MailboxManagerImpl>();
  }

  Texture* CreateTexture() {
    return new Texture(1);
  }

  void SetTarget(Texture* texture, GLenum target, GLuint max_level) {
    texture->SetTarget(target, max_level);
  }

  void SetLevelInfo(Texture* texture,
                    GLenum target,
                    GLint level,
                    GLenum internal_format,
                    GLsizei width,
                    GLsizei height,
                    GLsizei depth,
                    GLint border,
                    GLenum format,
                    GLenum type,
                    const gfx::Rect& cleared_rect) {
    texture->SetLevelInfo(target, level, internal_format, width, height, depth,
                          border, format, type, cleared_rect);
  }

  void SetLevelCleared(Texture* texture,
                       GLenum target,
                       GLint level,
                       bool cleared) {
    texture->SetLevelCleared(target, level, cleared);
  }

  GLenum SetParameter(Texture* texture, GLenum pname, GLint param) {
    return texture->SetParameteri(feature_info_.get(), pname, param);
  }

  void DestroyTexture(TextureBase* texture) { delete texture; }

  std::unique_ptr<MailboxManager> manager_;

 private:
  scoped_refptr<FeatureInfo> feature_info_;
};

// Tests basic produce/consume behavior.
TEST_F(MailboxManagerTest, Basic) {
  Texture* texture = CreateTexture();

  Mailbox name = Mailbox::GenerateLegacyMailboxForTesting();
  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // We can consume multiple times.
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // Destroy should cleanup the mailbox.
  DestroyTexture(texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
}

// Tests behavior with multiple produce on the same texture.
TEST_F(MailboxManagerTest, ProduceMultipleMailbox) {
  Texture* texture = CreateTexture();

  Mailbox name1 = Mailbox::GenerateLegacyMailboxForTesting();

  manager_->ProduceTexture(name1, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name1));

  // Can produce a second time with the same mailbox.
  manager_->ProduceTexture(name1, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name1));

  // Can produce again, with a different mailbox.
  Mailbox name2 = Mailbox::GenerateLegacyMailboxForTesting();
  manager_->ProduceTexture(name2, texture);

  // Still available under all mailboxes.
  EXPECT_EQ(texture, manager_->ConsumeTexture(name1));
  EXPECT_EQ(texture, manager_->ConsumeTexture(name2));

  // Destroy should cleanup all mailboxes.
  DestroyTexture(texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name1));
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name2));
}

// Tests behavior with multiple produce on the same mailbox with different
// textures.
TEST_F(MailboxManagerTest, ProduceMultipleTexture) {
  Texture* texture1 = CreateTexture();
  Texture* texture2 = CreateTexture();

  Mailbox name = Mailbox::GenerateLegacyMailboxForTesting();

  manager_->ProduceTexture(name, texture1);
  EXPECT_EQ(texture1, manager_->ConsumeTexture(name));

  // Producing a second time is ignored.
  manager_->ProduceTexture(name, texture2);
  EXPECT_EQ(texture1, manager_->ConsumeTexture(name));

  // Destroying the texture that's under no mailbox shouldn't have an effect.
  DestroyTexture(texture2);
  EXPECT_EQ(texture1, manager_->ConsumeTexture(name));

  // Destroying the texture that's bound should clean up.
  DestroyTexture(texture1);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
}

}  // namespace gles2
}  // namespace gpu
