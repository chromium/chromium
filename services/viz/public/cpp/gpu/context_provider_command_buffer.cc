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
#include "gpu/command_buffer/client/gles2_trace_implementation.h"
#include "gpu/command_buffer/client/gpu_switches.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/raster_implementation_gles.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/skia_utils.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/skia_limits.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/skia_bindings/gles2_implementation_with_grcontext_support.h"
#include "gpu/skia_bindings/grcontext_for_gles2_interface.h"
#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"
#include "skia/buildflags.h"
#include "third_party/skia/include/core/SkTraceMemoryDump.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gl/trace_util.h"

class SkDiscardableMemory;

namespace viz {

ContextProviderCommandBuffer::ContextProviderCommandBuffer(
    scoped_refptr<gpu::GpuChannelHost> channel,
    int32_t stream_id,
    gpu::SchedulingPriority stream_priority,
    gpu::SurfaceHandle surface_handle,
    const GURL& active_url,
    bool automatic_flushes,
    bool support_locking,
    const gpu::SharedMemoryLimits& memory_limits,
    const gpu::ContextCreationAttribs& attributes,
    command_buffer_metrics::ContextType type,
    base::SharedMemoryMapper* buffer_mapper)
    : base::subtle::RefCountedThreadSafeBase(
          base::subtle::GetRefCountPreference<ContextProviderCommandBuffer>()),
      stream_id_(stream_id),
      stream_priority_(stream_priority),
      surface_handle_(surface_handle),
      active_url_(active_url),
      automatic_flushes_(automatic_flushes),
      support_locking_(support_locking),
      memory_limits_(memory_limits),
      attributes_(attributes),
      context_type_(type),
      channel_(std::move(channel)),
      buffer_mapper_(buffer_mapper) {
  DETACH_FROM_SEQUENCE(context_sequence_checker_);
  DCHECK(channel_);
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
      surface_handle_, /*shared_command_buffer=*/nullptr, stream_priority_,
      attributes_, active_url_);
  if (bind_result_ != gpu::ContextResult::kSuccess) {
    DLOG(ERROR) << "GpuChannelHost failed to create command buffer.";
    command_buffer_metrics::UmaRecordContextInitFailed(context_type_);
    return bind_result_;
  }

  if (attributes_.context_type == gpu::CONTEXT_TYPE_WEBGPU) {
    DCHECK(!attributes_.enable_raster_interface);
    DCHECK(!attributes_.enable_gles2_interface);

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
  } else if (attributes_.enable_raster_interface &&
             !attributes_.enable_gles2_interface &&
             !attributes_.enable_grcontext) {
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
        attributes_.bind_generates_resource,
        attributes_.lose_context_when_out_of_memory, command_buffer_.get(),
        channel_->image_decode_accelerator_proxy());
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
  } else {
    // The GLES2 helper writes the command buffer protocol.
    auto gles2_helper =
        std::make_unique<gpu::gles2::GLES2CmdHelper>(command_buffer_.get());
    gles2_helper->SetAutomaticFlushes(automatic_flushes_);
    bind_result_ = gles2_helper->Initialize(memory_limits_.command_buffer_size);
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
    constexpr bool support_client_side_arrays = false;

    std::unique_ptr<gpu::gles2::GLES2Implementation> gles2_impl;
    if (attributes_.enable_grcontext) {
      // GLES2ImplementationWithGrContextSupport adds a bit of overhead, so
      // we only use it if grcontext_support was requested.
      gles2_impl = std::make_unique<
          skia_bindings::GLES2ImplementationWithGrContextSupport>(
          gles2_helper.get(), /*share_group=*/nullptr, transfer_buffer.get(),
          attributes_.bind_generates_resource,
          attributes_.lose_context_when_out_of_memory,
          support_client_side_arrays, command_buffer_.get());
    } else {
      gles2_impl = std::make_unique<gpu::gles2::GLES2Implementation>(
          gles2_helper.get(), /*share_group=*/nullptr, transfer_buffer.get(),
          attributes_.bind_generates_resource,
          attributes_.lose_context_when_out_of_memory,
          support_client_side_arrays, command_buffer_.get());
    }
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

  if (!attributes_.enable_gles2_interface)
    return nullptr;

  if (trace_impl_)
    return trace_impl_.get();
  return gles2_impl_.get();
}

gpu::raster::RasterInterface* ContextProviderCommandBuffer::RasterInterface() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  CheckValidSequenceOrLockAcquired();

  if (raster_interface_) {
    return raster_interface_.get();
  }

  if (!attributes_.enable_raster_interface) {
    return nullptr;
  }

#if BUILDFLAG(IS_ANDROID)
  // Android uses RasterDecoder exclusively.
  NOTREACHED();
#else
  if (!gles2_impl_.get()) {
    return nullptr;
  }

  raster_interface_ = std::make_unique<gpu::raster::RasterImplementationGLES>(
      gles2_impl_.get(), gles2_impl_.get(), ContextCapabilities());
  return raster_interface_.get();
#endif
}

gpu::ContextSupport* ContextProviderCommandBuffer::ContextSupport() {
  return impl_;
}

class GrDirectContext* ContextProviderCommandBuffer::GrContext() {
  DCHECK(bind_tried_);
  DCHECK_EQ(bind_result_, gpu::ContextResult::kSuccess);
  if (!attributes_.enable_grcontext ||
      !ContextSupport()->HasGrContextSupport()) {
    return nullptr;
  }
  CheckValidSequenceOrLockAcquired();

  if (gr_context_) {
    return gr_context_->get();
  }

  if (attributes_.enable_oop_rasterization) {
    return nullptr;
  }

  if (attributes_.context_type == gpu::CONTEXT_TYPE_WEBGPU) {
    return nullptr;
  }

  // TODO(vmiura): crbug.com/793508 Disable access to GrContext if
  // enable_gles2_interface is disabled, after removing any dependencies on
  // GrContext in OOP-Raster.

  size_t max_resource_cache_bytes;
  size_t max_glyph_cache_texture_bytes;
  gpu::DetermineGrCacheLimitsFromAvailableMemory(
      &max_resource_cache_bytes, &max_glyph_cache_texture_bytes);

  gpu::gles2::GLES2Interface* gl_interface;
  if (trace_impl_) {
    gl_interface = trace_impl_.get();
  } else {
    gl_interface = gles2_impl_.get();
  }

  gr_context_ = std::make_unique<skia_bindings::GrContextForGLES2Interface>(
      gl_interface, ContextSupport(), ContextCapabilities(),
      max_resource_cache_bytes, max_glyph_cache_texture_bytes);
  cache_controller_->SetGrContext(gr_context_->get());

  // If GlContext is already lost, also abandon the new GrContext.
  if (gr_context_->get() &&
      gles2_impl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
    gr_context_->get()->abandonContext();
  }

  return gr_context_->get();
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
  if (gr_context_)
    gr_context_->OnLostContext();

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

unsigned int ContextProviderCommandBuffer::GetGrGLTextureFormat(
    SharedImageFormat format) const {
  return SharedImageFormatRestrictedSinglePlaneUtils::ToGLTextureStorageFormat(
      format, ContextCapabilities().angle_rgbx_internal_format);
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

  if (gr_context_) {
    if (args.level_of_detail ==
        base::trace_event::MemoryDumpLevelOfDetail::kBackground) {
      gpu::raster::DumpBackgroundGrMemoryStatistics(gr_context_->get(), pmd);
    } else {
      gpu::raster::DumpGrMemoryStatistics(gr_context_->get(), pmd,
                                          gles2_impl_->ShareGroupTracingGUID());
    }
  }
  return true;
}

}  // namespace viz
