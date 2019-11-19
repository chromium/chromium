// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/mailbox_manager_sync.h"
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
  ~MailboxManagerTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    feature_info_ = new FeatureInfo;
    manager_ = std::make_unique<MailboxManagerImpl>();
    DCHECK(!manager_->UsesSync());
  }

  virtual void SetUpWithSynchronizer() {
    GpuServiceTest::SetUp();
    feature_info_ = new FeatureInfo;
    manager_ = std::make_unique<MailboxManagerSync>();
    DCHECK(manager_->UsesSync());
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

  DISALLOW_COPY_AND_ASSIGN(MailboxManagerTest);
};

// Tests basic produce/consume behavior.
TEST_F(MailboxManagerTest, Basic) {
  Texture* texture = CreateTexture();

  Mailbox name = Mailbox::Generate();
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

  Mailbox name1 = Mailbox::Generate();

  manager_->ProduceTexture(name1, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name1));

  // Can produce a second time with the same mailbox.
  manager_->ProduceTexture(name1, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name1));

  // Can produce again, with a different mailbox.
  Mailbox name2 = Mailbox::Generate();
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

  Mailbox name = Mailbox::Generate();

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

const GLsizei kMaxTextureWidth = 64;
const GLsizei kMaxTextureHeight = 64;
const GLsizei kMaxTextureDepth = 1;

class MailboxManagerSyncTest : public MailboxManagerTest {
 public:
  MailboxManagerSyncTest() = default;
  ~MailboxManagerSyncTest() override = default;

 protected:
  void SetUp() override {
    MailboxManagerTest::SetUpWithSynchronizer();
    manager2_ = std::make_unique<MailboxManagerSync>();
    context_ = new gl::GLContextStub();
    surface_ = new gl::GLSurfaceStub();
    context_->MakeCurrent(surface_.get());
  }

  Texture* DefineTexture() {
    Texture* texture = CreateTexture();
    const GLsizei levels_needed = TextureManager::ComputeMipMapCount(
        GL_TEXTURE_2D, kMaxTextureWidth, kMaxTextureHeight, kMaxTextureDepth);
    SetTarget(texture, GL_TEXTURE_2D, levels_needed);
    SetLevelInfo(texture, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
    SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return texture;
  }

  void SetupUpdateTexParamExpectations(GLuint texture_id,
                                       GLenum min,
                                       GLenum mag,
                                       GLenum wrap_s,
                                       GLenum wrap_t) {
    DCHECK(texture_id);
    const GLuint kCurrentTexture = 0;
    EXPECT_CALL(*gl_, GetIntegerv(GL_TEXTURE_BINDING_2D, testing::_))
        .WillOnce(testing::SetArgPointee<1>(kCurrentTexture))
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, texture_id))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_,
                TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t))
        .Times(1)
        .RetiresOnSaturation();
    EXPECT_CALL(*gl_, BindTexture(GL_TEXTURE_2D, kCurrentTexture))
        .Times(1)
        .RetiresOnSaturation();
  }

  void TearDown() override {
    context_->ReleaseCurrent(nullptr);
    MailboxManagerTest::TearDown();
  }

  std::unique_ptr<MailboxManager> manager2_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailboxManagerSyncTest);
};

TEST_F(MailboxManagerSyncTest, ProduceDestroy) {
  Texture* texture = DefineTexture();
  Mailbox name = Mailbox::Generate();

  testing::InSequence sequence;
  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  DestroyTexture(texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

TEST_F(MailboxManagerSyncTest, ProduceSyncDestroy) {
  testing::InSequence sequence;

  Texture* texture = DefineTexture();
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

TEST_F(MailboxManagerSyncTest, ProduceSyncMultipleMailbox) {
  testing::InSequence sequence;

  Texture* texture = DefineTexture();
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);
  manager_->PushTextureUpdates(g_sync_token);

  // Producing a second time with the same mailbox is ignored.
  Texture* old_texture = texture;
  texture = DefineTexture();
  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(old_texture, manager_->ConsumeTexture(name));

  DestroyTexture(old_texture);
  DestroyTexture(texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

// Duplicates a texture into a second manager instance, and then
// makes sure a redefinition becomes visible there too.
TEST_F(MailboxManagerSyncTest, ProduceConsumeResize) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture = DefineTexture();
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  Texture* new_texture = Texture::CheckedCast(manager2_->ConsumeTexture(name));
  EXPECT_NE(nullptr, new_texture);
  EXPECT_NE(texture, new_texture);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  // Resize original texture
  SetLevelInfo(texture, GL_TEXTURE_2D, 0, GL_RGBA, 16, 32, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, gfx::Rect(16, 32));
  // Should have been orphaned
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) == nullptr);

  // Synchronize again
  manager_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  manager2_->PullTextureUpdates(g_sync_token);
  GLsizei width, height;
  new_texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr);
  EXPECT_EQ(16, width);
  EXPECT_EQ(32, height);

  // Should have gotten a new attachment
  EXPECT_TRUE(texture->GetLevelImage(GL_TEXTURE_2D, 0) != nullptr);
  // Resize original texture again....
  SetLevelInfo(texture, GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, gfx::Rect(64, 64));
  // ...and immediately delete the texture which should save the changes.
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  DestroyTexture(texture);

  // Should be still around since there is a ref from manager2
  EXPECT_EQ(new_texture, manager2_->ConsumeTexture(name));

  // The last change to the texture should be visible without a sync point (i.e.
  // push).
  manager2_->PullTextureUpdates(g_sync_token);
  new_texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, nullptr);
  EXPECT_EQ(64, width);
  EXPECT_EQ(64, height);

  DestroyTexture(new_texture);
  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

