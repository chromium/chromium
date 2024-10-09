// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_swap_buffer_provider.h"

#include <dawn/dawn_proc.h>
#include <dawn/wire/WireClient.h>
#include <dawn/wire/WireServer.h>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "gpu/command_buffer/client/webgpu_interface_stub.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/drawing_buffer_test_helpers.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_cpp.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_native_test_support.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace blink {

namespace {

class MockWebGPUInterface : public gpu::webgpu::WebGPUInterfaceStub {
 public:
  MOCK_METHOD(gpu::webgpu::ReservedTexture,
              ReserveTexture,
              (WGPUDevice device, const WGPUTextureDescriptor* optionalDesc));

  // NOTE: Can switch to using mock if tracking state manually grows to be
  // unwieldy.
  void AssociateMailbox(GLuint,
                        GLuint,
                        GLuint,
                        GLuint,
                        uint64_t,
                        uint64_t internal_usage,
                        const WGPUTextureFormat*,
                        GLuint,
                        gpu::webgpu::MailboxFlags,
                        const gpu::Mailbox&) override {
    internal_usage_from_most_recent_associate_mailbox_call =
        static_cast<wgpu::TextureUsage>(internal_usage);
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
  wgpu::TextureUsage internal_usage_from_most_recent_associate_mailbox_call =
      wgpu::TextureUsage::None;

 private:
  uint64_t token_id_ = 42;
};

class FakeProviderClient : public WebGPUSwapBufferProvider::Client {
 public:
  void OnTextureTransferred() override {
    DCHECK(texture);
    texture = nullptr;
  }

  void SetNeedsCompositingUpdate() override {}

  scoped_refptr<WebGPUMailboxTexture> texture;
};

class WebGPUSwapBufferProviderForTests : public WebGPUSwapBufferProvider {
 public:
  WebGPUSwapBufferProviderForTests(
      bool* alive,
      FakeProviderClient* client,
      const wgpu::Device& device,
      scoped_refptr<DawnControlClientHolder> dawn_control_client,
      wgpu::TextureUsage usage,
      wgpu::TextureUsage internal_usage,
      wgpu::TextureFormat format,
      PredefinedColorSpace color_space,
      const gfx::HDRMetadata& hdr_metadata)
      : WebGPUSwapBufferProvider(client,
                                 dawn_control_client,
                                 device,
                                 usage,
                                 internal_usage,
                                 format,
                                 color_space,
                                 hdr_metadata),
        alive_(alive),
        client_(client) {
    texture_desc_ = {
        .usage = usage,
        .size = {0, 0, 1},
        .format = format,
    };
    texture_internal_usage_ = {{
        .internalUsage = internal_usage,
    }};
    texture_desc_.nextInChain = &texture_internal_usage_;
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
  raw_ptr<bool> alive_;
  raw_ptr<FakeProviderClient> client_;
  wgpu::TextureDescriptor texture_desc_;
  wgpu::DawnTextureInternalUsageDescriptor texture_internal_usage_;
};

class WireSerializer : public dawn::wire::CommandSerializer {
 public:
  size_t GetMaximumAllocationSize() const override { return sizeof(buf_); }

  void SetHandler(dawn::wire::CommandHandler* handler) { handler_ = handler; }

  void* GetCmdSpace(size_t size) override {
    if (size > sizeof(buf_)) {
      return nullptr;
    }
    if (sizeof(buf_) - size < offset_) {
      if (!Flush()) {
        return nullptr;
      }
    }
    char* result = &buf_[offset_];
    offset_ += size;
    return result;
  }

  bool Flush() override {
    bool success = handler_->HandleCommands(buf_, offset_) != nullptr;
    offset_ = 0;
    return success;
  }

 private:
  size_t offset_ = 0;
  char buf_[1024 * 1024];
  raw_ptr<dawn::wire::CommandHandler> handler_;
};

}  // anonymous namespace

class WebGPUSwapBufferProviderTest : public testing::Test {
 protected:
  static constexpr wgpu::TextureFormat kFormat =
      wgpu::TextureFormat::RGBA8Unorm;
  static constexpr wgpu::TextureUsage kUsage =
      wgpu::TextureUsage::RenderAttachment;
  static constexpr wgpu::TextureUsage kInternalUsage =
      wgpu::TextureUsage::CopyDst;

