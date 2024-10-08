// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <cstdint>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include <algorithm>
#include <set>
#include <vector>

#include "base/atomicops.h"
#include "base/command_line.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/task_graph.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gles2_command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/image_decode_accelerator_stub.h"
#include "gpu/ipc/service/raster_command_buffer_stub.h"
#include "gpu/ipc/service/webgpu_command_buffer_stub.h"
#include "ipc/ipc_channel.h"
#include "mojo/public/cpp/base/shared_memory_version.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "gpu/ipc/service/dcomp_texture_win.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif  // BUILDFLAG(IS_OZONE)

namespace gpu {

namespace {

#if BUILDFLAG(IS_ANDROID)
bool TryCreateStreamTexture(
    base::WeakPtr<GpuChannel> channel,
    int32_t stream_id,
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver) {
  if (!channel)
    return false;
  return channel->CreateStreamTexture(stream_id, std::move(receiver));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
bool TryCreateDCOMPTexture(
    base::WeakPtr<GpuChannel> channel,
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver) {
  if (!channel)
    return false;
  return channel->CreateDCOMPTexture(route_id, std::move(receiver));
}

bool TryRegisterOverlayStateObserver(
    base::WeakPtr<GpuChannel> channel,
    mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
        promotion_hint_observer,
    const gpu::Mailbox& mailbox) {
  if (!channel)
    return false;
  return channel->RegisterOverlayStateObserver(
      std::move(promotion_hint_observer), std::move(mailbox));
}
#endif  // BUILDFLAG(IS_WIN)

bool WillGetGmbConfigFromGpu() {
#if BUILDFLAG(IS_OZONE)
  // Ozone/X11 requires gpu initialization to be done before it can determine
  // what formats gmb can use. This limitation comes from the requirement to
  // have GLX bindings initialized. The buffer formats will be passed through
  // gpu extra info.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .fetch_buffer_formats_for_gmb_on_gpu;
#else
  return false;
#endif
}

}  // namespace

// This filter does the following:
// - handles the Nop message used for verifying sync tokens on the IO thread
// - forwards messages to child message filters
// - posts control and out of order messages to the main thread
// - forwards other messages to the scheduler
class GPU_IPC_SERVICE_EXPORT GpuChannelMessageFilter
    : public base::RefCountedThreadSafe<GpuChannelMessageFilter>,
      public mojom::GpuChannel {
 public:
  GpuChannelMessageFilter(
      gpu::GpuChannel* gpu_channel,
      const base::UnguessableToken& channel_token,
      Scheduler* scheduler,
      ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
      const gfx::GpuExtraInfo& gpu_extra_info,
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner);
  GpuChannelMessageFilter(const GpuChannelMessageFilter&) = delete;
  GpuChannelMessageFilter& operator=(const GpuChannelMessageFilter&) = delete;

  // Methods called on main thread.
  void Destroy();

  // Called when scheduler is enabled.
  void AddRoute(int32_t route_id, SequenceId sequence_id);
  void RemoveRoute(int32_t route_id);

  // Methods called on IO thread.

  void BindGpuChannel(
      mojo::PendingAssociatedReceiver<mojom::GpuChannel> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  friend class base::RefCountedThreadSafe<GpuChannelMessageFilter>;
  ~GpuChannelMessageFilter() override;

  SequenceId GetSequenceId(int32_t route_id) const;

  // mojom::GpuChannel:
  void CrashForTesting() override;
  void TerminateForTesting() override;
  void GetChannelToken(GetChannelTokenCallback callback) override;
  void Flush(FlushCallback callback) override;
  void GetSharedMemoryForFlushId(
      GetSharedMemoryForFlushIdCallback callback) override;
  void CreateCommandBuffer(
      mojom::CreateCommandBufferParamsPtr config,
      int32_t routing_id,
      base::UnsafeSharedMemoryRegion shared_state,
      mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
      mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
      CreateCommandBufferCallback callback) override;
  void DestroyCommandBuffer(int32_t routing_id,
                            DestroyCommandBufferCallback callback) override;
  void ScheduleImageDecode(mojom::ScheduleImageDecodeParamsPtr params,
                           uint64_t decode_release_count) override;
  void FlushDeferredRequests(std::vector<mojom::DeferredRequestPtr> requests,
                             uint32_t flushed_deferred_message_id) override;

  bool IsNativeBufferSupported(gfx::BufferFormat buffer_format,
                               gfx::BufferUsage buffer_usage);
  void CreateGpuMemoryBuffer(const gfx::Size& size,
                             const viz::SharedImageFormat& format,
                             gfx::BufferUsage buffer_usage,
                             CreateGpuMemoryBufferCallback callback) override;
  void GetGpuMemoryBufferHandleInfo(
      const gpu::Mailbox& mailbox,
      GetGpuMemoryBufferHandleInfoCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void CreateStreamTexture(
      int32_t stream_id,
      mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver,
      CreateStreamTextureCallback callback) override;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN)
  void CreateDCOMPTexture(
      int32_t route_id,
      mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
      CreateDCOMPTextureCallback callback) override;
  void RegisterOverlayStateObserver(
      mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
          promotion_hint_observer,
      const gpu::Mailbox& mailbox,
      RegisterOverlayStateObserverCallback callback) override;
  void CopyToGpuMemoryBufferAsync(
      const gpu::Mailbox& mailbox,
      const std::vector<gpu::SyncToken>& sync_token_dependencies,
      uint64_t release_count,
      CopyToGpuMemoryBufferAsyncCallback callback) override;
  void CopyNativeGmbToSharedMemorySync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory,
      CopyNativeGmbToSharedMemorySyncCallback callback) override;
  void CopyNativeGmbToSharedMemoryAsync(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory,
      CopyNativeGmbToSharedMemoryAsyncCallback callback) override;
#endif  // BUILDFLAG(IS_WIN)
  void WaitForTokenInRange(int32_t routing_id,
                           int32_t start,
                           int32_t end,
                           WaitForTokenInRangeCallback callback) override;
  void WaitForGetOffsetInRange(
      int32_t routing_id,
      uint32_t set_get_buffer_count,
      int32_t start,
      int32_t end,
      WaitForGetOffsetInRangeCallback callback) override;
#if BUILDFLAG(IS_FUCHSIA)
  void RegisterSysmemBufferCollection(mojo::PlatformHandle service_handle,
                                      mojo::PlatformHandle sysmem_token,
                                      const viz::SharedImageFormat& format,
                                      gfx::BufferUsage usage,
                                      bool register_with_image_pipe) override {
    base::AutoLock lock(gpu_channel_lock_);
    if (!gpu_channel_)
      return;

    scheduler_->ScheduleTask(Scheduler::Task(
        gpu_channel_->shared_image_stub()->sequence(),
        base::BindOnce(&gpu::GpuChannel::RegisterSysmemBufferCollection,
                       gpu_channel_->AsWeakPtr(), std::move(service_handle),
                       std::move(sysmem_token), format, usage,
                       register_with_image_pipe),
        std::vector<SyncToken>()));
  }
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Map of route id to scheduler sequence id.
  base::flat_map<int32_t, SequenceId> route_sequences_;
  mutable base::Lock gpu_channel_lock_;

  // Note that this field may be reset at any time by the owning GpuChannel's
  // thread, so it must be accessed under lock and must be tested for null
  // before dereferencing.
  raw_ptr<gpu::GpuChannel> gpu_channel_ GUARDED_BY(gpu_channel_lock_) = nullptr;

  // A token which can be retrieved by GetChannelToken to uniquely identify this
  // channel. Assigned at construction time by the GpuChannelManager, where the
  // token-to-GpuChannel mapping lives.
  const base::UnguessableToken channel_token_;

  raw_ptr<Scheduler> scheduler_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  scoped_refptr<ImageDecodeAcceleratorStub> image_decode_accelerator_stub_;
  const gfx::GpuExtraInfo gpu_extra_info_;
  gpu::GpuMemoryBufferConfigurationSet supported_gmb_configurations_;
  bool supported_gmb_configurations_inited_ = false;
  raw_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_ = nullptr;
  base::ThreadChecker io_thread_checker_;

  bool allow_process_kill_for_testing_ = false;

  std::optional<mojo::SharedMemoryVersionController> shared_memory_controller_;

  mojo::AssociatedReceiver<mojom::GpuChannel> receiver_{this};
};

GpuChannelMessageFilter::GpuChannelMessageFilter(
    gpu::GpuChannel* gpu_channel,
    const base::UnguessableToken& channel_token,
    Scheduler* scheduler,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
    const gfx::GpuExtraInfo& gpu_extra_info,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
    : gpu_channel_(gpu_channel),
      channel_token_(channel_token),
      scheduler_(scheduler),
      main_task_runner_(std::move(main_task_runner)),
      image_decode_accelerator_stub_(
          base::MakeRefCounted<ImageDecodeAcceleratorStub>(
              image_decode_accelerator_worker,
              gpu_channel,
              static_cast<int32_t>(
                  GpuChannelReservedRoutes::kImageDecodeAccelerator))),
      gpu_extra_info_(gpu_extra_info),
      gpu_memory_buffer_factory_(gpu_memory_buffer_factory) {
  // GpuChannel and CommandBufferStub implementations assume that it is not
  // possible to simultaneously execute tasks on these two task runners.
  DCHECK_EQ(main_task_runner_, gpu_channel->task_runner());
  io_thread_checker_.DetachFromThread();
  allow_process_kill_for_testing_ = gpu_channel->gpu_channel_manager()
                                        ->gpu_preferences()
                                        .enable_gpu_benchmarking_extension;

  if (base::FeatureList::IsEnabled(
          features::kConditionallySkipGpuChannelFlush)) {
    shared_memory_controller_.emplace();
  }
}

GpuChannelMessageFilter::~GpuChannelMessageFilter() {
  DCHECK(!gpu_channel_);
}

void GpuChannelMessageFilter::Destroy() {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_)
    return;

  image_decode_accelerator_stub_->Shutdown();

  // Ensure all sync points from this channel are released.
  for (const auto& entry : route_sequences_) {
    gpu_channel_->sync_point_manager()->EnsureFenceSyncReleased(
        SyncToken(CommandBufferNamespace::GPU_IO,
                  CommandBufferIdFromChannelAndRoute(gpu_channel_->client_id(),
                                                     entry.first),
                  UINT64_MAX),
        ReleaseCause::kForceRelease);
  }

  gpu_channel_ = nullptr;
}

void GpuChannelMessageFilter::AddRoute(int32_t route_id,
                                       SequenceId sequence_id) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  DCHECK(gpu_channel_);
  DCHECK(scheduler_);
  DCHECK(!route_sequences_.count(route_id));
  route_sequences_[route_id] = sequence_id;
}

void GpuChannelMessageFilter::RemoveRoute(int32_t route_id) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  DCHECK(gpu_channel_);
  DCHECK(scheduler_);
  DCHECK(route_sequences_.count(route_id));
  route_sequences_.erase(route_id);
}

