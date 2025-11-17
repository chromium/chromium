// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/context_cache_controller.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"
#include "ui/gl/trace_util.h"

class SkDiscardableMemory;

namespace viz {

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    base::PassKey<ContextProviderCommandBuffer> pass_key,
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    const GURL& active_url,
    bool automatic_flushes,
    bool support_locking,
    const gpu::SharedMemoryLimits& memory_limits,
    gpu::mojom::ContextCreationAttribsPtr attributes,
    command_buffer_metrics::ContextType type,
    base::SharedMemoryMapper* buffer_mapper)
    : base::subtle::RefCountedThreadSafeBase(
          base::subtle::GetRefCountPreference<ContextProviderCommandBuffer>()),
      stream_id_(stream_id),
      stream_priority_(stream_priority),
      active_url_(active_url),
      automatic_flushes_(automatic_flushes),
      support_locking_(support_locking),
      memory_limits_(memory_limits),
      attributes_(std::move(attributes)),
      context_type_(type),
      channel_(std::move(channel)),
      buffer_mapper_(buffer_mapper) {
  DETACH_FROM_SEQUENCE(context_sequence_checker_);
  DCHECK(channel_);
}

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_refptr<gpu::GpuChannelHost> channel)
    : ContextProviderCommandBuffer(
          base::PassKey<ContextProviderCommandBuffer>(),
          channel,
          /*stream_id=*/0,
          gpu::SchedulingPriority::kNormal,
          GURL(),
          /*automatic_flushes=*/false,
          /*support_locking=*/false,
          gpu::SharedMemoryLimits(),
          gpu::mojom::ContextCreationAttribs::NewGles(
              gpu::mojom::GLESCreationAttribs::New()),
          command_buffer_metrics::ContextType::FOR_TESTING) {}

// static
scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::CreateForGL(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    const GURL& active_url,
    command_buffer_metrics::ContextType type,
    bool lose_context_when_out_of_memory) {
  auto attributes = gpu::mojom::GLESCreationAttribs::New();
  attributes->lose_context_when_out_of_memory = lose_context_when_out_of_memory;

  return base::MakeRefCounted<ContextProviderCommandBuffer>(
      base::PassKey<ContextProviderCommandBuffer>(), std::move(channel),
      stream_id, stream_priority, active_url,
      /*automatic_flushes=*/false, /*support_locking=*/false,
      gpu::SharedMemoryLimits::ForMailboxContext(),
      gpu::mojom::ContextCreationAttribs::NewGles(std::move(attributes)), type);
}

// static
scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::CreateForWebGL(
    scoped_refptr<gpu::GpuChannelHost> channel,
    const GURL& active_url,
    WebGLContextType context_type,
    bool prefer_low_power_gpu,
    bool fail_if_major_performance_caveat) {
  auto attributes = gpu::mojom::GLESCreationAttribs::New();
  attributes->gpu_preference = prefer_low_power_gpu
                                   ? gl::GpuPreference::kLowPower
                                   : gl::GpuPreference::kHighPerformance;

  attributes->fail_if_major_perf_caveat = fail_if_major_performance_caveat;

  switch (context_type) {
    case WebGLContextType::kWebGL1:
      attributes->context_type = gpu::CONTEXT_TYPE_WEBGL1;
      break;
    case WebGLContextType::kWebGL2:
      attributes->context_type = gpu::CONTEXT_TYPE_WEBGL2;
      break;
  }

  return base::MakeRefCounted<ContextProviderCommandBuffer>(
      base::PassKey<ContextProviderCommandBuffer>(), std::move(channel),
      /*stream_id=*/0, gpu::SchedulingPriority::kNormal, active_url,
      /*automatic_flushes=*/true, /*support_locking=*/false,
      gpu::SharedMemoryLimits(),
      gpu::mojom::ContextCreationAttribs::NewGles(std::move(attributes)),
      command_buffer_metrics::ContextType::WEBGL);
}

// static
scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::CreateForWebGPU(
    scoped_refptr<gpu::GpuChannelHost> channel,
    const GURL& active_url,
    command_buffer_metrics::ContextType type,
    base::SharedMemoryMapper* buffer_mapper) {
  auto attributes = gpu::mojom::WebGPUCreationAttribs::New();

  return base::MakeRefCounted<ContextProviderCommandBuffer>(
      base::PassKey<ContextProviderCommandBuffer>(), std::move(channel),
      /*stream_id=*/0, gpu::SchedulingPriority::kNormal, active_url,
      /*automatic_flushes=*/true,
      /*support_locking=*/false, gpu::SharedMemoryLimits::ForWebGPUContext(),
      gpu::mojom::ContextCreationAttribs::NewWebgpu(std::move(attributes)),
      type, buffer_mapper);
}