  void SetUp() override {
    auto webgpu = std::make_unique<MockWebGPUInterface>();
    webgpu_ = webgpu.get();

    Platform::SetMainThreadTaskRunnerForTesting();

    auto provider = std::make_unique<WebGraphicsContext3DProviderForTests>(
        std::move(webgpu));
    sii_ = provider->SharedImageInterface();

    c2s_serializer_.SetHandler(&wire_server_);
    s2c_serializer_.SetHandler(&wire_client_);
#if !BUILDFLAG(USE_DAWN)
    // If not USE_DAWN, then Dawn wire is not linked into the Blink code.
    // Instead the proc table is used. Set the procs to the wire procs to
    // unittest this platform where Dawn is not enabled by default yet.
    dawnProcSetProcs(&dawn::wire::client::GetProcs());
#endif

    wgpu::InstanceDescriptor instance_desc = {};
    auto reservation = wire_client_.ReserveInstance(
        reinterpret_cast<WGPUInstanceDescriptor*>(&instance_desc));

    WGPUInstance native_instance = MakeNativeWGPUInstance();
    wire_server_.InjectInstance(native_instance, reservation.handle);
    GetDawnNativeProcs().instanceRelease(native_instance);

    instance_ = wgpu::Instance::Acquire(reservation.instance);

    wgpu::RequestAdapterOptions options = {
        .backendType = wgpu::BackendType::Null,
    };
    instance_.RequestAdapter(
        &options, wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter,
            const char*) { adapter_ = std::move(adapter); });
    ASSERT_TRUE(c2s_serializer_.Flush());
    ASSERT_TRUE(s2c_serializer_.Flush());
    ASSERT_NE(adapter_, nullptr);

    wgpu::DeviceDescriptor deviceDesc = {};
    adapter_.RequestDevice(&deviceDesc, wgpu::CallbackMode::AllowSpontaneous,
                           [&](wgpu::RequestDeviceStatus, wgpu::Device device,
                               const char*) { device_ = std::move(device); });
    ASSERT_TRUE(c2s_serializer_.Flush());
    ASSERT_TRUE(s2c_serializer_.Flush());
    ASSERT_NE(device_, nullptr);

    dawn_control_client_ = base::MakeRefCounted<DawnControlClientHolder>(
        std::move(provider), scheduler::GetSingleThreadTaskRunnerForTesting());

    provider_ = base::MakeRefCounted<WebGPUSwapBufferProviderForTests>(
        &provider_alive_, &client_, device_.Get(), dawn_control_client_, kUsage,
        kInternalUsage, kFormat, PredefinedColorSpace::kSRGB,
        gfx::HDRMetadata());
  }

  void TearDown() override { Platform::UnsetMainThreadTaskRunnerForTesting(); }

  gpu::webgpu::ReservedTexture ReserveTextureImpl(
      WGPUDevice device,
      const WGPUTextureDescriptor* desc) {
    auto reserved = wire_client_.ReserveTexture(device, desc);
    gpu::webgpu::ReservedTexture result;
    result.texture = reserved.texture;
    result.id = reserved.handle.id;
    result.generation = reserved.handle.generation;
    result.deviceId = reserved.deviceHandle.id;
    result.deviceGeneration = reserved.deviceHandle.generation;
    return result;
  }

  base::test::TaskEnvironment task_environment_;

  WireSerializer c2s_serializer_;
  WireSerializer s2c_serializer_;
  dawn::wire::WireClient wire_client_{{.serializer = &c2s_serializer_}};
  dawn::wire::WireServer wire_server_{
      {.procs = &GetDawnNativeProcs(), .serializer = &s2c_serializer_}};
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;

  scoped_refptr<DawnControlClientHolder> dawn_control_client_;
  raw_ptr<MockWebGPUInterface> webgpu_;
  raw_ptr<gpu::TestSharedImageInterface> sii_;
  FakeProviderClient client_;
  scoped_refptr<WebGPUSwapBufferProviderForTests> provider_;
  bool provider_alive_ = true;
};

TEST_F(WebGPUSwapBufferProviderTest,
       VerifyDestructionCompleteAfterAllResourceReleased) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource1;
  viz::ReleaseCallback release_callback1;