SequenceId GpuChannelMessageFilter::GetSequenceId(int32_t route_id) const {
  gpu_channel_lock_.AssertAcquired();
  auto it = route_sequences_.find(route_id);
  if (it == route_sequences_.end())
    return SequenceId();
  return it->second;
}

void GpuChannelMessageFilter::FlushDeferredRequests(
    std::vector<mojom::DeferredRequestPtr> requests,
    uint32_t flushed_deferred_message_id) {
  TRACE_EVENT0("gpu", "GpuChannelMessageFilter::FlushDeferredRequests");
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_)
    return;

  std::vector<Scheduler::Task> tasks;
  tasks.reserve(requests.size());
  for (auto& request : requests) {
    int32_t routing_id;
    switch (request->params->which()) {
#if BUILDFLAG(IS_ANDROID)
      case mojom::DeferredRequestParams::Tag::kDestroyStreamTexture:
        routing_id = request->params->get_destroy_stream_texture();
        break;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
      case mojom::DeferredRequestParams::Tag::kDestroyDcompTexture:
        routing_id = request->params->get_destroy_dcomp_texture();
        break;
#endif  // BUILDFLAG(IS_WIN)

      case mojom::DeferredRequestParams::Tag::kCommandBufferRequest:
        routing_id = request->params->get_command_buffer_request()->routing_id;
        break;

      case mojom::DeferredRequestParams::Tag::kSharedImageRequest:
        routing_id = static_cast<int32_t>(
            GpuChannelReservedRoutes::kSharedImageInterface);
        break;
    }

    auto it = route_sequences_.find(routing_id);
    if (it == route_sequences_.end()) {
      DLOG(ERROR) << "Invalid route id in flush list";
      continue;
    }

    SyncToken release;
    if (request->release_count != 0) {
      release = SyncToken(CommandBufferNamespace::GPU_IO,
                          CommandBufferIdFromChannelAndRoute(
                              gpu_channel_->client_id(), routing_id),
                          request->release_count);
    }

    tasks.emplace_back(
        /*sequence_id=*/it->second,
        base::BindOnce(&gpu::GpuChannel::ExecuteDeferredRequest,
                       gpu_channel_->AsWeakPtr(), std::move(request->params)),
        std::move(request->sync_token_fences), release);
  }

  // Threading: GpuChannelManager outlives gpu_channel_, so even though it is a
  // main thread object, we don't have a lifetime issue. However we may be
  // reading something stale here, but we don't synchronize anything here.
  if (gpu_channel_->gpu_channel_manager()->application_backgrounded()) {
    // We expect to clean shared images, so put it on this sequence, to make
    // sure that ordering is conserved, and we execute after.
    auto it = route_sequences_.find(
        static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface));
    tasks.emplace_back(it->second,
                       base::BindOnce(&gpu::GpuChannel::PerformImmediateCleanup,
                                      gpu_channel_->AsWeakPtr()),
                       std::vector<::gpu::SyncToken>());
  }

  scheduler_->ScheduleTasks(std::move(tasks));

  if (shared_memory_controller_) {
    // Update version shared with clients.
    shared_memory_controller_->SetVersion(flushed_deferred_message_id);
  }
}

