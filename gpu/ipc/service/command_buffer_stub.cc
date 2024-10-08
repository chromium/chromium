// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/command_buffer_stub.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_task_runner.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace gpu {

struct WaitForCommandState {
  using Callback = CommandBufferStub::WaitForStateCallback;

  WaitForCommandState(int32_t start, int32_t end, Callback callback)
      : start(start), end(end), callback(std::move(callback)) {}

  int32_t start;
  int32_t end;
  Callback callback;
};

namespace {

// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64_t kHandleMoreWorkPeriodMs = 2;
const int64_t kHandleMoreWorkPeriodBusyMs = 1;

// Prevents idle work from being starved.
const int64_t kMaxTimeSinceIdleMs = 10;

class DevToolsChannelData : public base::trace_event::ConvertableToTraceFormat {
 public:
  static std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
  CreateForChannel(GpuChannel* channel);

  DevToolsChannelData(const DevToolsChannelData&) = delete;
  DevToolsChannelData& operator=(const DevToolsChannelData&) = delete;

  ~DevToolsChannelData() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(value_, &tmp);
    *out += tmp;
  }

 private:
  explicit DevToolsChannelData(base::Value value) : value_(std::move(value)) {}
  base::Value value_;
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DevToolsChannelData::CreateForChannel(GpuChannel* channel) {
  base::Value::Dict res;
  res.Set("renderer_pid", static_cast<int>(channel->client_pid()));
  res.Set("used_bytes", static_cast<double>(channel->GetMemoryUsage()));
  return base::WrapUnique(new DevToolsChannelData(base::Value(std::move(res))));
}

}  // namespace

CommandBufferStub::CommandBufferStub(
    GpuChannel* channel,
    const mojom::CreateCommandBufferParams& init_params,
    CommandBufferId command_buffer_id,
    SequenceId sequence_id,
    int32_t stream_id,
    int32_t route_id)
    : channel_(channel),
      context_type_(init_params.attribs.context_type),
      active_url_(init_params.active_url),
      initialized_(false),
#if BUILDFLAG(IS_ANDROID)
      offscreen_(init_params.surface_handle == kNullSurfaceHandle),
#endif
      use_virtualized_gl_context_(false),
      command_buffer_id_(command_buffer_id),
      sequence_id_(sequence_id),
      scheduler_task_runner_(
          base::MakeRefCounted<SchedulerTaskRunner>(*channel_->scheduler(),
                                                    sequence_id_)),
      stream_id_(stream_id),
      route_id_(route_id),
      last_flush_id_(0),
      previous_processed_num_(0),
      wait_set_get_buffer_count_(0) {
  process_delayed_work_timer_.SetTaskRunner(channel_->task_runner());
}

CommandBufferStub::~CommandBufferStub() {
  Destroy();
}

void CommandBufferStub::ExecuteDeferredRequest(
    mojom::DeferredCommandBufferRequestParams& params,
    FenceSyncReleaseDelegate* release_delegate) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));

  // Reentrant call is not supported.
  CHECK(!release_delegate_);
  base::AutoReset<raw_ptr<FenceSyncReleaseDelegate>> auto_reset(
      &release_delegate_, release_delegate);

  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // RetireSyncPoint or WaitSyncPoint).
  ScopedContextOperation operation(*this);
  if (!operation.is_context_current())
    return;

  switch (params.which()) {
    case mojom::DeferredCommandBufferRequestParams::Tag::kAsyncFlush: {
      auto& flush = *params.get_async_flush();
      OnAsyncFlush(flush.put_offset, flush.flush_id, flush.sync_token_fences);
      break;
    }

    case mojom::DeferredCommandBufferRequestParams::Tag::kDestroyTransferBuffer:
      OnDestroyTransferBuffer(params.get_destroy_transfer_buffer());
      break;

    case mojom::DeferredCommandBufferRequestParams::Tag::
        kSetDefaultFramebufferSharedImage: {
      OnSetDefaultFramebufferSharedImage(
          params.get_set_default_framebuffer_shared_image()->mailbox,
          params.get_set_default_framebuffer_shared_image()->samples_count,
          params.get_set_default_framebuffer_shared_image()->preserve,
          params.get_set_default_framebuffer_shared_image()->needs_depth,
          params.get_set_default_framebuffer_shared_image()->needs_stencil);
      break;
    }
  }
}

