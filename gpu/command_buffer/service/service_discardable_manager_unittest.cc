// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_discardable_manager.h"

#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/test_helper.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_image_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_switches.h"

using ::testing::Pointee;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::InSequence;

namespace gpu {
namespace gles2 {
namespace {

void CreateLockedHandlesForTesting(
    std::unique_ptr<ServiceDiscardableHandle>* service_handle,
    std::unique_ptr<ClientDiscardableHandle>* client_handle) {
  const size_t kShmemSize = sizeof(uint32_t);
  base::UnsafeSharedMemoryRegion shared_mem =
      base::UnsafeSharedMemoryRegion::Create(kShmemSize);
  base::WritableSharedMemoryMapping shared_mem_mapping = shared_mem.Map();
  scoped_refptr<gpu::Buffer> buffer = MakeBufferFromSharedMemory(
      std::move(shared_mem), std::move(shared_mem_mapping));

  client_handle->reset(new ClientDiscardableHandle(buffer, 0, 0));
  service_handle->reset(new ServiceDiscardableHandle(buffer, 0, 0));
}

ServiceDiscardableHandle CreateLockedServiceHandleForTesting() {
  std::unique_ptr<ServiceDiscardableHandle> service_handle;
  std::unique_ptr<ClientDiscardableHandle> client_handle;
  CreateLockedHandlesForTesting(&service_handle, &client_handle);
  return *service_handle;
}

class MockDestructionObserver : public TextureManager::DestructionObserver {
 public:
  MOCK_METHOD1(OnTextureManagerDestroying, void(TextureManager* manager));
  MOCK_METHOD1(OnTextureRefDestroying, void(TextureRef* ref));
};

// A small texture that should never run up against our limits.
static const uint32_t kSmallTextureDim = 16;
static const size_t kSmallTextureSize = 4 * kSmallTextureDim * kSmallTextureDim;

}  // namespace

class ServiceDiscardableManagerTest : public GpuServiceTest {
 public:
  ServiceDiscardableManagerTest() : discardable_manager_(GpuPreferences()) {}
  ~ServiceDiscardableManagerTest() override = default;

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    decoder_.reset(
        new MockGLES2Decoder(&client_, &command_buffer_service_, &outputter_));
    feature_info_ = new FeatureInfo();
    context_group_ = scoped_refptr<ContextGroup>(new ContextGroup(
        gpu_preferences_, false, &mailbox_manager_, nullptr, nullptr, nullptr,
        feature_info_, false, &image_manager_, nullptr, nullptr,
        GpuFeatureInfo(), &discardable_manager_, nullptr,
        &shared_image_manager_));
    TestHelper::SetupContextGroupInitExpectations(
        gl_.get(), DisallowedFeatures(), "", "", CONTEXT_TYPE_OPENGLES2, false);
    context_group_->Initialize(decoder_.get(), CONTEXT_TYPE_OPENGLES2,
                               DisallowedFeatures());
    texture_manager_ = context_group_->texture_manager();
    texture_manager_->AddObserver(&destruction_observer_);
  }

  void TearDown() override {
    EXPECT_CALL(destruction_observer_, OnTextureManagerDestroying(_))
        .RetiresOnSaturation();
    // Texture manager will destroy the 6 black/default textures.
    EXPECT_CALL(*gl_, DeleteTextures(TextureManager::kNumDefaultTextures, _));

    context_group_->Destroy(decoder_.get(), true);
    context_group_ = nullptr;
    EXPECT_EQ(0u, discardable_manager_.NumCacheEntriesForTesting());
    GpuServiceTest::TearDown();
  }

  void ExpectUnlockedTextureDeletion(uint32_t client_id) {
    TextureRef* ref = discardable_manager_.UnlockedTextureRefForTesting(
        client_id, texture_manager_);
    ExpectTextureRefDeletion(ref);
  }

  void ExpectTextureDeletion(uint32_t client_id) {
    TextureRef* ref = texture_manager_->GetTexture(client_id);
    ExpectTextureRefDeletion(ref);
  }

  void ExpectTextureRefDeletion(TextureRef* ref) {
    EXPECT_NE(nullptr, ref);
    ref->AddObserver();
    EXPECT_CALL(destruction_observer_, OnTextureRefDestroying(ref))
        .WillOnce(Invoke([](TextureRef* ref) { ref->RemoveObserver(); }));
    EXPECT_CALL(*gl_, DeleteTextures(1, Pointee(ref->service_id())))
        .RetiresOnSaturation();
  }

