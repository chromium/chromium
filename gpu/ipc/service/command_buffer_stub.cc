// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/command_buffer_stub.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/hash.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/service/decoder_context.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/gpu_command_buffer_memory_tracker.h"
#include "gpu/command_buffer/service/gpu_fence_manager.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/command_buffer/service/transfer_buffer_manager.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "gpu/ipc/service/image_transport_surface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_workarounds.h"
#include "ui/gl/init/gl_factory.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

#if defined(OS_ANDROID)
#include "gpu/ipc/service/stream_texture_android.h"
#endif

namespace gpu {
struct WaitForCommandState {
  WaitForCommandState(int32_t start, int32_t end, IPC::Message* reply)
      : start(start), end(end), reply(reply) {}

  int32_t start;
  int32_t end;
  std::unique_ptr<IPC::Message> reply;
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
  ~DevToolsChannelData() override = default;

  void AppendAsTraceFormat(std::string* out) const override {
    std::string tmp;
    base::JSONWriter::Write(*value_, &tmp);
    *out += tmp;
  }

 private:
  explicit DevToolsChannelData(base::Value* value) : value_(value) {}
  std::unique_ptr<base::Value> value_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsChannelData);
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat>
DevToolsChannelData::CreateForChannel(GpuChannel* channel) {
  std::unique_ptr<base::DictionaryValue> res(new base::DictionaryValue);
  res->SetInteger("renderer_pid", channel->GetClientPID());
  res->SetDouble("used_bytes", channel->GetMemoryUsage());
  return base::WrapUnique(new DevToolsChannelData(res.release()));
}

}  // namespace

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
//
// static
void CommandBufferStub::FastSetActiveURL(const GURL& url,
                                         size_t url_hash,
                                         GpuChannel* channel) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // BlinkPlatformImpl::createOffscreenGraphicsContext3DProvider. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (url.is_empty())
    return;
  static size_t g_last_url_hash = 0;
  if (url_hash != g_last_url_hash) {
    g_last_url_hash = url_hash;
    DCHECK(channel && channel->gpu_channel_manager() &&
           channel->gpu_channel_manager()->delegate());
    channel->gpu_channel_manager()->delegate()->SetActiveURL(url);
  }
}

CommandBufferStub::CommandBufferStub(
    GpuChannel* channel,
    const GPUCreateCommandBufferConfig& init_params,
    CommandBufferId command_buffer_id,
    SequenceId sequence_id,
    int32_t stream_id,
    int32_t route_id)
    : channel_(channel),
      active_url_(init_params.active_url),
      active_url_hash_(base::Hash(active_url_.possibly_invalid_spec())),
      initialized_(false),
      surface_handle_(init_params.surface_handle),
      use_virtualized_gl_context_(false),
      command_buffer_id_(command_buffer_id),
      sequence_id_(sequence_id),
      stream_id_(stream_id),
      route_id_(route_id),
      last_flush_id_(0),
      waiting_for_sync_point_(false),
      previous_processed_num_(0),
      wait_set_get_buffer_count_(0) {}

CommandBufferStub::~CommandBufferStub() {
  Destroy();
}