// Makes sure changes are correctly published even when updates are
// pushed in both directions, i.e. makes sure we don't clobber a shared
// texture definition with an older version.
TEST_F(MailboxManagerSyncTest, ProduceConsumeBidirectional) {
  const GLuint kNewTextureId1 = 1234;
  const GLuint kNewTextureId2 = 4321;

  Texture* texture1 = DefineTexture();
  Mailbox name1 = Mailbox::Generate();
  Texture* texture2 = DefineTexture();
  Mailbox name2 = Mailbox::Generate();
  TextureBase* new_texture1 = nullptr;
  TextureBase* new_texture2 = nullptr;

  manager_->ProduceTexture(name1, texture1);
  manager2_->ProduceTexture(name2, texture2);

  // Make visible.
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PushTextureUpdates(g_sync_token);

  // Create textures in the other manager instances for texture1 and texture2,
  // respectively to create a real sharing scenario. Otherwise, there would
  // never be conflicting updates/pushes.
  {
    testing::InSequence sequence;
    EXPECT_CALL(*gl_, GenTextures(1, testing::_))
        .WillOnce(testing::SetArgPointee<1>(kNewTextureId1));
    SetupUpdateTexParamExpectations(
        kNewTextureId1, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
    new_texture1 = manager2_->ConsumeTexture(name1);
    EXPECT_CALL(*gl_, GenTextures(1, testing::_))
        .WillOnce(testing::SetArgPointee<1>(kNewTextureId2));
    SetupUpdateTexParamExpectations(
        kNewTextureId2, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
    new_texture2 = manager_->ConsumeTexture(name2);
  }
  EXPECT_EQ(kNewTextureId1, new_texture1->service_id());
  EXPECT_EQ(kNewTextureId2, new_texture2->service_id());

  // Make a change to texture1
  DCHECK_EQ(static_cast<GLuint>(GL_LINEAR), texture1->min_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR),
            SetParameter(texture1, GL_TEXTURE_MIN_FILTER, GL_NEAREST));

  // Make sure this does not clobber it with the previous version we pushed.
  manager_->PullTextureUpdates(g_sync_token);

  // Make a change to texture2
  DCHECK_EQ(static_cast<GLuint>(GL_LINEAR), texture2->mag_filter());
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR),
            SetParameter(texture2, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

  testing::Mock::VerifyAndClearExpectations(gl_.get());

  // Synchronize in both directions
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PushTextureUpdates(g_sync_token);
  // manager1 should see the change to texture2 mag_filter being applied.
  SetupUpdateTexParamExpectations(
      new_texture2->service_id(), GL_LINEAR, GL_NEAREST, GL_REPEAT, GL_REPEAT);
  manager_->PullTextureUpdates(g_sync_token);
  // manager2 should see the change to texture1 min_filter being applied.
  SetupUpdateTexParamExpectations(
      new_texture1->service_id(), GL_NEAREST, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  manager2_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture1);
  DestroyTexture(texture2);
  DestroyTexture(new_texture1);
  DestroyTexture(new_texture2);
}

TEST_F(MailboxManagerSyncTest, ClearedStateSynced) {
  const GLuint kNewTextureId = 1234;

  Texture* texture = DefineTexture();
  EXPECT_TRUE(texture->SafeToRenderFrom());

  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  Texture* new_texture = Texture::CheckedCast(manager2_->ConsumeTexture(name));
  EXPECT_NE(nullptr, new_texture);
  EXPECT_NE(texture, new_texture);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());
  EXPECT_TRUE(texture->SafeToRenderFrom());

  // Change cleared to false.
  SetLevelCleared(texture, texture->target(), 0, false);
  EXPECT_FALSE(texture->SafeToRenderFrom());

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  manager2_->PullTextureUpdates(g_sync_token);

  // Cleared state should be synced.
  EXPECT_FALSE(new_texture->SafeToRenderFrom());

  DestroyTexture(texture);
  DestroyTexture(new_texture);

  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

TEST_F(MailboxManagerSyncTest, SyncIncompleteTexture) {
  const GLuint kNewTextureId = 1234;

  // Create but not define texture.
  Texture* texture = CreateTexture();
  SetTarget(texture, GL_TEXTURE_2D, 1);
  EXPECT_FALSE(texture->IsDefined());

  Mailbox name = Mailbox::Generate();
  manager_->ProduceTexture(name, texture);
  EXPECT_EQ(texture, manager_->ConsumeTexture(name));

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  // Should sync to new texture which is not defined.
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(kNewTextureId, texture->min_filter(),
                                  texture->mag_filter(), texture->wrap_s(),
                                  texture->wrap_t());
  Texture* new_texture = Texture::CheckedCast(manager2_->ConsumeTexture(name));
  EXPECT_NE(nullptr, new_texture);
  EXPECT_NE(texture, new_texture);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());
  EXPECT_FALSE(new_texture->IsDefined());

  // Change cleared to false.
  SetLevelInfo(texture, GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  EXPECT_TRUE(texture->IsDefined());

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  manager2_->PullTextureUpdates(g_sync_token);

  // Cleared state should be synced.
  EXPECT_TRUE(new_texture->IsDefined());

  DestroyTexture(texture);
  DestroyTexture(new_texture);

  EXPECT_EQ(nullptr, manager_->ConsumeTexture(name));
  EXPECT_EQ(nullptr, manager2_->ConsumeTexture(name));
}

// Putting the same texture into multiple mailboxes should result in sharing
// only a single texture also within a synchronized manager instance.
TEST_F(MailboxManagerSyncTest, SharedThroughMultipleMailboxes) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture = DefineTexture();
  Mailbox name1 = Mailbox::Generate();
  Mailbox name2 = Mailbox::Generate();

  manager_->ProduceTexture(name1, texture);

  // Share
  manager_->PushTextureUpdates(g_sync_token);
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  manager2_->PullTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  TextureBase* new_texture = manager2_->ConsumeTexture(name1);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  manager_->ProduceTexture(name2, texture);

  // Synchronize
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  // name2 should return the same texture
  EXPECT_EQ(new_texture, manager2_->ConsumeTexture(name2));

  // Even after destroying the source texture, the original mailbox should
  // still exist.
  DestroyTexture(texture);
  EXPECT_EQ(new_texture, manager2_->ConsumeTexture(name1));
  DestroyTexture(new_texture);
}