bool CommandBufferStub::IsScheduled() {
  return (!command_buffer_.get() || command_buffer_->scheduled());
}

void CommandBufferStub::PollWork() {
  PerformWork();
}

void CommandBufferStub::PerformWork() {
  TRACE_EVENT0("gpu", "CommandBufferStub::PerformWork");
  UpdateActiveUrl();
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  if (decoder_context_.get() && !MakeCurrent())
    return;
  std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  CreateCacheUse(cache_use);

  if (decoder_context_) {
    uint32_t current_unprocessed_num =
        channel()->sync_point_manager()->GetUnprocessedOrderNum();
    // We're idle when no messages were processed or scheduled.
    bool is_idle = (previous_processed_num_ == current_unprocessed_num);
    if (!is_idle && !last_idle_time_.is_null()) {
      base::TimeDelta time_since_idle =
          base::TimeTicks::Now() - last_idle_time_;
      base::TimeDelta max_time_since_idle =
          base::Milliseconds(kMaxTimeSinceIdleMs);

      // Force idle when it's been too long since last time we were idle.
      if (time_since_idle > max_time_since_idle)
        is_idle = true;
    }

    if (is_idle) {
      last_idle_time_ = base::TimeTicks::Now();
      decoder_context_->PerformIdleWork();
    }

    decoder_context_->ProcessPendingQueries(false);
    decoder_context_->PerformPollingWork();
  }

  ScheduleDelayedWork(base::Milliseconds(kHandleMoreWorkPeriodBusyMs));
}

bool CommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    gpu::CommandBuffer::State state = command_buffer_->GetState();
    return command_buffer_->put_offset() != state.get_offset &&
           !error::IsError(state.error);
  }
  return false;
}

void CommandBufferStub::ScheduleDelayedWork(base::TimeDelta delay) {
  bool has_more_work =
      decoder_context_.get() && (decoder_context_->HasPendingQueries() ||
                                 decoder_context_->HasMoreIdleWork() ||
                                 decoder_context_->HasPollingWork());
  if (!has_more_work) {
    last_idle_time_ = base::TimeTicks();
    return;
  }

  base::TimeTicks current_time = base::TimeTicks::Now();
  // Just update the time if already scheduled.
  if (process_delayed_work_timer_.IsRunning()) {
    process_delayed_work_timer_.Stop();
    process_delayed_work_timer_.Start(
        FROM_HERE, current_time + delay,
        base::BindOnce(&CommandBufferStub::PollWork, AsWeakPtr()),
        base::subtle::DelayPolicy::kPrecise);
    return;
  }

  // Idle when no messages are processed between now and when
  // PollWork is called.
  previous_processed_num_ =
      channel()->sync_point_manager()->GetProcessedOrderNum();
  if (last_idle_time_.is_null())
    last_idle_time_ = current_time;

  // IsScheduled() returns true after passing all unschedule fences
  // and this is when we can start performing idle work. Idle work
  // is done synchronously so we can set delay to 0 and instead poll
  // for more work at the rate idle work is performed. This also ensures
  // that idle work is done as efficiently as possible without any
  // unnecessary delays.
  if (command_buffer_->scheduled() && decoder_context_->HasMoreIdleWork()) {
    delay = base::TimeDelta();
  }

  process_delayed_work_timer_.Start(
      FROM_HERE, current_time + delay,
      base::BindOnce(&CommandBufferStub::PollWork, AsWeakPtr()),
      base::subtle::DelayPolicy::kPrecise);
}