bool CommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "GPUTask",
               "data", DevToolsChannelData::CreateForChannel(channel()));
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  bool have_context = false;
  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // RetireSyncPoint or WaitSyncPoint).
  if (decoder_context_.get() &&
      message.type() != GpuCommandBufferMsg_SetGetBuffer::ID &&
      message.type() != GpuCommandBufferMsg_WaitForTokenInRange::ID &&
      message.type() != GpuCommandBufferMsg_WaitForGetOffsetInRange::ID &&
      message.type() != GpuCommandBufferMsg_RegisterTransferBuffer::ID &&
      message.type() != GpuCommandBufferMsg_DestroyTransferBuffer::ID &&
      message.type() != GpuCommandBufferMsg_WaitSyncToken::ID &&
      message.type() != GpuCommandBufferMsg_SignalSyncToken::ID &&
      message.type() != GpuCommandBufferMsg_SignalQuery::ID) {
    if (!MakeCurrent())
      return false;
    have_context = true;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CommandBufferStub, message)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetGetBuffer, OnSetGetBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_TakeFrontBuffer, OnTakeFrontBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ReturnFrontBuffer,
                        OnReturnFrontBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForTokenInRange,
                                    OnWaitForTokenInRange);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_WaitForGetOffsetInRange,
                                    OnWaitForGetOffsetInRange);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_WaitSyncToken, OnWaitSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncToken, OnSignalSyncToken)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalQuery, OnSignalQuery)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateImage, OnCreateImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyImage, OnDestroyImage);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateStreamTexture,
                        OnCreateStreamTexture)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_CreateGpuFenceFromHandle,
                        OnCreateGpuFenceFromHandle)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_GetGpuFenceHandle,
                        OnGetGpuFenceHandle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  CheckCompleteWaits();

  // Ensure that any delayed work that was created will be handled.
  if (have_context) {
    if (decoder_context_)
      decoder_context_->ProcessPendingQueries(false);
    ScheduleDelayedWork(
        base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodMs));
  }

  return handled;
}

bool CommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool CommandBufferStub::IsScheduled() {
  return (!command_buffer_.get() || command_buffer_->scheduled());
}

void CommandBufferStub::PollWork() {
  // Post another delayed task if we have not yet reached the time at which
  // we should process delayed work.
  base::TimeTicks current_time = base::TimeTicks::Now();
  DCHECK(!process_delayed_work_time_.is_null());
  if (process_delayed_work_time_ > current_time) {
    channel_->task_runner()->PostDelayedTask(
        FROM_HERE, base::Bind(&CommandBufferStub::PollWork, AsWeakPtr()),
        process_delayed_work_time_ - current_time);
    return;
  }
  process_delayed_work_time_ = base::TimeTicks();

  PerformWork();
}

void CommandBufferStub::PerformWork() {
  TRACE_EVENT0("gpu", "CommandBufferStub::PerformWork");
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  if (decoder_context_.get() && !MakeCurrent())
    return;

  if (decoder_context_) {
    uint32_t current_unprocessed_num =
        channel()->sync_point_manager()->GetUnprocessedOrderNum();
    // We're idle when no messages were processed or scheduled.
    bool is_idle = (previous_processed_num_ == current_unprocessed_num);
    if (!is_idle && !last_idle_time_.is_null()) {
      base::TimeDelta time_since_idle =
          base::TimeTicks::Now() - last_idle_time_;
      base::TimeDelta max_time_since_idle =
          base::TimeDelta::FromMilliseconds(kMaxTimeSinceIdleMs);

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

  ScheduleDelayedWork(
      base::TimeDelta::FromMilliseconds(kHandleMoreWorkPeriodBusyMs));
}

bool CommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    CommandBuffer::State state = command_buffer_->GetState();
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
  // |process_delayed_work_time_| is set if processing of delayed work is
  // already scheduled. Just update the time if already scheduled.
  if (!process_delayed_work_time_.is_null()) {
    process_delayed_work_time_ = current_time + delay;
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

  process_delayed_work_time_ = current_time + delay;
  channel_->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&CommandBufferStub::PollWork, AsWeakPtr()), delay);
}