bool GpuChannelMessageFilter::IsNativeBufferSupported(
    gfx::BufferFormat buffer_format,
    gfx::BufferUsage buffer_usage) {
  // Note that we are initializing the |supported_gmb_configurations_| here to
  // make sure gpu service have already initialized and required metadata like
  // supported buffer configurations have already been sent from browser
  // process to GPU process for wayland.
  if (!supported_gmb_configurations_inited_) {
    supported_gmb_configurations_inited_ = true;
    if (WillGetGmbConfigFromGpu()) {
#if BUILDFLAG(IS_OZONE_X11)
      for (const auto& config : gpu_extra_info_.gpu_memory_buffer_support_x11) {
        supported_gmb_configurations_.emplace(config);
      }
#endif  // BUILDFLAG(IS_OZONE_X11)
    } else {
      supported_gmb_configurations_ =
          gpu::GpuMemoryBufferSupport::GetNativeGpuMemoryBufferConfigurations();
    }
  }
  return base::Contains(supported_gmb_configurations_,
                        gfx::BufferUsageAndFormat(buffer_usage, buffer_format));
}

void GpuChannelMessageFilter::CreateGpuMemoryBuffer(
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage buffer_usage,
    CreateGpuMemoryBufferCallback callback) {
  if (!viz::HasEquivalentBufferFormat(format)) {
    // Client GMB code still operates on BufferFormat so the SharedImageFormat
    // received here must have an equivalent BufferFormat.
    LOG(ERROR) << "Invalid format." << format.ToString();
    std::move(callback).Run(gfx::GpuMemoryBufferHandle());
    return;
  }
  auto buffer_format = ToBufferFormat(format);
  gfx::GpuMemoryBufferHandle handle;

  if (IsNativeBufferSupported(buffer_format, buffer_usage)) {
    handle = gpu_memory_buffer_factory_->CreateNativeGmbHandle(
        MappableSIClientGmbId::kGpuChannel, size, buffer_format, buffer_usage);
  } else {
    if (gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(buffer_usage) &&
        gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
            size, buffer_format)) {
      handle = gpu::GpuMemoryBufferImplSharedMemory::CreateGpuMemoryBuffer(
          gfx::GpuMemoryBufferId(
              static_cast<int>(MappableSIClientGmbId::kGpuChannel)),
          size, buffer_format, buffer_usage);
    }
  }
  if (handle.is_null()) {
    LOG(ERROR) << "Buffer Handle is null.";
  }
  std::move(callback).Run(std::move(handle));
}