  viz::TransferableResource resource2;
  viz::ReleaseCallback release_callback2;

  viz::TransferableResource resource3;
  viz::ReleaseCallback release_callback3;

  // Produce resources.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource2,
                                                     &release_callback2));

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  viz::ReleaseCallback release_callback;

  // Produce one resource of size kSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kOtherSize.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kOtherSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kOtherSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kSize again.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);
}

TEST_F(WebGPUSwapBufferProviderTest, VerifyInsertAndWaitSyncTokenCorrectly) {
  const gfx::Size kSize(10, 10);

  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;

  // Produce the first resource, check that WebGPU will wait for the creation of
  // the shared image
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(sii_->MostRecentGeneratedToken(),
            webgpu_->most_recent_waited_token);

  // WebGPU should produce a token so that the next of user of the resource can
  // synchronize properly
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(webgpu_->most_recent_generated_token, resource.sync_token());

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

  // Produce some swap buffers
  viz::ReleaseCallback release_callback_0;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_0));

  viz::ReleaseCallback release_callback_1;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  viz::ReleaseCallback release_callback_2;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  EXPECT_FALSE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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

  // Create swap buffers
  const gfx::Size kSize(10, 10);

  viz::ReleaseCallback release_callback_1;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_1));

  viz::ReleaseCallback release_callback_2;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kOtherSize);

  EXPECT_TRUE(
      shared_images.insert(provider_->GetCurrentMailboxForTesting()).second);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback_3));

  viz::ReleaseCallback release_callback_4;
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(gfx::Size(10, 10));

  dawn_control_client_->Destroy();

  viz::TransferableResource resource;
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
  viz::ReleaseCallback release_callback1;

  viz::TransferableResource resource2;
  viz::ReleaseCallback release_callback2;

  // Produce and prepare transferable resource
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 1);

  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource1,
                                                     &release_callback1));
  EXPECT_EQ(webgpu_->num_associated_mailboxes, 0);

  // Produce 2nd resource but this time neuters the provider. Mailbox must also
  // be dissociated.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  viz::ReleaseCallback release_callback1;

  // Produce and prepare transferable resource
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(
          Invoke([&](WGPUDevice device, const WGPUTextureDescriptor* desc) {
            return ReserveTextureImpl(device, desc);
          }));
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
  viz::ReleaseCallback release_callback;

  // Produce one resource of size kSize and check that the descriptor passed to
  // ReserveTexture is correct..
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(Invoke(
          [&](WGPUDevice device, const WGPUTextureDescriptor* desc) -> auto {
            EXPECT_NE(desc, nullptr);
            EXPECT_EQ(desc->size.width, static_cast<uint32_t>(kSize.width()));
            EXPECT_EQ(desc->size.height, static_cast<uint32_t>(kSize.height()));
            EXPECT_EQ(desc->size.depthOrArrayLayers, 1u);
            EXPECT_EQ(desc->format, static_cast<WGPUTextureFormat>(kFormat));
            EXPECT_EQ(desc->usage, static_cast<WGPUTextureUsage>(kUsage));
            EXPECT_EQ(desc->dimension, WGPUTextureDimension_2D);
            EXPECT_EQ(desc->mipLevelCount, 1u);
            EXPECT_EQ(desc->sampleCount, 1u);
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);
  EXPECT_TRUE(provider_->PrepareTransferableResource(nullptr, &resource,
                                                     &release_callback));
  EXPECT_EQ(kSize, resource.size);
  std::move(release_callback).Run(gpu::SyncToken(), false /* lostResource */);

  // Produce one resource of size kOtherSize. The descriptor passed to
  // ReserveTexture is updated accordingly.
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(Invoke([&](WGPUDevice device,
                           const WGPUTextureDescriptor* desc) -> auto {
        EXPECT_EQ(desc->size.width, static_cast<uint32_t>(kOtherSize.width()));
        EXPECT_EQ(desc->size.height,
                  static_cast<uint32_t>(kOtherSize.height()));
        return ReserveTextureImpl(device, desc);
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
  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _)).Times(0);

  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroSize));
  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroWidth));
  EXPECT_EQ(nullptr, provider_->GetNewTexture(kZeroHeight));
}

