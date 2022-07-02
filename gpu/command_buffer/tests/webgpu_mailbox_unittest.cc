// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
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

#if BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/tests/gl_manager.h"
#include "ui/gl/gl_context.h"
#endif

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

struct WebGPUMailboxTestParams : WebGPUTest::Options {
  viz::ResourceFormat format;
};

std::ostream& operator<<(std::ostream& os,
                         const WebGPUMailboxTestParams& options) {
  switch (options.format) {
    case viz::ResourceFormat::RGBA_8888:
      os << "RGBA_8888";
      break;
    case viz::ResourceFormat::BGRA_8888:
      os << "BGRA_8888";
      break;
    case viz::ResourceFormat::RGBA_F16:
      os << "RGBA_F16";
      break;
    default:
      NOTREACHED();
  }
  if (options.enable_unsafe_webgpu) {
    os << "_UnsafeWebGPU";
  }
  if (options.force_fallback_adapter) {
    os << "_FallbackAdapter";
  }
  return os;
}

}  // namespace

class WebGPUMailboxTest
    : public WebGPUTest,
      public testing::WithParamInterface<WebGPUMailboxTestParams> {
 public:
  static std::vector<WebGPUMailboxTestParams> TestParams() {
    WebGPUMailboxTestParams options = {};

    WebGPUMailboxTestParams fallback_options = {};
    fallback_options.force_fallback_adapter = true;
    // Unsafe WebGPU currently required for SwiftShader fallback.
    fallback_options.enable_unsafe_webgpu = true;

    std::vector<WebGPUMailboxTestParams> params;

    for (viz::ResourceFormat format : {
// TODO(crbug.com/1241369): Handle additional formats.
#if !BUILDFLAG(IS_MAC)
           viz::ResourceFormat::RGBA_8888,
#endif  // !BUILDFLAG(IS_MAC)
               viz::ResourceFormat::BGRA_8888, viz::ResourceFormat::RGBA_F16,
         }) {
      WebGPUMailboxTestParams o = options;
      o.format = format;
      params.push_back(o);

      o = fallback_options;
      o.format = format;
      params.push_back(o);
    }
    return params;
  }

 protected:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    // Crashing on Mac M1. Currently missing stack trace. crbug.com/1271926
    // This must be checked before WebGPUTest::Initialize otherwise context
    // switched is locked and we cannot temporarily have this GLContext.
    GLManager gl_manager;
    gl_manager.Initialize(GLManager::Options());
    std::string renderer(gl_manager.context()->GetGLRenderer());
    if (renderer.find("Apple M1") != std::string::npos)
      mac_m1_ = true;
    gl_manager.Destroy();
#endif

    WebGPUTest::SetUp();
    Initialize(GetParam());

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

  void InitializeTextureColor(wgpu::Device device,
                              const Mailbox& mailbox,
                              wgpu::Color clearColor) {
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device.Get());

    webgpu()->AssociateMailbox(
        reservation.deviceId, reservation.deviceGeneration, reservation.id,
        reservation.generation, WGPUTextureUsage_RenderAttachment,
        webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox));
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Clear the texture using a render pass.
    wgpu::RenderPassColorAttachment color_desc = {};
    color_desc.view = texture.CreateView();
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearColor = clearColor;

    wgpu::RenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_desc;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(reservation.id, reservation.generation);
  }

#if BUILDFLAG(IS_MAC)
  bool mac_m1_ = false;
#endif
};

