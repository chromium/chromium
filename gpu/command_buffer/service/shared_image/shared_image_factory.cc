// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include <inttypes.h>
#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/gl_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/buildflags.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#endif

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)) && \
    BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"
#elif BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_VULKAN)
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"
#elif BUILDFLAG(IS_MAC)
#include "gpu/command_buffer/service/shared_image/iosurface_image_backing_factory.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#include "gpu/command_buffer/service/shared_image/d3d_image_backing_factory.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_angle_util_win.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#include "gpu/command_buffer/service/shared_image/ozone_image_backing_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_implementation.h"
#endif  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"
#endif

namespace gpu {

namespace {

#if defined(USE_OZONE) && BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)

bool ShouldUseExternalVulkanImageFactory() {
#if BUILDFLAG(ENABLE_VULKAN)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .uses_external_vulkan_image_factory;
#else
  return false;
#endif
}

#endif  // defined(USE_OZONE) && BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)

bool ShouldUseOzoneImageBackingFactory() {
#if defined(USE_OZONE)
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformRuntimeProperties()
      .supports_native_pixmaps;
#else
  return false;
#endif
}

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

enum DmaBufSupportedType {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  kNoPixmapNoVulkanExtNoGlExt = 0,
  kNoPixmapNoVulkanExtYesGlExt = 1,
  kNoPixmapYesVulkanExtNoGlExt = 2,
  kNoPixmapYesVulkanExtYesGlExt = 3,
  kYesPixmapNoVulkanExtNoGlExt = 4,
  kYesPixmapNoVulkanExtYesGlExt = 5,
  kYesPixmapYesVulkanExtNoGlExt = 6,
  kYesPixmapYesVulkanExtYesGlExt = 7,
  kNoPixmapNoVulkanExtYesGlxExt = 8,
  kNoPixmapYesVulkanExtYesGlxExt = 9,
  kYesPixmapNoVulkanExtYesGlxExt = 10,
  kYesPixmapYesVulkanExtYesGlxExt = 11,
  kMaxValue = kYesPixmapYesVulkanExtYesGlxExt
};

enum class GLExtType { kNone, kEGL, kGLX };

DmaBufSupportedType GetDmaBufSupportedType(bool pixmap_supported,
                                           bool vulkan_ext_supported,
                                           GLExtType gl_ext_supported) {
  if (pixmap_supported) {
    if (vulkan_ext_supported) {
      switch (gl_ext_supported) {
        case GLExtType::kNone:
          return kYesPixmapYesVulkanExtNoGlExt;
        case GLExtType::kEGL:
          return kYesPixmapYesVulkanExtYesGlExt;
        case GLExtType::kGLX:
          return kYesPixmapYesVulkanExtYesGlxExt;
      }
    } else {
      switch (gl_ext_supported) {
        case GLExtType::kNone:
          return kYesPixmapNoVulkanExtNoGlExt;
        case GLExtType::kEGL:
          return kYesPixmapNoVulkanExtYesGlExt;
        case GLExtType::kGLX:
          return kYesPixmapNoVulkanExtYesGlxExt;
      }
    }
  } else {
    if (vulkan_ext_supported) {
      switch (gl_ext_supported) {
        case GLExtType::kNone:
          return kNoPixmapYesVulkanExtNoGlExt;
        case GLExtType::kEGL:
          return kNoPixmapYesVulkanExtYesGlExt;
        case GLExtType::kGLX:
          return kNoPixmapYesVulkanExtYesGlxExt;
      }
    } else {
      switch (gl_ext_supported) {
        case GLExtType::kNone:
          return kNoPixmapNoVulkanExtNoGlExt;
        case GLExtType::kEGL:
          return kNoPixmapNoVulkanExtYesGlExt;
        case GLExtType::kGLX:
          return kNoPixmapNoVulkanExtYesGlxExt;
      }
    }
  }
}

void ReportDmaBufSupportMetric(bool pixmap_supported,
                               bool vulkan_ext_supported,
                               GLExtType gl_ext_supported) {
  DmaBufSupportedType type = GetDmaBufSupportedType(
      pixmap_supported, vulkan_ext_supported, gl_ext_supported);
  UMA_HISTOGRAM_ENUMERATION("GPU.SharedImage.DmaBufSupportedType", type);
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

bool SharedImageFactory::set_dmabuf_supported_metric_ = false;

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SharedContextState* context_state,
    MailboxManager* mailbox_manager,
    SharedImageManager* shared_image_manager,
    ImageFactory* image_factory,
    MemoryTracker* memory_tracker,
    bool is_for_display_compositor)
    : mailbox_manager_(mailbox_manager),
      shared_image_manager_(shared_image_manager),
      shared_context_state_(context_state),
      memory_tracker_(std::make_unique<MemoryTypeTracker>(memory_tracker)),
      is_for_display_compositor_(is_for_display_compositor),
      gr_context_type_(context_state ? context_state->gr_context_type()
                                     : GrContextType::kGL) {
#if BUILDFLAG(IS_MAC)
  // OSX
  DCHECK(gr_context_type_ == GrContextType::kGL ||
         gr_context_type_ == GrContextType::kMetal ||
         gr_context_type_ == GrContextType::kVulkan);
#endif

  scoped_refptr<gles2::FeatureInfo> feature_info;
  if (shared_context_state_) {
    feature_info = shared_context_state_->feature_info();
  }

  if (!feature_info) {
    // For some unit tests, shared_context_state_ could be nullptr.
    bool use_passthrough = gpu_preferences.use_passthrough_cmd_decoder &&
                           gles2::PassthroughCommandDecoderSupported();
    feature_info = new gles2::FeatureInfo(workarounds, gpu_feature_info);
    feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                             use_passthrough, gles2::DisallowedFeatures());
  }

