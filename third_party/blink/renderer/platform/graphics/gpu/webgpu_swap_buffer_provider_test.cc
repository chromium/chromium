// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace blink {

namespace {

class MockWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  MockWebGPUInterface() {
    // WebGPU functions the tests will call. No-op them since we don't have a
    // real WebGPU device.
    procs()->deviceReference = [](WGPUDevice) {};
    procs()->deviceRelease = [](WGPUDevice) {};
    procs()->textureReference = [](WGPUTexture) {};
    procs()->textureRelease = [](WGPUTexture) {};
  }

  MOCK_METHOD(gpu::webgpu::ReservedTexture,
              ReserveTexture,
              (WGPUDevice device, const WGPUTextureDescriptor* optionalDesc));

  // Could have used mock, but we only care about number of associated
  // mailboxes, so use override for now
  void AssociateMailbox(GLuint,
                        GLuint,
                        GLuint,
                        GLuint,
                        GLuint,
                        gpu::webgpu::MailboxFlags,
                        const GLbyte*) override {
    num_associated_mailboxes++;
  }
  void DissociateMailbox(GLuint, GLuint) override {
    num_associated_mailboxes--;
  }
  void DissociateMailboxForPresent(GLuint, GLuint, GLuint, GLuint) override {
    num_associated_mailboxes--;
  }

  // It is hard to use GMock with SyncTokens represented as GLByte*, instead we
  // remember which were the last sync tokens generated or waited upon.
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override {
    most_recent_generated_token =
        gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                       gpu::CommandBufferId(), ++token_id_);
    memcpy(sync_token, &most_recent_generated_token, sizeof(gpu::SyncToken));
  }
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override {
    most_recent_generated_token =
        gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                       gpu::CommandBufferId(), ++token_id_);
    most_recent_generated_token.SetVerifyFlush();
    memcpy(sync_token, &most_recent_generated_token, sizeof(gpu::SyncToken));
  }

  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token_data) override {
    memcpy(&most_recent_waited_token, sync_token_data, sizeof(gpu::SyncToken));
  }

  gpu::SyncToken most_recent_generated_token;
  gpu::SyncToken most_recent_waited_token;

  int num_associated_mailboxes = 0;

 private:
  uint64_t token_id_ = 42;
};

class FakeProviderClient : public WebGPUSwapBufferProvider::Client {
 public:
  void OnTextureTransferred() override {
    DCHECK(texture);
    texture = nullptr;
  }

  scoped_refptr<WebGPUMailboxTexture> texture;
};

class WebGPUSwapBufferProviderForTests : public WebGPUSwapBufferProvider {
 public:
  WebGPUSwapBufferProviderForTests(
      bool* alive,
      FakeProviderClient* client,
      WGPUDevice device,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUTextureUsage usage,
      WGPUTextureFormat format,
      PredefinedColorSpace color_space)
      : WebGPUSwapBufferProvider(client,
                                 dawn_control_client,
                                 device,
                                 usage,
                                 format,
                                 color_space),
        alive_(alive),
        client_(client) {
    texture_desc_.nextInChain = nullptr;
    texture_desc_.usage = usage;
    texture_desc_.format = format;
    texture_desc_.size = {0, 0, 1};
    texture_desc_.mipLevelCount = 1;
    texture_desc_.sampleCount = 1;
    texture_desc_.dimension = WGPUTextureDimension_2D;
    texture_desc_.viewFormatCount = 0;
    texture_desc_.viewFormats = nullptr;
  }
  ~WebGPUSwapBufferProviderForTests() override { *alive_ = false; }

  scoped_refptr<WebGPUMailboxTexture> GetNewTexture(const gfx::Size& size) {
    // The alpha type is an optimization hint so just pass in opaque here.
    texture_desc_.size.width = size.width();
    texture_desc_.size.height = size.height();
    client_->texture = WebGPUSwapBufferProvider::GetNewTexture(
        texture_desc_, kOpaque_SkAlphaType);
    return client_->texture;
  }

 private:
  bool* alive_;
  FakeProviderClient* client_;
  WGPUTextureDescriptor texture_desc_;
};

}  // anonymous namespace

class WebGPUSwapBufferProviderTest : public testing::Test {
 protected:
  static constexpr WGPUTextureFormat kFormat = WGPUTextureFormat_RGBA8Unorm;
  static constexpr WGPUTextureUsage kUsage = WGPUTextureUsage_RenderAttachment;

  void SetUp() override {
    auto webgpu = std::make_unique<MockWebGPUInterface>();
    webgpu_ = webgpu.get();

    Platform::SetMainThreadTaskRunnerForTesting();

    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::move(webgpu));
    sii_ = provider->SharedImageInterface();

