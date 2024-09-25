// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include <inttypes.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_shared_memory.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/trace_util.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"
#include "gpu/vulkan/vulkan_device_queue.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing_factory.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

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

namespace gpu {

namespace {

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
  NOTREACHED_IN_MIGRATION();
}

gfx::GpuMemoryBufferType GetNativeBufferType() {
#if BUILDFLAG(IS_APPLE)
  return gfx::GpuMemoryBufferType::IO_SURFACE_BUFFER;
#elif BUILDFLAG(IS_ANDROID)
  return gfx::GpuMemoryBufferType::ANDROID_HARDWARE_BUFFER;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
#elif BUILDFLAG(IS_WIN)
  return gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE;
#else
  return gfx::GpuMemoryBufferType::EMPTY_BUFFER;
#endif
}

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

std::size_t
SharedImageFactory::SharedImageRepresentationFactoryRefHash::operator()(
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& o) const {
  return std::hash<gpu::Mailbox>{}(o->mailbox());
}

std::size_t
SharedImageFactory::SharedImageRepresentationFactoryRefHash::operator()(
    const gpu::Mailbox& m) const {
  return std::hash<gpu::Mailbox>{}(m);
}

bool SharedImageFactory::SharedImageRepresentationFactoryRefKeyEqual::
operator()(
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& lhs,
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& rhs) const {
  return lhs->mailbox() == rhs->mailbox();
}

bool SharedImageFactory::SharedImageRepresentationFactoryRefKeyEqual::
operator()(const std::unique_ptr<SharedImageRepresentationFactoryRef>& lhs,
           const gpu::Mailbox& rhs) const {
  return lhs->mailbox() == rhs;
}

bool SharedImageFactory::SharedImageRepresentationFactoryRefKeyEqual::
operator()(
    const gpu::Mailbox& lhs,
    const std::unique_ptr<SharedImageRepresentationFactoryRef>& rhs) const {
  return lhs == rhs->mailbox();
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
      context_state_(context_state),
      memory_tracker_(std::make_unique<MemoryTypeTracker>(memory_tracker)),
      is_for_display_compositor_(is_for_display_compositor),
      gr_context_type_(context_state_ ? context_state_->gr_context_type()
                                      : GrContextType::kNone),
      gpu_preferences_(gpu_preferences),
#if BUILDFLAG(IS_MAC)
      texture_target_for_io_surfaces_(GetTextureTargetForIOSurfaces()),
#endif
      workarounds_(workarounds) {
  auto shared_memory_backing_factory =
      std::make_unique<SharedMemoryImageBackingFactory>();
  factories_.push_back(std::move(shared_memory_backing_factory));

  // if GL is disabled, it only needs SharedMemoryImageBackingFactory.
  if (gl::GetGLImplementation() == gl::kGLImplementationDisabled) {
    return;
  }

  CHECK(context_state_);
  scoped_refptr<gles2::FeatureInfo> feature_info =
      context_state_->feature_info();

  if (!feature_info) {
    // For some unit tests like SharedImageFactoryTest, |shared_context_state_|
    // could be nullptr.
    bool use_passthrough = gpu_preferences.use_passthrough_cmd_decoder &&
                           gles2::PassthroughCommandDecoderSupported();
    feature_info = new gles2::FeatureInfo(workarounds_, gpu_feature_info);
    feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                             use_passthrough, gles2::DisallowedFeatures());
  }

  // Skia specific factories can't be used without a Skia context.
  if (gr_context_type_ != GrContextType::kNone) {
    auto wrapped_sk_image_factory =
        std::make_unique<WrappedSkImageBackingFactory>(context_state_);
    factories_.push_back(std::move(wrapped_sk_image_factory));

    if (features::IsUsingRawDraw()) {
      auto factory = std::make_unique<RawDrawImageBackingFactory>();
      factories_.push_back(std::move(factory));
    }
  }

  bool use_gl =
      !is_for_display_compositor_ || gr_context_type_ == GrContextType::kGL;
  if (use_gl) {
    // On Windows readback is slower with GLTextureImageBacking than
    // D3DImageBacking so prefer D3DImageBacking for software GMBs.
    bool supports_cpu_upload = !BUILDFLAG(IS_WIN);
    auto gl_texture_backing_factory =
        std::make_unique<GLTextureImageBackingFactory>(
            gpu_preferences_, workarounds_, feature_info.get(),
            context_state_->progress_reporter(), supports_cpu_upload);
    factories_.push_back(std::move(gl_texture_backing_factory));
  }

#if BUILDFLAG(IS_WIN)
  if (gl::DirectCompositionSupported()) {
    factories_.push_back(
        std::make_unique<DCompImageBackingFactory>(context_state_));
  }
  if (D3DImageBackingFactory::IsD3DSharedImageSupported(gpu_preferences_)) {
    auto d3d_factory = std::make_unique<D3DImageBackingFactory>(
        context_state_->GetD3D11Device(),
        shared_image_manager_->dxgi_shared_handle_manager(),
        context_state_->GetGLFormatCaps(), workarounds_);
    d3d_backing_factory_ = d3d_factory.get();
    factories_.push_back(std::move(d3d_factory));
  }
  {
    auto gl_texture_backing_factory =
        std::make_unique<GLTextureImageBackingFactory>(
            gpu_preferences_, workarounds_, feature_info.get(),
            context_state_->progress_reporter(),
            /*supports_cpu_upload=*/true);
    factories_.push_back(std::move(gl_texture_backing_factory));
  }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_VULKAN)
  // If Chrome and ANGLE are sharing the same vulkan device queue, AngleVulkan
  // backing will be used for interop.
  if ((gr_context_type_ == GrContextType::kVulkan) &&
      (base::FeatureList::IsEnabled(features::kVulkanFromANGLE))) {
    auto factory = std::make_unique<AngleVulkanImageBackingFactory>(
        gpu_preferences_, workarounds_, context_state_);
    factories_.push_back(std::move(factory));
  }

#if BUILDFLAG(IS_WIN)
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state_);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(ENABLE_VULKAN)

  // Create EGLImageBackingFactory if egl images are supported. Note that the
  // factory creation is kept here to preserve the current preference of factory
  // to be used.
  auto* egl_display = gl::GetDefaultDisplayEGL();
  if (use_gl && egl_display && egl_display->ext->b_EGL_KHR_image_base &&
      egl_display->ext->b_EGL_KHR_gl_texture_2D_image &&
      egl_display->ext->b_EGL_KHR_fence_sync &&
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image) {
    auto egl_backing_factory = std::make_unique<EGLImageBackingFactory>(
        gpu_preferences_, workarounds_, feature_info.get());
    factories_.push_back(std::move(egl_backing_factory));
  }

