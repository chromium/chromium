// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <dawn/native/DawnNative.h>

#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

#define SKIP_TEST_IF(condition) \
  if (condition)                \
  GTEST_SKIP() << #condition

namespace gpu {
namespace {

class MockBufferMapCallback {
 public:
  MOCK_METHOD(void, Call, (wgpu::MapAsyncStatus status, const char* message));
};
std::unique_ptr<testing::StrictMock<MockBufferMapCallback>>
    mock_buffer_map_callback;

void ToMockBufferMapCallback(wgpu::MapAsyncStatus status, const char* message) {
  mock_buffer_map_callback->Call(status, message);
}

struct WebGPUMailboxTestParams : WebGPUTest::Options {
  viz::SharedImageFormat format;
};

std::ostream& operator<<(std::ostream& os,
                         const WebGPUMailboxTestParams& options) {
  DCHECK(options.format == viz::SinglePlaneFormat::kRGBA_8888 ||
         options.format == viz::SinglePlaneFormat::kBGRA_8888 ||
         options.format == viz::SinglePlaneFormat::kRGBA_F16);
  os << options.format.ToTestParamString();

  if (options.use_skia_graphite) {
    os << "_SkiaGraphite";
  }
  if (options.enable_unsafe_webgpu) {
    os << "_UnsafeWebGPU";
  }
  if (options.force_fallback_adapter) {
    os << "_FallbackAdapter";
  }

  return os;
}

uint32_t BytesPerTexel(viz::SharedImageFormat format) {
  if ((format == viz::SinglePlaneFormat::kRGBA_8888) ||
      (format == viz::SinglePlaneFormat::kBGRA_8888)) {
    return 4;
  }

  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return 8;
  }

  NOTREACHED_IN_MIGRATION();
  return 0;
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

    for (viz::SharedImageFormat format : {
// TODO(crbug.com/40823053): Support "rgba8unorm" canvas context format on Mac
#if !BUILDFLAG(IS_MAC)
           viz::SinglePlaneFormat::kRGBA_8888,
#endif  // !BUILDFLAG(IS_MAC)
               viz::SinglePlaneFormat::kBGRA_8888,
               viz::SinglePlaneFormat::kRGBA_F16,
         }) {
      WebGPUMailboxTestParams o = options;
      o.format = format;
#if BUILDFLAG(IS_LINUX)
      // Linux does not support creation of RGBA_F16 GpuMemoryBuffers, which
      // causes SharedImage creation in these tests to fail.
      if (o.format != viz::SinglePlaneFormat::kRGBA_F16) {
        params.push_back(o);
      }
#else
      params.push_back(o);
#endif

      // Test SwiftShader fallback both with and without SkiaGraphite
      o = fallback_options;
      o.format = format;

      o.use_skia_graphite = false;
      params.push_back(o);

      // Note: Only windows & Mac have Graphite supported for now.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      o.use_skia_graphite = true;
      params.push_back(o);
#endif
    }
    return params;
  }

 protected:
  void SetUp() override {
    SKIP_TEST_IF(!WebGPUSupported());
    SKIP_TEST_IF(!WebGPUSharedImageSupported());
    WebGPUTest::SetUp();
    Initialize(GetParam());

    device_ = GetNewDevice();

    mock_buffer_map_callback =
        std::make_unique<testing::StrictMock<MockBufferMapCallback>>();
  }

  void TearDown() override {
    mock_buffer_map_callback = nullptr;
    // Wait for all operations to catch any validation or device lost errors.
    PollUntilIdle();
    device_ = nullptr;
    WebGPUTest::TearDown();
  }

  struct AssociateMailboxCmdStorage {
    webgpu::cmds::AssociateMailboxImmediate cmd;

    // Immediate data is copied into the space immediately following `cmd`.
    // Allocate space to hold up to 1 mailbox and 2 view formats.
    GLbyte data[GL_MAILBOX_SIZE_CHROMIUM];
    std::array<WGPUTextureFormat, 2u> view_formats;
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

  enum class AccessType { Read, Write, ReadWrite };

