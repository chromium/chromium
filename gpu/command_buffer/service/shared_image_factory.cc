// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_factory.h"

#include <inttypes.h>

#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/wrapped_sk_image.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/trace_util.h"

#if defined(USE_X11) && BUILDFLAG(ENABLE_VULKAN)
#include "ui/base/ui_base_features.h"  // nogncheck
#endif

#if (defined(USE_X11) || defined(OS_FUCHSIA) || defined(OS_WIN)) && \
    BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/external_vk_image_factory.h"
#elif defined(OS_ANDROID) && BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/external_vk_image_factory.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#elif defined(OS_MAC)
#include "gpu/command_buffer/service/shared_image_backing_factory_iosurface.h"
#elif defined(OS_CHROMEOS)
#include "gpu/command_buffer/service/shared_image_backing_factory_ozone.h"
#endif

#if defined(OS_WIN)
#include "gpu/command_buffer/service/shared_image_backing_factory_d3d.h"
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
#include <lib/zx/channel.h>
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // defined(OS_FUCHSIA)

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "gpu/command_buffer/service/shared_image_backing_scoped_hardware_buffer_fence_sync.h"
#endif

namespace gpu {

// Overrides for flat_set lookups:
bool operator<(
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& lhs,
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& rhs) {
  return lhs->mailbox() < rhs->mailbox();
}

bool operator<(
    const Mailbox& lhs,
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& rhs) {
  return lhs < rhs->mailbox();
}

bool operator<(const std::unique_ptr<SharedImageRepresentationFactoryRef>& lhs,
               const Mailbox& rhs) {
  return lhs->mailbox() < rhs;
}

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SharedContextState* context_state,
    MailboxManager* mailbox_manager,
    SharedImageManager* shared_image_manager,
    ImageFactory* image_factory,
    MemoryTracker* memory_tracker,
    bool enable_wrapped_sk_image)
    : mailbox_manager_(mailbox_manager),
      shared_image_manager_(shared_image_manager),
      shared_context_state_(context_state),
      memory_tracker_(std::make_unique<MemoryTypeTracker>(memory_tracker)),
      gr_context_type_(context_state ? context_state->gr_context_type()
                                     : GrContextType::kGL) {
  bool use_gl = gl::GetGLImplementation() != gl::kGLImplementationNone;
  if (use_gl) {
    gl_backing_factory_ = std::make_unique<SharedImageBackingFactoryGLTexture>(
        gpu_preferences, workarounds, gpu_feature_info, image_factory,
        shared_image_manager->batch_access_manager(),
        shared_context_state_ ? shared_context_state_->progress_reporter()
                              : nullptr);
  }

  // TODO(ccameron): This block of code should be changed to a switch on
  // |gr_context_type|.
#if defined(USE_X11) && BUILDFLAG(ENABLE_VULKAN)
  if (!features::IsUsingOzonePlatform()) {
    if (gr_context_type_ == GrContextType::kVulkan) {
      interop_backing_factory_ =
          std::make_unique<ExternalVkImageFactory>(context_state);
    }
  } else if (gr_context_type_ == GrContextType::kVulkan) {
    LOG(ERROR) << "ERROR: gr_context_type_ is GrContextType::kVulkan and "
                  "interop_backing_factory_ is not set";
  }
#elif (defined(OS_FUCHSIA) || defined(OS_WIN)) && BUILDFLAG(ENABLE_VULKAN)
  if (gr_context_type_ == GrContextType::kVulkan) {
    interop_backing_factory_ =
        std::make_unique<ExternalVkImageFactory>(context_state);
  }
#elif defined(OS_ANDROID) && BUILDFLAG(ENABLE_VULKAN)
  // For Android
  if (gr_context_type_ == GrContextType::kVulkan) {
    external_vk_image_factory_ =
        std::make_unique<ExternalVkImageFactory>(context_state);
    const auto& enabled_extensions = context_state->vk_context_provider()
                                         ->GetDeviceQueue()
                                         ->enabled_extensions();
    if (gfx::HasExtension(
            enabled_extensions,
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
      interop_backing_factory_ = std::make_unique<SharedImageBackingFactoryAHB>(
          workarounds, gpu_feature_info);
    }
  } else {
    interop_backing_factory_ = std::make_unique<SharedImageBackingFactoryAHB>(
        workarounds, gpu_feature_info);
  }
#elif defined(OS_MAC)
  // OSX
  DCHECK(gr_context_type_ == GrContextType::kGL ||
         gr_context_type_ == GrContextType::kMetal);
#elif defined(OS_CHROMEOS)
  if (gr_context_type_ == GrContextType::kVulkan) {
    interop_backing_factory_ =
        std::make_unique<SharedImageBackingFactoryOzone>(context_state);
  }
#else
  // Others
  if (gr_context_type_ == GrContextType::kVulkan) {
    LOG(ERROR) << "ERROR: gr_context_type_ is GrContextType::kVulkan and "
                  "interop_backing_factory_ is not set";
  }
#endif
  if (enable_wrapped_sk_image && context_state) {
    wrapped_sk_image_factory_ =
        std::make_unique<raster::WrappedSkImageFactory>(context_state);
  }

#if defined(OS_WIN)
  // For Windows
  bool use_passthrough = gpu_preferences.use_passthrough_cmd_decoder &&
                         gles2::PassthroughCommandDecoderSupported();
  if (use_passthrough && gr_context_type_ == GrContextType::kGL) {
    // Only supported for passthrough command decoder.
    interop_backing_factory_ = std::make_unique<SharedImageBackingFactoryD3D>();
  }
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
  vulkan_context_provider_ = context_state->vk_context_provider();
#endif  // OS_FUCHSIA
}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(shared_images_.empty());
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           gpu::SurfaceHandle surface_handle,
                                           uint32_t usage) {
  bool allow_legacy_mailbox = false;
  auto* factory = GetFactoryByUsage(usage, format, &allow_legacy_mailbox);
  if (!factory)
    return false;
  auto backing = factory->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, IsSharedBetweenThreads(usage));
  return RegisterBacking(std::move(backing), allow_legacy_mailbox);
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::ResourceFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage,
                                           base::span<const uint8_t> data) {
  // For now, restrict this to SHARED_IMAGE_USAGE_DISPLAY with optional
  // SHARED_IMAGE_USAGE_SCANOUT.
  // TODO(ericrk): SCANOUT support for Vulkan by ensuring all interop factories
  // support this, and allowing them to be chosen here.
  constexpr uint32_t allowed_usage =
      SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_SCANOUT;
  if (usage & ~allowed_usage) {
    LOG(ERROR) << "Unsupported usage for SharedImage with initial data upload.";
    return false;
  }

  // Currently we only perform data uploads via two paths,
  // |gl_backing_factory_| for GL and |wrapped_sk_image_factory_| for Vulkan and
  // Dawn.
  // TODO(ericrk): Make this generic in the future.
  bool allow_legacy_mailbox = false;
  SharedImageBackingFactory* factory = nullptr;
  if (backing_factory_for_testing_) {
    factory = backing_factory_for_testing_;
  } else if (gr_context_type_ == GrContextType::kGL) {
    allow_legacy_mailbox = true;
    factory = gl_backing_factory_.get();
  } else {
    factory = wrapped_sk_image_factory_.get();
  }
  if (!factory)
    return false;
  auto backing =
      factory->CreateSharedImage(mailbox, format, size, color_space,
                                 surface_origin, alpha_type, usage, data);
  if (backing)
    backing->OnWriteSucceeded();
  return RegisterBacking(std::move(backing), allow_legacy_mailbox);
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           int client_id,
                                           gfx::GpuMemoryBufferHandle handle,
                                           gfx::BufferFormat format,
                                           SurfaceHandle surface_handle,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage) {
  // TODO(piman): depending on handle.type, choose platform-specific backing
  // factory, e.g. SharedImageBackingFactoryAHB.
  bool allow_legacy_mailbox = false;
  auto resource_format = viz::GetResourceFormat(format);
  auto* factory = GetFactoryByUsage(usage, resource_format,
                                    &allow_legacy_mailbox, handle.type);
  if (!factory)
    return false;
  auto backing = factory->CreateSharedImage(
      mailbox, client_id, std::move(handle), format, surface_handle, size,
      color_space, surface_origin, alpha_type, usage);
  if (backing)
    backing->OnWriteSucceeded();
  return RegisterBacking(std::move(backing), allow_legacy_mailbox);
}

