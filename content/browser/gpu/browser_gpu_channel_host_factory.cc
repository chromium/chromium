// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/browser_gpu_channel_host_factory.h"

#include <utility>

#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/host/gpu_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_memory_buffer_manager_singleton.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/ipc/common/gpu_watchdog_timeout.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/constants.mojom.h"

#if defined(OS_MAC)
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#endif

namespace content {

namespace {

#if defined(OS_ANDROID)
void TimedOut() {
  LOG(FATAL) << "Timed out waiting for GPU channel.";
}

void DumpGpuStackOnProcessThread() {
  GpuProcessHost* host =
      GpuProcessHost::Get(GPU_PROCESS_KIND_SANDBOXED, /*force_create=*/false);
  if (host) {
    host->DumpProcessStack();
  }
  GetUIThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(&TimedOut));
}

void TimerFired() {
  auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                         ? GetUIThreadTaskRunner({})
                         : GetIOThreadTaskRunner({});
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&DumpGpuStackOnProcessThread));
}
#endif  // OS_ANDROID

}  // namespace

BrowserGpuChannelHostFactory* BrowserGpuChannelHostFactory::instance_ = nullptr;

class BrowserGpuChannelHostFactory::EstablishRequest
    : public base::RefCountedThreadSafe<EstablishRequest> {
 public:
  static scoped_refptr<EstablishRequest> Create(int gpu_client_id,
                                                uint64_t gpu_client_tracing_id,
                                                bool sync);
  void Wait();
  void Cancel();

  void AddCallback(gpu::GpuChannelEstablishedCallback callback) {
    established_callbacks_.push_back(std::move(callback));
  }

  std::vector<gpu::GpuChannelEstablishedCallback> TakeCallbacks() {
    return std::move(established_callbacks_);
  }

  const scoped_refptr<gpu::GpuChannelHost>& gpu_channel() {
    return gpu_channel_;
  }

 private:
  friend class base::RefCountedThreadSafe<EstablishRequest>;
  EstablishRequest(int gpu_client_id, uint64_t gpu_client_tracing_id);
  ~EstablishRequest() {}
  void RestartTimeout();
  // Note |sync| is only true if EstablishGpuChannelSync is being called AND
  // ProcessHostOnUI is enabled. In that case we make the sync mojo call since
  // we're on the UI thread and therefore can't wait for an async mojo reply on
  // the same thread.
  void Establish(bool sync);
  void OnEstablished(mojo::ScopedMessagePipeHandle channel_handle,
                     const gpu::GPUInfo& gpu_info,
                     const gpu::GpuFeatureInfo& gpu_feature_info,
                     viz::GpuHostImpl::EstablishChannelStatus status);
  void FinishOnProcessThread();
  void FinishAndRunCallbacksOnMain();
  void FinishOnMain();
  void RunCallbacksOnMain();

  std::vector<gpu::GpuChannelEstablishedCallback> established_callbacks_;
  base::WaitableEvent event_;
  const int gpu_client_id_;
  const uint64_t gpu_client_tracing_id_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  bool finished_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
};

scoped_refptr<BrowserGpuChannelHostFactory::EstablishRequest>
BrowserGpuChannelHostFactory::EstablishRequest::Create(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id,
    bool sync) {
  scoped_refptr<EstablishRequest> establish_request =
      new EstablishRequest(gpu_client_id, gpu_client_tracing_id);
  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
    establish_request->Establish(sync);
  } else {
    // PostTask outside the constructor to ensure at least one reference exists.
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserGpuChannelHostFactory::EstablishRequest::Establish,
            establish_request, false));
  }

  return establish_request;
}

BrowserGpuChannelHostFactory::EstablishRequest::EstablishRequest(
    int gpu_client_id,
    uint64_t gpu_client_tracing_id)
    : event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      gpu_client_id_(gpu_client_id),
      gpu_client_tracing_id_(gpu_client_tracing_id),
      finished_(false),
#if defined(OS_MAC)
      main_task_runner_(ui::WindowResizeHelperMac::Get()->task_runner())
#else
      main_task_runner_(base::ThreadTaskRunnerHandle::Get())
#endif
{
}