  SharedImageUsageSet GetSharedImageUsage(AccessType access_type) {
    SharedImageUsageSet webgpu_usage;

    // With the fallback adapter, reading/writing from the SharedImage will
    // occur via Skia.
    SharedImageUsageSet fallback_usage;

    switch (access_type) {
      case AccessType::Read:
        webgpu_usage = SHARED_IMAGE_USAGE_WEBGPU_READ;
        fallback_usage = SHARED_IMAGE_USAGE_RASTER_READ;
        break;
      case AccessType::Write:
        webgpu_usage = SHARED_IMAGE_USAGE_WEBGPU_WRITE;
        fallback_usage = SHARED_IMAGE_USAGE_RASTER_WRITE;
        break;
      case AccessType::ReadWrite:
        webgpu_usage =
            SHARED_IMAGE_USAGE_WEBGPU_READ | SHARED_IMAGE_USAGE_WEBGPU_WRITE;
        fallback_usage =
            SHARED_IMAGE_USAGE_RASTER_READ | SHARED_IMAGE_USAGE_RASTER_WRITE;
        break;
    }

    auto si_usage = webgpu_usage;
    if (IsUsingFallbackAdapter()) {
      si_usage |= fallback_usage;
    }

    return si_usage;
  }

  void InitializeTextureColor(wgpu::Device device,
                              const Mailbox& mailbox,
                              wgpu::Color clearValue) {
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device.Get());

    webgpu()->AssociateMailbox(
        reservation.deviceId, reservation.deviceGeneration, reservation.id,
        reservation.generation, WGPUTextureUsage_RenderAttachment,
        webgpu::WEBGPU_MAILBOX_NONE, mailbox);
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Clear the texture using a render pass.
    wgpu::RenderPassColorAttachment color_desc = {};
    color_desc.view = texture.CreateView();
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearValue = clearValue;

    wgpu::RenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_desc;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(reservation.id, reservation.generation);
  }

  void UninitializeTexture(wgpu::Device device, wgpu::Texture texture) {
    wgpu::RenderPassColorAttachment color_desc = {};
    color_desc.view = texture.CreateView();
    color_desc.loadOp = wgpu::LoadOp::Load;
    color_desc.storeOp = wgpu::StoreOp::Discard;

    wgpu::RenderPassDescriptor render_pass_desc = {};
    render_pass_desc.colorAttachmentCount = 1;
    render_pass_desc.colorAttachments = &color_desc;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);
  }

  wgpu::Device device_;
};

