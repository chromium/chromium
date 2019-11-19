// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"

using testing::_;
using testing::Return;

namespace blink {

namespace {

class MockWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  MOCK_METHOD1(ReserveTexture, gpu::webgpu::ReservedTexture(WGPUDevice device));

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

 private:
  uint64_t token_id_ = 42;
};

class FakeProviderClient : public WebGPUSwapBufferProvider::Client {
 public:
  void OnTextureTransferred() override {}
};

class WebGPUSwapBufferProviderForTests : public WebGPUSwapBufferProvider {
 public:
  WebGPUSwapBufferProviderForTests(
      bool* alive,
      Client* client,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      WGPUTextureUsage usage,
      WGPUTextureFormat format)
      : WebGPUSwapBufferProvider(client, dawn_control_client, usage, format),
        alive_(alive) {}
  ~WebGPUSwapBufferProviderForTests() override { *alive_ = false; }

 private:
  bool* alive_;
};

}  // anonymous namespace

class WebGPUSwapBufferProviderTest : public testing::Test {
 protected:
  void SetUp() override {
    auto webgpu = std::make_unique<MockWebGPUInterface>();
    webgpu_ = webgpu.get();

    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::move(webgpu));
    sii_ = provider->SharedImageInterface();

    dawn_control_client_ =
        base::MakeRefCounted<DawnControlClientHolder>(std::move(provider));
    provider_ = base::MakeRefCounted<WebGPUSwapBufferProviderForTests>(
        &provider_alive_, &client_, dawn_control_client_,
        WGPUTextureUsage_OutputAttachment, WGPUTextureFormat_RGBA8Unorm);
  }

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  MockWebGPUInterface* webgpu_;
  viz::TestSharedImageInterface* sii_;
  FakeProviderClient client_;
  scoped_refptr<WebGPUSwapBufferProviderForTests> provider_;
  bool provider_alive_ = true;
};

TEST_F(WebGPUSwapBufferProviderTest,
       VerifyDestructionCompleteAfterAllResourceReleased) {
  const IntSize kSize(10, 10);

  viz::TransferableResource resource1;
  gpu::webgpu::ReservedTexture reservation1 = {
      reinterpret_cast<WGPUTexture>(&resource1), 1, 1};
  std::unique_ptr<viz::SingleReleaseCallback> release_callback1;

  viz::TransferableResource resource2;
  gpu::webgpu::ReservedTexture reservation2 = {
      reinterpret_cast<WGPUTexture>(&resource2), 2, 2};
  std::unique_ptr<viz::SingleReleaseCallback> release_callback2;

  viz::TransferableResource resource3;
  gpu::webgpu::ReservedTexture reservation3 = {
      reinterpret_cast<WGPUTexture>(&resource3), 3, 3};
  std::unique_ptr<viz::SingleReleaseCallback> release_callback3;

  // Produce resources.
  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation1));
  provider_->GetNewTexture(nullptr, kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));

  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation2));
  provider_->GetNewTexture(nullptr, kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource2,
                                                     &release_callback2));

  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation3));
  provider_->GetNewTexture(nullptr, kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource3,
                                                     &release_callback3));

  // Release resources one by one, the provider should only be freed when the
  // last one is called.
  provider_ = nullptr;
  release_callback1->Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, true);

  release_callback2->Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, true);

  release_callback3->Run(gpu::SyncToken(), false /* lostResource */);
  ASSERT_EQ(provider_alive_, false);
}

TEST_F(WebGPUSwapBufferProviderTest, VerifyResizingProperlyAffectsResources) {
  const IntSize kSize(10, 10);
  const IntSize kOtherSize(20, 20);

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1};
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  // Produce one resource of size kSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  provider_->GetNewTexture(nullptr, static_cast<IntSize>(kSize));
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(static_cast<gfx::Size>(kSize), sii_->MostRecentSize());
  release_callback->Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kOtherSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  provider_->GetNewTexture(nullptr, static_cast<IntSize>(kOtherSize));
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(static_cast<gfx::Size>(kOtherSize), sii_->MostRecentSize());
  release_callback->Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kSize again.
  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  provider_->GetNewTexture(nullptr, static_cast<IntSize>(kSize));
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(static_cast<gfx::Size>(kSize), sii_->MostRecentSize());
  release_callback->Run(gpu::SyncToken(), false /* lostResource */);
}

TEST_F(WebGPUSwapBufferProviderTest, VerifyInsertAndWaitSyncTokenCorrectly) {
  const IntSize kSize(10, 10);

  viz::TransferableResource resource;
  gpu::webgpu::ReservedTexture reservation = {
      reinterpret_cast<WGPUTexture>(&resource), 1, 1};
  std::unique_ptr<viz::SingleReleaseCallback> release_callback;

  // Produce the first resource, check that WebGPU will wait for the creation of
  // the shared image
  EXPECT_CALL(*webgpu_, ReserveTexture(_)).WillOnce(Return(reservation));
  provider_->GetNewTexture(nullptr, static_cast<IntSize>(kSize));
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
  release_callback->Run(release_token, false /* lostResource */);
  release_callback = nullptr;
  EXPECT_EQ(sii_->MostRecentDestroyToken(), release_token);
}

}  // namespace blink
