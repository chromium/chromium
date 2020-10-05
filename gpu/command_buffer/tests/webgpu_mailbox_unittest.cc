// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {
namespace {

class MockBufferMapCallback {
 public:
  MOCK_METHOD(void, Call, (WGPUBufferMapAsyncStatus status, void* userdata));
};
std::unique_ptr<testing::StrictMock<MockBufferMapCallback>>
    mock_buffer_map_callback;

void ToMockBufferMapCallback(WGPUBufferMapAsyncStatus status, void* userdata) {
  mock_buffer_map_callback->Call(status, userdata);
}

class MockUncapturedErrorCallback {
 public:
  MOCK_METHOD3(Call,
               void(WGPUErrorType type, const char* message, void* userdata));
};

std::unique_ptr<testing::StrictMock<MockUncapturedErrorCallback>>
    mock_device_error_callback;
void ToMockUncapturedErrorCallback(WGPUErrorType type,
                                   const char* message,
                                   void* userdata) {
  mock_device_error_callback->Call(type, message, userdata);
}

}  // namespace

class WebGPUMailboxTest : public WebGPUTest {
 protected:
  void SetUp() override {
    WebGPUTest::SetUp();
    Initialize(WebGPUTest::Options());
    mock_buffer_map_callback =
        std::make_unique<testing::StrictMock<MockBufferMapCallback>>();
    mock_device_error_callback =
        std::make_unique<testing::StrictMock<MockUncapturedErrorCallback>>();
  }

  void TearDown() override {
    mock_buffer_map_callback = nullptr;
    mock_device_error_callback = nullptr;
    WebGPUTest::TearDown();
  }

  struct AssociateMailboxCmdStorage {
    webgpu::cmds::AssociateMailboxImmediate cmd;
    GLbyte data[GL_MAILBOX_SIZE_CHROMIUM];
  };

  template <typename T>
  static error::Error ExecuteCmd(webgpu::WebGPUDecoder* decoder, const T& cmd) {
    static_assert(T::kArgFlags == cmd::kFixed,
                  "T::kArgFlags should equal cmd::kFixed");
    int entries_processed = 0;
    return decoder->DoCommands(1, (const void*)&cmd,
                               ComputeNumEntries(sizeof(cmd)),
                               &entries_processed);
  }

  template <typename T>
  static error::Error ExecuteImmediateCmd(webgpu::WebGPUDecoder* decoder,
                                          const T& cmd,
                                          size_t data_size) {
    static_assert(T::kArgFlags == cmd::kAtLeastN,
                  "T::kArgFlags should equal cmd::kAtLeastN");
    int entries_processed = 0;
    return decoder->DoCommands(1, (const void*)&cmd,
                               ComputeNumEntries(sizeof(cmd) + data_size),
                               &entries_processed);
  }
};

TEST_F(WebGPUMailboxTest, AssociateMailboxCmd) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();

  GetGpuServiceHolder()->ScheduleGpuTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::DawnDeviceClientID client_id,
         gpu::Mailbox mailbox) {
        // Error case: invalid mailbox
        {
          gpu::Mailbox bad_mailbox;
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 1, 0, WGPUTextureUsage_Sampled,
                       bad_mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(bad_mailbox.name)));
        }

        // Error case: device client id doesn't exist.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id + 1, 0, 1, 0, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: device generation is invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 42, 1, 0, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: texture ID invalid for the wire server.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 42, 42, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: invalid usage.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 42, 42, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: invalid texture usage.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 1, 0, WGPUTextureUsage_Force32,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Control case: test a successful call to AssociateMailbox
        // (1, 0) is a valid texture ID on dawn_wire server start.
        // The control case is not put first because it modifies the internal
        // state of the Dawn wire server and would make calls with the same
        // texture ID and generation invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 1, 0, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(decoder, cmd.cmd,
                                                         sizeof(mailbox.name)));
        }

        // Error case: associated to an already associated texture.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 1, 0, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Dissociate the image from the control case to remove its reference.
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(client_id, 1, 0);
          EXPECT_EQ(error::kNoError, ExecuteCmd(decoder, cmd));
        }
      },
      GetDecoder(), device_and_id.client_id, mailbox));

  GetGpuServiceHolder()->gpu_thread_task_runner()->RunsTasksInCurrentSequence();
}