TEST_P(WebGPUMailboxTest, AssociateMailboxCmd) {
  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);

  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device_.Get());

  GetGpuServiceHolder()->ScheduleGpuMainTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::ReservedTexture reservation,
         scoped_refptr<gpu::ClientSharedImage> shared_image) {
        const gpu::Mailbox& mailbox = shared_image->mailbox();

        // Error case: device client id doesn't exist.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId + 1, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: device generation is invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration + 1,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: texture ID invalid for the wire server.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id + 1, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: invalid texture usage.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       UINT64_MAX, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Error case: invalid internal texture usage.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding,
                       UINT64_MAX, webgpu::WEBGPU_MAILBOX_NONE,
                       0u, ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
          EXPECT_EQ(
              error::kInvalidArguments,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(mailbox.name)));
        }

        // Prep packed data for packing view formats and the mailbox.
        std::vector<GLuint> packed_data;
        packed_data.resize(sizeof(mailbox.name) / sizeof(uint32_t));
        memcpy(reinterpret_cast<char*>(packed_data.data()), &mailbox.name,
               sizeof(mailbox.name));

        uint32_t view_format_count = 0u;
        if (GetParam().format == viz::SinglePlaneFormat::kRGBA_F16) {
        } else if (GetParam().format == viz::SinglePlaneFormat::kRGBA_8888) {
          view_format_count = 1u;
          packed_data.push_back(
              static_cast<uint32_t>(WGPUTextureFormat_RGBA8UnormSrgb));
        } else if (GetParam().format == viz::SinglePlaneFormat::kBGRA_8888) {
          view_format_count = 2u;
          packed_data.push_back(
              static_cast<uint32_t>(WGPUTextureFormat_BGRA8UnormSrgb));
          packed_data.push_back(
              static_cast<uint32_t>(WGPUTextureFormat_BGRA8Unorm));
        } else {
          NOTREACHED_IN_MIGRATION();
        }

        // Error case: packed data empty.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation, UINT64_MAX, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u, 0u, packed_data.data());
          EXPECT_EQ(error::kOutOfBounds,
                    ExecuteImmediateCmd(decoder, cmd.cmd, 0u));
        }

        // Error case: packed data smaller than mailbox.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation, UINT64_MAX, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, view_format_count,
                       ComputeNumEntries(sizeof(mailbox.name)) - 1u,
                       packed_data.data());
          EXPECT_EQ(error::kOutOfBounds,
                    ExecuteImmediateCmd(decoder, cmd.cmd,
                                        sizeof(uint32_t) * packed_data.size()));
        }

        // Error case: packed data size incorrect.
        for (int adjustment : {-1, -2}) {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, view_format_count,
                       packed_data.size() + adjustment, packed_data.data());
          EXPECT_EQ(error::kOutOfBounds,
                    ExecuteImmediateCmd(decoder, cmd.cmd,
                                        sizeof(uint32_t) * packed_data.size()));
        }

        // Error case: view_format_count incorrect.
        for (int adjustment : {-1, 1}) {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE,
                       view_format_count + adjustment, packed_data.size(),
                       packed_data.data());
          EXPECT_EQ(error::kOutOfBounds,
                    ExecuteImmediateCmd(decoder, cmd.cmd,
                                        sizeof(uint32_t) * packed_data.size()));
        }

        // Control case: test a successful call to AssociateMailbox.
        // The control case is not put first because it modifies the internal
        // state of the Dawn wire server and would make calls with the same
        // texture ID and generation invalid.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, view_format_count,
                       packed_data.size(), packed_data.data());
          EXPECT_EQ(error::kNoError,
                    ExecuteImmediateCmd(decoder, cmd.cmd,
                                        sizeof(uint32_t) * packed_data.size()));
        }

        // Error case: associated to an already associated texture.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
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
      GetDecoder(), reservation, std::move(shared_image)));

  GetGpuServiceHolder()
      ->gpu_main_thread_task_runner()
      ->RunsTasksInCurrentSequence();
}

// Test that AssociateMailbox with a bad mailbox produces an error texture.
TEST_P(WebGPUMailboxTest, AssociateMailboxCmdBadMailboxMakesErrorTexture) {
  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);

  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device_.Get());

  GetGpuServiceHolder()->ScheduleGpuMainTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::ReservedTexture reservation,
         scoped_refptr<gpu::ClientSharedImage> shared_image) {
        // Error case: invalid mailbox
        {
          gpu::Mailbox bad_mailbox;
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(bad_mailbox.name)),
                       reinterpret_cast<const GLuint*>(&bad_mailbox.name));
          EXPECT_EQ(
              error::kNoError,
              ExecuteImmediateCmd(decoder, cmd.cmd, sizeof(bad_mailbox.name)));
        }
      },
      GetDecoder(), reservation, std::move(shared_image)));

  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Expect an error when creating a view since the texture is an error.
  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation,
                      texture.CreateView());
}