// A: produce texture1 into M, B: consume into new_texture
// B: produce texture2 into M, A: produce texture1 into M
// B: consume M should return new_texture
TEST_F(MailboxManagerSyncTest, ProduceBothWays) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture1 = DefineTexture();
  Texture* texture2 = DefineTexture();
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture1);

  // Share
  manager_->PushTextureUpdates(g_sync_token);
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(
      kNewTextureId, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT);
  TextureBase* new_texture = manager2_->ConsumeTexture(name);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  // Clobber
  manager2_->ProduceTexture(name, texture2);
  manager_->ProduceTexture(name, texture1);

  // Synchronize manager -> manager2
  manager_->PushTextureUpdates(g_sync_token);
  manager2_->PullTextureUpdates(g_sync_token);

  // name should return the original texture, and not texture2 or a new one.
  EXPECT_EQ(new_texture, manager2_->ConsumeTexture(name));

  DestroyTexture(texture1);
  DestroyTexture(texture2);
  DestroyTexture(new_texture);
}

// Test for crbug.com/816693
// A: produce texture (without images) into M, B: consume into new_texture
// B: push updates
TEST_F(MailboxManagerSyncTest, ProduceTextureNotDefined) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture = CreateTexture();
  const GLsizei levels_needed = TextureManager::ComputeMipMapCount(
      GL_TEXTURE_2D, kMaxTextureWidth, kMaxTextureHeight, kMaxTextureDepth);
  SetTarget(texture, GL_TEXTURE_2D, levels_needed);
  SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);

  // Share
  manager_->PushTextureUpdates(g_sync_token);
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(kNewTextureId, GL_LINEAR, GL_LINEAR,
                                  GL_REPEAT, GL_REPEAT);
  TextureBase* new_texture = manager2_->ConsumeTexture(name);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  // Change something so that the push recreates the TextureDefinition.
  SetParameter(Texture::CheckedCast(new_texture), GL_TEXTURE_MIN_FILTER,
               GL_NEAREST);

  // Synchronize manager2 -> manager
  manager2_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(1, GL_NEAREST, GL_LINEAR, GL_REPEAT,
                                  GL_REPEAT);
  manager_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture);
  DestroyTexture(new_texture);
}