bool CommandBufferStub::MakeCurrent() {
  if (decoder_context_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
  return false;
}

void CommandBufferStub::CreateCacheUse(
    std::optional<gles2::ProgramCache::ScopedCacheUse>& cache_use) {
  cache_use.emplace(
      channel_->gpu_channel_manager()->program_cache(),
      base::BindRepeating(&DecoderClient::CacheBlob, base::Unretained(this),
                          gpu::GpuDiskCacheType::kGlShaders));
}

void CommandBufferStub::Destroy() {
  UpdateActiveUrl();
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  if (wait_for_token_) {
    std::move(wait_for_token_->callback).Run(gpu::CommandBuffer::State());
    wait_for_token_.reset();
  }
  if (wait_for_get_offset_) {
    std::move(wait_for_get_offset_->callback).Run(gpu::CommandBuffer::State());
    wait_for_get_offset_.reset();
  }

  if (initialized_) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    // If we are currently shutting down the GPU process to help with recovery
    // (exit_on_context_lost workaround), then don't tell the browser about
    // offscreen context destruction here since it's not client-invoked, and
    // might bypass the 3D API blocking logic.
    if (offscreen() && !active_url_.is_empty() &&
        !gpu_channel_manager->delegate()->IsExiting()) {
      gpu_channel_manager->delegate()->DidDestroyOffscreenContext(
          active_url_.url());
    }
  }

  scoped_sync_point_client_state_.Reset();

  // Try to make the context current regardless of whether it was lost, so we
  // don't leak resources. Don't use GetGLContext()->MakeCurrent() since that
  // will make |have_context| false when RasterDecoder doesn't use GL.
  const bool have_context = decoder_context_ && decoder_context_->MakeCurrent();

  std::optional<gles2::ProgramCache::ScopedCacheUse> cache_use;
  if (have_context)
    CreateCacheUse(cache_use);

  for (auto& observer : destruction_observers_)
    observer.OnWillDestroyStub(have_context);

  share_group_ = nullptr;

  // Remove this after crbug.com/248395 is sorted out.
  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (decoder_context_) {
    auto* gr_shader_cache = channel_->gpu_channel_manager()->gr_shader_cache();
    std::optional<raster::GrShaderCache::ScopedCacheUse> gr_cache_use;
    if (gr_shader_cache)
      gr_cache_use.emplace(gr_shader_cache, channel_->client_id());

    decoder_context_->Destroy(have_context);
    decoder_context_.reset();
  }

  command_buffer_.reset();

  scheduler_task_runner_->ShutDown();

  // Note: `receiver_` runs tasks on `scheduler_task_runner_`, which is not the
  // current task runner when this method runs. Hence we must use this unsafe
  // reset to elide sequence safety checks. Its safety is guaranteed by the
  // above ShutDown() call which ensures no further tasks will run on the
  // sequence.
  receiver_.ResetFromAnotherSequenceUnsafe();
  client_.reset();
}

void CommandBufferStub::SetGetBuffer(int32_t shm_id) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  UpdateActiveUrl();
  TRACE_EVENT0("gpu", "CommandBufferStub::SetGetBuffer");
  if (command_buffer_) {
    command_buffer_->SetGetBuffer(shm_id);
    CheckCompleteWaits();
  }
}

CommandBufferServiceClient::CommandBatchProcessedResult
CommandBufferStub::OnCommandBatchProcessed() {
  GpuWatchdogThread* watchdog = channel_->gpu_channel_manager()->watchdog();
  if (watchdog)
    watchdog->ReportProgress();
  bool pause = channel_->scheduler()->ShouldYield(sequence_id_);
  return pause ? kPauseExecution : kContinueExecution;
}

void CommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  client_->OnDestroyed(state.context_lost_reason, state.error);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->DidLoseContext(state.context_lost_reason,
                                                  active_url_.url());

  CheckContextLost();
}