void BrowserGpuChannelHostFactory::EstablishRequest::RestartTimeout() {
  BrowserGpuChannelHostFactory* factory =
      BrowserGpuChannelHostFactory::instance();
  if (factory)
    factory->RestartTimeout();
}

void BrowserGpuChannelHostFactory::EstablishRequest::Establish(bool sync) {
  GpuProcessHost* host = GpuProcessHost::Get();
  if (!host) {
    LOG(ERROR) << "Failed to launch GPU process.";
    FinishOnProcessThread();
    return;
  }

  bool is_gpu_host = true;
  host->gpu_host()->EstablishGpuChannel(
      gpu_client_id_, gpu_client_tracing_id_, is_gpu_host, sync,
      base::BindOnce(
          &BrowserGpuChannelHostFactory::EstablishRequest::OnEstablished,
          this));
  host->gpu_host()->SetChannelClientPid(gpu_client_id_,
                                        base::GetCurrentProcId());
}

void BrowserGpuChannelHostFactory::EstablishRequest::OnEstablished(
    mojo::ScopedMessagePipeHandle channel_handle,
    const gpu::GPUInfo& gpu_info,
    const gpu::GpuFeatureInfo& gpu_feature_info,
    viz::GpuHostImpl::EstablishChannelStatus status) {
  if (!channel_handle.is_valid() &&
      status == viz::GpuHostImpl::EstablishChannelStatus::kGpuHostInvalid &&
      // Ask client every time instead of passing this down from UI thread to
      // avoid having the value be stale.
      GetContentClient()->browser()->AllowGpuLaunchRetryOnIOThread()) {
    DVLOG(1) << "Failed to create channel on existing GPU process. Trying to "
                "restart GPU process.";
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserGpuChannelHostFactory::EstablishRequest::RestartTimeout,
            this));
    auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                           ? GetUIThreadTaskRunner({})
                           : GetIOThreadTaskRunner({});
    // TODO(jam): can we ever enter this when it was a sync call?
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &BrowserGpuChannelHostFactory::EstablishRequest::Establish, this,
            false));
    return;
  }

  if (channel_handle.is_valid()) {
    gpu_channel_ = base::MakeRefCounted<gpu::GpuChannelHost>(
        gpu_client_id_, gpu_info, gpu_feature_info, std::move(channel_handle),
        GetIOThreadTaskRunner({}));
  }
  FinishOnProcessThread();
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnProcessThread() {
  event_.Signal();
  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
    FinishAndRunCallbacksOnMain();
  } else {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserGpuChannelHostFactory::EstablishRequest::
                           FinishAndRunCallbacksOnMain,
                       this));
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::
    FinishAndRunCallbacksOnMain() {
  FinishOnMain();
  RunCallbacksOnMain();
}

void BrowserGpuChannelHostFactory::EstablishRequest::FinishOnMain() {
  if (!finished_) {
    BrowserGpuChannelHostFactory* factory =
        BrowserGpuChannelHostFactory::instance();
    factory->GpuChannelEstablished(this);
    finished_ = true;
  }
}

void BrowserGpuChannelHostFactory::EstablishRequest::RunCallbacksOnMain() {
  std::vector<gpu::GpuChannelEstablishedCallback> established_callbacks;
  established_callbacks_.swap(established_callbacks);
  for (auto&& callback : std::move(established_callbacks))
    std::move(callback).Run(gpu_channel_);
}

void BrowserGpuChannelHostFactory::EstablishRequest::Wait() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  {
    // We're blocking the UI thread, which is generally undesirable.
    // In this case we need to wait for this before we can show any UI
    // /anyway/, so it won't cause additional jank.
    // TODO(piman): Make this asynchronous (http://crbug.com/125248).
    TRACE_EVENT0("browser",
                 "BrowserGpuChannelHostFactory::EstablishGpuChannelSync");
    base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
    event_.Wait();
  }
  FinishOnMain();
}

void BrowserGpuChannelHostFactory::EstablishRequest::Cancel() {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  finished_ = true;
  established_callbacks_.clear();
}

void BrowserGpuChannelHostFactory::Initialize(bool establish_gpu_channel) {
  DCHECK(!instance_);
  instance_ = new BrowserGpuChannelHostFactory();
  if (establish_gpu_channel) {
    instance_->EstablishGpuChannel(gpu::GpuChannelEstablishedCallback());
  }
}

