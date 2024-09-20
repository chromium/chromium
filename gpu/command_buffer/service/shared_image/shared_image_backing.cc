// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "third_party/skia/include/core/SkColorSpace.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {
namespace {

const char* BackingTypeToString(SharedImageBackingType type) {
  switch (type) {
    case SharedImageBackingType::kTest:
      return "TestImageBacking";
    case SharedImageBackingType::kExternalVkImage:
      return "ExternalVkImageBacking";
    case SharedImageBackingType::kD3D:
      return "D3DImageBacking";
    case SharedImageBackingType::kEGLImage:
      return "EGLImageBacking";
    case SharedImageBackingType::kAHardwareBuffer:
      return "AHardwareBufferImageBacking";
    case SharedImageBackingType::kAngleVulkan:
      return "AngleVulkanImageBacking";
    case SharedImageBackingType::kGLTexture:
      return "GLTextureImageBacking";
    case SharedImageBackingType::kOzone:
      return "OzoneImageBacking";
    case SharedImageBackingType::kRawDraw:
      return "RawDrawImageBacking";
    case SharedImageBackingType::kSharedMemory:
      return "SharedMemoryImageBacking";
    case SharedImageBackingType::kVideo:
      return "AndroidVideoImageBacking";
    case SharedImageBackingType::kWrappedSkImage:
      return "WrappedSkImage";
    case SharedImageBackingType::kCompound:
      return "CompoundImageBacking";
    case SharedImageBackingType::kDCOMPSurfaceProxy:
      return "DCOMPSurfaceProxy";
    case SharedImageBackingType::kIOSurface:
      return "IOSurface";
    case SharedImageBackingType::kDCompSurface:
      return "DCompSurface";
    case SharedImageBackingType::kDXGISwapChain:
      return "DXGISwapChain";
    case SharedImageBackingType::kWrappedGraphiteTexture:
      return "WrappedGraphiteTexture";
  }
  NOTREACHED();
}

}  // namespace

SharedImageBacking::SharedImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    SharedImageUsageSet usage,
    std::string debug_label,
    size_t estimated_size,
    bool is_thread_safe,
    std::optional<gfx::BufferUsage> buffer_usage)
    : mailbox_(mailbox),
      format_(format),
      size_(size),
      color_space_(color_space),
      surface_origin_(surface_origin),
      alpha_type_(alpha_type),
      usage_(usage),
      debug_label_(std::move(debug_label)),
      estimated_size_(estimated_size),
      buffer_usage_(std::move(buffer_usage)) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);

  if (is_thread_safe)
    lock_.emplace();
}

SharedImageBacking::~SharedImageBacking() = default;

void SharedImageBacking::OnContextLost() {
  AutoLock auto_lock(this);

  have_context_ = false;
}

SkImageInfo SharedImageBacking::AsSkImageInfo(int plane_index) const {
  gfx::Size plane_size = format_.GetPlaneSize(plane_index, size_);
  return SkImageInfo::Make(plane_size.width(), plane_size.height(),
                           viz::ToClosestSkColorType(
                               /*gpu_compositing=*/true, format(), plane_index),
                           alpha_type_, color_space_.ToSkColorSpace());
}