void GpuChannelMessageFilter::GetGpuMemoryBufferHandleInfo(
    const gpu::Mailbox& mailbox,
    GetGpuMemoryBufferHandleInfoCallback callback) {
  TRACE_EVENT0("gpu", "GpuChannelMessageFilter::GetGpuMemoryBufferHandleInfo");
  base::AutoLock auto_lock(gpu_channel_lock_);
  int32_t routing_id =
      static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface);
  auto it = route_sequences_.find(routing_id);
  if (it == route_sequences_.end()) {
    LOG(ERROR) << "Invalid route id in flush list.";
    std::move(callback).Run(gfx::GpuMemoryBufferHandle(),
                            viz::SharedImageFormat(), gfx::Size(),
                            gfx::BufferUsage::GPU_READ);
    return;
  }
  std::vector<Scheduler::Task> tasks;
  tasks.emplace_back(
      it->second,
      base::BindOnce(
          &gpu::GpuChannel::GetGpuMemoryBufferHandleInfo,
          gpu_channel_->AsWeakPtr(), mailbox,
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             std::move(callback))),
      std::vector<::gpu::SyncToken>());
  scheduler_->ScheduleTasks(std::move(tasks));
}

void GpuChannelMessageFilter::CrashForTesting() {
  if (allow_process_kill_for_testing_) {
    gl::Crash();
    return;
  }

  receiver_.ReportBadMessage("CrashForTesting is a test-only API");
}

void GpuChannelMessageFilter::TerminateForTesting() {
  if (allow_process_kill_for_testing_) {
    base::Process::TerminateCurrentProcessImmediately(0);
  }

  receiver_.ReportBadMessage("TerminateForTesting is a test-only API");
}

void GpuChannelMessageFilter::GetChannelToken(
    GetChannelTokenCallback callback) {
  std::move(callback).Run(channel_token_);
}

void GpuChannelMessageFilter::GetSharedMemoryForFlushId(
    GetSharedMemoryForFlushIdCallback callback) {
  CHECK(shared_memory_controller_);
  std::move(callback).Run(shared_memory_controller_->GetSharedMemoryRegion());
}

void GpuChannelMessageFilter::Flush(FlushCallback callback) {
  std::move(callback).Run();
}

void GpuChannelMessageFilter::CreateCommandBuffer(
    mojom::CreateCommandBufferParamsPtr params,
    int32_t routing_id,
    base::UnsafeSharedMemoryRegion shared_state,
    mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
    mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
    CreateCommandBufferCallback callback) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }

  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &gpu::GpuChannel::CreateCommandBuffer, gpu_channel_->AsWeakPtr(),
          std::move(params), routing_id, std::move(shared_state),
          std::move(receiver), std::move(client),
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             std::move(callback))));
}

void GpuChannelMessageFilter::DestroyCommandBuffer(
    int32_t routing_id,
    DestroyCommandBufferCallback callback) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }

  main_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&gpu::GpuChannel::DestroyCommandBuffer,
                     gpu_channel_->AsWeakPtr(), routing_id),
      std::move(callback));
}

void GpuChannelMessageFilter::ScheduleImageDecode(
    mojom::ScheduleImageDecodeParamsPtr params,
    uint64_t decode_release_count) {
  image_decode_accelerator_stub_->ScheduleImageDecode(std::move(params),
                                                      decode_release_count);
}

#if BUILDFLAG(IS_ANDROID)
void GpuChannelMessageFilter::CreateStreamTexture(
    int32_t stream_id,
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver,
    CreateStreamTextureCallback callback) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }
  main_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TryCreateStreamTexture, gpu_channel_->AsWeakPtr(),
                     stream_id, std::move(receiver)),
      std::move(callback));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
void GpuChannelMessageFilter::CreateDCOMPTexture(
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver,
    CreateDCOMPTextureCallback callback) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }
  main_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TryCreateDCOMPTexture, gpu_channel_->AsWeakPtr(),
                     route_id, std::move(receiver)),
      std::move(callback));
}

void GpuChannelMessageFilter::RegisterOverlayStateObserver(
    mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
        promotion_hint_observer,
    const gpu::Mailbox& mailbox,
    RegisterOverlayStateObserverCallback callback) {
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }
  main_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&TryRegisterOverlayStateObserver,
                     gpu_channel_->AsWeakPtr(),
                     std::move(promotion_hint_observer), mailbox),
      std::move(callback));
}

void GpuChannelMessageFilter::CopyToGpuMemoryBufferAsync(
    const gpu::Mailbox& mailbox,
    const std::vector<gpu::SyncToken>& sync_token_dependencies,
    uint64_t release_count,
    CopyToGpuMemoryBufferAsyncCallback callback) {
  TRACE_EVENT0("gpu", "GpuChannelMessageFilter::CopyToGpuMemoryBufferAsync");
  base::AutoLock auto_lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    std::move(callback).Run(false);
    receiver_.reset();
    return;
  }
  int32_t routing_id =
      static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface);
  auto it = route_sequences_.find(routing_id);
  if (it == route_sequences_.end()) {
    LOG(ERROR) << "Could not find SharedImageInterface route id!";
    std::move(callback).Run(false);
    return;
  }
  SyncToken release;
  if (release_count != 0) {
    release = SyncToken(CommandBufferNamespace::GPU_IO,
                        CommandBufferIdFromChannelAndRoute(
                            gpu_channel_->client_id(), routing_id),
                        release_count);
  }

  auto run_on_main = base::BindOnce(
      [](base::WeakPtr<gpu::GpuChannel> channel, const gpu::Mailbox& mailbox,
         CopyToGpuMemoryBufferAsyncCallback callback) {
        if (!channel) {
          std::move(callback).Run(false);
        }
        channel->shared_image_stub()->CopyToGpuMemoryBufferAsync(
            mailbox, std::move(callback));
      },
      gpu_channel_->AsWeakPtr(), mailbox,
      base::BindPostTask(base::SequencedTaskRunner::GetCurrentDefault(),
                         std::move(callback)));
  scheduler_->ScheduleTask(Scheduler::Task(it->second, std::move(run_on_main),
                                           sync_token_dependencies, release));
}