  MailboxManagerImpl mailbox_manager_;
  TraceOutputter outputter_;
  ImageManager image_manager_;
  ServiceDiscardableManager discardable_manager_;
  SharedImageManager shared_image_manager_;
  GpuPreferences gpu_preferences_;
  scoped_refptr<FeatureInfo> feature_info_;
  MockDestructionObserver destruction_observer_;
  TextureManager* texture_manager_;
  FakeCommandBufferServiceBase command_buffer_service_;
  FakeDecoderClient client_;
  std::unique_ptr<MockGLES2Decoder> decoder_;
  scoped_refptr<gles2::ContextGroup> context_group_;
};

TEST_F(ServiceDiscardableManagerTest, BasicUsage) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;

  // Create and insert a new texture.
  texture_manager_->CreateTexture(kClientId, kServiceId);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(kClientId, kSmallTextureSize,
                                           texture_manager_, handle);
  EXPECT_EQ(1u, discardable_manager_.NumCacheEntriesForTesting());
  EXPECT_TRUE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                           texture_manager_));
  EXPECT_NE(nullptr, texture_manager_->GetTexture(kClientId));

  // Unlock the texture, ServiceDiscardableManager should take ownership of the
  // TextureRef.
  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_FALSE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                            texture_manager_));
  EXPECT_EQ(nullptr, texture_manager_->GetTexture(kClientId));

  // Re-lock the texture, the TextureManager should now resume ownership of
  // the TextureRef.
  discardable_manager_.LockTexture(kClientId, texture_manager_);
  EXPECT_NE(nullptr, texture_manager_->GetTexture(kClientId));

  // Delete the texture from the TextureManager, it should also be removed from
  // the ServiceDiscardableManager.
  ExpectTextureDeletion(kClientId);
  texture_manager_->RemoveTexture(kClientId);
  EXPECT_EQ(0u, discardable_manager_.NumCacheEntriesForTesting());
}

TEST_F(ServiceDiscardableManagerTest, DeleteAtShutdown) {
  // Create 8 small textures (which will not hit memory limits), leaving every
  // other one unlocked.
  for (int i = 1; i <= 8; ++i) {
    texture_manager_->CreateTexture(i, i);
    auto handle = CreateLockedServiceHandleForTesting();
    discardable_manager_.InsertLockedTexture(i, kSmallTextureSize,
                                             texture_manager_, handle);
    if (i % 2) {
      TextureRef* texture_to_unbind;
      EXPECT_TRUE(discardable_manager_.UnlockTexture(i, texture_manager_,
                                                     &texture_to_unbind));
      EXPECT_NE(nullptr, texture_to_unbind);
    }
  }

  // Expect that all 8 will be deleted at shutdown, regardless of
  // locked/unlocked state.
  for (int i = 1; i <= 8; ++i) {
    if (i % 2) {
      ExpectUnlockedTextureDeletion(i);
    } else {
      ExpectTextureDeletion(i);
    }
  }

  // Let the test shut down, the expectations should be fulfilled.
}

TEST_F(ServiceDiscardableManagerTest, UnlockInvalid) {
  const GLuint kClientId = 1;
  gles2::TextureRef* texture_to_unbind;
  EXPECT_FALSE(discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                                  &texture_to_unbind));
  EXPECT_EQ(nullptr, texture_to_unbind);
}