bool SharedImageFactory::UpdateSharedImage(const Mailbox& mailbox) {
  return UpdateSharedImage(mailbox, nullptr);
}

bool SharedImageFactory::UpdateSharedImage(
    const Mailbox& mailbox,
    std::unique_ptr<gfx::GpuFence> in_fence) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    LOG(ERROR) << "UpdateSharedImage: Could not find shared image mailbox";
    return false;
  }
  (*it)->Update(std::move(in_fence));
  return true;
}

bool SharedImageFactory::DestroySharedImage(const Mailbox& mailbox) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    LOG(ERROR) << "DestroySharedImage: Could not find shared image mailbox";
    return false;
  }
  shared_images_.erase(it);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
  if (!have_context) {
    for (auto& shared_image : shared_images_)
      shared_image->OnContextLost();
  }
  shared_images_.clear();
}

#if defined(OS_WIN)
bool SharedImageFactory::CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                         const Mailbox& back_buffer_mailbox,
                                         viz::ResourceFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         GrSurfaceOrigin surface_origin,
                                         SkAlphaType alpha_type,
                                         uint32_t usage) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return false;

  SharedImageBackingFactoryD3D* d3d_backing_factory =
      static_cast<SharedImageBackingFactoryD3D*>(
          interop_backing_factory_.get());
  bool allow_legacy_mailbox = true;
  auto backings = d3d_backing_factory->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      surface_origin, alpha_type, usage);
  return RegisterBacking(std::move(backings.front_buffer),
                         allow_legacy_mailbox) &&
         RegisterBacking(std::move(backings.back_buffer), allow_legacy_mailbox);
}