bool CommandBufferStub::MakeCurrent() {
  if (decoder_context_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
  return false;
}

void CommandBufferStub::Destroy() {
  FastSetActiveURL(active_url_, active_url_hash_, channel_);
  // TODO(sunnyps): Should this use ScopedCrashKey instead?
  crash_keys::gpu_gl_context_is_virtual.Set(use_virtualized_gl_context_ ? "1"
                                                                        : "0");
  if (wait_for_token_) {
    Send(wait_for_token_->reply.release());
    wait_for_token_.reset();
  }
  if (wait_for_get_offset_) {
    Send(wait_for_get_offset_->reply.release());
    wait_for_get_offset_.reset();
  }

  if (initialized_) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    // If we are currently shutting down the GPU process to help with recovery
    // (exit_on_context_lost workaround), then don't tell the browser about
    // offscreen context destruction here since it's not client-invoked, and
    // might bypass the 3D API blocking logic.
    if ((surface_handle_ == gpu::kNullSurfaceHandle) &&
        !active_url_.is_empty() &&
        !gpu_channel_manager->is_exiting_for_lost_context()) {
      gpu_channel_manager->delegate()->DidDestroyOffscreenContext(active_url_);
    }
  }

  if (sync_point_client_state_) {
    sync_point_client_state_->Destroy();
    sync_point_client_state_ = nullptr;
  }

  bool have_context = false;
  if (decoder_context_ && decoder_context_->GetGLContext()) {
    // Try to make the context current regardless of whether it was lost, so we
    // don't leak resources.
    have_context =
        decoder_context_->GetGLContext()->MakeCurrent(surface_.get());
  }
  for (auto& observer : destruction_observers_)
    observer.OnWillDestroyStub(have_context);

  share_group_ = nullptr;

  // Remove this after crbug.com/248395 is sorted out.
  // Destroy the surface before the context, some surface destructors make GL
  // calls.
  surface_ = nullptr;

  if (decoder_context_) {
    decoder_context_->Destroy(have_context);
    decoder_context_.reset();
  }

  command_buffer_.reset();
}

void CommandBufferStub::OnCreateStreamTexture(uint32_t texture_id,
                                              int32_t stream_id,
                                              bool* succeeded) {
#if defined(OS_ANDROID)
  *succeeded = StreamTexture::Create(this, texture_id, stream_id);
#else
  *succeeded = false;
#endif
}

void CommandBufferStub::OnSetGetBuffer(int32_t shm_id) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnSetGetBuffer");
  if (command_buffer_)
    command_buffer_->SetGetBuffer(shm_id);
}

CommandBufferServiceClient::CommandBatchProcessedResult
CommandBufferStub::OnCommandBatchProcessed() {
  GpuWatchdogThread* watchdog = channel_->gpu_channel_manager()->watchdog();
  if (watchdog)
    watchdog->CheckArmed();
  bool pause = channel_->scheduler()->ShouldYield(sequence_id_);
  return pause ? kPauseExecution : kContinueExecution;
}

void CommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason, state.error);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->delegate()->DidLoseContext(
      (surface_handle_ == kNullSurfaceHandle), state.context_lost_reason,
      active_url_);

  CheckContextLost();
}

void CommandBufferStub::OnWaitForTokenInRange(int32_t start,
                                              int32_t end,
                                              IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnWaitForTokenInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_token_)
    LOG(ERROR) << "Got WaitForToken command while currently waiting for token.";
  channel_->scheduler()->RaisePriorityForClientWait(sequence_id_,
                                                    command_buffer_id_);
  wait_for_token_ =
      std::make_unique<WaitForCommandState>(start, end, reply_message);
  CheckCompleteWaits();
}

void CommandBufferStub::OnWaitForGetOffsetInRange(uint32_t set_get_buffer_count,
                                                  int32_t start,
                                                  int32_t end,
                                                  IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnWaitForGetOffsetInRange");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  if (wait_for_get_offset_) {
    LOG(ERROR)
        << "Got WaitForGetOffset command while currently waiting for offset.";
  }
  channel_->scheduler()->RaisePriorityForClientWait(sequence_id_,
                                                    command_buffer_id_);
  wait_for_get_offset_ =
      std::make_unique<WaitForCommandState>(start, end, reply_message);
  wait_set_get_buffer_count_ = set_get_buffer_count;
  CheckCompleteWaits();
}