void GpuChannelMessageFilter::CopyNativeGmbToSharedMemorySync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory,
    CopyNativeGmbToSharedMemorySyncCallback callback) {
  std::move(callback).Run(
      gpu_memory_buffer_factory_->FillSharedMemoryRegionWithBufferContents(
          std::move(buffer_handle), std::move(shared_memory)));
}

void GpuChannelMessageFilter::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory,
    CopyNativeGmbToSharedMemoryAsyncCallback callback) {
  std::move(callback).Run(
      gpu_memory_buffer_factory_->FillSharedMemoryRegionWithBufferContents(
          std::move(buffer_handle), std::move(shared_memory)));
}
#endif  // BUILDFLAG(IS_WIN)

void GpuChannelMessageFilter::WaitForTokenInRange(
    int32_t routing_id,
    int32_t start,
    int32_t end,
    WaitForTokenInRangeCallback callback) {
  base::AutoLock lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &gpu::GpuChannel::WaitForTokenInRange, gpu_channel_->AsWeakPtr(),
          routing_id, start, end,
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             std::move(callback))));
}

void GpuChannelMessageFilter::WaitForGetOffsetInRange(
    int32_t routing_id,
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end,
    WaitForGetOffsetInRangeCallback callback) {
  base::AutoLock lock(gpu_channel_lock_);
  if (!gpu_channel_) {
    receiver_.reset();
    return;
  }
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &gpu::GpuChannel::WaitForGetOffsetInRange, gpu_channel_->AsWeakPtr(),
          routing_id, set_get_buffer_count, start, end,
          base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                             std::move(callback))));
}

GpuChannel::GpuChannel(
    GpuChannelManager* gpu_channel_manager,
    const base::UnguessableToken& channel_token,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
    const gfx::GpuExtraInfo& gpu_extra_info,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory)
    : gpu_channel_manager_(gpu_channel_manager),
      scheduler_(scheduler),
      sync_point_manager_(sync_point_manager),
      client_id_(client_id),
      client_tracing_id_(client_tracing_id),
      task_runner_(task_runner),
      io_task_runner_(io_task_runner),
      share_group_(share_group),
      is_gpu_host_(is_gpu_host),
      filter_(base::MakeRefCounted<GpuChannelMessageFilter>(
          this,
          channel_token,
          scheduler,
          image_decode_accelerator_worker,
          gpu_extra_info,
          gpu_memory_buffer_factory,
          std::move(task_runner))) {
  DCHECK(gpu_channel_manager_);
  DCHECK(client_id_);
}

GpuChannel::~GpuChannel() {
  // Clear stubs first because of dependencies.
  stubs_.clear();

#if BUILDFLAG(IS_ANDROID)
  // Release any references to this channel held by StreamTexture.
  for (auto& stream_texture : stream_textures_) {
    stream_texture.second->ReleaseChannel();
  }
  stream_textures_.clear();
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
  // Release any references to this channel held by DCOMPTexture.
  for (auto& dcomp_texture : dcomp_textures_) {
    dcomp_texture.second->ReleaseChannel();
  }
  dcomp_textures_.clear();
#endif  // BUILDFLAG(IS_WIN)

  // Destroy filter first to stop posting tasks to scheduler.
  filter_->Destroy();

  for (const auto& kv : stream_sequences_)
    scheduler_->DestroySequence(kv.second);
}

std::unique_ptr<GpuChannel> GpuChannel::Create(
    GpuChannelManager* gpu_channel_manager,
    const base::UnguessableToken& channel_token,
    Scheduler* scheduler,
    SyncPointManager* sync_point_manager,
    scoped_refptr<gl::GLShareGroup> share_group,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    int32_t client_id,
    uint64_t client_tracing_id,
    bool is_gpu_host,
    ImageDecodeAcceleratorWorker* image_decode_accelerator_worker,
    const gfx::GpuExtraInfo& gpu_extra_info,
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory) {
  auto gpu_channel = base::WrapUnique(new GpuChannel(
      gpu_channel_manager, channel_token, scheduler, sync_point_manager,
      std::move(share_group), std::move(task_runner), std::move(io_task_runner),
      client_id, client_tracing_id, is_gpu_host,
      image_decode_accelerator_worker, gpu_extra_info,
      gpu_memory_buffer_factory));

  if (!gpu_channel->CreateSharedImageStub(gpu_extra_info)) {
    LOG(ERROR) << "GpuChannel: Failed to create SharedImageStub";
    return nullptr;
  }
  return gpu_channel;
}

void GpuChannel::Init(IPC::ChannelHandle channel_handle,
                      base::WaitableEvent* shutdown_event) {
  sync_channel_ = IPC::SyncChannel::Create(this, io_task_runner_.get(),
                                           task_runner_.get(), shutdown_event);
  sync_channel_->AddAssociatedInterfaceForIOThread(
      base::BindRepeating(&GpuChannelMessageFilter::BindGpuChannel, filter_));
  sync_channel_->Init(channel_handle, IPC::Channel::MODE_SERVER,
                      /*create_pipe_now=*/false);
  channel_ = sync_channel_.get();
}