bool SharedImageFactory::PresentSwapChain(const Mailbox& mailbox) {
  if (!SharedImageBackingFactoryD3D::IsSwapChainSupported())
    return false;
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    DLOG(ERROR) << "PresentSwapChain: Could not find shared image mailbox";
    return false;
  }
  (*it)->PresentSwapChain();
  return true;
}
#endif  // OS_WIN

#if defined(OS_FUCHSIA)
bool SharedImageFactory::RegisterSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  decltype(buffer_collections_)::iterator it;
  bool inserted;
  std::tie(it, inserted) =
      buffer_collections_.insert(std::make_pair(id, nullptr));

  if (!inserted) {
    DLOG(ERROR) << "RegisterSysmemBufferCollection: Could not register the "
                   "same buffer collection twice.";
    return false;
  }

  // If we don't have Vulkan then just drop the token. Sysmem will inform the
  // caller about the issue. The empty entry is kept in buffer_collections_, so
  // the caller can still call ReleaseSysmemBufferCollection().
  if (!vulkan_context_provider_)
    return true;

  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();
  DCHECK(device != VK_NULL_HANDLE);
  it->second = vulkan_context_provider_->GetVulkanImplementation()
                   ->RegisterSysmemBufferCollection(
                       device, id, std::move(token), format, usage, gfx::Size(),
                       0, register_with_image_pipe);

  return true;
}

bool SharedImageFactory::ReleaseSysmemBufferCollection(
    gfx::SysmemBufferCollectionId id) {
  auto removed = buffer_collections_.erase(id);
  return removed > 0;
}
#endif  // defined(OS_FUCHSIA)

// TODO(ericrk): Move this entirely to SharedImageManager.
bool SharedImageFactory::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd,
    int client_id,
    uint64_t client_tracing_id) {
  for (const auto& shared_image : shared_images_) {
    shared_image_manager_->OnMemoryDump(shared_image->mailbox(), pmd, client_id,
                                        client_tracing_id);
  }

  return true;
}