void CommandBufferStub::CheckCompleteWaits() {
  bool has_wait = wait_for_token_ || wait_for_get_offset_;
  if (has_wait) {
    CommandBuffer::State state = command_buffer_->GetState();
    if (wait_for_token_ &&
        (CommandBuffer::InRange(wait_for_token_->start, wait_for_token_->end,
                                state.token) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForTokenInRange::WriteReplyParams(
          wait_for_token_->reply.get(), state);
      Send(wait_for_token_->reply.release());
      wait_for_token_.reset();
    }
    if (wait_for_get_offset_ &&
        (((wait_set_get_buffer_count_ == state.set_get_buffer_count) &&
          CommandBuffer::InRange(wait_for_get_offset_->start,
                                 wait_for_get_offset_->end,
                                 state.get_offset)) ||
         state.error != error::kNoError)) {
      ReportState();
      GpuCommandBufferMsg_WaitForGetOffsetInRange::WriteReplyParams(
          wait_for_get_offset_->reply.get(), state);
      Send(wait_for_get_offset_->reply.release());
      wait_for_get_offset_.reset();
    }
  }
  if (has_wait && !(wait_for_token_ || wait_for_get_offset_)) {
    channel_->scheduler()->ResetPriorityForClientWait(sequence_id_,
                                                      command_buffer_id_);
  }
}

void CommandBufferStub::OnAsyncFlush(int32_t put_offset, uint32_t flush_id) {
  TRACE_EVENT1("gpu", "CommandBufferStub::OnAsyncFlush", "put_offset",
               put_offset);
  DCHECK(command_buffer_);
  // We received this message out-of-order. This should not happen but is here
  // to catch regressions. Ignore the message.
  DVLOG_IF(0, flush_id - last_flush_id_ >= 0x8000000U)
      << "Received a Flush message out-of-order";

  last_flush_id_ = flush_id;
  CommandBuffer::State pre_state = command_buffer_->GetState();
  FastSetActiveURL(active_url_, active_url_hash_, channel_);

  {
    auto* gr_shader_cache = channel_->gpu_channel_manager()->gr_shader_cache();
    base::Optional<raster::GrShaderCache::ScopedCacheUse> cache_use;
    if (gr_shader_cache)
      cache_use.emplace(gr_shader_cache, channel_->client_id());
    command_buffer_->Flush(put_offset, decoder_context_.get());
  }

  CommandBuffer::State post_state = command_buffer_->GetState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();

#if defined(OS_ANDROID)
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->DidAccessGpu();
#endif
}

void CommandBufferStub::OnRegisterTransferBuffer(
    int32_t id,
    base::UnsafeSharedMemoryRegion transfer_buffer) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnRegisterTransferBuffer");

  // Map the shared memory into this process.
  base::WritableSharedMemoryMapping mapping = transfer_buffer.Map();
  if (!mapping.IsValid()) {
    DVLOG(0) << "Failed to map shared memory.";
    return;
  }

  if (command_buffer_) {
    command_buffer_->RegisterTransferBuffer(
        id, MakeBufferFromSharedMemory(std::move(transfer_buffer),
                                       std::move(mapping)));
  }
}

void CommandBufferStub::OnDestroyTransferBuffer(int32_t id) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferStub::ReportState() {
  command_buffer_->UpdateState();
}

void CommandBufferStub::OnSignalSyncToken(const SyncToken& sync_token,
                                          uint32_t id) {
  if (!sync_point_client_state_->WaitNonThreadSafe(
          sync_token, channel_->task_runner(),
          base::Bind(&CommandBufferStub::OnSignalAck, this->AsWeakPtr(), id))) {
    OnSignalAck(id);
  }
}

void CommandBufferStub::OnSignalAck(uint32_t id) {
  CommandBuffer::State state = command_buffer_->GetState();
  ReportState();
  Send(new GpuCommandBufferMsg_SignalAck(route_id_, id, state));
}