TEST_P(WebGPUMailboxTest, DissociateMailboxCmd) {
  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);

  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device_.Get());

  GetGpuServiceHolder()->ScheduleGpuMainTask(base::BindOnce(
      [](webgpu::WebGPUDecoder* decoder, webgpu::ReservedTexture reservation,
         scoped_refptr<gpu::ClientSharedImage> shared_image) {
        const gpu::Mailbox& mailbox = shared_image->mailbox();

        // Associate a mailbox so we can later dissociate it.
        {
          AssociateMailboxCmdStorage cmd;
          cmd.cmd.Init(reservation.deviceId, reservation.deviceGeneration,
                       reservation.id, reservation.generation,
                       WGPUTextureUsage_TextureBinding, 0u,
                       webgpu::WEBGPU_MAILBOX_NONE, 0u,
                       ComputeNumEntries(sizeof(mailbox.name)),
                       reinterpret_cast<const GLuint*>(&mailbox.name));
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
      GetDecoder(), reservation, std::move(shared_image)));

  GetGpuServiceHolder()
      ->gpu_main_thread_task_runner()
      ->RunsTasksInCurrentSequence();
}

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// For simplicity of the test the image is shared between a Dawn device and
// itself: we render to it using the Dawn device, then re-associate it to a
// Dawn texture and read back the values that were written.
TEST_P(WebGPUMailboxTest, WriteToMailboxThenReadFromIt) {
  SKIP_TEST_IF(GetParam().format == viz::SinglePlaneFormat::kRGBA_F16);

  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Part 1: Write to the texture using Dawn
  InitializeTextureColor(device_, shared_image->mailbox(),
                         {0.0, 0.0, 1.0, 1.0});

  // Part 2: Read back the texture using Dawn
  {
    // Register the shared image as a Dawn texture in the wire.
    gpu::webgpu::ReservedTexture reservation =
        webgpu()->ReserveTexture(device_.Get());

    webgpu()->AssociateMailbox(
        reservation.deviceId, reservation.deviceGeneration, reservation.id,
        reservation.generation, WGPUTextureUsage_CopySrc,
        webgpu::WEBGPU_MAILBOX_NONE, shared_image->mailbox());
    wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

    // Copy the texture in a mappable buffer.
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

    wgpu::ImageCopyTexture copy_src = {};
    copy_src.texture = texture;
    copy_src.mipLevel = 0;
    copy_src.origin = {0, 0, 0};

    wgpu::ImageCopyBuffer copy_dst = {};
    copy_dst.buffer = readback_buffer;
    copy_dst.layout.offset = 0;
    copy_dst.layout.bytesPerRow = 256;

    wgpu::Extent3D copy_size = {1, 1, 1};

    wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device_.GetQueue();
    queue.Submit(1, &commands);

    webgpu()->DissociateMailbox(reservation.id, reservation.generation);

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4,
                             wgpu::CallbackMode::AllowSpontaneous,
                             ToMockBufferMapCallback);
    EXPECT_CALL(*mock_buffer_map_callback,
                Call(wgpu::MapAsyncStatus::Success, nullptr))
        .Times(1);

    WaitForCompletion(device_);

    const void* data = readback_buffer.GetConstMappedRange();
    if (GetParam().format == viz::SinglePlaneFormat::kRGBA_8888) {
      EXPECT_EQ(0xFFFF0000u, *static_cast<const uint32_t*>(data));
    } else if (GetParam().format == viz::SinglePlaneFormat::kBGRA_8888) {
      EXPECT_EQ(0xFF0000FFu, *static_cast<const uint32_t*>(data));
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

// Test that passing write usages when associating a mailbox fails if
// the SharedImage associated with the mailbox doesn't have WEBGPU_WRITE access.
TEST_P(WebGPUMailboxTest, PassWriteUsagesWhenAssociatingReadOnlyMailbox) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Create a texture for the mailbox, passing CopyDst as a usage.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation,
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);

  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation, encoder.Finish());

  WaitForCompletion(device_);
}

// Test that passing internal write usages when associating a mailbox fails if
// the SharedImage associated with the mailbox doesn't have WEBGPU_WRITE access.
TEST_P(WebGPUMailboxTest,
       PassInternalWriteUsagesWhenAssociatingReadOnlyMailbox) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Create a texture for the mailbox, passing CopyDst as an internal usage.
  webgpu()->AssociateMailbox(reservation.deviceId, reservation.deviceGeneration,
                             reservation.id, reservation.generation,
                             WGPUTextureUsage_CopySrc, WGPUTextureUsage_CopyDst,
                             webgpu::WEBGPU_MAILBOX_NONE,
                             shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);

  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation, encoder.Finish());

  WaitForCompletion(device_);
}

// Test that passing WEBGPU_MAILBOX_DISCARD when associating a mailbox fails if
// the SharedImage associated with the mailbox doesn't have WEBGPU_WRITE access.
TEST_P(WebGPUMailboxTest, PassDiscardWhenAssociatingReadOnlyMailbox) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Associate the mailbox. Using WEBGPU_MAILBOX_DISCARD will set the contents
  // to uncleared.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_CopySrc,
      webgpu::WEBGPU_MAILBOX_DISCARD, shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);

  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation, encoder.Finish());

  WaitForCompletion(device_);
}