TEST_P(WebGPUMailboxTest, AssociateMailboxCmd) {
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
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  wgpu::Device device = GetNewDevice();
  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device.Get());

  GetGpuServiceHolder()->ScheduleGpuTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::ReservedTexture reservation,
         gpu::Mailbox mailbox) {
        // Error case: invalid mailbox
        {
          gpu::Mailbox bad_mailbox;
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, bad_mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(bad_mailbox.name)));
        }

        // Error case: device client id doesn't exist.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId + 1, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: device generation is invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration + 1,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: texture ID invalid for the wire server.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id + 1, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: invalid texture usage.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_Force32, webgpu::WEBGPU_MAILBOX_NONE,
                       mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Control case: test a successful call to AssociateMailbox.
        // The control case is not put first because it modifies the internal
        // state of the Dawn wire server and would make calls with the same
        // texture ID and generation invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(decoder, cmd.cmd,
                                                         sizeof(mailbox.name)));
        }

        // Error case: associated to an already associated texture.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Dissociate the image from the control case to remove its reference.
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(reservation.id, reservation.generation);
          EXPECT_EQ(error::kNoError, ExecuteCmd(decoder, cmd));
        }
      },
      GetDecoder(), reservation, mailbox));

  GetGpuServiceHolder()->gpu_thread_task_runner()->RunsTasksInCurrentSequence();
}

TEST_P(WebGPUMailboxTest, DissociateMailboxCmd) {
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
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  wgpu::Device device = GetNewDevice();
  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device.Get());

  GetGpuServiceHolder()->ScheduleGpuTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::ReservedTexture reservation,
         gpu::Mailbox mailbox) {
        // Associate a mailbox so we can later dissociate it.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       webgpu::WEBGPU_MAILBOX_NONE, mailbox.name);
          EXPECT_EQ(error::kNoError, ExecuteImmediateCmd(decoder, cmd.cmd,
                                                         sizeof(mailbox.name)));
        }

        // Error case: wrong texture ID
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(reservation.id + 1, reservation.generation);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }

        // Error case: wrong texture generation
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(reservation.id, reservation.generation + 1);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }

        // Success case
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(reservation.id, reservation.generation);
          EXPECT_EQ(error::kNoError, ExecuteCmd(decoder, cmd));
        }

        // Error case: dissociate an already dissociated mailbox
        {
          webgpu::cmds::DissociateMailbox cmd;
          cmd.Init(reservation.id, reservation.generation);
          EXPECT_EQ(error::kInvalidArguments, ExecuteCmd(decoder, cmd));
        }
      },
      GetDecoder(), reservation, mailbox));

  GetGpuServiceHolder()->gpu_thread_task_runner()->RunsTasksInCurrentSequence();
}

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// For simplicity of the test the image is shared between a Dawn device and
// itself: we render to it using the Dawn device, then re-associate it to a
// Dawn texture and read back the values that were written.
TEST_P(WebGPUMailboxTest, WriteToMailboxThenReadFromIt) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }
  if (GetParam().format == viz::ResourceFormat::RGBA_F16) {
    LOG(ERROR) << "Test skipped because RGBA_F16 isn't supported.";
    return;
  }
  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  wgpu::Device device = GetNewDevice();

  // Part 1: Write to the texture using Dawn
  InitializeTextureColor(device, mailbox, {0.0, 0.0, 1.0, 1.0});

  // Part 2: Read back the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device.Get());

    webgpu()->AssociateMailbox(
        reservation.deviceId, reservation.deviceGeneration, reservation.id,
        reservation.generation, WGPUTextureUsage_CopySrc,
        webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox));
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Copy the texture in a mappable buffer.
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

    wgpu::ImageCopyTexture copy_src = {};
    copy_src.texture = texture;
    copy_src.mipLevel = 0;
    copy_src.origin = {0, 0, 0};

    wgpu::ImageCopyBuffer copy_dst = {};
    copy_dst.buffer = readback_buffer;
    copy_dst.layout.offset = 0;
    copy_dst.layout.bytesPerRow = 256;

    wgpu::Extent3D copy_size = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(reservation.id, reservation.generation);

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4, ToMockBufferMapCallback,
                             nullptr);
    EXPECT_CALL(*mock_buffer_map_callback,
                Call(WGPUBufferMapAsyncStatus_Success, nullptr))
        .Times(1);

    WaitForCompletion(device);

    const void* data = readback_buffer.GetConstMappedRange();
    switch (GetParam().format) {
      case viz::ResourceFormat::RGBA_8888:
        EXPECT_EQ(0xFFFF0000u, *static_cast<const uint32_t*>(data));
        break;
      case viz::ResourceFormat::BGRA_8888:
        EXPECT_EQ(0xFF0000FFu, *static_cast<const uint32_t*>(data));
        break;
      default:
        NOTREACHED();
    }
  }
}