// Verifies that GetLastWebGPUMailboxTexture() returns empty information if no
// swapbuffer has been created.
TEST_F(WebGPUSwapBufferProviderTest,
       GetLastWebGPUMailboxTextureReturnsEmptyWithoutSwapBuffer) {
  auto mailbox_texture = provider_->GetLastWebGPUMailboxTexture();
  EXPECT_EQ(mailbox_texture, nullptr);
}

// Verifies that GetLastWebGPUMailboxTexture() returns a correctly-configured
// texture if a swapbuffer has been created.
TEST_F(WebGPUSwapBufferProviderTest,
       GetLastWebGPUMailboxTextureReturnsValidTextureWithSwapBuffer) {
  const gfx::Size kSize(10, 20);

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillRepeatedly(Invoke(
          [&](WGPUDevice device, const WGPUTextureDescriptor* desc) -> auto {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  auto mailbox_texture = provider_->GetLastWebGPUMailboxTexture();
  EXPECT_NE(mailbox_texture, nullptr);

  auto texture = mailbox_texture->GetTexture();
  EXPECT_EQ(texture.GetUsage(), kUsage);
  EXPECT_EQ(texture.GetFormat(), kFormat);
  EXPECT_EQ(texture.GetDepthOrArrayLayers(), 1u);
  EXPECT_EQ(texture.GetDimension(), wgpu::TextureDimension::e2D);
  EXPECT_EQ(texture.GetMipLevelCount(), 1u);
  EXPECT_EQ(texture.GetSampleCount(), 1u);
  EXPECT_EQ(texture.GetHeight(), static_cast<uint32_t>(kSize.height()));
  EXPECT_EQ(texture.GetWidth(), static_cast<uint32_t>(kSize.width()));
}

// Verifies that GetNewTexture() passes client-specified internal usages to
// AssociateMailbox() and additionally adds RenderAttachment as an internal
// usage when associating the mailbox to ensure that lazy clearing is supported.
TEST_F(WebGPUSwapBufferProviderTest,
       GetNewTexturePassesClientSpecifiedInternalUsagePlusRenderAttachment) {
  ASSERT_EQ(kInternalUsage & wgpu::TextureUsage::RenderAttachment, 0);

  const gfx::Size kSize(10, 20);

  EXPECT_EQ(webgpu_->internal_usage_from_most_recent_associate_mailbox_call,
            wgpu::TextureUsage::None);

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillOnce(Invoke(
          [&](WGPUDevice device, const WGPUTextureDescriptor* desc) -> auto {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  EXPECT_EQ(webgpu_->internal_usage_from_most_recent_associate_mailbox_call,
            kInternalUsage | wgpu::TextureUsage::RenderAttachment);
}

// Verifies that GetLastMailboxTexture() passes client-specified internal usages
// to AssociateMailbox() and doesn't additionally add RenderAttachment (since
// it does not instruct the decoder to do lazy clearing on this texture).
TEST_F(WebGPUSwapBufferProviderTest,
       GetLastMailboxTexturePassesClientSpecifiedInternalUsage) {
  ASSERT_EQ(kInternalUsage & wgpu::TextureUsage::RenderAttachment, 0);

  const gfx::Size kSize(10, 20);

  EXPECT_EQ(webgpu_->internal_usage_from_most_recent_associate_mailbox_call,
            wgpu::TextureUsage::None);

  EXPECT_CALL(*webgpu_, ReserveTexture(device_.Get(), _))
      .WillRepeatedly(Invoke(
          [&](WGPUDevice device, const WGPUTextureDescriptor* desc) -> auto {
            return ReserveTextureImpl(device, desc);
          }));
  provider_->GetNewTexture(kSize);

  provider_->GetLastWebGPUMailboxTexture();
  EXPECT_EQ(webgpu_->internal_usage_from_most_recent_associate_mailbox_call,
            kInternalUsage);
}

}  // namespace blink