#if defined(OS_ANDROID)
bool SharedImageFactory::CreateSharedImageWithAHB(const Mailbox& out_mailbox,
                                                  const Mailbox& in_mailbox,
                                                  uint32_t usage) {
  auto it = shared_images_.find(in_mailbox);
  if (it == shared_images_.end()) {
    LOG(ERROR)
        << "CreateSharedImageWithAHB: Could not find shared image mailbox";
    return false;
  }
  auto ahb = (*it)->GetAHardwareBuffer();
  if (!ahb) {
    LOG(ERROR) << "CreateSharedImageWithAHB: AHardwareBuffer is null";
    return false;
  }
  auto backing =
      std::make_unique<SharedImageBackingScopedHardwareBufferFenceSync>(
          std::move(ahb), out_mailbox, (*it)->format(), (*it)->size(),
          (*it)->color_space(), (*it)->surface_origin(), (*it)->alpha_type(),
          usage, false);
  return RegisterBacking(std::move(backing), false /* allow_legacy_mailbox */);
}
#endif

void SharedImageFactory::RegisterSharedImageBackingFactoryForTesting(
    SharedImageBackingFactory* factory) {
  backing_factory_for_testing_ = factory;
}

bool SharedImageFactory::IsSharedBetweenThreads(uint32_t usage) {
  // If |shared_image_manager_| is thread safe, it means the display is running
  // on a separate thread (which uses a separate GL context or VkDeviceQueue).
  return shared_image_manager_->display_context_on_another_thread() &&
         (usage & SHARED_IMAGE_USAGE_DISPLAY);
}

bool SharedImageFactory::CanUseWrappedSkImage(uint32_t usage) const {
  if (!wrapped_sk_image_factory_)
    return false;

  constexpr auto kWrappedSkImageUsage = SHARED_IMAGE_USAGE_RASTER |
                                        SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
                                        SHARED_IMAGE_USAGE_DISPLAY;

  if (gr_context_type_ != GrContextType::kGL) {
    // For SkiaRenderer/Vulkan+Dawn use WrappedSkImage if the usage is only
    // raster and/or display.
    return (usage & kWrappedSkImageUsage) && !(usage & ~kWrappedSkImageUsage);
  } else {
    // For d SkiaRenderer/GL only use WrappedSkImages for OOP-R because
    // CopySubTexture() doesn't use Skia. https://crbug.com/984045
    return usage == kWrappedSkImageUsage;
  }
}

SharedImageBackingFactory* SharedImageFactory::GetFactoryByUsage(
    uint32_t usage,
    viz::ResourceFormat format,
    bool* allow_legacy_mailbox,
    gfx::GpuMemoryBufferType gmb_type) {
  if (backing_factory_for_testing_)
    return backing_factory_for_testing_;

  bool using_dawn = usage & SHARED_IMAGE_USAGE_WEBGPU;
  bool vulkan_usage = gr_context_type_ == GrContextType::kVulkan &&
                      (usage & SHARED_IMAGE_USAGE_DISPLAY);
  bool gl_usage = usage & SHARED_IMAGE_USAGE_GLES2;
  bool share_between_threads = IsSharedBetweenThreads(usage);
  bool share_between_gl_vulkan = gl_usage && vulkan_usage;
  bool using_interop_factory = share_between_gl_vulkan || using_dawn ||
                               (usage & SHARED_IMAGE_USAGE_VIDEO_DECODE) ||
                               (share_between_threads && vulkan_usage);

#if defined(OS_ANDROID)
  // Scanout on Android requires explicit fence synchronization which is only
  // supported by the interop factory.
  using_interop_factory |= usage & SHARED_IMAGE_USAGE_SCANOUT;
#elif defined(OS_MAC)
  // On macOS, there is no separate interop factory. Any GpuMemoryBuffer-backed
  // image can be used with both OpenGL and Metal.
  using_interop_factory = false;
#endif

  bool using_wrapped_sk_image = !using_interop_factory &&
                                !share_between_threads &&
                                CanUseWrappedSkImage(usage);
  if (using_wrapped_sk_image) {
    if (gmb_type == gfx::EMPTY_BUFFER ||
        wrapped_sk_image_factory_->CanImportGpuMemoryBuffer(gmb_type)) {
      *allow_legacy_mailbox = false;
      return wrapped_sk_image_factory_.get();
    }
  }

  using_interop_factory |= vulkan_usage;

  if (gmb_type != gfx::EMPTY_BUFFER) {
    bool interop_factory_supports_gmb =
        interop_backing_factory_ &&
        interop_backing_factory_->CanImportGpuMemoryBuffer(gmb_type);

    if (using_interop_factory && !interop_backing_factory_) {
      LOG(ERROR) << "Unable to screate SharedImage backing: no support for the "
                    "requested GpuMemoryBufferType.";
      return nullptr;
    }

    // If |interop_backing_factory_| supports supplied GMB type then use it
    // instead of |gl_backing_factory_|.
    using_interop_factory |= interop_factory_supports_gmb;
  }

  *allow_legacy_mailbox = !using_interop_factory &&
                          gr_context_type_ == GrContextType::kGL &&
                          !share_between_threads;

  if (using_interop_factory) {
    // TODO(crbug.com/969114): Not all shared image factory implementations
    // support concurrent read/write usage.
    if (usage & SHARED_IMAGE_USAGE_CONCURRENT_READ_WRITE) {
      LOG(ERROR) << "Unable to create SharedImage backing: Interoperability is "
                    "not supported for concurrent read/write usage";
      return nullptr;
    }

#if defined(OS_ANDROID)
    // On android, we sometime choose VkImage based backing factory as an
    // interop if the format is not supported by the AHB backing factory.
    auto* ahb_backing_factory = static_cast<SharedImageBackingFactoryAHB*>(
        interop_backing_factory_.get());
    if (ahb_backing_factory && ahb_backing_factory->IsFormatSupported(format))
      return ahb_backing_factory;
    if (share_between_threads) {
      LOG(FATAL) << "ExternalVkImageFactory currently do not support "
                    "cross-thread usage.";
    }
    *allow_legacy_mailbox = false;
    return external_vk_image_factory_.get();
#else   // defined(OS_ANDROID)
    LOG_IF(ERROR, !interop_backing_factory_)
        << "Unable to create SharedImage backing: GL / Vulkan interoperability "
           "is not supported on this platform";

    return interop_backing_factory_.get();
#endif  // !defined(OS_ANDROID)
  }

  return gl_backing_factory_.get();
}