#if BUILDFLAG(IS_ANDROID)
  bool is_ahb_supported = true;
  if (gr_context_type_ == GrContextType::kVulkan) {
    const auto& enabled_extensions = context_state_->vk_context_provider()
                                         ->GetDeviceQueue()
                                         ->enabled_extensions();
    is_ahb_supported = gfx::HasExtension(
        enabled_extensions,
        VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME);
  }
  if (is_ahb_supported) {
    auto ahb_factory = std::make_unique<AHardwareBufferImageBackingFactory>(
        feature_info.get(), gpu_preferences_);
    factories_.push_back(std::move(ahb_factory));
  }
#elif BUILDFLAG(IS_OZONE)
  // For all Ozone platforms - Desktop Linux, ChromeOS, Fuchsia, CastOS.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_native_pixmaps) {
    auto ozone_factory = std::make_unique<OzoneImageBackingFactory>(
        context_state_, workarounds_);
    factories_.push_back(std::move(ozone_factory));
  }
#if BUILDFLAG(ENABLE_VULKAN) && (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_FUCHSIA))
  if (gr_context_type_ == GrContextType::kVulkan) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(context_state_);
    factories_.push_back(std::move(external_vk_image_factory));
  }
#endif  // BUILDFLAG(ENABLE_VULKAN) && (BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_FUCHSIA))
#endif  // BUILDFLAG(IS_OZONE)