TEST_F(MailboxManagerSyncTest, ProduceTextureDefinedNotLevel0) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture = CreateTexture();
  const GLsizei levels_needed = TextureManager::ComputeMipMapCount(
      GL_TEXTURE_2D, kMaxTextureWidth, kMaxTextureHeight, kMaxTextureDepth);
  SetTarget(texture, GL_TEXTURE_2D, levels_needed);
  SetLevelInfo(texture, GL_TEXTURE_2D, 1, GL_RGBA, 1, 1, 1, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, gfx::Rect(1, 1));
  SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);

  // Share
  manager_->PushTextureUpdates(g_sync_token);
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(kNewTextureId, GL_LINEAR, GL_LINEAR,
                                  GL_REPEAT, GL_REPEAT);
  TextureBase* new_texture = manager2_->ConsumeTexture(name);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  // Change something so that the push recreates the TextureDefinition.
  SetParameter(Texture::CheckedCast(new_texture), GL_TEXTURE_MIN_FILTER,
               GL_NEAREST);

  // Synchronize manager2 -> manager
  manager2_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(1, GL_NEAREST, GL_LINEAR, GL_REPEAT,
                                  GL_REPEAT);
  manager_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture);
  DestroyTexture(new_texture);
}

TEST_F(MailboxManagerSyncTest, ProduceTextureDefined0Size) {
  const GLuint kNewTextureId = 1234;
  testing::InSequence sequence;

  Texture* texture = CreateTexture();
  const GLsizei levels_needed = TextureManager::ComputeMipMapCount(
      GL_TEXTURE_2D, kMaxTextureWidth, kMaxTextureHeight, kMaxTextureDepth);
  SetTarget(texture, GL_TEXTURE_2D, levels_needed);
  SetLevelInfo(texture, GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, gfx::Rect(0, 0));
  SetParameter(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  SetParameter(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);

  // Share
  manager_->PushTextureUpdates(g_sync_token);
  EXPECT_CALL(*gl_, GenTextures(1, testing::_))
      .WillOnce(testing::SetArgPointee<1>(kNewTextureId));
  SetupUpdateTexParamExpectations(kNewTextureId, GL_LINEAR, GL_LINEAR,
                                  GL_REPEAT, GL_REPEAT);
  TextureBase* new_texture = manager2_->ConsumeTexture(name);
  EXPECT_EQ(kNewTextureId, new_texture->service_id());

  // Change something so that the push recreates the TextureDefinition.
  SetParameter(Texture::CheckedCast(new_texture), GL_TEXTURE_MIN_FILTER,
               GL_NEAREST);

  // Synchronize manager2 -> manager
  manager2_->PushTextureUpdates(g_sync_token);
  SetupUpdateTexParamExpectations(1, GL_NEAREST, GL_LINEAR, GL_REPEAT,
                                  GL_REPEAT);
  manager_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture);
  DestroyTexture(new_texture);
}

TEST_F(MailboxManagerSyncTest, ProduceTextureNotBound) {
  testing::InSequence sequence;

  Texture* texture = CreateTexture();
  Mailbox name = Mailbox::Generate();

  manager_->ProduceTexture(name, texture);

  // Share
  manager_->PushTextureUpdates(g_sync_token);

  // Consume should fail.
  TextureBase* new_texture = manager2_->ConsumeTexture(name);
  EXPECT_EQ(nullptr, new_texture);

  // Synchronize manager2 -> manager
  manager2_->PushTextureUpdates(g_sync_token);
  manager_->PullTextureUpdates(g_sync_token);

  DestroyTexture(texture);
}

// TODO: Texture::level_infos_[][].size()

// TODO: unsupported targets and formats

}  // namespace gles2
}  // namespace gpu