void CommandBufferStub::OnSignalQuery(uint32_t query_id, uint32_t id) {
  if (decoder_context_) {
    decoder_context_->SetQueryCallback(
        query_id,
        base::BindOnce(&CommandBufferStub::OnSignalAck, this->AsWeakPtr(), id));
  } else {
    // Something went wrong, run callback immediately.
    VLOG(1) << "CommandBufferStub::OnSignalQueryk: No decoder to set query "
               "callback on. Running the callback immediately.";
    OnSignalAck(id);
  }
}

void CommandBufferStub::OnCreateGpuFenceFromHandle(
    uint32_t gpu_fence_id,
    const gfx::GpuFenceHandle& handle) {
  if (!context_group_->feature_info()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  if (decoder_context_->GetGpuFenceManager()->CreateGpuFenceFromHandle(
          gpu_fence_id, handle))
    return;

  // The insertion failed. This shouldn't happen, force context loss to avoid
  // inconsistent state.
  command_buffer_->SetParseError(error::kLostContext);
  CheckContextLost();
}

void CommandBufferStub::OnGetGpuFenceHandle(uint32_t gpu_fence_id) {
  if (!context_group_->feature_info()->feature_flags().chromium_gpu_fence) {
    DLOG(ERROR) << "CHROMIUM_gpu_fence unavailable";
    command_buffer_->SetParseError(error::kLostContext);
    return;
  }

  auto* manager = decoder_context_->GetGpuFenceManager();
  gfx::GpuFenceHandle handle;
  if (manager->IsValidGpuFence(gpu_fence_id)) {
    std::unique_ptr<gfx::GpuFence> gpu_fence =
        manager->GetGpuFence(gpu_fence_id);
    handle = gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle());
  } else {
    // Retrieval failed. This shouldn't happen, force context loss to avoid
    // inconsistent state.
    DLOG(ERROR) << "GpuFence not found";
    command_buffer_->SetParseError(error::kLostContext);
    CheckContextLost();
  }
  Send(new GpuCommandBufferMsg_GetGpuFenceHandleComplete(route_id_,
                                                         gpu_fence_id, handle));
}

void CommandBufferStub::OnFenceSyncRelease(uint64_t release) {
  SyncToken sync_token(CommandBufferNamespace::GPU_IO, command_buffer_id_,
                       release);
  MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent())
    mailbox_manager->PushTextureUpdates(sync_token);

  command_buffer_->SetReleaseCount(release);
  sync_point_client_state_->ReleaseFenceSync(release);
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

// TODO(sunnyps): Remove the wait command once all sync tokens are passed as
// task dependencies.
bool CommandBufferStub::OnWaitSyncToken(const SyncToken& sync_token) {
  DCHECK(!waiting_for_sync_point_);
  DCHECK(command_buffer_->scheduled());
  TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitSyncToken", this, "CommandBufferStub",
                           this);

  waiting_for_sync_point_ = sync_point_client_state_->WaitNonThreadSafe(
      sync_token, channel_->task_runner(),
      base::Bind(&CommandBufferStub::OnWaitSyncTokenCompleted, AsWeakPtr(),
                 sync_token));

  if (waiting_for_sync_point_) {
    command_buffer_->SetScheduled(false);
    channel_->OnCommandBufferDescheduled(this);
    return true;
  }

  MailboxManager* mailbox_manager = context_group_->mailbox_manager();
  if (mailbox_manager->UsesSync() && MakeCurrent())
    mailbox_manager->PullTextureUpdates(sync_token);
  return false;
}

void CommandBufferStub::OnWaitSyncTokenCompleted(const SyncToken& sync_token) {
  DCHECK(waiting_for_sync_point_);
  TRACE_EVENT_ASYNC_END1("gpu", "WaitSyncToken", this, "CommandBufferStub",
                         this);
  // Don't call PullTextureUpdates here because we can't MakeCurrent if we're
  // executing commands on another context. The WaitSyncToken command will run
  // again and call PullTextureUpdates once this command buffer gets scheduled.
  waiting_for_sync_point_ = false;
  command_buffer_->SetScheduled(true);
  channel_->OnCommandBufferScheduled(this);
}