// Test that passing WEBGPU_MAILBOX_DISCARD when associating a mailbox fails if
// the client doesn't pass a usage supporting lazy clearing.
TEST_P(WebGPUMailboxTest,
       PassDiscardWhenAssociatingMailboxWithoutUsageSupportingClearing) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Associate the mailbox without passing any usages or internal usages
  // supporting lazy clearing.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_CopySrc,
      webgpu::WEBGPU_MAILBOX_DISCARD, shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);

  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation, encoder.Finish());

  WaitForCompletion(device_);
}

// Test that an uninitialized writable shared image is lazily cleared by Dawn
// when it is accessed with an internal write usage supporting lazy clearing.
TEST_P(WebGPUMailboxTest,
       ReadWritableUninitializedSharedImageWhenAccessedWithInternalWriteUsage) {
  // Create the shared image. Note that it is uncleared by default.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Associate the mailbox, passing a read-only usage but RenderAttachment as an
  // internal usage.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_CopySrc,
      WGPUTextureUsage_RenderAttachment, webgpu::WEBGPU_MAILBOX_NONE,
      shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

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

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Map the buffer and assert the pixel is the correct value.
  readback_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size,
                           wgpu::CallbackMode::AllowSpontaneous,
                           ToMockBufferMapCallback);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(wgpu::MapAsyncStatus::Success, nullptr))
      .Times(1);

  WaitForCompletion(device_);

  const uint8_t* data = static_cast<const uint8_t*>(
      readback_buffer.GetConstMappedRange(0, buffer_desc.size));
  // Contents should be black because the texture was lazily cleared.
  for (uint32_t i = 0; i < buffer_desc.size; ++i) {
    EXPECT_EQ(data[i], uint8_t(0));
  }

  // Associate the SharedImage with a new Dawn texture in a read-only access.
  // The SharedImage should have been cleared at the end of the previous access,
  // and hence this read access should succeed.
  gpu::webgpu::ReservedTexture reservation2 =
      webgpu()->ReserveTexture(device_.Get());
  webgpu()->AssociateMailbox(
      reservation2.deviceId, reservation2.deviceGeneration, reservation2.id,
      reservation2.generation, WGPUTextureUsage_CopySrc,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image->mailbox());
  wgpu::Texture texture2 = wgpu::Texture::Acquire(reservation2.texture);

  copy_src.texture = texture2;
  wgpu::Buffer readback_buffer2 = device_.CreateBuffer(&buffer_desc);
  copy_dst.buffer = readback_buffer2;
  wgpu::CommandEncoder encoder2 = device_.CreateCommandEncoder();
  encoder2.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
  commands = encoder2.Finish();

  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation2.id, reservation2.generation);

  // Map the buffer.
  readback_buffer2.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size,
                            wgpu::CallbackMode::AllowSpontaneous,
                            ToMockBufferMapCallback);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(wgpu::MapAsyncStatus::Success, nullptr))
      .Times(1);

  WaitForCompletion(device_);
}

// Test that an uninitialized writable shared image is lazily cleared by Dawn
// when it is read if a usage supporting lazy clearing is passed.
TEST_P(WebGPUMailboxTest,
       ReadWritableUninitializedSharedImageWithUsageSupportingLazyClearing) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Set the texture contents to non-zero so we can test a lazy clear occurs.
  InitializeTextureColor(device_, shared_image->mailbox(), {1.0, 0, 0, 1.0});

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Associate the mailbox. Using WEBGPU_MAILBOX_DISCARD will set the contents
  // to uncleared.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation,
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_DISCARD, shared_image->mailbox());
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

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
  pass.End();

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

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

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Map the buffer and assert the pixel is the correct value.
  readback_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size,
                           wgpu::CallbackMode::AllowSpontaneous,
                           ToMockBufferMapCallback);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(wgpu::MapAsyncStatus::Success, nullptr))
      .Times(1);

  WaitForCompletion(device_);

  const uint8_t* data = static_cast<const uint8_t*>(
      readback_buffer.GetConstMappedRange(0, buffer_desc.size));
  // Contents should be black because the texture was lazily cleared.
  for (uint32_t i = 0; i < buffer_desc.size; ++i) {
    EXPECT_EQ(data[i], uint8_t(0));
  }
}

