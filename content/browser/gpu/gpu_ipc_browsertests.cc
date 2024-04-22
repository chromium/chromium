// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_stream_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/test/gpu_browsertest_helpers.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gl/gl_switches.h"

namespace {

// RunLoop implementation that runs until it observes OnContextLost().
class ContextLostRunLoop : public viz::ContextLostObserver {
 public:
  explicit ContextLostRunLoop(viz::RasterContextProvider* context_provider)
      : context_provider_(context_provider) {
    context_provider_->AddObserver(this);
  }

  ContextLostRunLoop(const ContextLostRunLoop&) = delete;
  ContextLostRunLoop& operator=(const ContextLostRunLoop&) = delete;

  ~ContextLostRunLoop() override { context_provider_->RemoveObserver(this); }

  void RunUntilContextLost() { run_loop_.Run(); }

 private:
  // viz::LostContextProvider:
  void OnContextLost() override { run_loop_.Quit(); }

  const raw_ptr<viz::RasterContextProvider> context_provider_;
  base::RunLoop run_loop_;
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

    provider_ = content::GpuBrowsertestCreateContext(
        std::move(gpu_channel_host), /*wants_raster_interface=*/false);
    auto result = provider_->BindToCurrentSequence();
    CHECK_EQ(result, gpu::ContextResult::kSuccess);
    gl_ = provider_->ContextGL();
    context_support_ = provider_->ContextSupport();

    ContentBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Must delete the context first.
    gl_ = nullptr;
    context_support_ = nullptr;
    provider_ = nullptr;
    ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  scoped_refptr<viz::ContextProviderCommandBuffer> provider_;
  raw_ptr<gpu::ContextSupport> context_support_ = nullptr;
  raw_ptr<gpu::gles2::GLES2Interface> gl_ = nullptr;
};

class TestGpuHostImplDelegate
    : public viz::GpuHostImplTestApi::HookDelegateBase {
 public:
  TestGpuHostImplDelegate() = default;
  ~TestGpuHostImplDelegate() override = default;

  TestGpuHostImplDelegate(const TestGpuHostImplDelegate&) = delete;
  TestGpuHostImplDelegate& operator=(const TestGpuHostImplDelegate&) = delete;

  // viz::GpuHostImpl::Delegate
  bool GpuAccessAllowed() const override { return false; }
};

}  // namespace

// Include the shared tests.
#define CONTEXT_TEST_F IN_PROC_BROWSER_TEST_F
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
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
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_Basic Basic
#else
#define MAYBE_Basic DISABLED_Basic
#endif
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest, MAYBE_Basic) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(GetGpuChannel() != nullptr);
}

#if !BUILDFLAG(IS_ANDROID)
// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

// Test fails on Chromeos + Mac, flaky on Windows because UI Compositor
// establishes a GPU channel.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
  ASSERT_EQ(provider->BindToCurrentSequence(), gpu::ContextResult::kSuccess);
  GpuProcessHost::CallOnUI(FROM_HERE, GPU_PROCESS_KIND_SANDBOXED,
                           false /* force_create */,
                           base::BindOnce([](GpuProcessHost* host) {
                             if (host)
                               host->gpu_service()->Crash();
                           }));
  run_loop.RunUntilContextLost();

  EXPECT_FALSE(IsChannelEstablished());
  EstablishAndWait();
  EXPECT_TRUE(IsChannelEstablished());
}

// Disabled outside linux like other tests here sadface.
// crbug.com/1224892: the test if flaky on linux and lacros.
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       DISABLED_CreateTransferBuffer) {
  DCHECK(!IsChannelEstablished());
  EstablishAndWait();

  gpu::ContextCreationAttribs attributes;
  attributes.bind_generates_resource = false;

  auto impl = std::make_unique<gpu::CommandBufferProxyImpl>(
      GetGpuChannel(), content::kGpuStreamIdDefault,
      base::SingleThreadTaskRunner::GetCurrentDefault());
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
  GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                      wait_for_io_run_loop.QuitClosure());
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

IN_PROC_BROWSER_TEST_F(BrowserGpuChannelHostFactoryTest,
                       CallbackOnSynchronousFailure) {
  // Ensure that there is no pending establish request.
  EstablishAndWait();

  viz::GpuHostImplTestApi test_api(GpuProcessHost::Get()->gpu_host());

  // This delegate disallows GPU access, which will cause EstablishGpuChannel()
  // to fail synchronously.
  test_api.HookDelegate(std::make_unique<TestGpuHostImplDelegate>());

  bool event = false;
  GetFactory()->EstablishGpuChannel(
      base::BindOnce(&BrowserGpuChannelHostFactoryTest::Signal,
                     base::Unretained(this), &event));

  // Expect that the callback has been called.
  EXPECT_TRUE(event);
}

}  // namespace content
