// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/peak_gpu_memory_tracker_impl.h"

#include "base/clang_profiling_buildflags.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/test/gpu_host_impl_test_api.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/peak_gpu_memory_tracker_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"

namespace content {

namespace {

const uint64_t kPeakMemoryMB = 42u;
const uint64_t kPeakMemory = kPeakMemoryMB * 1048576u;

// Test implementation of viz::mojom::GpuService which only implements the peak
// memory monitoring aspects.
class TestGpuService : public viz::mojom::GpuService {
 public:
  explicit TestGpuService(base::RepeatingClosure quit_closure)
      : quit_closure_(quit_closure) {}
  TestGpuService(const TestGpuService*) = delete;
  ~TestGpuService() override = default;
  TestGpuService& operator=(const TestGpuService&) = delete;

  // mojom::GpuService:
  void StartPeakMemoryMonitor(uint32_t sequence_num) override {
    quit_closure_.Run();
  }

  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override {
    base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>
        allocation_per_source;
    allocation_per_source[gpu::GpuPeakMemoryAllocationSource::UNKNOWN] =
        kPeakMemory;
    std::move(callback).Run(kPeakMemory, allocation_per_source);
  }

 private:
  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           EstablishGpuChannelCallback callback) override {}
  void SetChannelClientPid(int32_t client_id,
                           base::ProcessId client_pid) override {}
  void SetChannelDiskCacheHandle(
      int32_t client_id,
      const gpu::GpuDiskCacheHandle& handle) override {}
  void OnDiskCacheHandleDestoyed(
      const gpu::GpuDiskCacheHandle& handle) override {}
  void CloseChannel(int32_t client_id) override {}
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  void CreateArcVideoDecodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver)
      override {}
  void CreateArcVideoDecoder(
      mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) override {}
  void CreateArcVideoEncodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver)
      override {}
  void CreateArcVideoProtectedBufferAllocator(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver) override {}
  void CreateArcProtectedBufferManager(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver)
      override {}
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override {}
  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(IS_WIN)
  void RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle surface_handle,
      RegisterDCOMPSurfaceHandleCallback callback) override {}
  void UnregisterDCOMPSurfaceHandle(
      const base::UnguessableToken& token) override {}
#endif

  void BindClientGmbInterface(
      mojo::PendingReceiver<gpu::mojom::ClientGmbInterface> receiver,
      int client_id) override {}

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override {}

  void BindWebNNContextProvider(
      mojo::PendingReceiver<webnn::mojom::WebNNContextProvider> receiver,
      int32_t client_id) override {}

  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override {
    // While executing these tests on Linux, HostGpuMemoryBufferManager may
    // invoke this method and wait synchronously on the result to determine
    // whether NV12 GMBs are supported. It is thus necessary to invoke the
    // callback here. Passing an empty GMB handle is fine as it will simply
    // signify that NV12 GMBs are not supported, which is not information that
    // is relevant to these tests one way or the other.
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
  }
  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override {}
  void CopyGpuMemoryBuffer(::gfx::GpuMemoryBufferHandle buffer_handle,
                           ::base::UnsafeSharedMemoryRegion shared_memory,
                           CopyGpuMemoryBufferCallback callback) override {}
  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override {}
#if BUILDFLAG(IS_WIN)
  void RequestDXGIInfo(RequestDXGIInfoCallback callback) override {}
#endif
  void LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                  const std::string& key,
                  const std::string& data) override {}
  void WakeUpGpu() override {}
  void GpuSwitched(gl::GpuPreference active_gpu_heuristic) override {}
  void DisplayAdded() override {}
  void DisplayRemoved() override {}
  void DisplayMetricsChanged() override {}
  void DestroyAllChannels() override {}
  void OnBackgroundCleanup() override {}
  void OnBackgrounded() override {}
  void OnForegrounded() override {}
#if !BUILDFLAG(IS_ANDROID)
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) override {}
#endif
#if BUILDFLAG(IS_APPLE)
  void BeginCATransaction() override {}
  void CommitCATransaction(CommitCATransactionCallback callback) override {}
#endif
#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override {}
#endif
  void GetDawnInfo(bool collect_metrics,
                   GetDawnInfoCallback callback) override {}

  void Crash() override {}
  void Hang() override {}
  void ThrowJavaException() override {}

  base::RepeatingClosure quit_closure_;
};

}  // namespace

class PeakGpuMemoryTrackerImplTest : public ContentBrowserTest {
 public:
  PeakGpuMemoryTrackerImplTest() : gpu_service_thread_("Gpu Service") {}
  ~PeakGpuMemoryTrackerImplTest() override = default;

  // Waits until all messages to the mojo::Remote<viz::mojom::GpuService> have
  // been processed.
  void FlushRemoteForTesting() {
    gpu_host_impl_test_api_->FlushRemoteForTesting();
  }

