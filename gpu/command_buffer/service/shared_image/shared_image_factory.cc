// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include <inttypes.h>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if BUILDFLAG(IS_OZONE)
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/dcomp_image_backing_factory.h"
#include "ui/gl/direct_composition_support.h"
#include "ui/gl/gl_angle_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if defined(USE_EGL)
#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"
#include "ui/gl/gl_display.h"
#endif  // defined(USE_EGL)

namespace gpu {

namespace {

#if BUILDFLAG(IS_WIN)
// Only allow shmem overlays for NV12 on Windows.
constexpr bool kAllowShmOverlays = true;
#else
constexpr bool kAllowShmOverlays = false;
#endif

const char* GmbTypeToString(gfx::GpuMemoryBufferType type) {
  switch (type) {
    case gfx::EMPTY_BUFFER:
      return "empty";
    case gfx::SHARED_MEMORY_BUFFER:
      return "shared_memory";
    case gfx::IO_SURFACE_BUFFER:
    case gfx::NATIVE_PIXMAP:
    case gfx::DXGI_SHARED_HANDLE:
    case gfx::ANDROID_HARDWARE_BUFFER:
      return "platform";
  }
  NOTREACHED();
}

#if defined(USE_OZONE)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum FormatPixmapSupport { kNone = 0, kNV12 = 1, kYV12 = 2, kMaxValue = kYV12 };

// Return the supported format in order of fallback support.
FormatPixmapSupport GetFormatPixmapSupport(
    std::vector<gfx::BufferFormat> supported_formats) {
  FormatPixmapSupport val = FormatPixmapSupport::kNone;
  for (auto format : supported_formats) {
    if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
      val = FormatPixmapSupport::kNV12;
      break;
    } else if (format == gfx::BufferFormat::YVU_420) {
      val = FormatPixmapSupport::kYV12;
    }
  }
  return val;
}

// Set bool only once as formats supported on platform don't change on factory
// creation.
bool set_format_supported_metric = false;
#endif

void RecordIsNewMultiplanarFormat(bool is_multiplanar) {
  base::UmaHistogramBoolean("GPU.SharedImage.IsNewMultiplanarFormat",
                            is_multiplanar);
}

}  // namespace

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
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    bool is_for_display_compositor)
    : shared_image_manager_(shared_image_manager),
      shared_context_state_(context_state),
      memory_tracker_(std::make_unique<MemoryTypeTracker>(memory_tracker)),
      is_for_display_compositor_(is_for_display_compositor),
      gr_context_type_(context_state ? context_state->gr_context_type()
                                     : GrContextType::kGL) {
#if defined(USE_OZONE)
  if (!set_format_supported_metric) {
    bool is_pixmap_supported = ui::OzonePlatform::GetInstance()
                                   ->GetPlatformRuntimeProperties()
                                   .supports_native_pixmaps;
    // Only log histogram for formats that are used with real GMBs containing
    // native pixmap.
    if (is_pixmap_supported) {
      auto* factory =
          ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
      if (factory) {
        // Get all formats that are supported by platform.
        auto supported_formats = factory->GetSupportedFormatsForTexturing();
        auto format = GetFormatPixmapSupport(supported_formats);
        base::UmaHistogramEnumeration("GPU.SharedImage.FormatPixmapSupport",
                                      format);
      }
    }
    set_format_supported_metric = true;
  }
#endif

  auto shared_memory_backing_factory =
      std::make_unique<SharedMemoryImageBackingFactory>();
  factories_.push_back(std::move(shared_memory_backing_factory));

  // if GL is disabled, it only needs SharedMemoryImageBackingFactory.
  if (gl::GetGLImplementation() == gl::kGLImplementationDisabled) {
    return;
  }

  scoped_refptr<gles2::FeatureInfo> feature_info;
  if (shared_context_state_) {
    feature_info = shared_context_state_->feature_info();
  }

  if (!feature_info) {
    // For some unit tests like SharedImageFactoryTest, |shared_context_state_|
    // could be nullptr.
    bool use_passthrough = gpu_preferences.use_passthrough_cmd_decoder &&
                           gles2::PassthroughCommandDecoderSupported();
    feature_info = new gles2::FeatureInfo(workarounds, gpu_feature_info);
    feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                             use_passthrough, gles2::DisallowedFeatures());
  }

  if (context_state) {
    auto wrapped_sk_image_factory =
        std::make_unique<WrappedSkImageBackingFactory>(context_state);
    factories_.push_back(std::move(wrapped_sk_image_factory));
  }

  if (features::IsUsingRawDraw() && context_state) {
    auto factory = std::make_unique<RawDrawImageBackingFactory>();
    factories_.push_back(std::move(factory));
  }

  bool use_gl =
      gl::GetGLImplementation() != gl::kGLImplementationNone &&
      (!is_for_display_compositor_ || gr_context_type_ == GrContextType::kGL);
  if (use_gl) {
    auto gl_texture_backing_factory =
        std::make_unique<GLTextureImageBackingFactory>(
            gpu_preferences, workarounds, feature_info.get(),
            shared_context_state_ ? shared_context_state_->progress_reporter()
                                  : nullptr,
            /*for_cpu_upload_usage=*/false);
    factories_.push_back(std::move(gl_texture_backing_factory));
  }

