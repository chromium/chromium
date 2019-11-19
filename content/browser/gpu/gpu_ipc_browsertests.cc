// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/test/content_browser_test.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gl/gl_switches.h"

namespace {

// RunLoop implementation that runs until it observes OnContextLost().
class ContextLostRunLoop : public viz::ContextLostObserver {
 public:
  ContextLostRunLoop(viz::ContextProvider* context_provider)
      : context_provider_(context_provider) {
    context_provider_->AddObserver(this);
  }
  ~ContextLostRunLoop() override { context_provider_->RemoveObserver(this); }

  void RunUntilContextLost() { run_loop_.Run(); }

 private:
  // viz::LostContextProvider:
  void OnContextLost() override { run_loop_.Quit(); }

  viz::ContextProvider* const context_provider_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ContextLostRunLoop);
};

class ContextTestBase : public content::ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    // This may leave the provider_ null in some cases, so tests need to early
    // out.
    if (!content::GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr))
      return;

    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host =
        content::GpuBrowsertestEstablishGpuChannelSyncRunLoop();
    CHECK(gpu_channel_host);

    provider_ =
        content::GpuBrowsertestCreateContext(std::move(gpu_channel_host));
    auto result = provider_->BindToCurrentThread();
    CHECK_EQ(result, gpu::ContextResult::kSuccess);
    gl_ = provider_->ContextGL();
    context_support_ = provider_->ContextSupport();

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Must delete the context first.
    provider_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  gpu::gles2::GLES2Interface* gl_ = nullptr;
  gpu::ContextSupport* context_support_ = nullptr;

 private:
  scoped_refptr<viz::ContextProviderCommandBuffer> provider_;
};

}  // namespace

// Include the shared tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/client/gpu_context_tests.h"

namespace content {

class BrowserGpuChannelHostFactoryTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    if (!GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr))
      return;
    CHECK(GetFactory());
    ContentBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Start all tests without a gpu channel so that the tests exercise a
    // consistent codepath.
    command_line->AppendSwitch(switches::kDisableGpuEarlyInit);
  }

  void Signal(bool* event,
              scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
    CHECK_EQ(*event, false);
    *event = true;
    gpu_channel_host_ = std::move(gpu_channel_host);
  }

  void SignalAndQuitLoop(bool* event,
                         base::RunLoop* run_loop,
                         scoped_refptr<gpu::GpuChannelHost> gpu_channel_host) {
    Signal(event, std::move(gpu_channel_host));
    run_loop->Quit();
  }

 protected:
  gpu::GpuChannelEstablishFactory* GetFactory() {
    return BrowserMainLoop::GetInstance()->gpu_channel_establish_factory();
  }

  bool IsChannelEstablished() {
    return gpu_channel_host_ && !gpu_channel_host_->IsLost();
  }

  void EstablishAndWait() {
    gpu_channel_host_ = content::GpuBrowsertestEstablishGpuChannelSyncRunLoop();
  }

  gpu::GpuChannelHost* GetGpuChannel() { return gpu_channel_host_.get(); }

  scoped_refptr<gpu::GpuChannelHost> gpu_channel_host_;
};

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, MAYBE_Basic) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(GetGpuChannel() != nullptr);
}

#if !defined(OS_ANDROID)
// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_AlreadyEstablished AlreadyEstablished
#else
#define MAYBE_AlreadyEstablished DISABLED_AlreadyEstablished
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_AlreadyEstablished) {
  DCHECK(!IsChannelEstablished());
  scoped_refptr<gpu::GpuChannelHost> gpu_channel =
      GetFactory()->EstablishGpuChannelSync();

  // Expect established callback immediately.
  bool event = false;
  GetFactory()->EstablishGpuChannel(
      base::BindOnce(&BrowserGpuChannelHostFactoryTest::Signal,
                     base::Unretained(this), &event));
  EXPECT_TRUE(event);
  EXPECT_EQ(gpu_channel.get(), GetGpuChannel());
}
#endif