TEST_F(WebGPUMailboxTest, DissociateMailboxCmd) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();

  GetGpuServiceHolder()->ScheduleGpuTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::DawnDeviceClientID client_id,
         gpu::Mailbox mailbox) {
        // Associate a mailbox so we can later dissociate it.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(client_id, 0, 1, 0, WGPUTextureUsage_Sampled,
                       mailbox.name);
          EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(decoder, cmd.cmd,
                                                         sizeof(mailbox.name)));
        }

        // Error case: wrong texture ID
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(client_id, 42, 0);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }

        // Error case: wrong texture generation
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(client_id, 1, 42);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }

        // Error case: invalid client device ID
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(client_id + 1, 1, 0);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }

        // Success case
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(client_id, 1, 0);
          EXPECT_EQ(error::kNoError, ExecuteCmd(decoder, cmd));
        }
      },
      GetDecoder(), device_and_id.client_id, mailbox));

  GetGpuServiceHolder()->gpu_thread_task_runner()->RunsTasksInCurrentSequence();
}

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// For simplicity of the test the image is shared between a Dawn device and
// itself: we render to it using the Dawn device, then re-associate it to a
// Dawn texture and read back the values that were written.
TEST_F(WebGPUMailboxTest, WriteToMailboxThenReadFromIt) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();
  wgpu::Device device = device_and_id.device;
  webgpu::DawnDeviceClientID device_client_id = device_and_id.client_id;

  // Part 1: Write to the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device_client_id);

    webgpu()->AssociateMailbox(
        device_client_id, 0, reservation.id, reservation.generation,
        WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox));
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Clear the texture using a render pass.
    wgpu::RenderPassColorAttachmentDescriptor color_desc = {};
    color_desc.attachment = texture.CreateView();
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_desc;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(device_client_id, reservation.id,
                                reservation.generation);
  }

  // Part 2: Read back the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device_client_id);

    webgpu()->AssociateMailbox(device_client_id, 0, reservation.id,
                               reservation.generation, WGPUTextureUsage_CopySrc,
                               reinterpret_cast<GLbyte*>(&mailbox));
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Copy the texture in a mappable buffer.
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

    wgpu::TextureCopyView copy_src = {};
    copy_src.texture = texture;
    copy_src.mipLevel = 0;
    copy_src.origin = {0, 0, 0};

    wgpu::BufferCopyView copy_dst = {};
    copy_dst.buffer = readback_buffer;
    copy_dst.layout.offset = 0;
    copy_dst.layout.bytesPerRow = 256;
    copy_dst.layout.rowsPerImage = 0;

    wgpu::Extent3D copy_size = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(device_client_id, reservation.id,
                                reservation.generation);

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4, ToMockBufferMapCallback,
                             nullptr);
    EXPECT_CALL(*mock_buffer_map_callback,
                Call(WGPUBufferMapAsyncStatus_Success, nullptr))
        .Times(1);

    WaitForCompletion(device);

    const void* data = readback_buffer.GetConstMappedRange(0, 4);
    EXPECT_EQ(0xFF00FF00, *static_cast<const uint32_t*>(data));
  }
}

// Tests that using a shared image aftr it is dissociated produces an error.
TEST_F(WebGPUMailboxTest, ErrorWhenUsingTextureAfterDissociate) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create a the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Create the device, and expect a validation error.
  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();
  wgpu::Device device = device_and_id.device;
  webgpu::DawnDeviceClientID device_client_id = device_and_id.client_id;

  device.SetUncapturedErrorCallback(ToMockUncapturedErrorCallback, 0);

  // Associate and immediately dissociate the image.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_client_id);
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  webgpu()->AssociateMailbox(
      device_client_id, 0, reservation.id, reservation.generation,
      WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox));
  webgpu()->DissociateMailbox(device_client_id, reservation.id,
                              reservation.generation);

  // Try using the texture, it should produce a validation error.
  wgpu::TextureView view = texture.CreateView();
  EXPECT_CALL(*mock_device_error_callback,
              Call(WGPUErrorType_Validation, testing::_, testing::_))
      .Times(1);
  WaitForCompletion(device);
}