base::WeakPtr<GpuChannel> GpuChannel::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool GpuChannel::OnMessageReceived(const IPC::Message& msg) {
  // All messages should be pushed to channel_messages_ and handled separately.
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GpuChannel::OnChannelError() {
  gpu_channel_manager_->RemoveChannel(client_id_);
}

void GpuChannel::GetIsolationKey(
    const blink::WebGPUExecutionContextToken& token,
    GetIsolationKeyCallback cb) {
  gpu_channel_manager_->delegate()->GetIsolationKey(client_id_, token,
                                                    std::move(cb));
}

void GpuChannel::OnCommandBufferScheduled(CommandBufferStub* stub) {
  scheduler_->EnableSequence(stub->sequence_id());
}

void GpuChannel::OnCommandBufferDescheduled(CommandBufferStub* stub) {
  scheduler_->DisableSequence(stub->sequence_id());
}

CommandBufferStub* GpuChannel::LookupCommandBuffer(int32_t route_id) {
  auto it = stubs_.find(route_id);
  if (it == stubs_.end())
    return nullptr;

  return it->second.get();
}

bool GpuChannel::HasActiveStatefulContext() const {
  for (auto& kv : stubs_) {
    ContextType context_type = kv.second->context_type();
    if (context_type == CONTEXT_TYPE_WEBGL1 ||
        context_type == CONTEXT_TYPE_WEBGL2 ||
        context_type == CONTEXT_TYPE_WEBGPU) {
      return true;
    }
  }
  return false;
}

void GpuChannel::MarkAllContextsLost() {
  for (auto& kv : stubs_)
    kv.second->MarkContextLost();
}

bool GpuChannel::AddRoute(int32_t route_id, SequenceId sequence_id) {
  if (scheduler_)
    filter_->AddRoute(route_id, sequence_id);
  return true;
}

void GpuChannel::RemoveRoute(int32_t route_id) {
  if (scheduler_)
    filter_->RemoveRoute(route_id);
}

void GpuChannel::ExecuteDeferredRequest(
    mojom::DeferredRequestParamsPtr params,
    FenceSyncReleaseDelegate* release_delegate) {
  TRACE_EVENT0("gpu", "GpuChannel::ExecuteDeferredRequest");
  switch (params->which()) {
#if BUILDFLAG(IS_ANDROID)
    case mojom::DeferredRequestParams::Tag::kDestroyStreamTexture:
      DestroyStreamTexture(params->get_destroy_stream_texture());
      break;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
    case mojom::DeferredRequestParams::Tag::kDestroyDcompTexture:
      DestroyDCOMPTexture(params->get_destroy_dcomp_texture());
      break;
#endif  // BUILDFLAG(IS_WIN)

    case mojom::DeferredRequestParams::Tag::kCommandBufferRequest: {
      mojom::DeferredCommandBufferRequest& request =
          *params->get_command_buffer_request();
      CommandBufferStub* stub = LookupCommandBuffer(request.routing_id);
      if (!stub || !stub->IsScheduled()) {
        DVLOG(1) << "Invalid routing ID in deferred request";
        return;
      }

      stub->ExecuteDeferredRequest(*request.params, release_delegate);

      // If we get descheduled or yield while processing a message.
      if (stub->HasUnprocessedCommands() || !stub->IsScheduled()) {
        DCHECK_EQ(mojom::DeferredCommandBufferRequestParams::Tag::kAsyncFlush,
                  request.params->which());
        scheduler_->ContinueTask(
            stub->sequence_id(),
            base::BindOnce(&GpuChannel::ExecuteDeferredRequest, AsWeakPtr(),
                           std::move(params)));
      }
      break;
    }

    case mojom::DeferredRequestParams::Tag::kSharedImageRequest:
      shared_image_stub_->ExecuteDeferredRequest(
          std::move(params->get_shared_image_request()));
      break;
  }
}

void GpuChannel::GetGpuMemoryBufferHandleInfo(
    const gpu::Mailbox& mailbox,
    mojom::GpuChannel::GetGpuMemoryBufferHandleInfoCallback callback) {
  gfx::GpuMemoryBufferHandle handle;
  viz::SharedImageFormat format;
  gfx::Size size;
  gfx::BufferUsage buffer_usage;
  if (shared_image_stub_->GetGpuMemoryBufferHandleInfo(mailbox, handle, format,
                                                       size, buffer_usage)) {
    std::move(callback).Run(std::move(handle), format, size, buffer_usage);
  } else {
    std::move(callback).Run(gfx::GpuMemoryBufferHandle(),
                            viz::SharedImageFormat(), gfx::Size(),
                            gfx::BufferUsage::GPU_READ);
  }
}

void GpuChannel::PerformImmediateCleanup() {
  gpu_channel_manager()->PerformImmediateCleanup();
}

void GpuChannel::WaitForTokenInRange(
    int32_t routing_id,
    int32_t start,
    int32_t end,
    mojom::GpuChannel::WaitForTokenInRangeCallback callback) {
  CommandBufferStub* stub = LookupCommandBuffer(routing_id);
  if (!stub) {
    std::move(callback).Run(CommandBuffer::State());
    return;
  }

  stub->WaitForTokenInRange(start, end, std::move(callback));
}

void GpuChannel::WaitForGetOffsetInRange(
    int32_t routing_id,
    uint32_t set_get_buffer_count,
    int32_t start,
    int32_t end,
    mojom::GpuChannel::WaitForGetOffsetInRangeCallback callback) {
  CommandBufferStub* stub = LookupCommandBuffer(routing_id);
  if (!stub) {
    std::move(callback).Run(CommandBuffer::State());
    return;
  }

  stub->WaitForGetOffsetInRange(set_get_buffer_count, start, end,
                                std::move(callback));
}

mojom::GpuChannel& GpuChannel::GetGpuChannelForTesting() {
  return *filter_;
}

bool GpuChannel::CreateSharedImageStub(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  // SharedImageInterfaceProxy/Stub is a singleton per channel, using a reserved
  // route.
  const int32_t shared_image_route_id =
      static_cast<int32_t>(GpuChannelReservedRoutes::kSharedImageInterface);
  shared_image_stub_ = SharedImageStub::Create(this, shared_image_route_id);
  if (!shared_image_stub_) {
    return false;
  }
  shared_image_stub_->SetGpuExtraInfo(gpu_extra_info);
  filter_->AddRoute(shared_image_route_id, shared_image_stub_->sequence());
  return true;
}

#if BUILDFLAG(IS_ANDROID)
const CommandBufferStub* GpuChannel::GetOneStub() const {
  for (const auto& kv : stubs_) {
    const CommandBufferStub* stub = kv.second.get();
    if (stub->decoder_context() && !stub->decoder_context()->WasContextLost())
      return stub;
  }
  return nullptr;
}

void GpuChannel::DestroyStreamTexture(int32_t stream_id) {
  auto found = stream_textures_.find(stream_id);
  if (found == stream_textures_.end()) {
    LOG(ERROR) << "Trying to destroy a non-existent stream texture.";
    return;
  }
  found->second->ReleaseChannel();
  stream_textures_.erase(stream_id);
}
#endif

#if BUILDFLAG(IS_WIN)
void GpuChannel::DestroyDCOMPTexture(int32_t route_id) {
  auto found = dcomp_textures_.find(route_id);
  if (found == dcomp_textures_.end()) {
    LOG(ERROR) << "Trying to destroy a non-existent dcomp texture.";
    return;
  }
  found->second->ReleaseChannel();
  dcomp_textures_.erase(route_id);
}
#endif  // BUILDFLAG(IS_WIN)

// Helper to ensure CreateCommandBuffer below always invokes its response
// callback.
class ScopedCreateCommandBufferResponder {
 public:
  explicit ScopedCreateCommandBufferResponder(
      mojom::GpuChannel::CreateCommandBufferCallback callback)
      : callback_(std::move(callback)) {}
  ~ScopedCreateCommandBufferResponder() {
    std::move(callback_).Run(result_, capabilities_, gl_capabilities_);
  }

  void set_result(ContextResult result) { result_ = result; }
  void set_capabilities(const Capabilities& capabilities) {
    capabilities_ = capabilities;
  }
  void set_gl_capabilities(const GLCapabilities& gl_capabilities) {
    gl_capabilities_ = gl_capabilities;
  }

 private:
  mojom::GpuChannel::CreateCommandBufferCallback callback_;
  ContextResult result_ = ContextResult::kFatalFailure;
  Capabilities capabilities_;
  GLCapabilities gl_capabilities_;
};

void GpuChannel::CreateCommandBuffer(
    mojom::CreateCommandBufferParamsPtr init_params,
    int32_t route_id,
    base::UnsafeSharedMemoryRegion shared_state_shm,
    mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
    mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
    mojom::GpuChannel::CreateCommandBufferCallback callback) {
  ScopedCreateCommandBufferResponder responder(std::move(callback));
  TRACE_EVENT1("gpu", "GpuChannel::CreateCommandBuffer", "route_id", route_id);

#if BUILDFLAG(IS_ANDROID)
  if (init_params->surface_handle != kNullSurfaceHandle && !is_gpu_host_) {
    LOG(ERROR)
        << "ContextResult::kFatalFailure: "
           "attempt to create a view context on a non-privileged channel";
    return;
  }
#endif

  if (gpu_channel_manager_->delegate()->IsExiting()) {
    LOG(ERROR) << "ContextResult::kTransientFailure: trying to create command "
                  "buffer during process shutdown.";
    responder.set_result(ContextResult::kTransientFailure);
    return;
  }

  int32_t stream_id = init_params->stream_id;
  int32_t share_group_id = init_params->share_group_id;
  CommandBufferStub* share_group = LookupCommandBuffer(share_group_id);

  if (!share_group && share_group_id != MSG_ROUTING_NONE) {
    LOG(ERROR) << "ContextResult::kFatalFailure: invalid share group id";
    return;
  }

  if (share_group && stream_id != share_group->stream_id()) {
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "stream id does not match share group stream id";
    return;
  }

  if (share_group && !share_group->decoder_context()) {
    // This should catch test errors where we did not Initialize the
    // share_group's CommandBuffer.
    LOG(ERROR) << "ContextResult::kFatalFailure: "
                  "shared context was not initialized";
    return;
  }

  if (share_group && share_group->decoder_context()->WasContextLost()) {
    // The caller should retry to get a context.
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "shared context was already lost";
    responder.set_result(ContextResult::kTransientFailure);
    return;
  }

  CommandBufferId command_buffer_id =
      CommandBufferIdFromChannelAndRoute(client_id_, route_id);

  SequenceId sequence_id = stream_sequences_[stream_id];
  if (sequence_id.is_null()) {
    sequence_id =
        scheduler_->CreateSequence(init_params->stream_priority, task_runner_);
    stream_sequences_[stream_id] = sequence_id;
  }

  std::unique_ptr<CommandBufferStub> stub;
  if (init_params->attribs.context_type == CONTEXT_TYPE_WEBGPU) {
    if (!gpu_channel_manager_->gpu_preferences().enable_webgpu) {
      DLOG(ERROR) << "ContextResult::kFatalFailure: WebGPU not enabled";
      return;
    }

    stub = std::make_unique<WebGPUCommandBufferStub>(
        this, *init_params, command_buffer_id, sequence_id, stream_id,
        route_id);
  } else if (init_params->attribs.enable_raster_interface &&
             !init_params->attribs.enable_gles2_interface &&
             !init_params->attribs.enable_grcontext) {
    stub = std::make_unique<RasterCommandBufferStub>(
        this, *init_params, command_buffer_id, sequence_id, stream_id,
        route_id);
  } else {
    stub = std::make_unique<GLES2CommandBufferStub>(
        this, *init_params, command_buffer_id, sequence_id, stream_id,
        route_id);
  }

  stub->BindEndpoints(std::move(receiver), std::move(client), io_task_runner_);

  auto stub_result =
      stub->Initialize(share_group, *init_params, std::move(shared_state_shm));
  if (stub_result != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "GpuChannel::CreateCommandBuffer(): failed to initialize "
                   "CommandBufferStub";
    responder.set_result(stub_result);
    return;
  }

  if (!AddRoute(route_id, sequence_id)) {
    LOG(ERROR) << "ContextResult::kFatalFailure: failed to add route";
    return;
  }

  responder.set_result(ContextResult::kSuccess);
  responder.set_capabilities(stub->decoder_context()->GetCapabilities());
  responder.set_gl_capabilities(stub->decoder_context()->GetGLCapabilities());
  stubs_[route_id] = std::move(stub);
}

void GpuChannel::DestroyCommandBuffer(int32_t route_id) {
  TRACE_EVENT1("gpu", "GpuChannel::OnDestroyCommandBuffer", "route_id",
               route_id);

  std::unique_ptr<CommandBufferStub> stub;
  auto it = stubs_.find(route_id);
  if (it != stubs_.end()) {
    stub = std::move(it->second);
    stubs_.erase(it);
  }
  // In case the renderer is currently blocked waiting for a sync reply from the
  // stub, we need to make sure to reschedule the correct stream here.
  if (stub && !stub->IsScheduled()) {
    // This stub won't get a chance to be scheduled so do that now.
    OnCommandBufferScheduled(stub.get());
  }

  RemoveRoute(route_id);
}

#if BUILDFLAG(IS_ANDROID)
bool GpuChannel::CreateStreamTexture(
    int32_t stream_id,
    mojo::PendingAssociatedReceiver<mojom::StreamTexture> receiver) {
  auto found = stream_textures_.find(stream_id);
  if (found != stream_textures_.end()) {
    LOG(ERROR)
        << "Trying to create a StreamTexture with an existing stream_id.";
    return false;
  }
  scoped_refptr<StreamTexture> stream_texture =
      StreamTexture::Create(this, stream_id, std::move(receiver));
  if (!stream_texture) {
    return false;
  }
  stream_textures_.emplace(stream_id, std::move(stream_texture));
  return true;
}
#endif

#if BUILDFLAG(IS_WIN)
bool GpuChannel::CreateDCOMPTexture(
    int32_t route_id,
    mojo::PendingAssociatedReceiver<mojom::DCOMPTexture> receiver) {
  auto found = dcomp_textures_.find(route_id);
  if (found != dcomp_textures_.end()) {
    LOG(ERROR) << "Trying to create a DCOMPTexture with an existing route_id.";
    return false;
  }
  scoped_refptr<DCOMPTexture> dcomp_texture =
      DCOMPTexture::Create(this, route_id, std::move(receiver));
  if (!dcomp_texture) {
    return false;
  }
  dcomp_textures_.emplace(route_id, std::move(dcomp_texture));
  return true;
}

bool GpuChannel::RegisterOverlayStateObserver(
    mojo::PendingRemote<gpu::mojom::OverlayStateObserver>
        promotion_hint_observer,
    const gpu::Mailbox& mailbox) {
  viz::OverlayStateService* overlay_state_service =
      viz::OverlayStateService::GetInstance();
  if (!overlay_state_service)
    return false;
  overlay_state_service->RegisterObserver(std::move(promotion_hint_observer),
                                          std::move(mailbox));
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
void GpuChannel::RegisterSysmemBufferCollection(
    mojo::PlatformHandle service_handle,
    mojo::PlatformHandle sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  shared_image_stub_->RegisterSysmemBufferCollection(
      zx::eventpair(service_handle.TakeHandle()),
      zx::channel(sysmem_token.TakeHandle()), format, usage,
      register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

std::optional<gpu::GpuDiskCacheHandle> GpuChannel::GetCacheHandleForType(
    gpu::GpuDiskCacheType type) {
  auto it = caches_.find(type);
  if (it == caches_.end()) {
    return {};
  }
  return it->second;
}

void GpuChannel::RegisterCacheHandle(const gpu::GpuDiskCacheHandle& handle) {
  gpu::GpuDiskCacheType type = gpu::GetHandleType(handle);

  // We should never be registering multiple different caches of the same type.
  const auto it = caches_.find(type);
  if (it != caches_.end() && it->second != handle) {
    LOG(ERROR) << "GpuChannel cannot register multiple different caches of the "
                  "same type.";
    return;
  }

  caches_[gpu::GetHandleType(handle)] = handle;
}

void GpuChannel::CacheBlob(gpu::GpuDiskCacheType type,
                           const std::string& key,
                           const std::string& shader) {
  auto handle = GetCacheHandleForType(type);
  if (!handle) {
    return;
  }
  gpu_channel_manager_->delegate()->StoreBlobToDisk(*handle, key, shader);
}

uint64_t GpuChannel::GetMemoryUsage() const {
  // Collect the unique memory trackers in use by the |stubs_|.
  base::flat_set<MemoryTracker*> unique_memory_trackers;
  unique_memory_trackers.reserve(stubs_.size());
  uint64_t size = 0;
  for (const auto& kv : stubs_) {
    size += kv.second->GetMemoryTracker()->GetSize();
    MemoryTracker* tracker = kv.second->GetContextGroupMemoryTracker();
    if (!tracker || !unique_memory_trackers.insert(tracker).second) {
      // We already counted that tracker.
      continue;
    }
    size += tracker->GetSize();
  }
  size += shared_image_stub_->GetSize();

  return size;
}

}  // namespace gpu