TEST_F(ServiceDiscardableManagerTest, Limits) {
  // Size textures so that four fit in the discardable manager.
  const size_t cache_size_limit = 4 * 1024 * 1024;
  const size_t texture_size = cache_size_limit / 4;
  const size_t large_texture_size = 3 * texture_size;

  discardable_manager_.SetCacheSizeLimitForTesting(cache_size_limit);

  // Create 4 textures, this should fill up the discardable cache.
  for (int i = 1; i < 5; ++i) {
    texture_manager_->CreateTexture(i, i);
    auto handle = CreateLockedServiceHandleForTesting();
    discardable_manager_.InsertLockedTexture(i, texture_size, texture_manager_,
                                             handle);
  }

  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(3, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(1, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(2, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(4, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);

  // Allocate four more textures - the previous 4 should be evicted / deleted in
  // LRU order.
  {
    InSequence s;
    ExpectUnlockedTextureDeletion(3);
    ExpectUnlockedTextureDeletion(1);
    ExpectUnlockedTextureDeletion(2);
    ExpectUnlockedTextureDeletion(4);
  }

  for (int i = 5; i < 9; ++i) {
    texture_manager_->CreateTexture(i, i);
    auto handle = CreateLockedServiceHandleForTesting();
    discardable_manager_.InsertLockedTexture(i, texture_size, texture_manager_,
                                             handle);
  }

  // Ensure that the above expectations are handled by this point.
  Mock::VerifyAndClearExpectations(gl_.get());
  Mock::VerifyAndClearExpectations(&destruction_observer_);

  // Unlock the next four textures:
  EXPECT_TRUE(discardable_manager_.UnlockTexture(5, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(6, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(8, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(7, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);

  // Allocate one more *large* texture, it should evict the LRU 3 textures.
  {
    InSequence s;
    ExpectUnlockedTextureDeletion(5);
    ExpectUnlockedTextureDeletion(6);
    ExpectUnlockedTextureDeletion(8);
  }

  texture_manager_->CreateTexture(9, 9);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(9, large_texture_size,
                                           texture_manager_, handle);

  // Expect the two remaining textures to clean up.
  ExpectTextureDeletion(9);
  ExpectUnlockedTextureDeletion(7);
}

TEST_F(ServiceDiscardableManagerTest, TextureSizeChanged) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;

  texture_manager_->CreateTexture(kClientId, kServiceId);
  TextureRef* texture_ref = texture_manager_->GetTexture(kClientId);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(kClientId, 0, texture_manager_,
                                           handle);
  EXPECT_EQ(0u, discardable_manager_.TotalSizeForTesting());
  texture_manager_->SetTarget(texture_ref, GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(texture_ref, GL_TEXTURE_2D, 0, GL_RGBA,
                                 kSmallTextureDim, kSmallTextureDim, 1, 0,
                                 GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(kSmallTextureDim, kSmallTextureDim));
  EXPECT_EQ(kSmallTextureSize, discardable_manager_.TotalSizeForTesting());

  ExpectTextureDeletion(kClientId);
}

TEST_F(ServiceDiscardableManagerTest, OwnershipOnUnlock) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;

  std::unique_ptr<ServiceDiscardableHandle> service_handle;
  std::unique_ptr<ClientDiscardableHandle> client_handle;
  CreateLockedHandlesForTesting(&service_handle, &client_handle);
  texture_manager_->CreateTexture(kClientId, kServiceId);
  discardable_manager_.InsertLockedTexture(kClientId, kSmallTextureSize,
                                           texture_manager_, *service_handle);

  // Ensure that the service ref count is used to determine ownership changes.
  client_handle->Lock();
  TextureRef* texture_to_unbind;
  discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                     &texture_to_unbind);
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                           texture_manager_));

  // Get the counts back in sync.
  discardable_manager_.LockTexture(kClientId, texture_manager_);
  discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                     &texture_to_unbind);
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_FALSE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                            texture_manager_));

  // Re-lock the texture twice.
  client_handle->Lock();
  discardable_manager_.LockTexture(kClientId, texture_manager_);
  client_handle->Lock();
  discardable_manager_.LockTexture(kClientId, texture_manager_);

  // Ensure that unlocking once doesn't cause us to unbind the texture.
  discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                     &texture_to_unbind);
  EXPECT_EQ(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                           texture_manager_));

  // The second unlock should unbind/unlock the texture.
  discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                     &texture_to_unbind);
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_FALSE(discardable_manager_.IsEntryLockedForTesting(kClientId,
                                                            texture_manager_));

  ExpectUnlockedTextureDeletion(kClientId);
}

TEST_F(ServiceDiscardableManagerTest, BindGeneratedTextureLock) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;
  const GLuint kGeneratedServiceId = 3;

  // Create and insert a new texture.
  texture_manager_->CreateTexture(kClientId, kServiceId);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(kClientId, kSmallTextureSize,
                                           texture_manager_, handle);

  // Unlock the texture, ServiceDiscardableManager should take ownership of the
  // TextureRef.
  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_EQ(nullptr, texture_manager_->GetTexture(kClientId));

  // Generate a new texture for the given client id, similar to "bind generates
  // resource" behavior.
  texture_manager_->CreateTexture(kClientId, kGeneratedServiceId);
  TextureRef* generated_texture_ref = texture_manager_->GetTexture(kClientId);

  // Re-lock the texture, the TextureManager should delete the returned
  // texture and keep the generated one.
  ExpectUnlockedTextureDeletion(kClientId);
  discardable_manager_.LockTexture(kClientId, texture_manager_);
  EXPECT_EQ(generated_texture_ref, texture_manager_->GetTexture(kClientId));

  // Delete the texture from the TextureManager, it should also be removed from
  // the ServiceDiscardableManager.
  ExpectTextureDeletion(kClientId);
  texture_manager_->RemoveTexture(kClientId);
  EXPECT_EQ(0u, discardable_manager_.NumCacheEntriesForTesting());
}