void CommandBufferStub::WaitForTokenInRange(int32_t start,
                                            int32_t end,
                                            WaitForStateCallback callback) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  UpdateActiveUrl();
  TRACE_EVENT0("gpu", "CommandBufferStub::WaitForTokenInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_token_)
    LOG(ERROR) << "Got WaitForToken command while currently waiting for token.";
  channel_->scheduler()->SetSequencePriority(sequence_id_,
                                             SchedulingPriority::kHigh);
  wait_for_token_ =
      std::make_unique<WaitForCommandState>(start, end, std::move(callback));
  CheckCompleteWaits();
}

void CommandBufferStub::WaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                                int32_t start,
                                                int32_t end,
                                                WaitForStateCallback callback) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  UpdateActiveUrl();
  TRACE_EVENT0("gpu", "CommandBufferStub::WaitForGetOffsetInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_get_offset_) {
    LOG(ERROR)
        << "Got WaitForGetOffset command while currently waiting for offset.";
  }
  channel_->scheduler()->SetSequencePriority(sequence_id_,
                                             SchedulingPriority::kHigh);
  wait_for_get_offset_ =
      std::make_unique<WaitForCommandState>(start, end, std::move(callback));
  wait_set_get_buffer_count_ = set_get_buffer_count;
  CheckCompleteWaits();
}

void CommandBufferStub::CheckCompleteWaits() {
  bool has_wait = wait_for_token_ || wait_for_get_offset_;
  if (has_wait) {
    gpu::CommandBuffer::State state = command_buffer_->GetState();
    if (wait_for_token_ &&
        (gpu::CommandBuffer::InRange(wait_for_token_->start,
                                     wait_for_token_->end, state.token) ||
         state.error != error::kNoError)) {
      ReportState();
      std::move(wait_for_token_->callback).Run(state);
      wait_for_token_.reset();
    }
    if (wait_for_get_offset_ &&
        (((wait_set_get_buffer_count_ == state.set_get_buffer_count) &&
          gpu::CommandBuffer::InRange(wait_for_get_offset_->start,
                                      wait_for_get_offset_->end,
                                      state.get_offset)) ||
         state.error != error::kNoError)) {
      ReportState();
      std::move(wait_for_get_offset_->callback).Run(state);
      wait_for_get_offset_.reset();
    }
  }
  if (has_wait && !(wait_for_token_ || wait_for_get_offset_)) {
    channel_->scheduler()->SetSequencePriority(
        sequence_id_,
        channel_->scheduler()->GetSequenceDefaultPriority(sequence_id_));
  }
}

void CommandBufferStub::OnAsyncFlush(
    int32_t put_offset,
    uint32_t flush_id,
    const std::vector<SyncToken>& sync_token_fences) {
  TRACE_EVENT1("gpu", "CommandBufferStub::OnAsyncFlush", "put_offset",
               put_offset);
  DCHECK(command_buffer_);
  // We received this message out-of-order. This should not happen but is here
  // to catch regressions. Ignore the message.
  DVLOG_IF(0, flush_id - last_flush_id_ >= 0x8000000U)
      << "Received a Flush message out-of-order";

  last_flush_id_ = flush_id;
  gpu::CommandBuffer::State pre_state = command_buffer_->GetState();
  UpdateActiveUrl();

  {
    auto* gr_shader_cache = channel_->gpu_channel_manager()->gr_shader_cache();
    std::optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache)
      cache_use.emplace(gr_shader_cache, channel_->client_id());
    command_buffer_->Flush(put_offset, decoder_context_.get());
  }

  gpu::CommandBuffer::State post_state = command_buffer_->GetState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();

#if BUILDFLAG(IS_ANDROID)
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->DidAccessGpu();
#endif
}

void CommandBufferStub::RegisterTransferBuffer(
    int32_t id,
    base::UnsafeSharedMemoryRegion transfer_buffer) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  UpdateActiveUrl();

  TRACE_EVENT0("gpu", "CommandBufferStub::OnRegisterTransferBuffer");

  // Map the shared memory into this process.
  base::WritableSharedMemoryMapping mapping = transfer_buffer.Map();
  if (!mapping.IsValid() || (mapping.size() > UINT32_MAX)) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }

  if (command_buffer_) {
    command_buffer_->RegisterTransferBuffer(
        id, MakeBufferFromSharedMemory(std::move(transfer_buffer),
                                       std::move(mapping)));
  }
}