    dawn_control_client_ = base::MakeRefCounted<DawnControlClientHolder>(
        std::move(provider), scheduler::GetSingleThreadTaskRunnerForTesting());

    provider_ = base::MakeRefCounted<WebGPUSwapBufferProviderForTests>(
        &provider_alive_, &client_, fake_device_, dawn_control_client_, kUsage,
        kFormat, PredefinedColorSpace::kSRGB);
  }

  void TearDown() override { Platform::UnsetMainThreadTaskRunnerForTesting(); }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  MockWebGPUInterface* webgpu_;
  viz::TestSharedImageInterface* sii_;
  FakeProviderClient client_;
  scoped_refptr<WebGPUSwapBufferProviderForTests> provider_;
  bool provider_alive_ = true;
  WGPUDevice fake_device_ = reinterpret_cast<WGPUDevice>(this);
};

TEST_F(WebGPUSwapBufferProviderTest,
       VerifyDestructionCompleteAfterAllResourceReleased) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource1;
  gpu::webgpu::ReservedTexture reservation1 = {
      reinterpret_cast<WGPUTexture>(&resource1), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback1;

  viz::TransferableResource resource2;
  gpu::webgpu::ReservedTexture reservation2 = {
      reinterpret_cast<WGPUTexture>(&resource2), 2, 2, 1, 1};
  viz::ReleaseCallback release_callback2;

  viz::TransferableResource resource3;
  gpu::webgpu::ReservedTexture reservation3 = {
      reinterpret_cast<WGPUTexture>(&resource3), 3, 3, 1, 1};
  viz::ReleaseCallback release_callback3;

  // Produce resources.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation1));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));

  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation2));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource2,
                                                     &release_callback2));

  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation3));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource3,
                                                     &release_callback3));

  // Release resources one by one, the provider should only be freed when the
  // last one is called.
  provider_ = nullptr;
  std::move(release_callback1).Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, true);

  std::move(release_callback2).Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, true);

  std::move(release_callback3).Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, false);
}

TEST_F(WebGPUSwapBufferProviderTest, VerifyResizingProperlyAffectsResources) {
  const gfx::Size kSize(10, 10);
  const gfx::Size kOtherSize(20, 20);

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback;

  // Produce one resource of size kSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kOtherSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kOtherSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kOtherSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kSize again.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
}

TEST_F(WebGPUSwapBufferProviderTest, VerifyInsertAndWaitSyncTokenCorrectly) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback;

  // Produce the first resource, check that WebGPU will wait for the creation of
  // the shared image
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(sii_->MostRecentGeneratedToken(),
            webgpu_->most_recent_waited_token);

  // WebGPU should produce a token so that the next of user of the resource can
  // synchronize properly
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(webgpu_->most_recent_generated_token,
            resource.mailbox_holder.sync_token);

  // Check that the release token is used to synchronize the shared image
  // destruction
  gpu::SyncToken release_token;
  webgpu_->GenSyncTokenCHROMIUM(release_token.GetData());
  std::move(release_callback).Run(release_token, false /* lostResource */);

  // Release the unused swap buffers held by the provider.
  provider_ = nullptr;

  EXPECT_EQ(sii_->MostRecentDestroyToken(), release_token);
}

// Ensures swap buffers will be recycled.
// Creates two swap buffers, destroys them, then creates them again.
TEST_F(WebGPUSwapBufferProviderTest, ReuseSwapBuffers) {
  const gfx::Size kSize(10, 10);

  base::flat_set<gpu::Mailbox> shared_images = {};

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};

  // Produce some swap buffers
  viz::ReleaseCallback release_callback_0;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_0));

  viz::ReleaseCallback release_callback_1;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  viz::ReleaseCallback release_callback_2;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_2));

  // Destroy the swap buffers.
  std::move(release_callback_0).Run(gpu::SyncToken(), false /* lostResource */);
  std::move(release_callback_1).Run(gpu::SyncToken(), false /* lostResource */);
  std::move(release_callback_2).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce two swap buffers
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_FALSE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_FALSE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_2));
}

// Ensures swap buffers will NOT be recycled if resized.
// Creates two swap buffers of a size, destroys them, then creates them again
// with a different size.
TEST_F(WebGPUSwapBufferProviderTest, ReuseSwapBufferResize) {
  base::flat_set<gpu::Mailbox> shared_images = {};

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};

  // Create swap buffers
  const gfx::Size kSize(10, 10);

  viz::ReleaseCallback release_callback_1;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  viz::ReleaseCallback release_callback_2;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_2));

  // Destroy swap buffers
  std::move(release_callback_1).Run(gpu::SyncToken(), false /* lostResource */);
  std::move(release_callback_2).Run(gpu::SyncToken(), false /* lostResource */);

  // Create swap buffers again with different size.
  const gfx::Size kOtherSize(20, 20);

  viz::ReleaseCallback release_callback_3;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kOtherSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_3));

  viz::ReleaseCallback release_callback_4;
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(kOtherSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_4));
}