// Test that an uninitialized writable shared image is lazily cleared by Dawn
// when it is read if an internal usage supporting lazy clearing is passed.
TEST_P(
    WebGPUMailboxTest,
    ReadWritableUninitializedSharedImageWithInternalUsageSupportingLazyClearing) {
  // Create the shared image.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Set the texture contents to non-zero so we can test a lazy clear occurs.
  InitializeTextureColor(device_, shared_image->mailbox(), {1.0, 0, 0, 1.0});

  // Register the shared image as a Dawn texture in the wire.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());

  // Associate the mailbox. Using WEBGPU_MAILBOX_DISCARD will set the contents
  // to uncleared.
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_CopySrc,
      WGPUTextureUsage_RenderAttachment, webgpu::WEBGPU_MAILBOX_DISCARD,
      shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Copy the texture in a mappable buffer.
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.size = BytesPerTexel(GetParam().format);
  buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
  wgpu::Buffer readback_buffer = device_.CreateBuffer(&buffer_desc);

  wgpu::ImageCopyTexture copy_src = {};
  copy_src.texture = texture;
  copy_src.mipLevel = 0;
  copy_src.origin = {0, 0, 0};

  wgpu::ImageCopyBuffer copy_dst = {};
  copy_dst.buffer = readback_buffer;
  copy_dst.layout.offset = 0;
  copy_dst.layout.bytesPerRow = 256;

  wgpu::Extent3D copy_size = {1, 1, 1};

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commands);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  // Map the buffer and assert the pixel is the correct value.
  readback_buffer.MapAsync(wgpu::MapMode::Read, 0, buffer_desc.size,
                           wgpu::CallbackMode::AllowSpontaneous,
                           ToMockBufferMapCallback);
  EXPECT_CALL(*mock_buffer_map_callback,
              Call(wgpu::MapAsyncStatus::Success, nullptr))
      .Times(1);

  WaitForCompletion(device_);

  const uint8_t* data = static_cast<const uint8_t*>(
      readback_buffer.GetConstMappedRange(0, buffer_desc.size));
  // Contents should be black because the texture was lazily cleared.
  for (uint32_t i = 0; i < buffer_desc.size; ++i) {
    EXPECT_EQ(data[i], uint8_t(0));
  }
}

// Tests that using a shared image aftr it is dissociated produces an error.
TEST_P(WebGPUMailboxTest, ErrorWhenUsingTextureAfterDissociate) {
  // Create the shared image.
  // NOTE: It's necessary to add WEBGPU_WRITE access as the created SharedImage
  // will be uncleared and hence require lazy clearing on access.
  // WebGPUDecoderImpl might also need to fall back to using Skia to read and
  // write, making it necessary to add those usages as well.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  webgpu()->WaitSyncTokenCHROMIUM(mailbox_produced_token.GetConstData());

  // Associate and immediately dissociate the image.
  gpu::webgpu::ReservedTexture reservation =
      webgpu()->ReserveTexture(device_.Get());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // NOTE: Accessing an uncleared Dawn texture requires passing a usage that
  // supports lazy clearing (otherwise AssociateMailbox() will generate an
  // error, which is not the error case that this test is looking to test).
  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation,
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image->mailbox());
  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  wgpu::TextureDescriptor dst_desc = {};
  dst_desc.size = {1, 1};
  dst_desc.usage = wgpu::TextureUsage::CopyDst;
  DCHECK(GetParam().format == viz::SinglePlaneFormat::kRGBA_8888 ||
         GetParam().format == viz::SinglePlaneFormat::kBGRA_8888 ||
         GetParam().format == viz::SinglePlaneFormat::kRGBA_F16);
  dst_desc.format = ToDawnFormat(GetParam().format);

  wgpu::ImageCopyTexture src_image = {};
  src_image.texture = texture;

  wgpu::ImageCopyTexture dst_image = {};
  dst_image.texture = device_.CreateTexture(&dst_desc);

  wgpu::Extent3D extent = {1, 1};

  // Try using the texture in a copy command; it should produce a validation
  // error.
  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  encoder.CopyTextureToTexture(&src_image, &dst_image, &extent);
  wgpu::CommandBuffer commandBuffer = encoder.Finish();

  // Wait so it's clear the validation error after this when we call Submit.
  WaitForCompletion(device_);

  EXPECT_WEBGPU_ERROR(device_, wgpu::ErrorType::Validation,
                      device_.GetQueue().Submit(1, &commandBuffer));
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
  // Create a the shared images.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image_a =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  scoped_refptr<gpu::ClientSharedImage> shared_image_b =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);

  // Associate both mailboxes
  gpu::webgpu::ReservedTexture reservation_a =
      webgpu()->ReserveTexture(device_.Get());
  webgpu()->AssociateMailbox(
      reservation_a.deviceId, reservation_a.deviceGeneration, reservation_a.id,
      reservation_a.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image_a->mailbox());

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(device_.Get());
  webgpu()->AssociateMailbox(
      reservation_b.deviceId, reservation_b.deviceGeneration, reservation_b.id,
      reservation_b.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image_b->mailbox());

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
  // Create a the shared images.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image_a =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);

  scoped_refptr<gpu::ClientSharedImage> shared_image_b =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
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
      webgpu::WEBGPU_MAILBOX_NONE, shared_image_a->mailbox());

  gpu::webgpu::ReservedTexture reservation_b =
      webgpu()->ReserveTexture(device_b.Get());
  webgpu()->AssociateMailbox(
      reservation_b.deviceId, reservation_b.deviceGeneration, reservation_b.id,
      reservation_b.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image_b->mailbox());

  // Dissociate both mailboxes in the same order.
  webgpu()->DissociateMailbox(reservation_a.id, reservation_a.generation);
  webgpu()->DissociateMailbox(reservation_b.id, reservation_b.generation);

  // Send all the previous commands to the WebGPU decoder.
  webgpu()->FlushCommands();
}