void BrowserGpuChannelHostFactory::Terminate() {
  DCHECK(instance_);
  delete instance_;
  instance_ = nullptr;
}

void BrowserGpuChannelHostFactory::MaybeCloseChannel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!gpu_channel_ || !gpu_channel_->HasOneRef())
    return;

  gpu_channel_->DestroyChannel();
  gpu_channel_ = nullptr;
}

void BrowserGpuChannelHostFactory::CloseChannel() {
  if (gpu_channel_) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI) &&
      gpu_memory_buffer_manager_) {
    delete gpu_memory_buffer_manager_.release();
  } else {
    gpu_memory_buffer_manager_ = nullptr;
  }
}

BrowserGpuChannelHostFactory::BrowserGpuChannelHostFactory()
    : gpu_client_id_(ChildProcessHostImpl::GenerateChildProcessUniqueId()),
      gpu_client_tracing_id_(
          memory_instrumentation::mojom::kServiceTracingProcessId),
      gpu_memory_buffer_manager_(
          new GpuMemoryBufferManagerSingleton(gpu_client_id_)) {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuShaderDiskCache)) {
    DCHECK(GetContentClient());
    base::FilePath cache_dir =
        GetContentClient()->browser()->GetShaderDiskCacheDirectory();
    auto task_runner = base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                           ? GetUIThreadTaskRunner({})
                           : GetIOThreadTaskRunner({});
    if (!cache_dir.empty()) {
      task_runner->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BrowserGpuChannelHostFactory::InitializeShaderDiskCacheOnIO,
              gpu_client_id_, cache_dir));
    }

    bool use_gr_shader_cache = base::FeatureList::IsEnabled(
                                   features::kDefaultEnableOopRasterization) ||
                               features::IsUsingSkiaRenderer();
    if (use_gr_shader_cache) {
      base::FilePath gr_cache_dir =
          GetContentClient()->browser()->GetGrShaderDiskCacheDirectory();
      if (!gr_cache_dir.empty()) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(
                &BrowserGpuChannelHostFactory::InitializeGrShaderDiskCacheOnIO,
                gr_cache_dir));
      }
    }
  }
}

BrowserGpuChannelHostFactory::~BrowserGpuChannelHostFactory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (pending_request_.get())
    pending_request_->Cancel();
  if (gpu_channel_) {
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI) &&
      gpu_memory_buffer_manager_) {
    // Delete this on the main thread (since otherwise the unique_ptr has a
    // trait to delete it on the IO thread).
    delete gpu_memory_buffer_manager_.release();
  }
}

void BrowserGpuChannelHostFactory::EstablishGpuChannel(
    gpu::GpuChannelEstablishedCallback callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  EstablishGpuChannel(std::move(callback), false);
}

// Blocking the UI thread to open a GPU channel is not supported on Android.
// (Opening the initial channel to a child process involves handling a reply
// task on the UI thread first, so we cannot block here.)
scoped_refptr<gpu::GpuChannelHost>
BrowserGpuChannelHostFactory::EstablishGpuChannelSync() {
#if defined(OS_ANDROID)
  NOTREACHED();
  return nullptr;
#else
  EstablishGpuChannel(gpu::GpuChannelEstablishedCallback(), true);

  if (!base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
    if (pending_request_.get())
      pending_request_->Wait();
  }
  return gpu_channel_;
#endif
}

