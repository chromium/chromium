// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/peak_gpu_memory_tracker_impl.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

namespace {

const uint64_t kPeakMemory = 42u;

// Test implementation of viz::mojom::GpuService which only implements the peak
// memory monitoring aspects.
class TestGpuService : public viz::mojom::GpuService {
 public:
  TestGpuService() = default;
  TestGpuService(const TestGpuService*) = delete;
  ~TestGpuService() override = default;
  TestGpuService& operator=(const TestGpuService&) = delete;

  // mojom::GpuService:
  void StartPeakMemoryMonitor(uint32_t sequence_num) override {
    peak_memory_monitor_started_ = true;
  }

  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override {
    std::move(callback).Run(kPeakMemory);
  }

  bool peak_memory_monitor_started() const {
    return peak_memory_monitor_started_;
  }

 private:
  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           bool cache_shaders_on_disk,
                           EstablishGpuChannelCallback callback) override {}
  void CloseChannel(int32_t client_id) override {}
#if defined(OS_CHROMEOS)
  void CreateArcVideoDecodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver)
      override {}
  void CreateArcVideoEncodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver)
      override {}
  void CreateArcVideoProtectedBufferAllocator(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver) override {}
  void CreateArcProtectedBufferManager(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver)
      override {}
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override {}
  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override {}
#endif  // defined(OS_CHROMEOS)
  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override {}
  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override {}
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override {}
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override {}
#if defined(OS_WIN)
  void RequestCompleteGpuInfo(
      RequestCompleteGpuInfoCallback callback) override {}
  void GetGpuSupportedRuntimeVersion(
      GetGpuSupportedRuntimeVersionCallback callback) override {}
#endif
  void RequestHDRStatus(RequestHDRStatusCallback callback) override {}
  void LoadedShader(int32_t client_id,
                    const std::string& key,
                    const std::string& data) override {}
  void WakeUpGpu() override {}
  void GpuSwitched(gl::GpuPreference active_gpu_heuristic) override {}
  void DestroyAllChannels() override {}
  void OnBackgroundCleanup() override {}
  void OnBackgrounded() override {}
  void OnForegrounded() override {}
#if defined(OS_MACOSX)
  void BeginCATransaction() override {}
  void CommitCATransaction(CommitCATransactionCallback callback) override {}
#endif
  void Crash() override {}
  void Hang() override {}
  void ThrowJavaException() override {}
  void Stop(StopCallback callback) override {}

  bool peak_memory_monitor_started_ = false;
};

// Runs |task| on the Browser's IO thread, and blocks the Main thread until that
// task has ran.
void PostTaskToIOThreadAndWait(base::OnceClosure task) {
  base::RunLoop run_loop;
  base::PostTaskAndReply(FROM_HERE, {content::BrowserThread::IO},
                         std::move(task), run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace

class PeakGpuMemoryTrackerImplTest : public ContentBrowserTest {
 public:
  PeakGpuMemoryTrackerImplTest() = default;
  ~PeakGpuMemoryTrackerImplTest() override = default;

  // Waits until all messages to the mojo::Remote<viz::mojom::GpuService> have
  // been processed.
  void FlushRemoteForTesting() {
    PostTaskToIOThreadAndWait(
        base::BindOnce(&viz::GpuHostImplTestApi::FlushRemoteForTesting,
                       base::Unretained(gpu_host_impl_test_api_.get())));
  }

  // Initializes the TestGpuService, and installs it as the active service.
  void InitOnIOThread() {
    gpu_host_impl_test_api_ = std::make_unique<viz::GpuHostImplTestApi>(
        GpuProcessHost::Get()->gpu_host());
    test_gpu_service_ = std::make_unique<TestGpuService>();
    mojo::Remote<viz::mojom::GpuService> gpu_service_remote;
    gpu_service_receiver_ =
        std::make_unique<mojo::Receiver<viz::mojom::GpuService>>(
            test_gpu_service_.get(),
            gpu_service_remote.BindNewPipeAndPassReceiver());
    gpu_host_impl_test_api_->SetGpuService(std::move(gpu_service_remote));
  }

  // Callback to provide to a PeakGpuMemoryTracker. Tests must provide
  // |runloop_closure| and run the base::RunLoop. This will then quit the loop
  // once the response from mojo has been received and reposted to the main
  // thread.
  void PeakMemoryCallback(base::OnceClosure runloop_closure,
                          uint64_t peak_memory) {
    peak_memory_ = peak_memory;
    std::move(runloop_closure).Run();
  }

  uint64_t peak_memory() const { return peak_memory_; }

  // Provides access to the TestGpuService on the Main Thread for test
  // verifications. All mojo calls should be performed on the IO Thread.
  TestGpuService* gpu_service() const { return test_gpu_service_.get(); }

  // Setup requires that we have the Browser threads still initialized.
  // ContentBrowserTest:
  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    PostTaskToIOThreadAndWait(base::BindOnce(
        &PeakGpuMemoryTrackerImplTest::InitOnIOThread, base::Unretained(this)));
  }
  void PostRunTestOnMainThread() override {
    PostTaskToIOThreadAndWait(base::BindOnce(
        [](std::unique_ptr<mojo::Receiver<viz::mojom::GpuService>>
               gpu_service_receiver) {},
        std::move(gpu_service_receiver_)));
    ContentBrowserTest::PostRunTestOnMainThread();
  }

 private:
  uint64_t peak_memory_ = 0;
  std::unique_ptr<TestGpuService> test_gpu_service_ = nullptr;
  std::unique_ptr<viz::GpuHostImplTestApi> gpu_host_impl_test_api_ = nullptr;
  std::unique_ptr<mojo::Receiver<viz::mojom::GpuService>>
      gpu_service_receiver_ = nullptr;
};

// Verifies that when a PeakGpuMemoryTracker is destroyed, that the client's
// callback is appropriately called.
IN_PROC_BROWSER_TEST_F(PeakGpuMemoryTrackerImplTest, PeakGpuMemoryCallback) {
  base::RunLoop run_loop;
  std::unique_ptr<PeakGpuMemoryTracker> tracker = PeakGpuMemoryTracker::Create(
      base::BindOnce(&PeakGpuMemoryTrackerImplTest::PeakMemoryCallback,
                     base::Unretained(this), run_loop.QuitClosure()));
  FlushRemoteForTesting();
  // No report in response to creation.
  EXPECT_EQ(0u, peak_memory());
  // However the serive should have started monitoring.
  EXPECT_TRUE(gpu_service()->peak_memory_monitor_started());

  // Deleting the tracker should start a request for peak Gpu memory usage, with
  // the callback being a posted task.
  tracker.reset();
  FlushRemoteForTesting();
  // Wait for PeakMemoryCallback to be ran on this thread.
  run_loop.Run();
  EXPECT_EQ(kPeakMemory, peak_memory());
}

}  // namespace content