void CommandBufferStub::CreateGpuFenceFromHandle(uint32_t id,
                                                 gfx::GpuFenceHandle handle) {
  DLOG(ERROR) << "CreateGpuFenceFromHandle unsupported.";
}

void CommandBufferStub::GetGpuFenceHandle(uint32_t id,
                                          GetGpuFenceHandleCallback callback) {
  DLOG(ERROR) << "GetGpuFenceHandle unsupported.";
  std::move(callback).Run(gfx::GpuFenceHandle());
}

void CommandBufferStub::OnDestroyTransferBuffer(int32_t id) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferStub::ReportState() {
  command_buffer_->UpdateState();
}

void CommandBufferStub::SignalSyncToken(const SyncToken& sync_token,
                                        uint32_t id) {
  UpdateActiveUrl();

  if (channel_->scheduler()
          ->task_graph()
          ->sync_point_manager()
          ->IsSyncTokenReleased(sync_token)) {
    OnSignalAck(id);
    return;
  }

  auto callback =
      base::BindOnce(&CommandBufferStub::OnSignalAck, this->AsWeakPtr(), id);
  channel_->scheduler()->ScheduleTask(Scheduler::Task(
      sequence_id_, std::move(callback), {sync_token}, SyncToken()));
}

void CommandBufferStub::OnSignalAck(uint32_t id) {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  ReportState();
  client_->OnSignalAck(id, state);
}

void CommandBufferStub::SignalQuery(uint32_t query_id, uint32_t id) {
  UpdateActiveUrl();
  if (decoder_context_) {
    decoder_context_->SetQueryCallback(
        query_id,
        base::BindOnce(&CommandBufferStub::OnSignalAck, this->AsWeakPtr(), id));
  } else {
    // Something went wrong, run callback immediately.
    VLOG(1) << "CommandBufferStub::SignalQuery: No decoder to set query "
               "callback on. Running the callback immediately.";
    OnSignalAck(id);
  }
}

void CommandBufferStub::OnFenceSyncRelease(uint64_t release) {
  SyncToken sync_token(CommandBufferNamespace::GPU_IO, command_buffer_id_,
                       release);
  command_buffer_->SetReleaseCount(release);

  CHECK(release_delegate_);
  release_delegate_->Release(release);
}

void CommandBufferStub::OnDescheduleUntilFinished() {
  DCHECK(command_buffer_->scheduled());
  DCHECK(decoder_context_->HasPollingWork());

  command_buffer_->SetScheduled(false);
  channel_->OnCommandBufferDescheduled(this);
}

void CommandBufferStub::OnRescheduleAfterFinished() {
  DCHECK(!command_buffer_->scheduled());

  command_buffer_->SetScheduled(true);
  channel_->OnCommandBufferScheduled(this);
}

void CommandBufferStub::ScheduleGrContextCleanup() {
  channel_->gpu_channel_manager()->ScheduleGrContextCleanup();
}

void CommandBufferStub::HandleReturnData(base::span<const uint8_t> data) {
  client_->OnReturnData(std::vector<uint8_t>(data.begin(), data.end()));
}

bool CommandBufferStub::ShouldYield() {
  return channel_->scheduler()->ShouldYield(sequence_id_);
}

void CommandBufferStub::OnConsoleMessage(int32_t id,
                                         const std::string& message) {
  client_->OnConsoleMessage(message);
}

void CommandBufferStub::CacheBlob(gpu::GpuDiskCacheType type,
                                  const std::string& key,
                                  const std::string& shader) {
  channel_->CacheBlob(type, key, shader);
}