#if BUILDFLAG(IS_WIN)
  if (gl::DirectCompositionSupported()) {
    factories_.push_back(std::make_unique<DCompImageBackingFactory>());
  }
  if (D3DImageBackingFactory::IsD3DSharedImageSupported(gpu_preferences)) {
    // TODO(sunnyps): Should we get the device from SharedContextState instead?
    auto d3d_factory = std::make_unique<D3DImageBackingFactory>(
        gl::QueryD3D11DeviceObjectFromANGLE(),
        shared_image_manager_->dxgi_shared_handle_manager());
    d3d_backing_factory_ = d3d_factory.get();
    factories_.push_back(std::move(d3d_factory));
  }
#endif  // BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_ANDROID)
  if (use_gl) {
    auto gl_texture_backing_factory =
        std::make_unique<GLTextureImageBackingFactory>(
            gpu_preferences, workarounds, feature_info.get(),
            shared_context_state_ ? shared_context_state_->progress_reporter()
                                  : nullptr,
            /*for_cpu_upload_usage=*/true);
    factories_.push_back(std::move(gl_texture_backing_factory));
  }
#endif

#if BUILDFLAG(ENABLE_VULKAN)
  // If Chrome and ANGLE are sharing the same vulkan device queue, AngleVulkan
  // backing will be used for interop.
  if ((gr_context_type_ == GrContextType::kVulkan) &&
      (base::FeatureList::IsEnabled(features::kVulkanFromANGLE))) {
    auto factory = std::make_unique<AngleVulkanImageBackingFactory>(
        gpu_preferences, workarounds, context_state);
    factories_.push_back(std::move(factory));
  }

#if BUILDFLAG(IS_WIN)
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_VULKAN)

#if defined(USE_EGL)
  // Create EGLImageBackingFactory if egl images are supported. Note that the
  // factory creation is kept here to preserve the current preference of factory
  // to be used.
  auto* egl_display = gl::GetDefaultDisplayEGL();
  if (use_gl && egl_display && egl_display->ext->b_EGL_KHR_image_base &&
      egl_display->ext->b_EGL_KHR_gl_texture_2D_image &&
      egl_display->ext->b_EGL_KHR_fence_sync &&
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image) {
    auto egl_backing_factory = std::make_unique<EGLImageBackingFactory>(
        gpu_preferences, workarounds, feature_info.get());
    factories_.push_back(std::move(egl_backing_factory));
  }
#endif  // defined(USE_EGL)