bool SharedImageFactory::RegisterBacking(
    std::unique_ptr<SharedImageBacking> backing,
    bool allow_legacy_mailbox) {
  if (!backing) {
    LOG(ERROR) << "CreateSharedImage: could not create backing.";
    return false;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_tracker_.get());

  if (!shared_image) {
    LOG(ERROR) << "CreateSharedImage: could not register backing.";
    return false;
  }

  shared_image->RegisterImageFactory(this);

  // TODO(ericrk): Remove this once no legacy cases remain.
  if (allow_legacy_mailbox &&
      !shared_image->ProduceLegacyMailbox(mailbox_manager_)) {
    LOG(ERROR) << "CreateSharedImage: could not convert shared_image to legacy "
                  "mailbox.";
    return false;
  }

  shared_images_.emplace(std::move(shared_image));
  return true;
}

SharedImageRepresentationFactory::SharedImageRepresentationFactory(
    SharedImageManager* manager,
    MemoryTracker* tracker)
    : manager_(manager),
      tracker_(std::make_unique<MemoryTypeTracker>(tracker)) {}

SharedImageRepresentationFactory::~SharedImageRepresentationFactory() {
  DCHECK_EQ(0u, tracker_->GetMemRepresented());
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageRepresentationFactory::ProduceGLTexture(const Mailbox& mailbox) {
  return manager_->ProduceGLTexture(mailbox, tracker_.get());
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageRepresentationFactory::ProduceRGBEmulationGLTexture(
    const Mailbox& mailbox) {
  return manager_->ProduceRGBEmulationGLTexture(mailbox, tracker_.get());
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageRepresentationFactory::ProduceGLTexturePassthrough(
    const Mailbox& mailbox) {
  return manager_->ProduceGLTexturePassthrough(mailbox, tracker_.get());
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageRepresentationFactory::ProduceSkia(
    const Mailbox& mailbox,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceSkia(mailbox, tracker_.get(), context_state);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageRepresentationFactory::ProduceDawn(const Mailbox& mailbox,
                                              WGPUDevice device) {
  return manager_->ProduceDawn(mailbox, tracker_.get(), device);
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageRepresentationFactory::ProduceOverlay(const gpu::Mailbox& mailbox) {
  return manager_->ProduceOverlay(mailbox, tracker_.get());
}

}  // namespace gpu