void CommandBufferStub::AddDestructionObserver(DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void CommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

std::unique_ptr<MemoryTracker> CommandBufferStub::CreateMemoryTracker() const {
  MemoryTrackerFactory current_factory = GetMemoryTrackerFactory();
  if (current_factory)
    return current_factory.Run();

  return std::make_unique<GpuCommandBufferMemoryTracker>(
      command_buffer_id_, channel_->client_tracing_id(),
      channel_->task_runner(),
      channel_->gpu_channel_manager()->peak_memory_monitor());
}

// static
void CommandBufferStub::SetMemoryTrackerFactoryForTesting(
    MemoryTrackerFactory factory) {
  SetOrGetMemoryTrackerFactory(factory);
}

void CommandBufferStub::BindEndpoints(
    mojo::PendingAssociatedReceiver<mojom::CommandBuffer> receiver,
    mojo::PendingAssociatedRemote<mojom::CommandBufferClient> client,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  DCHECK(!receiver_);
  DCHECK(!client_);

  IPC::ScopedAllowOffSequenceChannelAssociatedBindings allow_binding;
  receiver_.Bind(std::move(receiver), scheduler_task_runner_);
  client_.Bind(std::move(client), std::move(io_task_runner));
}

MemoryTracker* CommandBufferStub::GetMemoryTracker() const {
  return memory_tracker_.get();
}

scoped_refptr<Buffer> CommandBufferStub::GetTransferBuffer(int32_t id) {
  return command_buffer_->GetTransferBuffer(id);
}

void CommandBufferStub::RegisterTransferBufferForTest(
    int32_t id,
    scoped_refptr<Buffer> buffer) {
  command_buffer_->RegisterTransferBuffer(id, std::move(buffer));
}

void CommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  gpu::CommandBuffer::State state = command_buffer_->GetState();

  // Check the error reason and robustness extension to get a better idea if the
  // GL context was lost. We might try restarting the GPU process to recover
  // from actual GL context loss but it's unnecessary for other types of parse
  // errors.
  if (state.error == error::kLostContext) {
    bool was_lost_by_robustness =
        decoder_context_ &&
        decoder_context_->WasContextLostByRobustnessExtension();
    channel_->gpu_channel_manager()->OnContextLost(/*context_lost_count=*/-1,
                                                   !was_lost_by_robustness,
                                                   state.context_lost_reason);
  }

  CheckCompleteWaits();
}

void CommandBufferStub::UpdateActiveUrl() {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // BlinkPlatformImpl::createOffscreenGraphicsContext3DProvider. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (!active_url_.is_empty())
    ContextUrl::SetActiveUrl(active_url_);
}

void CommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetState().error == error::kLostContext) {
    return;
  }

  command_buffer_->SetContextLostReason(error::kUnknown);
  if (decoder_context_)
    decoder_context_->MarkContextLost(error::kUnknown);
  command_buffer_->SetParseError(error::kLostContext);
}

// static
CommandBufferStub::MemoryTrackerFactory
CommandBufferStub::GetMemoryTrackerFactory() {
  return SetOrGetMemoryTrackerFactory(base::NullCallback());
}

// static
CommandBufferStub::MemoryTrackerFactory
CommandBufferStub::SetOrGetMemoryTrackerFactory(MemoryTrackerFactory factory) {
  static base::NoDestructor<MemoryTrackerFactory> current_factory{
      base::NullCallback()};
  if (factory)
    *current_factory = factory;
  return *current_factory;
}

CommandBufferStub::ScopedContextOperation::ScopedContextOperation(
    CommandBufferStub& stub)
    : stub_(stub) {
  stub_.UpdateActiveUrl();
  if (stub_.decoder_context_ && stub_.MakeCurrent()) {
    have_context_ = true;
    stub_.CreateCacheUse(cache_use_);
  }
}

CommandBufferStub::ScopedContextOperation::~ScopedContextOperation() {
  stub_.CheckCompleteWaits();
  if (have_context_) {
    if (stub_.decoder_context_) {
      stub_.decoder_context_->ProcessPendingQueries(/*did_finish=*/false);
    }
    stub_.ScheduleDelayedWork(base::Milliseconds(kHandleMoreWorkPeriodMs));
  }
}

}  // namespace gpu
