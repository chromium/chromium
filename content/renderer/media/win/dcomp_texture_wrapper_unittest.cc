// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "content/renderer/media/win/dcomp_texture_factory.h"
#include "content/renderer/media/win/dcomp_texture_wrapper_impl.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/gpu_memory_buffer_handle_info.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;
using Microsoft::WRL::ComPtr;

// Stubs out methods of ClientSII that DCompTextureWrapperImpl invokes and for
// which the production versions cannot run in the context of the unittest.
class StubClientSharedImageInterface : public gpu::ClientSharedImageInterface {
 public:
  StubClientSharedImageInterface()
      : gpu::ClientSharedImageInterface(nullptr, nullptr) {}
  gpu::SyncToken GenVerifiedSyncToken() override { return gpu::SyncToken(); }

  // TODO(crbug.com/40263579): Remove this implementation when MappableSI is
  // fully launched for DcomTextureWrapperImpl. Eventually look into refactoring
  // code to use TestSharedImageInterface instead of
  // StubClientSharedImageInterface.
  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gfx::GpuMemoryBufferHandle handle) override {
    return base::MakeRefCounted<gpu::ClientSharedImage>(
        gpu::Mailbox::Generate(), si_info.meta, gpu::SyncToken(), holder_,
        handle.type);
  }

  // This implementation is used for test when MappableSI is enabled.
  scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
      const gpu::SharedImageInfo& si_info,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferUsage buffer_usage,
      gfx::GpuMemoryBufferHandle buffer_handle) override {
    return base::MakeRefCounted<gpu::ClientSharedImage>(
        gpu::Mailbox::Generate(), si_info.meta, gpu::SyncToken(),
        gpu::GpuMemoryBufferHandleInfo(std::move(buffer_handle),
                                       si_info.meta.format, si_info.meta.size,
                                       buffer_usage),
        holder_);
  }

 protected:
  ~StubClientSharedImageInterface() override = default;
};

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  TestGpuChannelHost()
      : gpu::GpuChannelHost(
            0 /* channel_id */,
            gpu::GPUInfo(),
            gpu::GpuFeatureInfo(),
            gpu::SharedImageCapabilities(),
            mojo::ScopedMessagePipeHandle(
                mojo::MessagePipeHandle(mojo::kInvalidHandleValue))) {}

  scoped_refptr<gpu::ClientSharedImageInterface>
  CreateClientSharedImageInterface() override {
    return base::MakeRefCounted<StubClientSharedImageInterface>();
  }

 protected:
  ~TestGpuChannelHost() override {}
};

class StubDCOMPTextureFactory : public content::DCOMPTextureFactory {
 public:
  StubDCOMPTextureFactory(
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
      : DCOMPTextureFactory(base::MakeRefCounted<TestGpuChannelHost>(),
                            media_task_runner) {}

 protected:
  ~StubDCOMPTextureFactory() override {}
};

class DCOMPTextureWrapperTest : public testing::Test {
 public:
  DCOMPTextureWrapperTest() {
    mock_factory_ = base::MakeRefCounted<StubDCOMPTextureFactory>(
        task_environment_.GetMainThreadTaskRunner());
  }

  static void CreateDXBackedVideoFrameTestTask(
      std::unique_ptr<media::DCOMPTextureWrapper> dcomp_texture_wrapper,
      base::OnceClosure closure);

 protected:
  base::test::TaskEnvironment task_environment_;
  scoped_refptr<StubDCOMPTextureFactory> mock_factory_;
};

void DCOMPTextureWrapperTest::CreateDXBackedVideoFrameTestTask(
    std::unique_ptr<media::DCOMPTextureWrapper> dcomp_texture_wrapper,
    base::OnceClosure closure) {
  gfx::GpuMemoryBufferHandle dx_handle;
  base::WaitableEvent wait_event;
  dx_handle.dxgi_handle =
      base::win::ScopedHandle(CreateEvent(nullptr, FALSE, FALSE, nullptr));
  dx_handle.dxgi_token = gfx::DXGIHandleToken();
  dx_handle.type = gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
  gfx::Size frame_size(1920, 1080);

  dcomp_texture_wrapper->CreateVideoFrame(
      frame_size, std::move(dx_handle),
      base::BindOnce(
          [](gfx::Size orig_frame_size, base::WaitableEvent* wait_event,
             scoped_refptr<media::VideoFrame> frame,
             const gpu::Mailbox& mailbox) {
            EXPECT_EQ(frame->coded_size().width(), orig_frame_size.width());
            EXPECT_EQ(frame->coded_size().height(), orig_frame_size.height());
            wait_event->Signal();
          },
          frame_size, &wait_event));
  wait_event.Wait();
  std::move(closure).Run();
}

TEST_F(DCOMPTextureWrapperTest, CreateDXBackedVideoFrame) {
  std::unique_ptr<media::DCOMPTextureWrapper> dcomp_texture_wrapper =
      content::DCOMPTextureWrapperImpl::Create(
          mock_factory_, task_environment_.GetMainThreadTaskRunner());
  RunLoop run_loop;

  task_environment_.GetMainThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DCOMPTextureWrapperTest::CreateDXBackedVideoFrameTestTask,
                     std::move(dcomp_texture_wrapper), run_loop.QuitClosure()));

  run_loop.Run();
}