void CommandBufferStub::OnCreateImage(
    GpuCommandBufferMsg_CreateImage_Params params) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnCreateImage");
  const int32_t id = params.id;
  const gfx::Size& size = params.size;
  const gfx::BufferFormat& format = params.format;
  const uint64_t image_release_count = params.image_release_count;

  gles2::ImageManager* image_manager = channel_->image_manager();
  DCHECK(image_manager);
  if (image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image already exists with same ID.";
    return;
  }

  if (!gpu::IsImageFromGpuMemoryBufferFormatSupported(
          format, decoder_context_->GetCapabilities())) {
    LOG(ERROR) << "Format is not supported.";
    return;
  }

  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, format)) {
    LOG(ERROR) << "Invalid image size for format.";
    return;
  }

  scoped_refptr<gl::GLImage> image = channel()->CreateImageForGpuMemoryBuffer(
      std::move(params.gpu_memory_buffer), size, format, surface_handle_);
  if (!image.get())
    return;

  image_manager->AddImage(image.get(), id);
  if (image_release_count)
    sync_point_client_state_->ReleaseFenceSync(image_release_count);
}

void CommandBufferStub::OnDestroyImage(int32_t id) {
  TRACE_EVENT0("gpu", "CommandBufferStub::OnDestroyImage");

  gles2::ImageManager* image_manager = channel_->image_manager();
  DCHECK(image_manager);
  if (!image_manager->LookupImage(id)) {
    LOG(ERROR) << "Image with ID doesn't exist.";
    return;
  }

  image_manager->RemoveImage(id);
}

void CommandBufferStub::OnConsoleMessage(int32_t id,
                                         const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg =
      new GpuCommandBufferMsg_ConsoleMsg(route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void CommandBufferStub::CacheShader(const std::string& key,
                                    const std::string& shader) {
  channel_->CacheShader(key, shader);
}

void CommandBufferStub::AddDestructionObserver(DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void CommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

std::unique_ptr<MemoryTracker> CommandBufferStub::CreateMemoryTracker(
    const GPUCreateCommandBufferConfig init_params) const {
  return std::make_unique<GpuCommandBufferMemoryTracker>(
      channel_->client_id(), channel_->client_tracing_id(),
      command_buffer_id_.GetUnsafeValue(), init_params.attribs.context_type,
      channel_->task_runner());
}

MemoryTracker* CommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

bool CommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  CommandBuffer::State state = command_buffer_->GetState();
  bool was_lost = state.error == error::kLostContext;

  if (was_lost) {
    bool was_lost_by_robustness =
        decoder_context_ &&
        decoder_context_->WasContextLostByRobustnessExtension();

    // Work around issues with recovery by allowing a new GPU process to launch.
    if ((was_lost_by_robustness ||
         context_group_->feature_info()->workarounds().exit_on_context_lost)) {
      channel_->gpu_channel_manager()->MaybeExitOnContextLost();
    }

    // Lose all other contexts if the reset was triggered by the robustness
    // extension instead of being synthetic.
    if (was_lost_by_robustness &&
        (gl::GLContext::LosesAllContextsOnContextLost() ||
         use_virtualized_gl_context_)) {
      channel_->LoseAllContexts();
    }
  }

  CheckCompleteWaits();
  return was_lost;
}

void CommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetState().error == error::kLostContext)
    return;

  command_buffer_->SetContextLostReason(error::kUnknown);
  if (decoder_context_)
    decoder_context_->MarkContextLost(error::kUnknown);
  command_buffer_->SetParseError(error::kLostContext);
}

}  // namespace gpu