#if BUILDFLAG(IS_APPLE)
  {
    auto iosurface_backing_factory =
        std::make_unique<IOSurfaceImageBackingFactory>(
            gr_context_type_, context_state_->GetMaxTextureSize(),
            feature_info.get(), context_state_->progress_reporter(),
#if BUILDFLAG(IS_MAC)
            texture_target_for_io_surfaces_);
#else
            GL_TEXTURE_2D);
#endif
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
                                           SharedImageUsageSet usage,
                                           std::string debug_label) {
  auto* factory = GetFactoryByUsage(usage, format, size,
                                    /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
  if (!factory) {
    LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER, debug_label);
    return false;
  }

  auto backing = factory->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, SharedImageUsageSet(usage), std::move(debug_label),
      IsSharedBetweenThreads(usage));

  DVLOG_IF(1, !!backing) << "CreateSharedImage[" << backing->GetName()
                         << "] size=" << size.ToString()
                         << " usage=" << CreateLabelForSharedImageUsage(usage)
                         << " format=" << format.ToString();

  return RegisterBacking(std::move(backing));
}

bool SharedImageFactory::IsNativeBufferSupported(gfx::BufferFormat format,
                                                 gfx::BufferUsage usage) {
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
                        gfx::BufferUsageAndFormat(usage, format));
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::SharedImageFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           SurfaceHandle surface_handle,
                                           SharedImageUsageSet usage,
                                           std::string debug_label,
                                           gfx::BufferUsage buffer_usage) {
  if (!viz::HasEquivalentBufferFormat(format)) {
    // Client GMB code still operates on BufferFormat so the SharedImageFormat
    // received here must have an equivalent BufferFormat.
    LOG(ERROR) << "Invalid format " << format.ToString();
    return false;
  }

  auto buffer_format = ToBufferFormat(format);
  auto native_buffer_supported =
      IsNativeBufferSupported(buffer_format, buffer_usage);

  std::unique_ptr<SharedImageBacking> backing;
  if (native_buffer_supported) {
    auto* factory = GetFactoryByUsage(usage, format, size,
                                      /*pixel_data=*/{}, GetNativeBufferType());
    if (!factory) {
      LogGetFactoryFailed(usage, format, GetNativeBufferType(), debug_label);
      return false;
    }

    backing = factory->CreateSharedImage(
        mailbox, format, surface_handle, size, color_space, surface_origin,
        alpha_type, SharedImageUsageSet(usage), debug_label,
        IsSharedBetweenThreads(usage), buffer_usage);

    if (backing) {
      DVLOG(1) << "CreateSharedImageBackedByBuffer[" << backing->GetName()
               << "] size=" << size.ToString()
               << " usage=" << CreateLabelForSharedImageUsage(usage)
               << " format=" << format.ToString();
    }
  } else {
    // If native buffers are not supported, try to create shared memory based
    // backings.
    if (gpu::GpuMemoryBufferImplSharedMemory::IsUsageSupported(buffer_usage) &&
        gpu::GpuMemoryBufferImplSharedMemory::IsSizeValidForFormat(
            size, buffer_format)) {
      // Clear the external sampler prefs for shared memory case if it is set.
      // https://issues.chromium.org/339546249.
      if (format.PrefersExternalSampler()) {
        format.ClearPrefersExternalSampler();
      }
      auto* factory =
          GetFactoryByUsage(usage, format, size,
                            /*pixel_data=*/{}, gfx::SHARED_MEMORY_BUFFER);

      bool use_compound = false;
      if (!factory && !IsSharedBetweenThreads(usage)) {
        // Check if CompoundImageBacking can hold shared memory buffer plus
        // another GPU backing type to satisfy requirements.
        if (CompoundImageBacking::IsValidSharedMemoryBufferFormat(size,
                                                                  format)) {
          factory =
              GetFactoryByUsage(CompoundImageBacking::GetGpuSharedImageUsage(
                                    SharedImageUsageSet(usage)),
                                format, size,
                                /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
          use_compound = factory != nullptr;
        }
      }

      if (!factory) {
        LogGetFactoryFailed(usage, format, gfx::SHARED_MEMORY_BUFFER,
                            debug_label);
        return false;
      }

      if (use_compound) {
        backing = CompoundImageBacking::CreateSharedMemory(
            factory, mailbox, format, size, color_space, surface_origin,
            alpha_type, usage, debug_label, buffer_usage);
      } else {
        backing = factory->CreateSharedImage(
            mailbox, format, surface_handle, size, color_space, surface_origin,
            alpha_type, SharedImageUsageSet(usage), debug_label,
            IsSharedBetweenThreads(usage), buffer_usage);
      }

      if (backing) {
        DVLOG(1) << "CreateSharedImageBackedByBuffer[" << backing->GetName()
                 << "] size=" << size.ToString()
                 << " usage=" << CreateLabelForSharedImageUsage(usage)
                 << " format=" << format.ToString();
        backing->OnWriteSucceeded();
      }
    }
  }
  return RegisterBacking(std::move(backing));
}

bool SharedImageFactory::CreateSharedImage(const Mailbox& mailbox,
                                           viz::SharedImageFormat format,
                                           const gfx::Size& size,
                                           const gfx::ColorSpace& color_space,
                                           GrSurfaceOrigin surface_origin,
                                           SkAlphaType alpha_type,
                                           SharedImageUsageSet usage,
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

  auto backing = factory->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type,
      SharedImageUsageSet(usage), std::move(debug_label),
      IsSharedBetweenThreads(usage), data);
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
    SharedImageUsageSet usage,
    std::string debug_label,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  gfx::GpuMemoryBufferType gmb_type = buffer_handle.type;

  bool use_compound = false;
  SharedImageBackingFactory* factory =
      GetFactoryByUsage(usage, format, size, {}, gmb_type);

  if (!factory && gmb_type == gfx::SHARED_MEMORY_BUFFER &&
      !IsSharedBetweenThreads(usage)) {
    // Check if CompoundImageBacking can hold shared memory buffer plus
    // another GPU backing type to satisfy requirements.
    if (CompoundImageBacking::IsValidSharedMemoryBufferFormat(size, format)) {
      factory =
          GetFactoryByUsage(CompoundImageBacking::GetGpuSharedImageUsage(
                                SharedImageUsageSet(usage)),
                            format, size, /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
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
        factory, mailbox, std::move(buffer_handle), format, size, color_space,
        surface_origin, alpha_type, usage, std::move(debug_label));
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

bool SharedImageFactory::SetSharedImagePurgeable(const Mailbox& mailbox,
                                                 bool purgeable) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    LOG(ERROR)
        << "SetSharedImagePurgeable: Could not find shared image mailbox";
    return false;
  }
  (*it)->SetPurgeable(purgeable);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
  if (!have_context) {
    for (auto& shared_image : shared_images_) {
      shared_image->OnContextLost();
    }
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
                                         gpu::SharedImageUsageSet usage) {
  if (!D3DImageBackingFactory::IsSwapChainSupported(gpu_preferences_)) {
    return false;
  }

  auto backings = d3d_backing_factory_->CreateSwapChain(
      front_buffer_mailbox, back_buffer_mailbox, format, size, color_space,
      surface_origin, alpha_type, usage);
  return RegisterBacking(std::move(backings.front_buffer)) &&
         RegisterBacking(std::move(backings.back_buffer));
}

bool SharedImageFactory::PresentSwapChain(const Mailbox& mailbox) {
  if (!D3DImageBackingFactory::IsSwapChainSupported(gpu_preferences_)) {
    return false;
  }
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
    const viz::SharedImageFormat& format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  auto* vulkan_context_provider = context_state_->vk_context_provider();
  VkDevice device =
      vulkan_context_provider->GetDeviceQueue()->GetVulkanDevice();
  DCHECK(device != VK_NULL_HANDLE);
  auto buffer_format = ToBufferFormat(format);
  vulkan_context_provider->GetVulkanImplementation()
      ->RegisterSysmemBufferCollection(
          device, std::move(service_handle), std::move(sysmem_token),
          buffer_format, usage, gfx::Size(), 0, register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

bool SharedImageFactory::CopyToGpuMemoryBuffer(const Mailbox& mailbox) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    DLOG(ERROR) << "CopyToGpuMemoryBuffer: Could not find shared image mailbox";
    return false;
  }
  return (*it)->CopyToGpuMemoryBuffer();
}

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CopyToGpuMemoryBufferAsync(
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    DLOG(ERROR)
        << "CopyToGpuMemoryBufferAsync: Could not find shared image mailbox";
    return false;
  }
  (*it)->CopyToGpuMemoryBufferAsync(std::move(callback));
  return true;
}
#endif

bool SharedImageFactory::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle& handle,
    viz::SharedImageFormat& format,
    gfx::Size& size,
    gfx::BufferUsage& buffer_usage) {
  auto it = shared_images_.find(mailbox);
  if (it == shared_images_.end()) {
    LOG(ERROR)
        << "GetGpuMemoryBufferHandleInfo: Could not find shared image mailbox";
    return false;
  }
  (*it)->GetGpuMemoryBufferHandleInfo(handle, format, size, buffer_usage);
  return true;
}