// static
scoped_refptr<ContextProviderCommandBuffer>
ContextProviderCommandBuffer::CreateForRaster(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    const GURL& active_url,
    bool automatic_flushes,
    bool support_locking,
    const gpu::SharedMemoryLimits& memory_limits,
    command_buffer_metrics::ContextType type,
    bool enable_gpu_rasterization,
    bool lose_context_when_out_of_memory) {
  auto attributes = gpu::mojom::RasterCreationAttribs::New();
  attributes->enable_gpu_rasterization = enable_gpu_rasterization;
  attributes->lose_context_when_out_of_memory = lose_context_when_out_of_memory;

  return base::MakeRefCounted<ContextProviderCommandBuffer>(
      base::PassKey<ContextProviderCommandBuffer>(), std::move(channel),
      stream_id, stream_priority, active_url, automatic_flushes,
      support_locking, memory_limits,
      gpu::mojom::ContextCreationAttribs::NewRaster(std::move(attributes)),
      type);
}

ContextProviderCommandBuffer::~ContextProviderCommandBuffer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_sequence_checker_);

  if (bind_tried_ && bind_result_ == gpu::ContextResult::kSuccess) {
    // Clear the lock to avoid DCHECKs that the lock is being held during
    // shutdown.
    command_buffer_->SetLock(nullptr);
    // Disconnect lost callbacks during destruction.
    impl_->SetLostContextCallback(base::OnceClosure());
    // Unregister memory dump provider.
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }
}

gpu::CommandBufferProxyImpl*
ContextProviderCommandBuffer::GetCommandBufferProxy() {
  return command_buffer_.get();
}

void ContextProviderCommandBuffer::AddRef() const {
  base::subtle::RefCountedThreadSafeBase::AddRefWithCheck();
}

void ContextProviderCommandBuffer::Release() const {
  if (base::subtle::RefCountedThreadSafeBase::Release()) {
    if (default_task_runner_ &&
        !default_task_runner_->RunsTasksInCurrentSequence()) {
      default_task_runner_->DeleteSoon(FROM_HERE, this);
    } else {
      delete this;
    }
  }
}