#if BUILDFLAG(IS_ANDROID)
  bool is_ahb_supported =
      base::AndroidHardwareBufferCompat::IsSupportAvailable();
  if (gr_context_type_ == GrContextType::kVulkan) {
    const auto& enabled_extensions = context_state->vk_context_provider()
                                         ->GetDeviceQueue()
                                         ->enabled_extensions();
    is_ahb_supported =
        is_ahb_supported &&
        gfx::HasExtension(
            enabled_extensions,
            VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
  }
  if (is_ahb_supported) {
    auto ahb_factory = std::make_unique<AHardwareBufferImageBackingFactory>(
        feature_info.get(), gpu_preferences);
    factories_.push_back(std::move(ahb_factory));
  }
  if (gr_context_type_ == GrContextType::kVulkan &&
      !base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#elif BUILDFLAG(IS_OZONE)
  // For all Ozone platforms - Desktop Linux, ChromeOS, Fuchsia, CastOS.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_native_pixmaps) {
    auto ozone_factory = std::make_unique<OzoneImageBackingFactory>(
        context_state, workarounds, gpu_preferences);
    factories_.push_back(std::move(ozone_factory));
  }
#if BUILDFLAG(ENABLE_VULKAN)
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
#if BUILDFLAG(IS_FUCHSIA)
    vulkan_context_provider_ = context_state->vk_context_provider();
#endif  // BUILDFLAG(IS_FUCHSIA)
  }
#endif  // BUILDFLAG(ENABLE_VULKAN)
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_APPLE)
  {
    // For some unit tests like SharedImageFactoryTest, |shared_context_state_|
    // could be nullptr.
    int32_t max_texture_size = shared_context_state_
                                   ? shared_context_state_->GetMaxTextureSize()
                                   : 8192;
    auto* progress_reporter = shared_context_state_
                                  ? shared_context_state_->progress_reporter()
                                  : nullptr;
    auto iosurface_backing_factory =
        std::make_unique<IOSurfaceImageBackingFactory>(
            gpu_preferences.gr_context_type, max_texture_size,
            feature_info.get(), progress_reporter);
    factories_.push_back(std::move(iosurface_backing_factory));
  }
#endif
}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(shared_images_.empty());
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::SharedImageFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           gpu::SurfaceHandle surface_handle,
                                           uint32_t usage,
                                           std::string debug_label) {
  auto* factory = GetFactoryByUsage(usage, format, size,
                                    /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
  if (!factory) {
    LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER, debug_label);
    return false;
  }

  auto backing = factory->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, std::move(debug_label), IsSharedBetweenThreads(usage));

  if (backing) {
    DVLOG(1) << "CreateSharedImage[" << backing->GetName()
             << "] size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " format=" << format.ToString();
  }
  return RegisterBacking(std::move(backing));
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::SharedImageFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage,
                                           std::string debug_label,
                                           base::span<const uint8_t> data) {
  if (!format.is_single_plane()) {
    // Pixel upload path only supports single-planar formats.
    LOG(ERROR) << "Invalid format " << format.ToString();
    return false;
  }

  SharedImageBackingFactory* factory = nullptr;
  if (backing_factory_for_testing_) {
    factory = backing_factory_for_testing_;
  } else {
    factory = GetFactoryByUsage(usage, format, size, data, gfx::EMPTY_BUFFER);
  }

  if (!factory) {
    LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER, debug_label);
    return false;
  }

  auto backing = factory->CreateSharedImage(mailbox, format, size, color_space,
                                            surface_origin, alpha_type, usage,
                                            std::move(debug_label), data);
  if (backing) {
    DVLOG(1) << "CreateSharedImagePixels[" << backing->GetName()
             << "] with pixels size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " format=" << format.ToString();

    backing->OnWriteSucceeded();
  }
  return RegisterBacking(std::move(backing));
}