void SharedImageFactory::RegisterSharedImageBackingFactoryForTesting(
    SharedImageBackingFactory* factory) {
  backing_factory_for_testing_ = factory;
}

gpu::SharedImageCapabilities SharedImageFactory::MakeCapabilities() {
  gpu::SharedImageCapabilities shared_image_caps;
  shared_image_caps.supports_scanout_shared_images =
      SharedImageManager::SupportsScanoutImages();

#if BUILDFLAG(IS_WIN)
  // Scanout for software video frames is supported on Windows except on D3D9.
  shared_image_caps.supports_scanout_shared_images_for_software_video_frames =
      gl::QueryD3D11DeviceObjectFromANGLE();
#endif

  const bool is_angle_metal =
      gl::GetGLImplementation() == gl::kGLImplementationEGLANGLE &&
      gl::GetANGLEImplementation() == gl::ANGLEImplementation::kMetal;
  const bool is_skia_graphite =
      gr_context_type_ == GrContextType::kGraphiteDawn ||
      gr_context_type_ == GrContextType::kGraphiteMetal;
  shared_image_caps.supports_luminance_shared_images =
      !is_angle_metal && !is_skia_graphite;
  shared_image_caps.supports_r16_shared_images =
      is_angle_metal || is_skia_graphite;
  shared_image_caps.supports_native_nv12_mappable_shared_images =
      IsNativeBufferSupported(gfx::BufferFormat::YUV_420_BIPLANAR,
                              gfx::BufferUsage::GPU_READ_CPU_READ_WRITE);
  shared_image_caps.disable_r8_shared_images =
      workarounds_.r8_egl_images_broken;
  shared_image_caps.disable_webgpu_shared_images =
      workarounds_.disable_webgpu_shared_images;
  if (!context_state_) {
    shared_image_caps.is_r16f_supported = false;
  } else if (is_skia_graphite || gr_context_type_ == GrContextType::kVulkan) {
    // R16F is always supported with Dawn and Vulkan contexts.
    shared_image_caps.is_r16f_supported = true;
  } else if (gr_context_type_ == GrContextType::kGL) {
    CHECK(context_state_->gr_context());
    // With Skia GL, R16F is supported only with GLES 3.0 and above.
    shared_image_caps.is_r16f_supported =
        context_state_->feature_info()->gl_version_info().IsAtLeastGLES(3, 0) &&
        context_state_->gr_context()->colorTypeSupportedAsImage(
            kA16_float_SkColorType);
  }

#if BUILDFLAG(IS_MAC)
  shared_image_caps.texture_target_for_io_surfaces =
      texture_target_for_io_surfaces_;
#endif

#if BUILDFLAG(IS_WIN)
  shared_image_caps.shared_image_d3d =
      D3DImageBackingFactory::IsD3DSharedImageSupported(gpu_preferences_);
  shared_image_caps.shared_image_swap_chain =
      shared_image_caps.shared_image_d3d &&
      D3DImageBackingFactory::IsSwapChainSupported(gpu_preferences_);
#endif  // BUILDFLAG(IS_WIN)

  return shared_image_caps;
}