bool SharedImageBacking::CopyToGpuMemoryBuffer() {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void SharedImageBacking::CopyToGpuMemoryBufferAsync(
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(CopyToGpuMemoryBuffer());
}

void SharedImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {}

bool SharedImageBacking::UploadFromMemory(
    const std::vector<SkPixmap>& pixmaps) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool SharedImageBacking::ReadbackToMemory(
    const std::vector<SkPixmap>& pixmaps) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void SharedImageBacking::ReadbackToMemoryAsync(
    const std::vector<SkPixmap>& pixmaps,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(ReadbackToMemory(pixmaps));
}

bool SharedImageBacking::PresentSwapChain() {
  return false;
}

base::trace_event::MemoryAllocatorDump* SharedImageBacking::OnMemoryDump(
    const std::string& dump_name,
    base::trace_event::MemoryAllocatorDumpGuid client_guid,
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t client_tracing_id) {
  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  auto byte_size = GetEstimatedSizeForMemoryDump();
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  byte_size);

  dump->AddString("type", "", GetName());
  dump->AddString("dimensions", "", size().ToString());
  dump->AddString("format", "", format().ToString());
  dump->AddString("usage", "", CreateLabelForSharedImageUsage(usage()));
  dump->AddString("debug label", "", debug_label_);
  dump->AddScalar("purgeable", "bool", IsPurgeable());
#if BUILDFLAG(IS_CHROMEOS)
  dump->AddScalar("non_exo_size", "bool", IsImportedFromExo() ? 0 : byte_size);
#endif

  // Add ownership edge to `client_guid` which expresses shared ownership with
  // the client process.
  pmd->CreateSharedGlobalAllocatorDump(client_guid);
  pmd->AddOwnershipEdge(dump->guid(), client_guid,
                        static_cast<int>(gpu::TracingImportance::kNotOwner));

  return dump;
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedImageBacking::ProduceGLTexturePassthrough(SharedImageManager* manager,
                                                MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<SkiaImageRepresentation> SharedImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  // For testing if no context state, then default to SkiaGanesh representation.
  if (!context_state) {
    return ProduceSkiaGanesh(manager, tracker, context_state);
  }

  switch (context_state->gr_context_type()) {
    case gpu::GrContextType::kNone:
      // `kNone` signifies that the GPU process is being used only for WebGL via
      // SwiftShader. Skia is not initialized and should never be used in this
      // case but renderer/extension processes find out about software
      // compositing fallback asynchronously. They could issue GPU work before
      // finding out.
      // TODO(crbug.com/335279173): This would never be reached if clients found
      // out about compositing mode from the GPU process when they initialize a
      // GPU channel.
      return nullptr;
    case gpu::GrContextType::kGL:
    case gpu::GrContextType::kVulkan:
      return ProduceSkiaGanesh(manager, tracker, context_state);
    case gpu::GrContextType::kGraphiteMetal:
    case gpu::GrContextType::kGraphiteDawn:
      return ProduceSkiaGraphite(manager, tracker, context_state);
      // NOTE: Do not add a default case to force any new types to be
      // handled here on addition.
  }

  NOTREACHED();
}

std::unique_ptr<SkiaGaneshImageRepresentation>
SharedImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return nullptr;
}

std::unique_ptr<SkiaGraphiteImageRepresentation>
SharedImageBacking::ProduceSkiaGraphite(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  return nullptr;
}

std::unique_ptr<DawnImageRepresentation> SharedImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  return nullptr;
}

std::unique_ptr<DawnBufferRepresentation> SharedImageBacking::ProduceDawnBuffer(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    const wgpu::Device& device,
    wgpu::BackendType backend_type) {
  return nullptr;
}

std::unique_ptr<OverlayImageRepresentation> SharedImageBacking::ProduceOverlay(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<MemoryImageRepresentation> SharedImageBacking::ProduceMemory(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<RasterImageRepresentation> SharedImageBacking::ProduceRaster(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  return nullptr;
}

std::unique_ptr<VideoImageRepresentation> SharedImageBacking::ProduceVideo(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    VideoDevice device) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<VulkanImageRepresentation> SharedImageBacking::ProduceVulkan(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  return nullptr;
}
#endif

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<LegacyOverlayImageRepresentation>
SharedImageBacking::ProduceLegacyOverlay(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  return nullptr;
}
#endif

#if BUILDFLAG(IS_WIN)
void SharedImageBacking::UpdateExternalFence(
    scoped_refptr<gfx::D3DSharedFence> external_fence) {
  NOTIMPLEMENTED_LOG_ONCE();
}
#endif

void SharedImageBacking::UpdateEstimatedSize(size_t estimated_size_bytes) {
  if (estimated_size_bytes == estimated_size_)
    return;

  if (!refs_.empty()) {
    // Propagate the estimated size the memory tracker.
    auto* memory_tracker = refs_[0]->tracker();
    if (estimated_size_ < estimated_size_bytes) {
      memory_tracker->TrackMemAlloc(estimated_size_bytes - estimated_size_);
    } else {
      memory_tracker->TrackMemFree(estimated_size_ - estimated_size_bytes);
    }
  }

  estimated_size_ = estimated_size_bytes;
}

void SharedImageBacking::SetNotRefCounted() {
  DCHECK(!HasAnyRefs());
  is_ref_counted_ = false;
}

void SharedImageBacking::AddRef(SharedImageRepresentation* representation) {
  AutoLock auto_lock(this);
  DCHECK(is_ref_counted_);

  bool first_ref = refs_.empty();
  refs_.push_back(representation);

  if (first_ref) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
  }
}

void SharedImageBacking::ReleaseRef(SharedImageRepresentation* representation) {
  AutoLock auto_lock(this);
  DCHECK(is_ref_counted_);

  auto found = base::ranges::find(refs_, representation);
  CHECK(found != refs_.end(), base::NotFatalUntil::M130);

  // If the found representation is the first (owning) ref, free the attributed
  // memory.
  bool released_owning_ref = found == refs_.begin();
  if (released_owning_ref)
    (*found)->tracker()->TrackMemFree(estimated_size_);

  refs_.erase(found);

  if (!released_owning_ref)
    return;

  if (!refs_.empty()) {
    refs_[0]->tracker()->TrackMemAlloc(estimated_size_);
    return;
  }
}

const MemoryTracker* SharedImageBacking::GetMemoryTracker() const {
  AutoLock auto_lock(this);
  if (refs_.empty())
    return nullptr;

  return refs_[0]->tracker()->memory_tracker();
}

void SharedImageBacking::RegisterImageFactory(SharedImageFactory* factory) {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);
  DCHECK(!factory_);

  factory_ = factory;
}