bool SharedImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  if (format.IsLegacyMultiplanar()) {
    // Use this for multi-planar and real single-planar formats. All legacy
    // multi-planar GMBs must go through CreateSharedImage() that takes
    // BufferPlane parameter.
    LOG(ERROR) << "Invalid format " << format.ToString();
    return false;
  }

  if (!viz::HasEquivalentBufferFormat(format)) {
    // Client GMB code still operates on BufferFormat so the SharedImageFormat
    // received here must have an equivalent BufferFormat.
    LOG(ERROR) << "Invalid format " << format.ToString();
    return false;
  }

  // Log UMA for multiplanar shared image formats.
  if (format.is_multi_plane()) {
    RecordIsNewMultiplanarFormat(/*is_multiplanar*/ true);
  }

  gfx::GpuMemoryBufferType gmb_type = buffer_handle.type;

  bool use_compound = false;
  SharedImageBackingFactory* factory =
      GetFactoryByUsage(usage, format, size, {}, gmb_type);

  if (!factory && gmb_type == gfx::SHARED_MEMORY_BUFFER &&
      !IsSharedBetweenThreads(usage)) {
    // Check if CompoundImageBacking can hold shared memory buffer plus
    // another GPU backing type to satisfy requirements.
    if (CompoundImageBacking::IsValidSharedMemoryBufferFormat(size, format)) {
      factory = GetFactoryByUsage(usage | SHARED_IMAGE_USAGE_CPU_UPLOAD, format,
                                  size, /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
      use_compound = factory != nullptr;
    }
  }

  if (!factory) {
    LogGetFactoryFailed(usage, format, gmb_type, debug_label);
    return false;
  }

  std::unique_ptr<SharedImageBacking> backing;
  if (use_compound) {
    backing = CompoundImageBacking::CreateSharedMemory(
        factory, kAllowShmOverlays, mailbox, std::move(buffer_handle), format,
        size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label));
  } else {
    backing = factory->CreateSharedImage(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label), std::move(buffer_handle));
  }

  if (backing) {
    DVLOG(1) << "CreateSharedImageWithBuffer[" << backing->GetName()
             << "] size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " format=" << format.ToString()
             << " gmb_type=" << GmbTypeToString(gmb_type);

    backing->OnWriteSucceeded();
  }
  return RegisterBacking(std::move(backing));
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           gfx::GpuMemoryBufferHandle handle,
                                           gfx::BufferFormat format,
                                           gfx::BufferPlane plane,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage,
                                           std::string debug_label) {
  auto si_format = viz::GetSharedImageFormat(format);
  gfx::GpuMemoryBufferType gmb_type = handle.type;

  // Log UMA for multiplanar shared image formats.
  if (si_format.IsLegacyMultiplanar()) {
    RecordIsNewMultiplanarFormat(/*is_multiplanar*/ false);
  }

  bool use_compound = false;
  auto* factory = GetFactoryByUsage(usage, si_format, size,
                                    /*pixel_data=*/{}, gmb_type);

  if (!factory && gmb_type == gfx::SHARED_MEMORY_BUFFER &&
      !IsSharedBetweenThreads(usage)) {
    // Check if CompoundImageBacking can hold shared memory buffer plus
    // another GPU backing type to satisfy requirements.
    if (CompoundImageBacking::IsValidSharedMemoryBufferFormat(size, format,
                                                              plane)) {
      // For shared memory backed compound backings, we need to check if the
      // corresponding GPU backing can support the format and size for the given
      // plane rather than the original GMB format and size.
      const auto plane_format =
          viz::GetSharedImageFormat(GetPlaneBufferFormat(plane, format));
      const gfx::Size plane_size = GetPlaneSize(plane, size);
      factory =
          GetFactoryByUsage(usage | SHARED_IMAGE_USAGE_CPU_UPLOAD, plane_format,
                            plane_size, /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
      use_compound = factory != nullptr;
    }
  }

  if (!factory) {
    LogGetFactoryFailed(usage, si_format, gmb_type, debug_label);
    return false;
  }

  std::unique_ptr<SharedImageBacking> backing;
  if (use_compound) {
    backing = CompoundImageBacking::CreateSharedMemory(
        factory, kAllowShmOverlays, mailbox, std::move(handle), format, plane,
        size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label));
  } else {
    backing = factory->CreateSharedImage(
        mailbox, std::move(handle), format, plane, size, color_space,
        surface_origin, alpha_type, usage, std::move(debug_label));
  }

  if (backing) {
    DVLOG(1) << "CreateSharedImage[" << backing->GetName()
             << "] from handle size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " buffer_format=" << gfx::BufferFormatToString(format)
             << " gmb_type=" << GmbTypeToString(gmb_type);

    backing->OnWriteSucceeded();
  }
  return RegisterBacking(std::move(backing));
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

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                         const Mailbox& back_buffer_mailbox,
                                         viz::SharedImageFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         GrSurfaceOrigin surface_origin,
                                         SkAlphaType alpha_type,
                                         uint32_t usage) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
    return false;

  auto backings = d3d_backing_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      surface_origin, alpha_type, usage);
  return RegisterBacking(std::move(backings.front_buffer)) &&
         RegisterBacking(std::move(backings.back_buffer));
}