  if (!set_dmabuf_supported_metric_) {
    bool pixmap_supported = ShouldUseOzoneImageBackingFactory();
    bool vulkan_ext_supported = false;
    GLExtType gl_ext_type = GLExtType::kNone;

#if BUILDFLAG(ENABLE_VULKAN)
    if (gr_context_type_ == GrContextType::kVulkan && context_state) {
      const auto& enabled_extensions = context_state->vk_context_provider()
                                           ->GetDeviceQueue()
                                           ->enabled_extensions();
      vulkan_ext_supported =
          gfx::HasExtension(enabled_extensions,
                            VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) &&
          gfx::HasExtension(enabled_extensions,
                            VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    }
#endif  // BUILDFLAG(ENABLE_VULKAN)

    bool egl_ext_supported =
        gl::GLSurfaceEGL::GetGLDisplayEGL()->ext->b_EGL_KHR_image;
    bool glx_ext_supported = false;
#if defined(USE_OZONE)
#if BUILDFLAG(OZONE_PLATFORM_X11)
    ui::GLOzone* gl_ozone = ui::OzonePlatform::GetInstance()
                                ->GetSurfaceFactoryOzone()
                                ->GetCurrentGLOzone();
    // This checks for extension support on both GLOzoneEGLX11 and GLOzoneGLX.
    glx_ext_supported = gl_ozone && gl_ozone->CanImportNativePixmap();
#endif  // BUILDFLAG(OZONE_PLATFORM_X11)
#endif  // defined(USE_OZONE)
    if (egl_ext_supported) {
      gl_ext_type = GLExtType::kEGL;
    } else if (glx_ext_supported) {
      gl_ext_type = GLExtType::kGLX;
    }

    ReportDmaBufSupportMetric(pixmap_supported, vulkan_ext_supported,
                              gl_ext_type);
    set_dmabuf_supported_metric_ = true;
  }

  auto shared_memory_backing_factory =
      std::make_unique<SharedMemoryImageBackingFactory>();
  factories_.push_back(std::move(shared_memory_backing_factory));

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
                                  : nullptr);
    factories_.push_back(std::move(gl_texture_backing_factory));
  }

#if BUILDFLAG(IS_WIN)
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
    auto gl_image_backing_factory = std::make_unique<GLImageBackingFactory>(
        gpu_preferences, workarounds, feature_info.get(), image_factory,
        shared_context_state_ ? shared_context_state_->progress_reporter()
                              : nullptr,
        /*for_shared_memory_gmbs=*/true);
    factories_.push_back(std::move(gl_image_backing_factory));
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
#endif

#if BUILDFLAG(IS_WIN)
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#elif BUILDFLAG(IS_ANDROID)
  if (use_gl) {
    auto egl_backing_factory = std::make_unique<EGLImageBackingFactory>(
        gpu_preferences, workarounds, feature_info.get());
    factories_.push_back(std::move(egl_backing_factory));
  }
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
        feature_info.get());
    factories_.push_back(std::move(ahb_factory));
  }
  if (gr_context_type_ == GrContextType::kVulkan &&
      !base::FeatureList::IsEnabled(features::kVulkanFromANGLE)) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#elif defined(USE_OZONE)
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)
  // Desktop Linux, not ChromeOS.
  if (ShouldUseOzoneImageBackingFactory()) {
    auto ozone_factory =
        std::make_unique<OzoneImageBackingFactory>(context_state, workarounds);
    factories_.push_back(std::move(ozone_factory));
  }
  if (gr_context_type_ == GrContextType::kVulkan &&
      (!ShouldUseOzoneImageBackingFactory() ||
       ShouldUseExternalVulkanImageFactory())) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#elif BUILDFLAG(IS_FUCHSIA)
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto ozone_factory =
        std::make_unique<OzoneImageBackingFactory>(context_state, workarounds);
    factories_.push_back(std::move(ozone_factory));
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state);
    factories_.push_back(std::move(external_vk_image_factory));
  }
  vulkan_context_provider_ = context_state->vk_context_provider();