bool SharedImageFactory::HasSharedImage(const Mailbox& mailbox) const {
  return shared_images_.contains(mailbox);
}

void SharedImageFactory::SetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  gpu_extra_info_ = gpu_extra_info;
}

bool SharedImageFactory::IsSharedBetweenThreads(
    gpu::SharedImageUsageSet usage) {
  // Ignore for mipmap usage.
  usage.RemoveAll(SHARED_IMAGE_USAGE_MIPMAP);
  // Ignore for delegated compositing.
  usage.RemoveAll(SHARED_IMAGE_USAGE_RASTER_DELEGATED_COMPOSITING);

  // Raw Draw backings will be write accessed on the GPU main thread, and
  // be read accessed on the compositor thread.
  if (usage.Has(SHARED_IMAGE_USAGE_RAW_DRAW)) {
    return true;
  }

  // DISPLAY is for gpu composition and SCANOUT for overlays.
  constexpr gpu::SharedImageUsageSet kDisplayCompositorUsage =
      SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE |
      SHARED_IMAGE_USAGE_SCANOUT;

  // Image is used on display compositor gpu thread if it's used by display
  // compositor and if display compositor runs on a separate thread. Image is
  // used by display compositor if it has kDisplayCompositorUsage or is being
  // created by display compositor.
  const bool used_by_display_compositor_gpu_thread =
      (usage.HasAny(kDisplayCompositorUsage) || is_for_display_compositor_) &&
      shared_image_manager_->display_context_on_another_thread();

  // If it has usage other than kDisplayCompositorUsage OR if it is not created
  // by display compositor, it means that it is used by the gpu main thread.
  const bool used_by_main_gpu_thread =
      !kDisplayCompositorUsage.HasAll(usage) || !is_for_display_compositor_;
  return used_by_display_compositor_gpu_thread && used_by_main_gpu_thread;
}