// Test that passing a descriptor to ReserveTexture produces a client-side
// WGPUTexture that correctly reflects said descriptor.
TEST_P(WebGPUMailboxTest, ReflectionOfDescriptor) {
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
      device_.Get(), reinterpret_cast<const WGPUTextureDescriptor*>(&desc1));
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
      device_.Get(), reinterpret_cast<const WGPUTextureDescriptor*>(&desc2));
  wgpu::Texture texture2 = wgpu::Texture::Acquire(reservation2.texture);

  ASSERT_EQ(desc2.size.width, texture2.GetWidth());
  ASSERT_EQ(desc2.size.height, texture2.GetHeight());
  ASSERT_EQ(desc2.size.depthOrArrayLayers, texture2.GetDepthOrArrayLayers());
  ASSERT_EQ(desc2.format, texture2.GetFormat());
  ASSERT_EQ(desc2.usage, texture2.GetUsage());
  ASSERT_EQ(desc2.dimension, texture2.GetDimension());
  ASSERT_EQ(desc2.sampleCount, texture2.GetSampleCount());
  ASSERT_EQ(desc2.mipLevelCount, texture2.GetMipLevelCount());

  // Associate mailboxes so that releasing the reserved wgpu::Textures does not
  // fail. Note that these texture parameters do not match. It doesn't matter
  // since the textures are not used in this test except for frontend
  // reflection.
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image1 =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);
  scoped_refptr<gpu::ClientSharedImage> shared_image2 =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::Read),
                              "TestLabel"},
                             kNullSurfaceHandle);
  webgpu()->AssociateMailbox(
      reservation1.deviceId, reservation1.deviceGeneration, reservation1.id,
      reservation1.generation, static_cast<WGPUTextureUsage>(desc1.usage),
      webgpu::WEBGPU_MAILBOX_NONE, shared_image1->mailbox());
  webgpu()->AssociateMailbox(
      reservation2.deviceId, reservation2.deviceGeneration, reservation2.id,
      reservation2.generation, static_cast<WGPUTextureUsage>(desc2.usage),
      webgpu::WEBGPU_MAILBOX_NONE, shared_image2->mailbox());
}