// Test that an uninitialized shared image is lazily cleared by Dawn when it is
// read.
TEST_P(WebGPUMailboxTest, ReadUninitializedSharedImage) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  wgpu::Device device = GetNewDevice();

  // Set the texture contents to non-zero so we can test a lazy clear occurs.
  InitializeTextureColor(device, mailbox, {1.0, 0, 0, 1.0});

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device.Get());

  // Associate the mailbox. Using WEBGPU_MAILBOX_DISCARD will set the contents
  // to uncleared.
  webgpu()->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                             reservation.id, reservation.generation,
                             WGPUTextureUsage_CopySrc,
                             webgpu::WEBGPU_MAILBOX_DISCARD,
                             reinterpret_cast<const GLbyte*>(&mailbox));
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = 4;
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Map the buffer and assert the pixel is the correct value.
  readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4, ToMockBufferMapCallback,
                           nullptr);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(WGPUBufferMapAsyncStatus_Success, nullptr))
      .Times(1);

  WaitForCompletion(device);

  const void* data = readback_buffer.GetConstMappedRange(0, 4);
  // Contents should be black because the texture was lazily cleared.
  EXPECT_EQ(0x00000000u, *static_cast<const uint32_t*>(data));
}

// Test that an uninitialized shared image is lazily cleared by Dawn when it is
// read.
TEST_P(WebGPUMailboxTest, ReadWritableUninitializedSharedImage) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  wgpu::Device device = GetNewDevice();

  // Set the texture contents to non-zero so we can test a lazy clear occurs.
  InitializeTextureColor(device, mailbox, {1.0, 0, 0, 1.0});

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device.Get());

  // Associate the mailbox. Using WEBGPU_MAILBOX_DISCARD will set the contents
  // to uncleared.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation,
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_DISCARD,
      reinterpret_cast<const GLbyte*>(&mailbox));
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Read the texture using a render pass. Load+Store the contents.
  // Uninitialized contents should not be loaded.
  wgpu::RenderPassColorAttachment color_desc = {};
  color_desc.view = texture.CreateView();
  color_desc.loadOp = wgpu::LoadOp::Load;
  color_desc.storeOp = wgpu::StoreOp::Store;

  wgpu::RenderPassDescriptor render_pass_desc = {};
  render_pass_desc.colorAttachmentCount = 1;
  render_pass_desc.colorAttachments = &color_desc;

  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
  pass.EndPass();

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = 4;
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device.GetQueue();
  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Map the buffer and assert the pixel is the correct value.
  readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4, ToMockBufferMapCallback,
                           nullptr);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(WGPUBufferMapAsyncStatus_Success, nullptr))
      .Times(1);

  WaitForCompletion(device);

  const void* data = readback_buffer.GetConstMappedRange(0, 4);
  // Contents should be black because the texture was lazily cleared.
  EXPECT_EQ(0x00000000u, *static_cast<const uint32_t*>(data));
}