// This is a regression test for an issue when using multiple shared images
// where a `ScopedAccess` was destroyed after it's `SharedImageRepresentation`.
// The code was similar to the following.
//
//   struct Pair {
//       unique_ptr<Representation> representation;
//       unique_ptr<Access> access;
//   };
//
//   base::flat_map<Key, Pair> map;
//   map.erase(some_iterator);
//
// In the Pair destructor C++ guarantees that `access` is destroyed before
// `representation` but `erase` can move one element over another, causing
// the move-assignment operator to be called. In this case the defaulted
// move-assignment would first move `representation` then `access`. Causing
// incorrect member destruction order for the move-to object.
TEST_F(WebGPUMailboxTest, UseA_UseB_DestroyA_DestroyB) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create a the shared images.
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox_a = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  Mailbox mailbox_b = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  // Get a WebGPU device to associate the shared images to.
  DeviceAndClientID device_and_id = GetNewDeviceAndClientID();
  wgpu::Device device = device_and_id.device;
  webgpu::DawnDeviceClientID device_client_id = device_and_id.client_id;

  // Associate both mailboxes
  gpu::webgpu::ReservedTexture reservation_a =
      webgpu()->ReserveTexture(device_client_id);
  webgpu()->AssociateMailbox(
      device_client_id, 0, reservation_a.id, reservation_a.generation,
      WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox_a));

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(device_client_id);
  webgpu()->AssociateMailbox(
      device_client_id, 0, reservation_b.id, reservation_b.generation,
      WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox_b));

  // Dissociate both mailboxes in the same order.
  webgpu()->DissociateMailbox(device_client_id, reservation_a.id,
                              reservation_a.generation);
  webgpu()->DissociateMailbox(device_client_id, reservation_b.id,
                              reservation_b.generation);

  // Send all the previous commands to the WebGPU decoder.
  webgpu()->FlushCommands();
}

// Regression test for a bug where the (id, generation) for associated shared
// images was stored globally instead of per-device. This meant that of two
// devices tried to create shared images with the same (id, generation) (which
// is possible because they can be on different Dawn wires) they would conflict.
TEST_F(WebGPUMailboxTest, AssociateOnTwoDevicesAtTheSameTime) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create a the shared images.
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox_a = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  Mailbox mailbox_b = sii->CreateSharedImage(
      viz::ResourceFormat::RGBA_8888, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  // Two WebGPU devices to associate the shared images to.
  DeviceAndClientID device_and_id_a = GetNewDeviceAndClientID();
  webgpu::DawnDeviceClientID client_id_a = device_and_id_a.client_id;

  DeviceAndClientID device_and_id_b = GetNewDeviceAndClientID();
  webgpu::DawnDeviceClientID client_id_b = device_and_id_b.client_id;

  // Associate both mailboxes
  gpu::webgpu::ReservedTexture reservation_a =
      webgpu()->ReserveTexture(client_id_a);
  webgpu()->AssociateMailbox(
      client_id_a, 0, reservation_a.id, reservation_a.generation,
      WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox_a));

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(client_id_b);
  webgpu()->AssociateMailbox(
      client_id_b, 0, reservation_b.id, reservation_b.generation,
      WGPUTextureUsage_OutputAttachment, reinterpret_cast<GLbyte*>(&mailbox_b));

  // Dissociate both mailboxes in the same order.
  webgpu()->DissociateMailbox(client_id_a, reservation_a.id,
                              reservation_a.generation);
  webgpu()->DissociateMailbox(client_id_b, reservation_b.id,
                              reservation_b.generation);

  // Send all the previous commands to the WebGPU decoder.
  webgpu()->FlushCommands();
}

}  // namespace gpu