// Test that if some other GL context is current when
// Associate/DissociateMailbox occurs, the operations do not fail. Some WebGPU
// shared image backings rely on GL and need to be responsible for making the
// context current.
TEST_P(WebGPUMailboxTest, AssociateDissociateMailboxWhenNotCurrent) {
  // Create the shared image
  SharedImageInterface* sii = GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image =
      sii->CreateSharedImage({GetParam().format,
                              {1, 1},
                              gfx::ColorSpace::CreateSRGB(),
                              GetSharedImageUsage(AccessType::ReadWrite),
                              "TestLabel"},
                             kNullSurfaceHandle);

  scoped_refptr<gl::GLContext> gl_context1;
  scoped_refptr<gl::GLContext> gl_context2;
  scoped_refptr<gl::GLSurface> gl_surface1;
  scoped_refptr<gl::GLSurface> gl_surface2;

  // Create and make a new gl context current.
  // Contexts must be created on the GPU thread, so this creates it on the GPU
  // thread and sets a scoped_refptr on the main thread.
  auto CreateAndMakeGLContextCurrent =
      [&](scoped_refptr<gl::GLContext>* gl_context_out,
          scoped_refptr<gl::GLSurface>* gl_surface_out) {
        GetGpuServiceHolder()->ScheduleGpuMainTask(base::BindOnce(
            [](scoped_refptr<gl::GLContext>* gl_context_out,
               scoped_refptr<gl::GLSurface>* gl_surface_out) {
              auto gl_surface = gl::init::CreateOffscreenGLSurface(
                  gl::GetDefaultDisplay(), gfx::Size(4, 4));
              auto gl_context = gl::init::CreateGLContext(
                  nullptr, gl_surface.get(), gl::GLContextAttribs());

              EXPECT_TRUE(gl_context->MakeCurrent(gl_surface.get()))
                  << "Failed to make GL context current";

              *gl_context_out = std::move(gl_context);
              *gl_surface_out = std::move(gl_surface);
            },
            gl_context_out, gl_surface_out));
        GetGpuServiceHolder()
            ->gpu_main_thread_task_runner()
            ->RunsTasksInCurrentSequence();
      };

  webgpu::ReservedTexture reservation = webgpu()->ReserveTexture(device_.Get());

  // Create a GL context and make it current.
  CreateAndMakeGLContextCurrent(&gl_context1, &gl_surface1);

  webgpu()->AssociateMailbox(
      reservation.deviceId, reservation.deviceGeneration, reservation.id,
      reservation.generation, WGPUTextureUsage_RenderAttachment,
      webgpu::WEBGPU_MAILBOX_NONE, shared_image->mailbox());
  wgpu::Texture texture = wgpu::Texture::Acquire(reservation.texture);

  // Clear the texture using a render pass.
  wgpu::RenderPassColorAttachment color_desc = {};
  color_desc.view = texture.CreateView();
  color_desc.loadOp = wgpu::LoadOp::Clear;
  color_desc.storeOp = wgpu::StoreOp::Store;
  color_desc.clearValue = {0.0, 1.0, 0.0, 1.0};

  wgpu::RenderPassDescriptor render_pass_desc = {};
  render_pass_desc.colorAttachmentCount = 1;
  render_pass_desc.colorAttachments = &color_desc;

  wgpu::CommandEncoder encoder = device_.CreateCommandEncoder();
  wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_desc);
  pass.End();
  wgpu::CommandBuffer commands = encoder.Finish();

  wgpu::Queue queue = device_.GetQueue();
  queue.Submit(1, &commands);

  WaitForCompletion(device_);

  // Create another context and make it current.
  // This is a distinct context to catch errors where Associate/Dissociate
  // always use the current context, and in the test, these just so happen to be
  // identical.
  CreateAndMakeGLContextCurrent(&gl_context2, &gl_surface2);

  webgpu()->DissociateMailbox(reservation.id, reservation.generation);

  WaitForCompletion(device_);

  // Delete the GL contexts on the GPU thread.
  GetGpuServiceHolder()->ScheduleGpuMainTask(
      base::BindOnce([](scoped_refptr<gl::GLContext> gl_context1,
                        scoped_refptr<gl::GLContext> gl_context2,
                        scoped_refptr<gl::GLSurface> gl_surface1,
                        scoped_refptr<gl::GLSurface> gl_surface2) {},
                     std::move(gl_context1), std::move(gl_context2),
                     std::move(gl_surface1), std::move(gl_surface2)));
}

INSTANTIATE_TEST_SUITE_P(,
                         WebGPUMailboxTest,
                         ::testing::ValuesIn(WebGPUMailboxTest::TestParams()),
                         ::testing::PrintToStringParamName());

}  // namespace gpu