// Test fails on Windows because GPU Channel set-up fails.
#if !defined(OS_WIN)
#define MAYBE_GrContextKeepsGpuChannelAlive GrContextKeepsGpuChannelAlive
#else
#define MAYBE_GrContextKeepsGpuChannelAlive \
    DISABLED_GrContextKeepsGpuChannelAlive
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_GrContextKeepsGpuChannelAlive) {
  // Test for crbug.com/551143
  // This test verifies that holding a reference to the GrContext created by
  // a viz::ContextProviderCommandBuffer will keep the gpu channel alive after
  // the
  // provider has been destroyed. Without this behavior, user code would have
  // to be careful to destroy objects in the right order to avoid using freed
  // memory as a function pointer in the GrContext's GrGLInterface instance.
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();

  // Step 2: verify that holding onto the provider's GrContext will
  // retain the host after provider is destroyed.
  scoped_refptr<viz::ContextProviderCommandBuffer> provider =
      content::GpuBrowsertestCreateContext(GetGpuChannel());
  ASSERT_EQ(provider->BindToCurrentThread(), gpu::ContextResult::kSuccess);

  sk_sp<GrContext> gr_context = sk_ref_sp(provider->GrContext());

  SkImageInfo info = SkImageInfo::MakeN32Premul(100, 100);
  sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(
      gr_context.get(), SkBudgeted::kNo, info);
  EXPECT_TRUE(surface);

  // Destroy the GL context after we made a surface.
  provider = nullptr;

  // New surfaces will fail to create now.
  sk_sp<SkSurface> surface2 =
      SkSurface::MakeRenderTarget(gr_context.get(), SkBudgeted::kNo, info);
  EXPECT_FALSE(surface2);

  // Drop our reference to the gr_context also.
  gr_context = nullptr;

  // After the context provider is destroyed, the surface no longer has access
  // to the GrContext, even though it's alive. Use the canvas after the provider
  // and GrContext have been locally unref'ed. This should work fine as the
  // GrContext has been abandoned when the GL context provider was destroyed
  // above.
  SkPaint greenFillPaint;
  greenFillPaint.setColor(SK_ColorGREEN);
  greenFillPaint.setStyle(SkPaint::kFill_Style);
  // Passes by not crashing
  surface->getCanvas()->drawRect(SkRect::MakeWH(100, 100), greenFillPaint);
}

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#define MAYBE_CrashAndRecover CrashAndRecover
#else
#define MAYBE_CrashAndRecover DISABLED_CrashAndRecover
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       MAYBE_CrashAndRecover) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  scoped_refptr<gpu::GpuChannelHost> host = GetGpuChannel();

  scoped_refptr<viz::ContextProviderCommandBuffer> provider =
      content::GpuBrowsertestCreateContext(GetGpuChannel());
  ContextLostRunLoop run_loop(provider.get());
  ASSERT_EQ(provider->BindToCurrentThread(), gpu::ContextResult::kSuccess);
  GpuProcessHost::CallOnIO(GPU_PROCESS_KIND_SANDBOXED, false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->Crash();
                           }));
  run_loop.RunUntilContextLost();

  EXPECT_FALSE(IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(IsChannelEstablished());
}

using GpuProcessHostBrowserTest = BrowserGpuChannelHostFactoryTest;

IN_PROC_BROWSER_TEST_F(GpuProcessHostBrowserTest, Shutdown) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  base::RunLoop run_loop;
  StopGpuProcess(run_loop.QuitClosure());
  run_loop.Run();
}

// Disabled outside linux like other tests here sadface.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, CreateTransferBuffer) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();

  // This is for an offscreen context, so the default framebuffer doesn't need
  // any alpha, depth, stencil, antialiasing.
  gpu::ContextCreationAttribs attributes;
  attributes.alpha_size = -1;
  attributes.depth_size = 0;
  attributes.stencil_size = 0;
  attributes.samples = 0;
  attributes.sample_buffers = 0;
  attributes.bind_generates_resource = false;

  auto impl = std::make_unique<gpu::CommandBufferProxyImpl>(
      GetGpuChannel(), GetFactory()->GetGpuMemoryBufferManager(),
      content::kGpuStreamIdDefault, base::ThreadTaskRunnerHandle::Get());
  ASSERT_EQ(
      impl->Initialize(gpu::kNullSurfaceHandle, nullptr,
                       content::kGpuStreamPriorityDefault, attributes, GURL()),
      gpu::ContextResult::kSuccess);

  // Creating a transfer buffer works normally.
  int32_t id = -1;
  scoped_refptr<gpu::Buffer> buffer = impl->CreateTransferBuffer(100, &id);
  EXPECT_TRUE(buffer);
  EXPECT_GE(id, 0);

  // If the context is lost, creating a transfer buffer still works. This is
  // important for initializing a client side context. If it is lost for some
  // transient reason, we don't want that to be confused with a fatal error,
  // like failing to make a transfer buffer.

  // Lose the connection to the gpu to lose the context.
  GetGpuChannel()->DestroyChannel();
  // It's not visible until we run the task queue.
  EXPECT_EQ(impl->GetLastState().error, gpu::error::kNoError);

  // Wait to see the error occur. The DestroyChannel() will destroy the IPC
  // channel on the IO thread, which then notifies the main thread about the
  // error state.
  base::RunLoop wait_for_io_run_loop;
  base::CreateSingleThreadTaskRunner({BrowserThread::IO})
      ->PostTask(FROM_HERE, wait_for_io_run_loop.QuitClosure());
  // Waits for the IO thread to run.
  wait_for_io_run_loop.Run();

  // Waits for the main thread to run.
  base::RunLoop().RunUntilIdle();
  // The error has become visible on the main thread now.
  EXPECT_NE(impl->GetLastState().error, gpu::error::kNoError);

  // Creating a transfer buffer still works.
  id = -1;
  buffer = impl->CreateTransferBuffer(100, &id);
  EXPECT_TRUE(buffer);
  EXPECT_GE(id, 0);
}
#endif

class GpuProcessHostDisableGLBrowserTest : public GpuProcessHostBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    GpuProcessHostBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kUseGL,
                                    gl::kGLImplementationDisabledName);
  }
};

// Android and CrOS don't support disabling GL.
#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(GpuProcessHostDisableGLBrowserTest, CreateAndDestroy) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  base::RunLoop run_loop;
  StopGpuProcess(run_loop.QuitClosure());
  run_loop.Run();
}
#endif

}  // namespace content