// Tests that using a shared image aftr it is dissociated produces an error.
TEST_P(WebGPUMailboxTest, ErrorWhenUsingTextureAfterDissociate) {
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
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Create the device, and expect a validation error.
  wgpu::Device device = GetNewDevice();

  device.SetUncapturedErrorCallback(ToMockUncapturedErrorCallback, 0);

  // Associate and immediately dissociate the image.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device.Get());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_CopySrc,
      webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox));
  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  wgpu::TextureDescriptor dst_desc = {};
  dst_desc.size = {1, 1};
  dst_desc.usage = wgpu::TextureUsage::CopyDst;
  switch (GetParam().format) {
    case viz::ResourceFormat::RGBA_8888:
      dst_desc.format = wgpu::TextureFormat::RGBA8Unorm;
      break;
    case viz::ResourceFormat::BGRA_8888:
      dst_desc.format = wgpu::TextureFormat::BGRA8Unorm;
      break;
    case viz::ResourceFormat::RGBA_F16:
      dst_desc.format = wgpu::TextureFormat::RGBA16Float;
      break;
    default:
      NOTREACHED();
  }

  wgpu::ImageCopyTexture src_image = {};
  src_image.texture = texture;

  wgpu::ImageCopyTexture dst_image = {};
  dst_image.texture = device.CreateTexture(&dst_desc);

  wgpu::Extent3D extent = {1, 1};

  // Try using the texture in a copy command; it should produce a validation
  // error.
  wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
  encoder.CopyTextureToTexture(&src_image, &dst_image, &extent);
  wgpu::CommandBuffer commandBuffer = encoder.Finish();

  // Wait so it's clear the validation error after this when we call Submit.
  WaitForCompletion(device);
  device.GetQueue().Submit(1, &commandBuffer);

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
TEST_P(WebGPUMailboxTest, UseA_UseB_DestroyA_DestroyB) {
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
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);
  Mailbox mailbox_b = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  // Get a WebGPU device to associate the shared images to.
  wgpu::Device device = GetNewDevice();

  // Associate both mailboxes
  gpu::webgpu::ReservedTexture reservation_a =
      webgpu()->ReserveTexture(device.Get());
  webgpu()->AssociateMailbox(
      reservation_a.deviceId, reservation_a.deviceGeneration, reservation_a.id,
      reservation_a.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox_a));

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(device.Get());
  webgpu()->AssociateMailbox(
      reservation_b.deviceId, reservation_b.deviceGeneration, reservation_b.id,
      reservation_b.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox_b));

  // Dissociate both mailboxes in the same order.
  webgpu()->DissociateMailbox(reservation_a.id, reservation_a.generation);
  webgpu()->DissociateMailbox(reservation_b.id, reservation_b.generation);

  // Send all the previous commands to the WebGPU decoder.
  webgpu()->FlushCommands();
}

// Regression test for a bug where the (id, generation) for associated shared
// images was stored globally instead of per-device. This meant that of two
// devices tried to create shared images with the same (id, generation) (which
// is possible because they can be on different Dawn wires) they would conflict.
TEST_P(WebGPUMailboxTest, AssociateOnTwoDevicesAtTheSameTime) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

#if BUILDFLAG(IS_MAC)
  // Crashing on Mac M1. Currently missing stack trace. crbug.com/1271926
  if (mac_m1_)
    return;
#endif

  // Create a the shared images.
  SharedImageInterface* sii = GetSharedImageInterface();
  Mailbox mailbox_a = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  Mailbox mailbox_b = sii->CreateSharedImage(
      GetParam().format, {1, 1}, gfx::ColorSpace::CreateSRGB(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, SHARED_IMAGE_USAGE_WEBGPU,
      kNullSurfaceHandle);

  // Two WebGPU devices to associate the shared images to.
  wgpu::Device device_a = GetNewDevice();
  wgpu::Device device_b = GetNewDevice();

  // Associate both mailboxes
  gpu::webgpu::ReservedTexture reservation_a =
      webgpu()->ReserveTexture(device_a.Get());
  webgpu()->AssociateMailbox(
      reservation_a.deviceId, reservation_a.deviceGeneration, reservation_a.id,
      reservation_a.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox_a));

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(device_b.Get());
  webgpu()->AssociateMailbox(
      reservation_b.deviceId, reservation_b.deviceGeneration, reservation_b.id,
      reservation_b.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, reinterpret_cast<const GLbyte*>(&mailbox_b));

  // Dissociate both mailboxes in the same order.
  webgpu()->DissociateMailbox(reservation_a.id, reservation_a.generation);
  webgpu()->DissociateMailbox(reservation_b.id, reservation_b.generation);

  // Send all the previous commands to the WebGPU decoder.
  webgpu()->FlushCommands();
}