bool SharedImageFactory::PresentSwapChain(const Mailbox& mailbox) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
    return false;
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    DLOG(ERROR) << "PresentSwapChain: Could not find shared image mailbox";
    return false;
  }
  (*it)->PresentSwapChain();
  return true;
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
void SharedImageFactory::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  VkDevice device =
      vulkan_context_provider_->GetDeviceQueue()->GetVulkanDevice();
  DCHECK(device != VK_NULL_HANDLE);
  vulkan_context_provider_->GetVulkanImplementation()
      ->RegisterSysmemBufferCollection(
          device, std::move(service_handle), std::move(sysmem_token), format,
          usage, gfx::Size(), 0, register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CopyToGpuMemoryBuffer(const Mailbox& mailbox) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    DLOG(ERROR) << "UpdateSharedImage: Could not find shared image mailbox";
    return false;
  }
  return (*it)->CopyToGpuMemoryBuffer();
}
#endif

void SharedImageFactory::RegisterSharedImageBackingFactoryForTesting(
    SharedImageBackingFactory* factory) {
  backing_factory_for_testing_ = factory;
}

bool SharedImageFactory::IsSharedBetweenThreads(uint32_t usage) {
  // Ignore for mipmap usage.
  usage &= ~SHARED_IMAGE_USAGE_MIPMAP;
  // Ignore for delegated compositing.
  usage &= ~SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING;

  // Raw Draw backings will be write accessed on the GPU main thread, and
  // be read accessed on the compositor thread.
  if (usage & SHARED_IMAGE_USAGE_RAW_DRAW)
    return true;

  // DISPLAY is for gpu composition and SCANOUT for overlays.
  constexpr int kDisplayCompositorUsage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                                          SHARED_IMAGE_USAGE_DISPLAY_WRITE |
                                          SHARED_IMAGE_USAGE_SCANOUT;

  // Image is used on display compositor gpu thread if it's used by display
  // compositor and if display compositor runs on a separate thread. Image is
  // used by display compositor if it has kDisplayCompositorUsage or is being
  // created by display compositor.
  const bool used_by_display_compositor_gpu_thread =
      ((usage & kDisplayCompositorUsage) || is_for_display_compositor_) &&
      shared_image_manager_->display_context_on_another_thread();

  // If it has usage other than kDisplayCompositorUsage OR if it is not created
  // by display compositor, it means that it is used by the gpu main thread.
  const bool used_by_main_gpu_thread =
      usage & ~kDisplayCompositorUsage || !is_for_display_compositor_;
  return used_by_display_compositor_gpu_thread && used_by_main_gpu_thread;
}

SharedImageBackingFactory* SharedImageFactory::GetFactoryByUsage(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    base::span<const uint8_t> pixel_data,
    gfx::GpuMemoryBufferType gmb_type) {
  if (backing_factory_for_testing_)
    return backing_factory_for_testing_;

  bool share_between_threads = IsSharedBetweenThreads(usage);
  for (auto& factory : factories_) {
    if (factory->CanCreateSharedImage(usage, format, size,
                                      share_between_threads, gmb_type,
                                      gr_context_type_, pixel_data)) {
      return factory.get();
    }
  }

  return nullptr;
}

