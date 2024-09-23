// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/shared_image_stub.h"

#include <inttypes.h>

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_peak_memory.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_shared_image_interface.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_context.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace {

constexpr char kSICreationFailureError[] =
    "SharedImageStub: Unable to create shared image";

}  // namespace

#if BUILDFLAG(IS_WIN)
namespace base {
bool operator<(const scoped_refptr<gfx::D3DSharedFence>& lhs,
               const scoped_refptr<gfx::D3DSharedFence>& rhs) {
  return lhs->GetDXGIHandleToken() < rhs->GetDXGIHandleToken();
}

bool operator<(const gfx::DXGIHandleToken& lhs,
               const scoped_refptr<gfx::D3DSharedFence>& rhs) {
  return lhs < rhs->GetDXGIHandleToken();
}

bool operator<(const scoped_refptr<gfx::D3DSharedFence>& lhs,
               const gfx::DXGIHandleToken& rhs) {
  return lhs->GetDXGIHandleToken() < rhs;
}

}  // namespace base
#endif

namespace gpu {
SharedImageStub::SharedImageStub(GpuChannel* channel, int32_t route_id)
    : channel_(channel),
      command_buffer_id_(
          CommandBufferIdFromChannelAndRoute(channel->client_id(), route_id)),
      sequence_(
          channel->scheduler()->CreateSequence(SchedulingPriority::kLow,
                                               channel_->task_runner(),
                                               CommandBufferNamespace::GPU_IO,
                                               command_buffer_id_)) {}

SharedImageStub::~SharedImageStub() {
  channel_->scheduler()->DestroySequence(sequence_);
  if (factory_ && factory_->HasImages()) {
    // Some of the backings might require a current GL context to be destroyed.
    bool have_context = MakeContextCurrent(/*needs_gl=*/true);
    factory_->DestroyAllSharedImages(have_context);
  }
}

const scoped_refptr<gpu::GpuChannelSharedImageInterface>&
SharedImageStub::shared_image_interface() {
  return gpu_channel_shared_image_interface_;
}

std::unique_ptr<SharedImageStub> SharedImageStub::Create(GpuChannel* channel,
                                                         int32_t route_id) {
  auto stub = base::WrapUnique(new SharedImageStub(channel, route_id));
  ContextResult result = stub->Initialize();
  if (result == ContextResult::kSuccess)
    return stub;

  // If it's not a transient failure, treat it as fatal.
  if (result != ContextResult::kTransientFailure)
    return nullptr;

  // For transient failure, retry once to create a shared context state and
  // hence factory again.
  if (stub->Initialize() != ContextResult::kSuccess) {
    return nullptr;
  }
  return stub;
}

void SharedImageStub::ExecuteDeferredRequest(
    mojom::DeferredSharedImageRequestPtr request) {
  switch (request->which()) {
    case mojom::DeferredSharedImageRequest::Tag::kNop:
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImage:
      OnCreateSharedImage(std::move(request->get_create_shared_image()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImageWithData:
      OnCreateSharedImageWithData(
          std::move(request->get_create_shared_image_with_data()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCreateSharedImageWithBuffer:
      OnCreateSharedImageWithBuffer(
          std::move(request->get_create_shared_image_with_buffer()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kRegisterUploadBuffer:
      OnRegisterSharedImageUploadBuffer(
          std::move(request->get_register_upload_buffer()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kUpdateSharedImage: {
      auto& update = *request->get_update_shared_image();
      OnUpdateSharedImage(update.mailbox, std::move(update.in_fence_handle));
      break;
    }

    case mojom::DeferredSharedImageRequest::Tag::kAddReferenceToSharedImage: {
      const auto& add_ref = *request->get_add_reference_to_shared_image();
      OnAddReference(add_ref.mailbox);
      break;
    }

    case mojom::DeferredSharedImageRequest::Tag::kDestroySharedImage:
      OnDestroySharedImage(request->get_destroy_shared_image());
      break;

    case mojom::DeferredSharedImageRequest::Tag::kCopyToGpuMemoryBuffer: {
      auto& params = *request->get_copy_to_gpu_memory_buffer();
      OnCopyToGpuMemoryBuffer(params.mailbox);
      break;
    }

#if BUILDFLAG(IS_WIN)
    case mojom::DeferredSharedImageRequest::Tag::kCreateSwapChain:
      OnCreateSwapChain(std::move(request->get_create_swap_chain()));
      break;

    case mojom::DeferredSharedImageRequest::Tag::kPresentSwapChain:
      OnPresentSwapChain(request->get_present_swap_chain()->mailbox);
      break;
    case mojom::DeferredSharedImageRequest::Tag::kRegisterDxgiFence: {
      auto& reg = *request->get_register_dxgi_fence();
      OnRegisterDxgiFence(reg.mailbox, reg.dxgi_token,
                          std::move(reg.fence_handle));
      break;
    }
    case mojom::DeferredSharedImageRequest::Tag::kUpdateDxgiFence: {
      auto& update = *request->get_update_dxgi_fence();
      OnUpdateDxgiFence(update.mailbox, update.dxgi_token, update.fence_value);
      break;
    }
    case mojom::DeferredSharedImageRequest::Tag::kUnregisterDxgiFence: {
      auto& unregister = *request->get_unregister_dxgi_fence();
      OnUnregisterDxgiFence(unregister.mailbox, unregister.dxgi_token);
      break;
    }
#endif  // BUILDFLAG(IS_WIN)
  }
}

bool SharedImageStub::GetGpuMemoryBufferHandleInfo(
    const gpu::Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle& handle,
    viz::SharedImageFormat& format,
    gfx::Size& size,
    gfx::BufferUsage& buffer_usage) {
  TRACE_EVENT0("gpu", "SharedImageStub::GetGpuMemoryBufferHandleInfo");
  // Note that we are not making |context_state_| current here as of now since
  // it is not needed to get the handle from the backings. Make context current
  // if we find that it is required.

  if (!factory_->GetGpuMemoryBufferHandleInfo(mailbox, handle, format, size,
                                              buffer_usage)) {
    LOG(ERROR) << "SharedImageStub: Unable to get GpuMemoryBufferHandle";
    return false;
  }
  return true;
}

bool SharedImageStub::CreateSharedImage(const Mailbox& mailbox,
                                        gfx::GpuMemoryBufferHandle handle,
                                        viz::SharedImageFormat format,
                                        const gfx::Size& size,
                                        const gfx::ColorSpace& color_space,
                                        GrSurfaceOrigin surface_origin,
                                        SkAlphaType alpha_type,
                                        SharedImageUsageSet usage,
                                        std::string debug_label) {
  TRACE_EVENT2("gpu", "SharedImageStub::CreateSharedImage", "width",
               size.width(), "height", size.height());
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  if (format.PrefersExternalSampler()) {
    LOG(ERROR) << "SharedImageStub: Incompatible format.";
    OnError();
    return false;
  }
#endif

  bool needs_gl = HasGLES2ReadOrWriteUsage(usage);
  if (!MakeContextCurrent(needs_gl)) {
    OnError();
    return false;
  }

  if (!factory_->CreateSharedImage(mailbox, format, size, color_space,
                                   surface_origin, alpha_type, usage,
                                   GetLabel(debug_label), std::move(handle))) {
    LOG(ERROR) << kSICreationFailureError;
    OnError();
    return false;
  }
  return true;
}

bool SharedImageStub::UpdateSharedImage(const Mailbox& mailbox,
                                        gfx::GpuFenceHandle in_fence_handle) {
  TRACE_EVENT0("gpu", "SharedImageStub::UpdateSharedImage");
  std::unique_ptr<gfx::GpuFence> in_fence;
  if (!in_fence_handle.is_null())
    in_fence = std::make_unique<gfx::GpuFence>(std::move(in_fence_handle));
  if (!MakeContextCurrent()) {
    OnError();
    return false;
  }
  if (!factory_->UpdateSharedImage(mailbox, std::move(in_fence))) {
    LOG(ERROR) << "SharedImageStub: Unable to update shared image";
    OnError();
    return false;
  }
  return true;
}

void SharedImageStub::SetGpuExtraInfo(const gfx::GpuExtraInfo& gpu_extra_info) {
  CHECK(factory_);
  factory_->SetGpuExtraInfo(gpu_extra_info);
}

void SharedImageStub::OnCreateSharedImage(
    mojom::CreateSharedImageParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateSharedImage", "width",
               params->si_info->meta.size.width(), "height",
               params->si_info->meta.size.height());
  bool needs_gl = HasGLES2ReadOrWriteUsage(params->si_info->meta.usage);
  if (!MakeContextCurrent(needs_gl)) {
    OnError();
    return;
  }

  if (!factory_->CreateSharedImage(
          params->mailbox, params->si_info->meta.format,
          params->si_info->meta.size, params->si_info->meta.color_space,
          params->si_info->meta.surface_origin,
          params->si_info->meta.alpha_type, gpu::kNullSurfaceHandle,
          params->si_info->meta.usage,
          GetLabel(params->si_info->debug_label))) {
    LOG(ERROR) << kSICreationFailureError;
    OnError();
    return;
  }
}

void SharedImageStub::OnCreateSharedImageWithData(
    mojom::CreateSharedImageWithDataParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateSharedImageWithData", "width",
               params->si_info->meta.size.width(), "height",
               params->si_info->meta.size.height());
  bool needs_gl = HasGLES2ReadOrWriteUsage(params->si_info->meta.usage);
  if (!MakeContextCurrent(needs_gl)) {
    OnError();
    return;
  }

  base::CheckedNumeric<size_t> safe_required_span_size =
      params->pixel_data_offset;
  safe_required_span_size += params->pixel_data_size;
  size_t required_span_size;
  if (!safe_required_span_size.AssignIfValid(&required_span_size)) {
    LOG(ERROR) << "SharedImageStub: upload data size and offset is invalid";
    OnError();
    return;
  }

  auto memory =
      upload_memory_mapping_.GetMemoryAsSpan<uint8_t>(required_span_size);
  if (memory.empty()) {
    LOG(ERROR) << "SharedImageStub: upload data does not have expected size";
    OnError();
    return;
  }

  auto subspan =
      memory.subspan(params->pixel_data_offset, params->pixel_data_size);

  if (!factory_->CreateSharedImage(
          params->mailbox, params->si_info->meta.format,
          params->si_info->meta.size, params->si_info->meta.color_space,
          params->si_info->meta.surface_origin,
          params->si_info->meta.alpha_type, params->si_info->meta.usage,
          GetLabel(params->si_info->debug_label), subspan)) {
    LOG(ERROR) << kSICreationFailureError;
    OnError();
    return;
  }

  // If this is the last upload using a given buffer, release it.
  if (params->done_with_shm) {
    upload_memory_mapping_ = base::ReadOnlySharedMemoryMapping();
    upload_memory_ = base::ReadOnlySharedMemoryRegion();
  }
}

void SharedImageStub::OnCreateSharedImageWithBuffer(
    mojom::CreateSharedImageWithBufferParamsPtr params) {
  TRACE_EVENT2("gpu", "SharedImageStub::OnCreateSharedImageWithBuffer", "width",
               params->si_info->meta.size.width(), "height",
               params->si_info->meta.size.height());
  if (!CreateSharedImage(
          params->mailbox, std::move(params->buffer_handle),
          params->si_info->meta.format, params->si_info->meta.size,
          params->si_info->meta.color_space,
          params->si_info->meta.surface_origin,
          params->si_info->meta.alpha_type, params->si_info->meta.usage,
          GetLabel(params->si_info->debug_label))) {
    return;
  }
}

void SharedImageStub::OnUpdateSharedImage(const Mailbox& mailbox,
                                          gfx::GpuFenceHandle in_fence_handle) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnUpdateSharedImage");

  if (!UpdateSharedImage(mailbox, std::move(in_fence_handle)))
    return;
}

void SharedImageStub::OnAddReference(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnUpdateSharedImage");
  if (!factory_->AddSecondaryReference(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Unable to add secondary reference";
    OnError();
    return;
  }
}

void SharedImageStub::OnDestroySharedImage(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnDestroySharedImage");
  bool needs_gl =
      HasGLES2ReadOrWriteUsage(factory_->GetUsageForMailbox(mailbox));
  if (!MakeContextCurrent(needs_gl)) {
    OnError();
    return;
  }

  if (!factory_->DestroySharedImage(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Unable to destroy shared image";
    OnError();
    return;
  }

#if BUILDFLAG(IS_WIN)
  registered_dxgi_fences_.erase(mailbox);
#endif
}

void SharedImageStub::OnCopyToGpuMemoryBuffer(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnCopyToGpuMemoryBuffer");
  if (!MakeContextCurrent()) {
    OnError();
    return;
  }
  if (!factory_->CopyToGpuMemoryBuffer(mailbox)) {
    DLOG(ERROR) << "SharedImageStub: Unable to update shared GMB";
    OnError();
    return;
  }
}

#if BUILDFLAG(IS_WIN)
void SharedImageStub::CopyToGpuMemoryBufferAsync(
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("gpu", "SharedImageStub::CopyToGpuMemoryBufferAsync");
  auto split_cb = base::SplitOnceCallback(std::move(callback));
  if (!factory_->CopyToGpuMemoryBufferAsync(mailbox,
                                            std::move(split_cb.first))) {
    DLOG(ERROR) << "SharedImageStub: Unable to update shared GMB";
    std::move(split_cb.second).Run(false);
    OnError();
    return;
  }
}

void SharedImageStub::OnCreateSwapChain(
    mojom::CreateSwapChainParamsPtr params) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnCreateSwapChain");
  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  if (!factory_->CreateSwapChain(params->front_buffer_mailbox,
                                 params->back_buffer_mailbox, params->format,
                                 params->size, params->color_space,
                                 params->surface_origin, params->alpha_type,
                                 SharedImageUsageSet(params->usage))) {
    DLOG(ERROR) << "SharedImageStub: Unable to create swap chain";
    OnError();
    return;
  }
}

void SharedImageStub::OnPresentSwapChain(const Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnPresentSwapChain");
  if (!MakeContextCurrent()) {
    OnError();
    return;
  }

  if (!factory_->PresentSwapChain(mailbox)) {
    DLOG(ERROR) << "SharedImageStub: Unable to present swap chain";
    OnError();
    return;
  }
}

void SharedImageStub::OnRegisterDxgiFence(const Mailbox& mailbox,
                                          gfx::DXGIHandleToken dxgi_token,
                                          gfx::GpuFenceHandle fence_handle) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnRegisterDxgiFence");
  if (!factory_->HasSharedImage(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Trying to register a fence handle to a "
                  "invalid SharedImage.";
    OnError();
    return;
  }

  auto& mailbox_fences = registered_dxgi_fences_[mailbox];
  auto it = mailbox_fences.find(dxgi_token);
  if (it != mailbox_fences.end()) {
    LOG(ERROR) << "SharedImageStub: Trying to register the same fence handle "
                  "multiple times in SharedImage.";
    OnError();
    return;
  }

  mailbox_fences.emplace_hint(mailbox_fences.begin(),
                              gfx::D3DSharedFence::CreateFromScopedHandle(
                                  fence_handle.Release(), dxgi_token));
}

void SharedImageStub::OnUpdateDxgiFence(const Mailbox& mailbox,
                                        gfx::DXGIHandleToken dxgi_token,
                                        uint64_t fence_value) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnUpdateDxgiFence");
  if (!factory_->HasSharedImage(mailbox)) {
    LOG(ERROR) << "SharedImageStub: Trying to register a fence handle to a "
                  "invalid SharedImage.";
    OnError();
    return;
  }

  auto mailbox_fences_it = registered_dxgi_fences_.find(mailbox);
  if (mailbox_fences_it == registered_dxgi_fences_.end()) {
    LOG(ERROR) << "Trying to update a fence on shared image with no registered "
                  "fences.";
    OnError();
    return;
  }

  auto& mailbox_fences = mailbox_fences_it->second;
  auto fence_it = mailbox_fences.find(dxgi_token);
  if (fence_it == mailbox_fences.end()) {
    LOG(ERROR) << "Trying to update a fence that has not been registered with "
                  "shared image.";
    OnError();
    return;
  }

  scoped_refptr<gfx::D3DSharedFence> fence = *fence_it;
  fence->Update(fence_value);

  channel_->gpu_channel_manager()->shared_image_manager()->UpdateExternalFence(
      mailbox, std::move(fence));
}

void SharedImageStub::OnUnregisterDxgiFence(const Mailbox& mailbox,
                                            gfx::DXGIHandleToken dxgi_token) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnUnregisterDxgiFence");
  auto mailbox_fences_it = registered_dxgi_fences_.find(mailbox);
  if (mailbox_fences_it == registered_dxgi_fences_.end()) {
    LOG(ERROR) << "Trying to unregister a fence on shared image with no "
                  "registered fences.";
    OnError();
    return;
  }

  auto& mailbox_fences = mailbox_fences_it->second;
  auto fence_it = mailbox_fences.find(dxgi_token);
  if (fence_it == mailbox_fences.end()) {
    LOG(ERROR) << "Trying to unregister a fence that has not been registered "
                  "with shared image.";
    OnError();
    return;
  }

  mailbox_fences.erase(fence_it);
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageStub::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  if (!service_handle || !sysmem_token) {
    OnError();
    return;
  }

  factory_->RegisterSysmemBufferCollection(std::move(service_handle),
                                           std::move(sysmem_token), format,
                                           usage, register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

void SharedImageStub::OnRegisterSharedImageUploadBuffer(
    base::ReadOnlySharedMemoryRegion shm) {
  TRACE_EVENT0("gpu", "SharedImageStub::OnRegisterSharedImageUploadBuffer");
  upload_memory_ = std::move(shm);
  upload_memory_mapping_ = upload_memory_.Map();
  if (!upload_memory_mapping_.IsValid()) {
    LOG(ERROR)
        << "SharedImageStub: Unable to map shared memory for upload data";
    OnError();
    return;
  }
}

bool SharedImageStub::MakeContextCurrent(bool needs_gl) {
  // Software Renderer doesn't have valid context_state_.
  if (!context_state_) {
    return true;
  }

  if (context_state_->context_lost()) {
    LOG(ERROR) << "SharedImageStub: context already lost";
    return false;
  }

  // |factory_| never writes to the surface, so pass nullptr to
  // improve performance. https://crbug.com/457431
  auto* context = context_state_->real_context();
  if (context->IsCurrent(nullptr))
    return !context_state_->CheckResetStatus(needs_gl);
  return context_state_->MakeCurrent(/*surface=*/nullptr, needs_gl);
}

ContextResult SharedImageStub::Initialize() {
  auto* channel_manager = channel_->gpu_channel_manager();
  DCHECK(!context_state_);

  if (gl::GetGLImplementation() != gl::kGLImplementationDisabled) {
    ContextResult result;
    context_state_ = channel_manager->GetSharedContextState(&result);
    if (result != ContextResult::kSuccess) {
      LOG(ERROR) << "SharedImageStub: unable to create context";
      context_state_ = nullptr;
      return result;
    }
    DCHECK(context_state_);
    DCHECK(!context_state_->context_lost());
    // Some shared image backing factories will use GL in ctor, so we need GL
    // even if chrome is using non-GL backing.
    if (!MakeContextCurrent(/*needs_gl=*/true)) {
      context_state_ = nullptr;
      return ContextResult::kTransientFailure;
    }
  }

  factory_ = std::make_unique<SharedImageFactory>(
      channel_manager->gpu_preferences(),
      channel_manager->gpu_driver_bug_workarounds(),
      channel_manager->gpu_feature_info(), context_state_.get(),
      channel_manager->shared_image_manager(), this,
      /*is_for_display_compositor=*/false);
  gpu_channel_shared_image_interface_ =
      base::MakeRefCounted<GpuChannelSharedImageInterface>(
          weak_factory_.GetWeakPtr());
  return ContextResult::kSuccess;
}

void SharedImageStub::OnError() {
  channel_->OnChannelError();
}

void SharedImageStub::TrackMemoryAllocatedChange(int64_t delta) {
  DCHECK(delta >= 0 || size_ >= static_cast<uint64_t>(-delta));
  uint64_t old_size = size_;
  size_ += delta;
  channel_->gpu_channel_manager()
      ->peak_memory_monitor()
      ->OnMemoryAllocatedChange(
          command_buffer_id_, old_size, size_,
          GpuPeakMemoryAllocationSource::SHARED_IMAGE_STUB);
}

uint64_t SharedImageStub::GetSize() const {
  return size_;
}

uint64_t SharedImageStub::ClientTracingId() const {
  return channel_->client_tracing_id();
}

int SharedImageStub::ClientId() const {
  return channel_->client_id();
}

uint64_t SharedImageStub::ContextGroupTracingId() const {
  return command_buffer_id_.GetUnsafeValue();
}

SharedImageStub::SharedImageDestructionCallback
SharedImageStub::GetSharedImageDestructionCallback(const Mailbox& mailbox) {
  return base::BindOnce(&SharedImageStub::DestroySharedImage,
                        weak_factory_.GetWeakPtr(), mailbox);
}

void SharedImageStub::DestroySharedImage(const Mailbox& mailbox,
                                         const SyncToken& sync_token) {
  // If there is no sync token, we don't need to wait.
  if (!sync_token.HasData()) {
    OnDestroySharedImage(mailbox);
    return;
  }

  auto done_cb = base::BindOnce(&SharedImageStub::OnDestroySharedImage,
                                weak_factory_.GetWeakPtr(), mailbox);
  channel_->scheduler()->ScheduleTask(
      gpu::Scheduler::Task(sequence_, std::move(done_cb),
                           std::vector<gpu::SyncToken>({sync_token})));
}

std::string SharedImageStub::GetLabel(const std::string& debug_label) const {
  // For cross process shared images, compose the label from the client pid for
  // easier identification in debug tools.
  return debug_label + "_Pid:" + base::NumberToString(channel_->client_pid());
}

}  // namespace gpu