// Test that passing a descriptor to ReserveTexture produces a client-side
// WGPUTexture that correctly reflects said descriptor.
TEST_P(WebGPUMailboxTest, ReflectionOfDescriptor) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }

  wgpu::Device device = GetNewDevice();

  // Check that reserving a texture with a full descriptor give the same data
  // back through reflection.
  wgpu::TextureDescriptor desc1 = {};
  desc1.size = {1, 2, 3};
  desc1.format = wgpu::TextureFormat::R32Float;
  desc1.usage = wgpu::TextureUsage::CopyDst;
  desc1.dimension = wgpu::TextureDimension::e2D;
  desc1.sampleCount = 1;
  desc1.mipLevelCount = 1;
  gpu::webgpu::ReservedTexture reservation1 = webgpu()->ReserveTexture(
      device.Get(), reinterpret_cast<const WGPUTextureDescriptor*>(&desc1));
  wgpu::Texture texture1 = wgpu::Texture::Acquire(reservation1.texture);

  ASSERT_EQ(desc1.size.width, texture1.GetWidth());
  ASSERT_EQ(desc1.size.height, texture1.GetHeight());
  ASSERT_EQ(desc1.size.depthOrArrayLayers, texture1.GetDepthOrArrayLayers());
  ASSERT_EQ(desc1.format, texture1.GetFormat());
  ASSERT_EQ(desc1.usage, texture1.GetUsage());
  ASSERT_EQ(desc1.dimension, texture1.GetDimension());
  ASSERT_EQ(desc1.sampleCount, texture1.GetSampleCount());
  ASSERT_EQ(desc1.mipLevelCount, texture1.GetMipLevelCount());

  // Test with a different descriptor to check data is not hardcoded. Not that
  // this is actually not a valid descriptor (diimension == 1D with height !=
  // 1), but that it should still be reflected exactly.
  wgpu::TextureDescriptor desc2 = {};
  desc2.size = {4, 5, 6};
  desc2.format = wgpu::TextureFormat::RGBA8Unorm;
  desc2.usage = wgpu::TextureUsage::CopySrc;
  desc2.dimension = wgpu::TextureDimension::e1D;
  desc2.sampleCount = 4;
  desc2.mipLevelCount = 3;
  gpu::webgpu::ReservedTexture reservation2 = webgpu()->ReserveTexture(
      device.Get(), reinterpret_cast<const WGPUTextureDescriptor*>(&desc2));
  wgpu::Texture texture2 = wgpu::Texture::Acquire(reservation2.texture);

  ASSERT_EQ(desc2.size.width, texture2.GetWidth());
  ASSERT_EQ(desc2.size.height, texture2.GetHeight());
  ASSERT_EQ(desc2.size.depthOrArrayLayers, texture2.GetDepthOrArrayLayers());
  ASSERT_EQ(desc2.format, texture2.GetFormat());
  ASSERT_EQ(desc2.usage, texture2.GetUsage());
  ASSERT_EQ(desc2.dimension, texture2.GetDimension());
  ASSERT_EQ(desc2.sampleCount, texture2.GetSampleCount());
  ASSERT_EQ(desc2.mipLevelCount, texture2.GetMipLevelCount());
}

INSTANTIATE_TEST_SUITE_P(,
                         WebGPUMailboxTest,
                         ::testing::ValuesIn(WebGPUMailboxTest::TestParams()),
                         ::testing::PrintToStringParamName());

}  // namespace gpu