void SharedImageFactory::LogGetFactoryFailed(uint32_t usage,
                                             viz::SharedImageFormat format,
                                             gfx::GpuMemoryBufferType gmb_type,
                                             const std::string& debug_label) {
  LOG(ERROR) << "Could not find SharedImageBackingFactory with params: usage: "
             << CreateLabelForSharedImageUsage(usage)
             << ", format: " << format.ToString()
             << ", share_between_threads: " << IsSharedBetweenThreads(usage)
             << ", gmb_type: " << GmbTypeToString(gmb_type)
             << ", debug_label: " << debug_label;
}

bool SharedImageFactory::RegisterBacking(
    std::unique_ptr<SharedImageBacking> backing) {
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

  shared_images_.emplace(std::move(shared_image));
  return true;
}

bool SharedImageFactory::AddSecondaryReference(const gpu::Mailbox& mailbox) {
  if (shared_images_.contains(mailbox)) {
    LOG(ERROR) << "AddSecondaryReference: Can't have more than one reference.";
    return false;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->AddSecondaryReference(mailbox,
                                                   memory_tracker_.get());

  if (!shared_image) {
    return false;
  }

  shared_images_.emplace(std::move(shared_image));
  return true;
}

uint32_t SharedImageFactory::GetUsageForMailbox(const Mailbox& mailbox) {
  auto iter = shared_images_.find(mailbox);
  if (iter == shared_images_.end()) {
    return 0;
  }
  return (*iter)->usage();
}

SharedImageRepresentationFactory::SharedImageRepresentationFactory(
    SharedImageManager* manager,
    MemoryTracker* tracker)
    : manager_(manager),
      tracker_(std::make_unique<MemoryTypeTracker>(tracker)) {}

SharedImageRepresentationFactory::~SharedImageRepresentationFactory() {
  DCHECK_EQ(0u, tracker_->GetMemRepresented());
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageRepresentationFactory::ProduceGLTexture(const Mailbox& mailbox) {
  return manager_->ProduceGLTexture(mailbox, tracker_.get());
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedImageRepresentationFactory::ProduceGLTexturePassthrough(
    const Mailbox& mailbox) {
  return manager_->ProduceGLTexturePassthrough(mailbox, tracker_.get());
}

std::unique_ptr<SkiaImageRepresentation>
SharedImageRepresentationFactory::ProduceSkia(
    const Mailbox& mailbox,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceSkia(mailbox, tracker_.get(), context_state);
}

std::unique_ptr<DawnImageRepresentation>
SharedImageRepresentationFactory::ProduceDawn(
    const Mailbox& mailbox,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
  return manager_->ProduceDawn(mailbox, tracker_.get(), device, backend_type,
                               std::move(view_formats));
}

std::unique_ptr<OverlayImageRepresentation>
SharedImageRepresentationFactory::ProduceOverlay(const gpu::Mailbox& mailbox) {
  return manager_->ProduceOverlay(mailbox, tracker_.get());
}

std::unique_ptr<MemoryImageRepresentation>
SharedImageRepresentationFactory::ProduceMemory(const gpu::Mailbox& mailbox) {
  return manager_->ProduceMemory(mailbox, tracker_.get());
}

std::unique_ptr<RasterImageRepresentation>
SharedImageRepresentationFactory::ProduceRaster(const Mailbox& mailbox) {
  return manager_->ProduceRaster(mailbox, tracker_.get());
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<LegacyOverlayImageRepresentation>
SharedImageRepresentationFactory::ProduceLegacyOverlay(
    const gpu::Mailbox& mailbox) {
  return manager_->ProduceLegacyOverlay(mailbox, tracker_.get());
}
#endif

}  // namespace gpu
