// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dawn/wire/client/webgpu_cpp.h>

#include "build/build_config.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {
namespace {

class MockBufferMapCallback {
 public:
  MOCK_METHOD(void,
              Call,
              (wgpu::MapAsyncStatus status, wgpu::StringView message));
};
std::unique_ptr<testing::StrictMock<MockBufferMapCallback>>
    mock_buffer_map_callback;

void ToMockBufferMapCallback(wgpu::MapAsyncStatus status,
                             wgpu::StringView message) {
  mock_buffer_map_callback->Call(status, message);
}

}  // namespace

class SharedImageGLBackingProduceDawnTest : public WebGPUTest {
 protected:
  void SetUp() override {
    WebGPUTest::SetUp();
    WebGPUTest::Options option;
    Initialize(option);

    if (ShouldSkipTest()) {
      return;
    }

    gl_context_ = std::make_unique<GLInProcessContext>();
    ContextResult result =
        gl_context_->Initialize(GetGpuServiceHolder()->task_executor());
    ASSERT_EQ(result, ContextResult::kSuccess);
    mock_buffer_map_callback =
        std::make_unique<testing::StrictMock<MockBufferMapCallback>>();
  }

  void TearDown() override {
    WebGPUTest::TearDown();
    gl_context_.reset();
    mock_buffer_map_callback = nullptr;
  }

  bool ShouldSkipTest() {
// Windows is the only platform enabled passthrough in this test.
#if BUILDFLAG(IS_WIN)
    // Skip the test if there is no GPU service holder. It is not created if
    // Dawn is not supported on the platform (Win7).
    return GetGpuServiceHolder() == nullptr;
#else
    return true;
#endif  // BUILDFLAG(IS_WIN)
  }

  gles2::GLES2Implementation* gl() { return gl_context_->GetImplementation(); }

  std::unique_ptr<GLInProcessContext> gl_context_;
};

// Tests using Associate/DissociateMailbox to share an image with Dawn.
// We render to the `SharedImage` via GL, re-associate it to a Dawn texture,
// and read back the values that were written.
#if BUILDFLAG(USE_DAWN)
TEST_F(SharedImageGLBackingProduceDawnTest, Basic) {
  if (ShouldSkipTest())
    return;
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped because WebGPU isn't supported";
    return;
  }
  if (!WebGPUSharedImageSupported()) {
    LOG(ERROR) << "Test skipped because WebGPUSharedImage isn't supported";
    return;
  }

  // Create the shared image
  SharedImageInterface* sii = gl_context_->GetSharedImageInterface();
  scoped_refptr<gpu::ClientSharedImage> shared_image = sii->CreateSharedImage(
      {viz::SinglePlaneFormat::kRGBA_8888,
       {1, 1},
       gfx::ColorSpace::CreateSRGB(),
       SharedImageUsageSet(
           {SHARED_IMAGE_USAGE_GLES2_WRITE, SHARED_IMAGE_USAGE_WEBGPU_READ}),
       "TestLabel"},
      kNullSurfaceHandle);
  SyncToken mailbox_produced_token = sii->GenVerifiedSyncToken();
  auto texture = shared_image->CreateGLTexture(gl());
  auto scoped_access =
      texture->BeginAccess(mailbox_produced_token, /*readonly=*/false);
  GLuint fbo = 0;
  gl()->GenFramebuffers(1, &fbo);
  gl()->BindFramebuffer(GL_FRAMEBUFFER, fbo);

  // Attach the texture to FBO.
  gl()->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, /* Hard code */
                             scoped_access->texture_id(), 0);

  // Set the clear color to green.
  gl()->ClearColor(0.0f, 1.0f, 0.0f, 1.0f);
  gl()->Clear(GL_COLOR_BUFFER_BIT);

  SyncToken gl_op_token = gpu::SharedImageTexture::ScopedAccess::EndAccess(
      std::move(scoped_access));

  wgpu::Device device = GetNewDevice();
  wgpu::TextureDescriptor desc = {
      .usage = wgpu::TextureUsage::CopySrc,
  };

  {
    // Register the shared image as a Dawn texture in the wire.
    std::unique_ptr<WebGPUTextureScopedAccess> webgpu_scoped_access =
        shared_image->BeginWebGPUTextureAccess(
            webgpu(), gl_op_token, device, desc, /*usage=*/0,
            /*mailbox_flag=*/webgpu::WEBGPU_MAILBOX_NONE);

    // Copy the texture in a mappable buffer.
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.size = 4;
    buffer_desc.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
    wgpu::Buffer readback_buffer = device.CreateBuffer(&buffer_desc);

    wgpu::TexelCopyTextureInfo copy_src = {};
    copy_src.texture = webgpu_scoped_access->texture();
    copy_src.mipLevel = 0;
    copy_src.origin = {0, 0, 0};

    wgpu::TexelCopyBufferInfo copy_dst = {};
    copy_dst.buffer = readback_buffer;
    copy_dst.layout.offset = 0;
    copy_dst.layout.bytesPerRow = 256;

    wgpu::Extent3D copy_size = {1, 1, 1};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    encoder.CopyTextureToBuffer(&copy_src, &copy_dst, &copy_size);
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetQueue();
    queue.Submit(1, &commands);

    WebGPUTextureScopedAccess::EndAccess(std::move(webgpu_scoped_access));

    // Map the buffer and assert the pixel is the correct value.
    readback_buffer.MapAsync(wgpu::MapMode::Read, 0, 4,
                             wgpu::CallbackMode::AllowSpontaneous,
                             ToMockBufferMapCallback);
    EXPECT_CALL(*mock_buffer_map_callback,
                Call(wgpu::MapAsyncStatus::Success, testing::_))
        .Times(1);
    WaitForCompletion(device);

    const void* data = readback_buffer.GetConstMappedRange(0, 4);
    EXPECT_EQ(0xFF00FF00, *static_cast<const uint32_t*>(data));
  }
}
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace gpu