// Regression test for crbug.com/1236418 where calling
// PrepareTransferableResource twice after the context is destroyed would hit a
// DCHECK.
TEST_F(WebGPUSwapBufferProviderTest,
       PrepareTransferableResourceTwiceAfterDestroy) {
  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};

  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation));
  provider_->GetNewTexture(gfx::Size(10, 10));

  dawn_control_client_->Destroy();

  viz::ReleaseCallback release_callback_1;
  EXPECT_FALSE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                      &release_callback_1));

  viz::ReleaseCallback release_callback_2;
  EXPECT_FALSE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                      &release_callback_2));
}

// Test that checks mailbox is dissociated when Neuter() is called.
TEST_F(WebGPUSwapBufferProviderTest, VerifyMailboxDissociationOnNeuter) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource1;
  gpu::webgpu::ReservedTexture reservation1 = {
      reinterpret_cast<WGPUTexture>(&resource1), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback1;

  viz::TransferableResource resource2;
  gpu::webgpu::ReservedTexture reservation2 = {
      reinterpret_cast<WGPUTexture>(&resource2), 2, 2, 1, 1};
  viz::ReleaseCallback release_callback2;

  // Produce and prepare transferable resource
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation1));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 1);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 0);

  // Produce 2nd resource but this time neuters the provider. Mailbox must also
  // be dissociated.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation2));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 1);

  provider_->Neuter();
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 0);
}

// Test that checks mailbox is not dissociated twice when both
// PrepareTransferableResource() and Neuter() are called.
TEST_F(WebGPUSwapBufferProviderTest, VerifyNoDoubleMailboxDissociation) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource1;
  gpu::webgpu::ReservedTexture reservation1 = {
      reinterpret_cast<WGPUTexture>(&resource1), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback1;

  // Produce and prepare transferable resource
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Return(reservation1));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 1);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 0);

  // Calling Neuter() won't dissociate mailbox again.
  provider_->Neuter();
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 0);
}

TEST_F(WebGPUSwapBufferProviderTest, ReserveTextureDescriptorForReflection) {
  const gfx::Size kSize(10, 10);
  const gfx::Size kOtherSize(20, 20);

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1, 1, 1};
  viz::ReleaseCallback release_callback;

  // Produce one resource of size kSize and check that the descriptor passed to
  // ReserveTexture is correct..
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Invoke([&](WGPUDevice device,
                           const WGPUTextureDescriptor* desc) -> auto{
        EXPECT_NE(desc, nullptr);
        EXPECT_EQ(desc->size.width, static_cast<uint32_t>(kSize.width()));
        EXPECT_EQ(desc->size.height, static_cast<uint32_t>(kSize.height()));
        EXPECT_EQ(desc->size.depthOrArrayLayers, 1u);
        EXPECT_EQ(desc->format, kFormat);
        EXPECT_EQ(desc->usage, kUsage);
        EXPECT_EQ(desc->dimension, WGPUTextureDimension_2D);
        EXPECT_EQ(desc->mipLevelCount, 1u);
        EXPECT_EQ(desc->sampleCount, 1u);
        return reservation;
      }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kOtherSize. The descriptor passed to
  // ReserveTexture is updated accordingly.
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _))
      .WillOnce(Invoke([&](WGPUDevice device,
                           const WGPUTextureDescriptor* desc) -> auto{
        EXPECT_EQ(desc->size.width, static_cast<uint32_t>(kOtherSize.width()));
        EXPECT_EQ(desc->size.height,
                  static_cast<uint32_t>(kOtherSize.height()));
        return reservation;
      }));
  provider_->GetNewTexture(kOtherSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kOtherSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
}

// Ensures that requests for zero size textures (width == 0 or height == 0) do
// not attempt to reserve a texture.
TEST_F(WebGPUSwapBufferProviderTest, VerifyZeroSizeRejects) {
  const gfx::Size kZeroSize(0, 0);
  const gfx::Size kZeroWidth(0, 10);
  const gfx::Size kZeroHeight(10, 0);

  // None of these calls should result in ReserveTexture being called
  EXPECT_CALL(*webgpu_, ReserveTexture(fake_device_, _)).Times(0);

  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroSize));
  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroWidth));
  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroHeight));
}

}  // namespace blink