SharedImageBackingFactory* SharedImageFactory::GetFactoryByUsage(
    gpu::SharedImageUsageSet usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    base::span<const uint8_t> pixel_data,
    gfx::GpuMemoryBufferType gmb_type) {
  if (backing_factory_for_testing_)
    return backing_factory_for_testing_;

  bool share_between_threads = IsSharedBetweenThreads(usage);
  for (auto& factory : factories_) {
    if (factory->CanCreateSharedImage(SharedImageUsageSet(usage), format, size,
                                      share_between_threads, gmb_type,
                                      gr_context_type_, pixel_data)) {
      return factory.get();
    }
  }

  return nullptr;
}

void SharedImageFactory::LogGetFactoryFailed(gpu::SharedImageUsageSet usage,
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

SharedImageUsageSet SharedImageFactory::GetUsageForMailbox(
    const Mailbox& mailbox) {
  auto iter = shared_images_.find(mailbox);
  if (iter == shared_images_.end()) {
    return SharedImageUsageSet();
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
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceDawn(mailbox, tracker_.get(), device, backend_type,
                               std::move(view_formats), context_state);
}

std::unique_ptr<DawnBufferRepresentation>
SharedImageRepresentationFactory::ProduceDawnBuffer(
    const Mailbox& mailbox,
    const wgpu::Device& device,
    wgpu::BackendType backend_type) {
  return manager_->ProduceDawnBuffer(mailbox, tracker_.get(), device,
                                     backend_type);
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

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_OZONE)
std::unique_ptr<VulkanImageRepresentation>
SharedImageRepresentationFactory::ProduceVulkan(
    const gpu::Mailbox& mailbox,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  return manager_->ProduceVulkan(mailbox, tracker_.get(), vulkan_device_queue,
                                 vulkan_impl, needs_detiling);
}
#endif

}  // namespace gpu