void SharedImageBacking::UnregisterImageFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(factory_thread_checker_);

  factory_ = nullptr;
}

const char* SharedImageBacking::GetName() const {
  return BackingTypeToString(GetType());
}

bool SharedImageBacking::HasAnyRefs() const {
  AutoLock auto_lock(this);

  return !refs_.empty();
}

void SharedImageBacking::OnReadSucceeded() {
  AutoLock auto_lock(this);
  if (scoped_write_uma_) {
    scoped_write_uma_->SetConsumed();
    scoped_write_uma_.reset();
  }
}

void SharedImageBacking::OnWriteSucceeded() {
  AutoLock auto_lock(this);
  scoped_write_uma_.emplace();
}

size_t SharedImageBacking::GetEstimatedSize() const {
  AutoLock auto_lock(this);
  return estimated_size_;
}

size_t SharedImageBacking::GetEstimatedSizeForMemoryDump() const {
  AutoLock auto_lock(this);
  return estimated_size_;
}

bool SharedImageBacking::have_context() const {
  return have_context_;
}

SharedImageBacking::AutoLock::AutoLock(
    const SharedImageBacking* shared_image_backing)
    : auto_lock_(InitializeLock(shared_image_backing)) {}

SharedImageBacking::AutoLock::~AutoLock() = default;

base::Lock* SharedImageBacking::AutoLock::InitializeLock(
    const SharedImageBacking* shared_image_backing) {
  if (!shared_image_backing->lock_)
    return nullptr;

  return &shared_image_backing->lock_.value();
}

ClearTrackingSharedImageBacking::ClearTrackingSharedImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SharedImageUsageSet usage,
    std::string debug_label,
    size_t estimated_size,
    bool is_thread_safe,
    std::optional<gfx::BufferUsage> buffer_usage)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         surface_origin,
                         alpha_type,
                         usage,
                         std::move(debug_label),
                         estimated_size,
                         is_thread_safe,
                         std::move(buffer_usage)) {}

gfx::Rect ClearTrackingSharedImageBacking::ClearedRect() const {
  AutoLock auto_lock(this);
  return ClearedRectInternal();
}

void ClearTrackingSharedImageBacking::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);
}

gfx::Rect ClearTrackingSharedImageBacking::ClearedRectInternal() const {
  return cleared_rect_;
}

void ClearTrackingSharedImageBacking::SetClearedRectInternal(
    const gfx::Rect& cleared_rect) {
  cleared_rect_ = cleared_rect;
}

scoped_refptr<gfx::NativePixmap> SharedImageBacking::GetNativePixmap() {
  return nullptr;
}

gfx::GpuMemoryBufferHandle SharedImageBacking::GetGpuMemoryBufferHandle() {
  // Reaching here is invalid since this method should be only called for
  // backings which implements it,i.e., memory buffer handle should only be
  // retrieved from the backings which supports native buffer or shared memory.
  NOTREACHED_IN_MIGRATION();
  return gfx::GpuMemoryBufferHandle();
}

bool SharedImageBacking::IsPurgeable() const {
  return false;
}

bool SharedImageBacking::IsImportedFromExo() {
  return false;
}

}  // namespace gpu