void BrowserGpuChannelHostFactory::EstablishGpuChannel(
    gpu::GpuChannelEstablishedCallback callback,
    bool sync) {
  if (gpu_channel_.get() && gpu_channel_->IsLost()) {
    DCHECK(!pending_request_.get());
    // Recreate the channel if it has been lost.
    gpu_channel_->DestroyChannel();
    gpu_channel_ = nullptr;
  }

  std::vector<gpu::GpuChannelEstablishedCallback> callbacks;
  if (base::FeatureList::IsEnabled(features::kProcessHostOnUI) && sync &&
      !gpu_channel_ && pending_request_) {
    // There's a previous request. Cancel it since we must call the synchronous
    // version of the mojo method and the previous call was asynchronous.
    callbacks = pending_request_->TakeCallbacks();
    GpuProcessHost* host = GpuProcessHost::Get();
    if (host)
      host->gpu_host()->CloseChannel(gpu_client_id_);
    pending_request_->Cancel();
    pending_request_ = nullptr;
  }

  if (!gpu_channel_.get() && !pending_request_.get()) {
    // We should only get here if the context was lost.
    pending_request_ =
        EstablishRequest::Create(gpu_client_id_, gpu_client_tracing_id_, sync);
    // Sync and timeouts aren't currently compatible, which is fine since sync
    // isn't used on Android while timeouts are only used on Android.
    if (!sync)
      RestartTimeout();
  }

  if (!callback.is_null()) {
    if (gpu_channel_.get()) {
      std::move(callback).Run(gpu_channel_);
    } else {
      DCHECK(pending_request_);
      pending_request_->AddCallback(std::move(callback));
    }
  }

  for (auto& cb : callbacks)
    std::move(cb).Run(gpu_channel_);
}

gpu::GpuMemoryBufferManager*
BrowserGpuChannelHostFactory::GetGpuMemoryBufferManager() {
  return gpu_memory_buffer_manager_.get();
}

// Ensures that any pending timeout is cancelled when we are backgrounded.
// Restarts the timeout when we return to the foreground.
void BrowserGpuChannelHostFactory::SetApplicationVisible(bool is_visible) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (is_visible_ == is_visible)
    return;

  is_visible_ = is_visible;
  if (is_visible_) {
    RestartTimeout();
  } else {
    timeout_.Stop();
  }
}

gpu::GpuChannelHost* BrowserGpuChannelHostFactory::GetGpuChannel() {
  if (gpu_channel_.get() && !gpu_channel_->IsLost())
    return gpu_channel_.get();

  return nullptr;
}

void BrowserGpuChannelHostFactory::GpuChannelEstablished(
    EstablishRequest* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!pending_request_ || pending_request_ == request);
  gpu_channel_ = request->gpu_channel();
  pending_request_ = nullptr;
  timeout_.Stop();
  if (gpu_channel_)
    GetContentClient()->SetGpuInfo(gpu_channel_->gpu_info());
}

void BrowserGpuChannelHostFactory::RestartTimeout() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
// Only implement timeout on Android, which does not have a software fallback.
#if defined(OS_ANDROID)
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kDisableTimeoutsForProfiling)) {
    return;
  }
  // Only enable it for out of process GPU. In-process generally only has false
  // positives.
  if (cl->HasSwitch(switches::kSingleProcess) ||
      cl->HasSwitch(switches::kInProcessGPU)) {
    return;
  }

  // Don't restart the timeout if we aren't visible. This function will be
  // re-called when we become visible again.
  if (!pending_request_ || !is_visible_)
    return;

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER) || \
    BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  constexpr int64_t kGpuChannelTimeoutInSeconds = 40;
#else
  // This is also monitored by the GPU watchdog (restart or initialization
  // event) in the GPU process. Make this slightly longer than the GPU watchdog
  // timeout to give the GPU a chance to crash itself before crashing the
  // browser.
  int64_t kGpuChannelTimeoutInSeconds =
      gpu::kGpuWatchdogTimeout.InSeconds() * gpu::kRestartFactor + 5;
#endif

  timeout_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(kGpuChannelTimeoutInSeconds),
                 base::BindOnce(&TimerFired));
#endif  // OS_ANDROID
}

// static
void BrowserGpuChannelHostFactory::InitializeShaderDiskCacheOnIO(
    int gpu_client_id,
    const base::FilePath& cache_dir) {
  GetShaderCacheFactorySingleton()->SetCacheInfo(gpu_client_id, cache_dir);
  GetShaderCacheFactorySingleton()->SetCacheInfo(
      gpu::kDisplayCompositorClientId, cache_dir);
}

// static
void BrowserGpuChannelHostFactory::InitializeGrShaderDiskCacheOnIO(
    const base::FilePath& cache_dir) {
  GetShaderCacheFactorySingleton()->SetCacheInfo(gpu::kGrShaderCacheClientId,
                                                 cache_dir);
}

}  // namespace content