#elif BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  auto ozone_factory =
      std::make_unique<OzoneImageBackingFactory>(context_state, workarounds);
  factories_.push_back(std::move(ozone_factory));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // defined(USE_OZONE)

#if BUILDFLAG(IS_MAC)
  // TODO(hitawala): Temporary factory that will be replaced with Ozone and
  // other backings
  if (use_gl) {
    auto gl_image_backing_factory = std::make_unique<GLImageBackingFactory>(
        gpu_preferences, workarounds, feature_info.get(), image_factory,
        shared_context_state_ ? shared_context_state_->progress_reporter()
                              : nullptr,
        /*for_shared_memory_gmbs=*/false);
    factories_.push_back(std::move(gl_image_backing_factory));
  }
#endif
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
  auto* factory = GetFactoryByUsage(usage, format, &allow_legacy_mailbox,
                                    /*is_pixel_used=*/false, gfx::EMPTY_BUFFER);
  if (!factory)
    return false;

  auto backing = factory->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, IsSharedBetweenThreads(usage));
  DVLOG(1) << "CreateSharedImage[" << backing->GetName()
           << "] size=" << size.ToString()
           << " usage=" << CreateLabelForSharedImageUsage(usage)
           << " resource_format=" << viz::ResourceFormatToString(format);
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

  bool allow_legacy_mailbox = false;
  SharedImageBackingFactory* factory = nullptr;
  if (backing_factory_for_testing_) {
    factory = backing_factory_for_testing_;
  } else {
    factory = GetFactoryByUsage(usage, format, &allow_legacy_mailbox,
                                /*is_pixel_used=*/true, gfx::EMPTY_BUFFER);
  }
  if (!factory)
    return false;

  auto backing =
      factory->CreateSharedImage(mailbox, format, size, color_space,
                                 surface_origin, alpha_type, usage, data);
  if (backing) {
    DVLOG(1) << "CreateSharedImagePixels[" << backing->GetName()
             << "] with pixels size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " resource_format=" << viz::ResourceFormatToString(format);

    backing->OnWriteSucceeded();
  }
  return RegisterBacking(std::move(backing), allow_legacy_mailbox);
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           int client_id,
                                           gfx::GpuMemoryBufferHandle handle,
                                           gfx::BufferFormat format,
                                           gfx::BufferPlane plane,
                                           SurfaceHandle surface_handle,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           uint32_t usage) {
  bool allow_legacy_mailbox = false;
  auto resource_format = viz::GetResourceFormat(format);
  gfx::GpuMemoryBufferType gmb_type = handle.type;

  bool use_compound = false;
  auto* factory =
      GetFactoryByUsage(usage, resource_format, &allow_legacy_mailbox,
                        /*is_pixel_used=*/false, gmb_type, &use_compound);
  if (!factory)
    return false;

  std::unique_ptr<SharedImageBacking> backing;
  if (use_compound) {
    backing = CompoundImageBacking::CreateSharedMemory(
        factory, mailbox, std::move(handle), format, plane, surface_handle,
        size, color_space, surface_origin, alpha_type, usage);
  } else {
    backing = factory->CreateSharedImage(
        mailbox, client_id, std::move(handle), format, plane, surface_handle,
        size, color_space, surface_origin, alpha_type, usage);
  }

  if (backing) {
    DVLOG(1) << "CreateSharedImage[" << backing->GetName()
             << "] from handle size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " buffer_format=" << gfx::BufferFormatToString(format)
             << " gmb_type=" << GmbTypeToString(gmb_type);

    backing->OnWriteSucceeded();
  }
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

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CreateSwapChain(const Mailbox& front_buffer_mailbox,
                                         const Mailbox& back_buffer_mailbox,
                                         viz::ResourceFormat format,
                                         const gfx::Size& size,
                                         const gfx::ColorSpace& color_space,
                                         GrSurfaceOrigin surface_origin,
                                         SkAlphaType alpha_type,
                                         uint32_t usage) {
  if (!D3DImageBackingFactory::IsSwapChainSupported())
    return false;

  bool allow_legacy_mailbox = true;
  auto backings = d3d_backing_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      surface_origin, alpha_type, usage);
  return RegisterBacking(std::move(backings.front_buffer),
                         allow_legacy_mailbox) &&
         RegisterBacking(std::move(backings.back_buffer), allow_legacy_mailbox);
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
#endif  // BUILDFLAG(IS_FUCHSIA)

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

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CreateSharedImageVideoPlanes(
    base::span<const Mailbox> mailboxes,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat format,
    const gfx::Size& size,
    uint32_t usage) {
  if (!d3d_backing_factory_)
    return false;

  auto backings = d3d_backing_factory_->CreateSharedImageVideoPlanes(
      mailboxes, std::move(handle), format, size, usage);

  if (backings.size() != gfx::NumberOfPlanesForLinearBufferFormat(format))
    return false;

  for (auto& backing : backings) {
    if (!RegisterBacking(std::move(backing), /*allow_legacy_mailbox=*/false))
      return false;
  }
  return true;
}

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
  constexpr int kDisplayCompositorUsage =
      SHARED_IMAGE_USAGE_DISPLAY | SHARED_IMAGE_USAGE_SCANOUT;

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
    viz::ResourceFormat format,
    bool* allow_legacy_mailbox,
    bool is_pixel_used,
    gfx::GpuMemoryBufferType gmb_type,
    bool* use_compound_backing) {
  if (backing_factory_for_testing_)
    return backing_factory_for_testing_;

  bool share_between_threads = IsSharedBetweenThreads(usage);
  for (auto& factory : factories_) {
    if (factory->IsSupported(usage, format, share_between_threads, gmb_type,
                             gr_context_type_, allow_legacy_mailbox,
                             is_pixel_used)) {
      return factory.get();
    } else if (use_compound_backing && gmb_type == gfx::SHARED_MEMORY_BUFFER) {
      // Check if backing type supports CPU upload with no buffer handle so it
      // can be used with a compound backing instead.
      if (factory->IsSupported(usage | SHARED_IMAGE_USAGE_CPU_UPLOAD, format,
                               share_between_threads, gfx::EMPTY_BUFFER,
                               gr_context_type_, allow_legacy_mailbox,
                               is_pixel_used)) {
        *use_compound_backing = true;
        return factory.get();
      }
    }
  }

  LOG(ERROR) << "Could not find SharedImageBackingFactory with params: usage: "
             << CreateLabelForSharedImageUsage(usage) << ", format: " << format
             << ", share_between_threads: " << share_between_threads
             << ", gmb_type: " << GmbTypeToString(gmb_type);
  return nullptr;
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

std::unique_ptr<GLTextureImageRepresentation>
SharedImageRepresentationFactory::ProduceGLTexture(const Mailbox& mailbox) {
  return manager_->ProduceGLTexture(mailbox, tracker_.get());
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageRepresentationFactory::ProduceRGBEmulationGLTexture(
    const Mailbox& mailbox) {
  return manager_->ProduceRGBEmulationGLTexture(mailbox, tracker_.get());
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
SharedImageRepresentationFactory::ProduceDawn(const Mailbox& mailbox,
                                              WGPUDevice device,
                                              WGPUBackendType backend_type) {
  return manager_->ProduceDawn(mailbox, tracker_.get(), device, backend_type);
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
