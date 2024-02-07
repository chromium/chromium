// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/texture_pool.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/sequence_id.h"
#include "media/gpu/android/mock_abstract_texture.h"
#include "media/gpu/test/fake_command_buffer_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using gpu::gles2::AbstractTexture;
using testing::_;
using testing::NiceMock;
using testing::Return;

class TexturePoolTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    helper_ = base::MakeRefCounted<FakeCommandBufferHelper>(task_runner_);
    texture_pool_ = new TexturePool(helper_);
    // Random sync token that HasData().
    sync_token_ = gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                                 gpu::CommandBufferId::FromUnsafeValue(1), 1);
    ASSERT_TRUE(sync_token_.HasData());
  }

  ~TexturePoolTest() override {
    helper_->StubLost();
    base::RunLoop().RunUntilIdle();
  }

  using WeakTexture = base::WeakPtr<MockAbstractTexture>;

  WeakTexture CreateAndAddTexture() {
    std::unique_ptr<MockAbstractTexture> texture =
        std::make_unique<MockAbstractTexture>();
    WeakTexture texture_weak = texture->AsWeakPtr();

    texture_pool_->AddTexture(std::move(texture));

    return texture_weak;
  }

  base::test::TaskEnvironment task_environment_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  gpu::SyncToken sync_token_;

  scoped_refptr<FakeCommandBufferHelper> helper_;
  scoped_refptr<TexturePool> texture_pool_;
};

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithContext) {
  // Test that adding then deleting a texture destroys it.
  WeakTexture texture = CreateAndAddTexture();
  texture_pool_->ReleaseTexture(texture.get(), sync_token_);

  // The texture should still exist until the sync token is cleared.
  ASSERT_TRUE(texture);

  // Once the sync token is released, then the context should be made current
  // and the texture should be destroyed.
  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, AddAndReleaseTexturesWithoutContext) {
  // Test that adding then deleting a texture destroys it, even if the context
  // was lost.
  WeakTexture texture = CreateAndAddTexture();
  helper_->ContextLost();
  texture_pool_->ReleaseTexture(texture.get(), sync_token_);
  ASSERT_TRUE(texture);

  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, NonEmptyPoolAfterStubDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  helper_->StubLost();
}

TEST_F(TexturePoolTest,
       NonEmptyPoolAfterStubWithoutContextDestructionDoesntCrash) {
  // Make sure that we can delete the stub, and verify that pool teardown still
  // works (doesn't crash) even though the pool is not empty.
  CreateAndAddTexture();

  helper_->ContextLost();
  helper_->StubLost();
}

TEST_F(TexturePoolTest, TexturePoolRetainsReferenceWhileWaiting) {
  // Dropping our reference to |texture_pool_| while it's waiting for a sync
  // token shouldn't prevent the wait from completing.
  WeakTexture texture = CreateAndAddTexture();
  texture_pool_->ReleaseTexture(texture.get(), sync_token_);

  // The texture should still exist until the sync token is cleared.
  ASSERT_TRUE(texture);

  // Drop the texture pool while it's waiting.  Nothing should happen.
  texture_pool_ = nullptr;
  ASSERT_TRUE(texture);

  // The texture should be destroyed after the sync token completes.
  helper_->ReleaseSyncToken(sync_token_);
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

TEST_F(TexturePoolTest, TexturePoolReleasesImmediatelyWithoutSyncToken) {
  // If we don't provide a sync token, then it should release the texture.
  WeakTexture texture = CreateAndAddTexture();
  texture_pool_->ReleaseTexture(texture.get(), gpu::SyncToken());
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(texture);
}

}  // namespace media