gpu::ContextResult ContextProviderCommandBuffer::BindToCurrentSequence() {
  // This is called on the sequence the context will be used.
  DCHECK_CALLED_ON_VALID_SEQUENCE(context_sequence_checker_);
  CHECK(channel_);

  if (bind_tried_)
    return bind_result_;

  bind_tried_ = true;

  // This is for no swiftshader and no software rasterization.
  // Return kFatalFailure to indicate no retry is necessary. Should update
  // Renderer Thread Impl or Blink so it won't keep retrying
  // BindToCurrentSequence.
  if (channel_->gpu_info().gl_implementation_parts.gl ==
      gl::kGLImplementationDisabled) {
    bind_result_ = gpu::ContextResult::kFatalFailure;
    return bind_result_;
  }

  // Any early-out should set this to a failure code and return it.
  bind_result_ = gpu::ContextResult::kSuccess;

  if (!default_task_runner_) {
    default_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  }
  // This command buffer is a client-side proxy to the command buffer in the
  // GPU process.
  command_buffer_ = std::make_unique<gpu::CommandBufferProxyImpl>(
      channel_, stream_id_, default_task_runner_, buffer_mapper_);
  bind_result_ = command_buffer_->Initialize(
      stream_priority_, attributes_.Clone(), active_url_,
      command_buffer_metrics::ContextTypeToString(context_type_));
  if (bind_result_ != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    command_buffer_metrics::UmaRecordContextInitFailed(context_type_);
    return bind_result_;
  }

  switch (attributes_->which()) {
    case gpu::mojom::ContextCreationAttribs::Tag::kWebgpu: {
      auto webgpu_helper =
          std::make_unique<gpu::webgpu::WebGPUCmdHelper>(command_buffer_.get());
      webgpu_helper->SetAutomaticFlushes(automatic_flushes_);
      bind_result_ =
          webgpu_helper->Initialize(memory_limits_.command_buffer_size);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize WebGPUCmdHelper.";
        return bind_result_;
      }

      // The transfer buffer is used to serialize Dawn commands
      auto transfer_buffer =
          std::make_unique<gpu::TransferBuffer>(webgpu_helper.get());

      // The WebGPUImplementation exposes the WebGPUInterface, as well as the
      // gpu::ContextSupport interface.
      auto webgpu_impl = std::make_unique<gpu::webgpu::WebGPUImplementation>(
          webgpu_helper.get(), transfer_buffer.get(), command_buffer_.get());
      bind_result_ = webgpu_impl->Initialize(memory_limits_);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize WebGPUImplementation.";
        return bind_result_;
      }

      std::string type_name =
          command_buffer_metrics::ContextTypeToString(context_type_);
      std::string unique_context_name =
          base::StringPrintf("%s-%p", type_name.c_str(), webgpu_impl.get());

      // IMPORTANT: These hold raw_ptrs to each other, so must be set together.
      // See note in the header (and keep it up to date if things change).
      impl_ = webgpu_impl.get();
      webgpu_interface_ = std::move(webgpu_impl);
      transfer_buffer_ = std::move(transfer_buffer);
      helper_ = std::move(webgpu_helper);
    } break;
    case gpu::mojom::ContextCreationAttribs::Tag::kRaster: {
      // The raster helper writes the command buffer protocol.
      auto raster_helper =
          std::make_unique<gpu::raster::RasterCmdHelper>(command_buffer_.get());
      raster_helper->SetAutomaticFlushes(automatic_flushes_);
      bind_result_ =
          raster_helper->Initialize(memory_limits_.command_buffer_size);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize RasterCmdHelper.";
        return bind_result_;
      }
      // The transfer buffer is used to copy resources between the client
      // process and the GPU process.
      auto transfer_buffer =
          std::make_unique<gpu::TransferBuffer>(raster_helper.get());

      // The RasterImplementation exposes the RasterInterface, as well as the
      // gpu::ContextSupport interface.
      DCHECK(channel_);
      auto raster_impl = std::make_unique<gpu::raster::RasterImplementation>(
          raster_helper.get(), transfer_buffer.get(),
          attributes_->get_raster()->lose_context_when_out_of_memory,
          command_buffer_.get());
      bind_result_ = raster_impl->Initialize(memory_limits_);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize RasterImplementation.";
        return bind_result_;
      }

      std::string type_name =
          command_buffer_metrics::ContextTypeToString(context_type_);
      std::string unique_context_name =
          base::StringPrintf("%s-%p", type_name.c_str(), raster_impl.get());
      raster_impl->TraceBeginCHROMIUM("gpu_toplevel",
                                      unique_context_name.c_str());

      // IMPORTANT: These hold raw_ptrs to each other, so must be set together.
      // See note in the header (and keep it up to date if things change).
      impl_ = raster_impl.get();
      raster_interface_ = std::move(raster_impl);
      transfer_buffer_ = std::move(transfer_buffer);
      helper_ = std::move(raster_helper);
    } break;
    case gpu::mojom::ContextCreationAttribs::Tag::kGles: {
      // The GLES2 helper writes the command buffer protocol.
      auto gles2_helper =
          std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer_.get());
      gles2_helper->SetAutomaticFlushes(automatic_flushes_);
      bind_result_ =
          gles2_helper->Initialize(memory_limits_.command_buffer_size);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize GLES2CmdHelper.";
        return bind_result_;
      }

      // The transfer buffer is used to copy resources between the client
      // process and the GPU process.
      auto transfer_buffer =
          std::make_unique<gpu::TransferBuffer>(gles2_helper.get());

      // The GLES2Implementation exposes the OpenGLES2 API, as well as the
      // gpu::ContextSupport interface.
      auto gles2_impl = std::make_unique<gpu::gles2::GLES2Implementation>(
          gles2_helper.get(), /*share_group=*/nullptr, transfer_buffer.get(),
          attributes_->get_gles()->lose_context_when_out_of_memory,
          command_buffer_.get());
      bind_result_ = gles2_impl->Initialize(memory_limits_);
      if (bind_result_ != gpu::ContextResult::kSuccess) {
        DLOG(ERROR) << "Failed to initialize GLES2Implementation.";
        return bind_result_;
      }

      // IMPORTANT: These hold raw_ptrs to each other, so must be set together.
      // See note in the header (and keep it up to date if things change).
      impl_ = gles2_impl.get();
      gles2_impl_ = std::move(gles2_impl);
      transfer_buffer_ = std::move(transfer_buffer);
      helper_ = std::move(gles2_helper);
    } break;
  }

  if (command_buffer_->GetLastState().error != gpu::error::kNoError) {
    // The context was DOA, which can be caused by other contexts and we
    // could try again.
    LOG(ERROR) << "ContextResult::kTransientFailure: "
                  "Context dead on arrival. Last error: "
               << command_buffer_->GetLastState().error;
    bind_result_ = gpu::ContextResult::kTransientFailure;
    return bind_result_;
  }

  cache_controller_ =
      std::make_unique<ContextCacheController>(impl_, default_task_runner_);

  // TODO(crbug.com/40586882): SetLostContextCallback should probably work on
  // WebGPU contexts too.
  if (impl_) {
    impl_->SetLostContextCallback(
        base::BindOnce(&ContextProviderCommandBuffer::OnLostContext,
                       // |this| owns the impl_, which holds the callback.
                       base::Unretained(this)));
  }

  if (gles2_impl_) {
    // Grab the implementation directly instead of going through ContextGL()
    // because the lock hasn't been acquired yet.
    gpu::gles2::GLES2Interface* gl = gles2_impl_.get();
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableGpuClientTracing)) {
      // This wraps the real GLES2Implementation and we should always use this
      // instead when it's present.
      // IMPORTANT: This holds a raw_ptr to gles2_impl_.
      trace_impl_ = std::make_unique<gpu::gles2::GLES2TraceImplementation>(
          gles2_impl_.get());
      gl = trace_impl_.get();
    }

    // Do this last once the context is set up.
    std::string type_name =
        command_buffer_metrics::ContextTypeToString(context_type_);
    std::string unique_context_name =
        base::StringPrintf("%s-%p", type_name.c_str(), gles2_impl_.get());
    gl->TraceBeginCHROMIUM("gpu_toplevel", unique_context_name.c_str());
  }

  // If support_locking_ is true, the context may be used from multiple
  // threads, and any async callstacks will need to hold the same lock, so
  // give it to the command buffer and cache controller.
  // We don't hold a lock here since there's no need, so set the lock very last
  // to prevent asserts that we're not holding it.
  if (support_locking_) {
    command_buffer_->SetLock(&context_lock_);
    cache_controller_->SetLock(&context_lock_);
  }

  shared_image_interface_ = channel_->CreateClientSharedImageInterface();
  DCHECK(shared_image_interface_);

  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          this, "ContextProviderCommandBuffer", default_task_runner_,
          base::trace_event::MemoryDumpProvider::Options());
  return bind_result_;
}

