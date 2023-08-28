// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "content/renderer/media/win/dcomp_texture_factory.h"
#include "content/renderer/media/win/dcomp_texture_wrapper_impl.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "media/base/mock_filters.h"
#include "media/base/test_helpers.h"
#include "media/base/win/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using base::RunLoop;
using Microsoft::WRL::ComPtr;

// Stubs out methods of ClientSII that DCompTextureWrapperImpl invokes and for
// which the production versions cannot run in the context of the unittest.
class StubClientSharedImageInterface : public gpu::ClientSharedImageInterface {
 public:
  StubClientSharedImageInterface() : gpu::ClientSharedImageInterface(nullptr) {}
  gpu::SyncToken GenVerifiedSyncToken() override { return gpu::SyncToken(); }

  gpu::Mailbox CreateSharedImage(viz::SharedImageFormat format,
                                 const gfx::Size& size,
                                 const gfx::ColorSpace& color_space,
                                 GrSurfaceOrigin surface_origin,
                                 SkAlphaType alpha_type,
                                 uint32_t usage,
                                 base::StringPiece debug_label,
                                 gfx::GpuMemoryBufferHandle handle) override {
    return gpu::Mailbox();
  }
};

class TestGpuChannelHost : public gpu::GpuChannelHost {
 public:
  TestGpuChannelHost()
      : gpu::GpuChannelHost(
            0 /* channel_id */,
            gpu::GPUInfo(),
            gpu::GpuFeatureInfo(),
            mojo::ScopedMessagePipeHandle(
                mojo::MessagePipeHandle(mojo::kInvalidHandleValue))) {}

  std::unique_ptr<gpu::ClientSharedImageInterface>
  CreateClientSharedImageInterface() override {
    return std::make_unique<StubClientSharedImageInterface>();
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