  // Initializes the TestGpuService. Must be done on the GPU process thread in
  // order to support processing of synchronous Mojo messages that are
  // dispatched on the UI thread.
  void InitOnGpuServiceThread(
      mojo::PendingReceiver<viz::mojom::GpuService> receiver,
      base::RepeatingClosure quit_closure) {
    gpu_service_thread_state_.test_gpu_service =
        std::make_unique<TestGpuService>(std::move(quit_closure));
    gpu_service_thread_state_.gpu_service_receiver =
        std::make_unique<mojo::Receiver<viz::mojom::GpuService>>(
            gpu_service_thread_state_.test_gpu_service.get(),
            std::move(receiver));
  }

  void SetTestingCallback(input::PeakGpuMemoryTracker* tracker,
                          base::OnceClosure callback) {
    static_cast<PeakGpuMemoryTrackerImpl*>(tracker)
        ->post_gpu_service_callback_for_testing_ = std::move(callback);
  }

  // Setup requires that we have the Browser threads still initialized.
  // ContentBrowserTest:
  void PreRunTestOnMainThread() override {
    gpu_service_thread_.StartAndWaitForTesting();

    run_loop_for_start_ = std::make_unique<base::RunLoop>();
    ContentBrowserTest::PreRunTestOnMainThread();

    gpu_host_impl_test_api_ = std::make_unique<viz::GpuHostImplTestApi>(
        GpuProcessHost::Get()->gpu_host());
    mojo::Remote<viz::mojom::GpuService> gpu_service_remote;
    auto receiver = gpu_service_remote.BindNewPipeAndPassReceiver();
    gpu_host_impl_test_api_->SetGpuService(std::move(gpu_service_remote));

    PostTaskToGpuServiceThreadAndWait(
        base::BindOnce(&PeakGpuMemoryTrackerImplTest::InitOnGpuServiceThread,
                       base::Unretained(this), std::move(receiver),
                       run_loop_for_start_->QuitClosure()));
  }
  void PostRunTestOnMainThread() override {
    PostTaskToGpuServiceThreadAndWait(
        base::BindOnce([](GpuServiceThreadState gpu_service_thread_state) {},
                       std::move(gpu_service_thread_state_)));
    gpu_service_thread_.Stop();

    ContentBrowserTest::PostRunTestOnMainThread();
  }

  void WaitForStartPeakMemoryMonitor() { run_loop_for_start_->Run(); }

 private:
  // Posts task to the GPU service thread and waits for it to complete.
  void PostTaskToGpuServiceThreadAndWait(base::OnceClosure task) {
    base::WaitableEvent completion_event;

    gpu_service_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure task, base::WaitableEvent* wait_event) {
              std::move(task).Run();
              wait_event->Signal();
            },
            std::move(task), &completion_event));

    completion_event.Wait();
  }

  struct GpuServiceThreadState {
    std::unique_ptr<TestGpuService> test_gpu_service;
    std::unique_ptr<mojo::Receiver<viz::mojom::GpuService>>
        gpu_service_receiver;
  };

  base::Thread gpu_service_thread_;
  std::unique_ptr<base::RunLoop> run_loop_for_start_;
  std::unique_ptr<viz::GpuHostImplTestApi> gpu_host_impl_test_api_;
  GpuServiceThreadState gpu_service_thread_state_;
};

// Verifies that when a PeakGpuMemoryTracker is destroyed, that the browser's
// callback properly updates the histograms.
IN_PROC_BROWSER_TEST_F(PeakGpuMemoryTrackerImplTest, PeakGpuMemoryCallback) {
  base::HistogramTester histogram;
  base::RunLoop run_loop;
  std::unique_ptr<input::PeakGpuMemoryTracker> tracker =
      PeakGpuMemoryTrackerFactory::Create(
          input::PeakGpuMemoryTracker::Usage::PAGE_LOAD);
  SetTestingCallback(tracker.get(), run_loop.QuitClosure());
  FlushRemoteForTesting();
  // No report in response to creation.
  histogram.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 0);
  histogram.ExpectTotalCount(
      "Memory.GPU.PeakMemoryAllocationSource.PageLoad.Unknown", 0);
  // However the serive should have started monitoring.
  WaitForStartPeakMemoryMonitor();

  // Deleting the tracker should start a request for peak Gpu memory usage, with
  // the callback being a posted task.
  tracker.reset();
  FlushRemoteForTesting();
  // Wait for callback to be ran on the UI thread, which will call the
  // QuitClosure.
  run_loop.Run();
  histogram.ExpectUniqueSample("Memory.GPU.PeakMemoryUsage2.PageLoad",
                               kPeakMemoryMB, 1);
  histogram.ExpectUniqueSample(
      "Memory.GPU.PeakMemoryAllocationSource2.PageLoad.Unknown", kPeakMemoryMB,
      1);
}

}  // namespace content