TEST_F(ServiceDiscardableManagerTest, BindGeneratedTextureInitialization) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;
  const GLuint kGeneratedServiceId = 3;

  // Create and insert a new texture.
  texture_manager_->CreateTexture(kClientId, kServiceId);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(kClientId, kSmallTextureSize,
                                           texture_manager_, handle);

  // Unlock the texture, ServiceDiscardableManager should take ownership of the
  // TextureRef.
  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_EQ(nullptr, texture_manager_->GetTexture(kClientId));

  // Generate a new texture for the given client id, similar to "bind generates
  // resource" behavior.
  texture_manager_->CreateTexture(kClientId, kGeneratedServiceId);
  TextureRef* generated_texture_ref = texture_manager_->GetTexture(kClientId);

  // Re-initialize the texture, the TextureManager should delete the old
  // texture and keep the generated one.
  ExpectUnlockedTextureDeletion(kClientId);
  discardable_manager_.InsertLockedTexture(kClientId, kSmallTextureSize,
                                           texture_manager_, handle);
  EXPECT_EQ(generated_texture_ref, texture_manager_->GetTexture(kClientId));

  ExpectTextureDeletion(kClientId);
}

TEST_F(ServiceDiscardableManagerTest, BindGeneratedTextureSizeChange) {
  const GLuint kClientId = 1;
  const GLuint kServiceId = 2;
  const GLuint kGeneratedServiceId = 3;

  // Create and insert a new texture.
  texture_manager_->CreateTexture(kClientId, kServiceId);
  auto handle = CreateLockedServiceHandleForTesting();
  discardable_manager_.InsertLockedTexture(kClientId, 0, texture_manager_,
                                           handle);

  // Unlock the texture, ServiceDiscardableManager should take ownership of the
  // TextureRef.
  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(kClientId, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_EQ(nullptr, texture_manager_->GetTexture(kClientId));

  // Generate a new texture for the given client id, similar to "bind generates
  // resource" behavior.
  texture_manager_->CreateTexture(kClientId, kGeneratedServiceId);
  TextureRef* generated_texture_ref = texture_manager_->GetTexture(kClientId);

  // Re-size the generated texture. The tracked size should update.
  EXPECT_EQ(0u, discardable_manager_.TotalSizeForTesting());
  texture_manager_->SetTarget(generated_texture_ref, GL_TEXTURE_2D);
  texture_manager_->SetLevelInfo(generated_texture_ref, GL_TEXTURE_2D, 0,
                                 GL_RGBA, kSmallTextureDim, kSmallTextureDim, 1,
                                 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                 gfx::Rect(kSmallTextureDim, kSmallTextureDim));
  EXPECT_EQ(kSmallTextureSize, discardable_manager_.TotalSizeForTesting());

  ExpectUnlockedTextureDeletion(kClientId);
  ExpectTextureDeletion(kClientId);
}

TEST_F(ServiceDiscardableManagerTest, MemoryPressure) {
  // Size textures so that four fit in the discardable manager.
  const size_t cache_size_limit = 4 * 1024 * 1024;
  const size_t texture_size = cache_size_limit / 4;

  discardable_manager_.SetCacheSizeLimitForTesting(cache_size_limit);

  // Create 4 textures, this should fill up the discardable cache.
  for (int i = 1; i < 5; ++i) {
    texture_manager_->CreateTexture(i, i);
    auto handle = CreateLockedServiceHandleForTesting();
    discardable_manager_.InsertLockedTexture(i, texture_size, texture_manager_,
                                             handle);
  }

  // A memory pressure call should have no impact, as all textures are locked.
  discardable_manager_.HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Unlock one texture.
  gles2::TextureRef* texture_to_unbind;
  EXPECT_TRUE(discardable_manager_.UnlockTexture(3, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);

  // Send memory pressure critical again - this should delete the unlocked
  // texture, but not the others.
  ExpectUnlockedTextureDeletion(3);
  discardable_manager_.HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Unlock the remaining textures
  EXPECT_TRUE(discardable_manager_.UnlockTexture(1, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(2, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);
  EXPECT_TRUE(discardable_manager_.UnlockTexture(4, texture_manager_,
                                                 &texture_to_unbind));
  EXPECT_NE(nullptr, texture_to_unbind);

  // Send memory pressure moderate - this should delete all but one texture
  // (cache is capped at 1/4 size).
  {
    InSequence s;
    ExpectUnlockedTextureDeletion(1);
    ExpectUnlockedTextureDeletion(2);
  }
  discardable_manager_.HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Send memory pressure critical again - this should delete the remaining
  // textures.
  ExpectUnlockedTextureDeletion(4);
  discardable_manager_.HandleMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}

}  // namespace gles2
}  // namespace gpu