gpu::gles2::GLES2Interface* ContextProviderCommandBuffer::ContextGL() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();

  if (trace_impl_)
    return trace_impl_.get();
  return gles2_impl_.get();
}

gpu::raster::RasterInterface* ContextProviderCommandBuffer::RasterInterface() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();

  return raster_interface_.get();
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return impl_;
}

gpu::SharedImageInterface*
ContextProviderCommandBuffer::SharedImageInterface() {
  return shared_image_interface_.get();
}

ContextCacheController* ContextProviderCommandBuffer::CacheController() {
  CheckValidSequenceOrLockAcquired();
  return cache_controller_.get();
}

void ContextProviderCommandBuffer::SetDefaultTaskRunner(
    scoped_refptr<base::SingleThreadTaskRunner> default_task_runner) {
  DCHECK(!bind_tried_);
  DCHECK(!default_task_runner_);
  default_task_runner_ = std::move(default_task_runner);
}

base::Lock* ContextProviderCommandBuffer::GetLock() {
  if (!support_locking_)
    return nullptr;
  return &context_lock_;
}

const gpu::Capabilities& ContextProviderCommandBuffer::ContextCapabilities()
    const {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();
  // Skips past the trace_impl_ as it doesn't have capabilities.
  return impl_->capabilities();
}

const gpu::GpuFeatureInfo& ContextProviderCommandBuffer::GetGpuFeatureInfo()
    const {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();
  if (!command_buffer_ || !command_buffer_->channel()) {
    static const base::NoDestructor<gpu::GpuFeatureInfo>
        default_gpu_feature_info;
    return *default_gpu_feature_info;
  }
  return command_buffer_->channel()->gpu_feature_info();
}

void ContextProviderCommandBuffer::OnLostContext() {
  CheckValidSequenceOrLockAcquired();

  // Observers may drop the last persistent references to `this`, but there may
  // be weak references in use further up the stack. This task is posted to
  // ensure that destruction is deferred until it's safe.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::DoNothingWithBoundArgs(base::WrapRefCounted(this)));

  for (auto& observer : observers_)
    observer.OnContextLost();

  gpu::CommandBuffer::State state = GetCommandBufferProxy()->GetLastState();
  command_buffer_metrics::UmaRecordContextLost(context_type_, state.error,
                                               state.context_lost_reason);
}

void ContextProviderCommandBuffer::AddObserver(ContextLostObserver* obs) {
  CheckValidSequenceOrLockAcquired();
  observers_.AddObserver(obs);
}

void ContextProviderCommandBuffer::RemoveObserver(ContextLostObserver* obs) {
  CheckValidSequenceOrLockAcquired();
  observers_.RemoveObserver(obs);
}

gpu::webgpu::WebGPUInterface* ContextProviderCommandBuffer::WebGPUInterface() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();

  return webgpu_interface_.get();
}

bool ContextProviderCommandBuffer::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);

  base::AutoLockMaybe hold_if_supported(GetLock());

  impl_->OnMemoryDump(args, pmd);
  helper_->OnMemoryDump(args, pmd);

  return true;
}

}  // namespace viz
