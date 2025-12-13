// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"

#include <inttypes.h>

#include <memory>

#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/common/shared_image_trace_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/compound_image_backing.h"
#include "gpu/command_buffer/service/shared_image/cpu_readback_upload_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/egl_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/raw_draw_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_copy_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_copy_strategy.h"
#include "gpu/command_buffer/service/shared_image/shared_memory_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/gpu_memory_buffer_handle.h"
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
#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image/dawn_image_backing_factory.h"
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {

namespace {

const char* GmbTypeToString(gfx::GpuMemoryBufferType type) {
  switch (type) {
    case gfx::EMPTY_BUFFER:
      return "empty";
    case gfx::SHARED_MEMORY_BUFFER:
      return "shared_memory";
#if BUILDFLAG(IS_APPLE)
    case gfx::IO_SURFACE_BUFFER:
      return "platform";
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
    case gfx::NATIVE_PIXMAP:
      return "platform";
#endif
#if BUILDFLAG(IS_WIN)
    case gfx::DXGI_SHARED_HANDLE:
      return "platform";
#endif
#if BUILDFLAG(IS_ANDROID)
    case gfx::ANDROID_HARDWARE_BUFFER:
      return "platform";
#endif
  }
  NOTREACHED();
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

}  // namespace

SharedImageFactory::SharedImageFactory(
    const GpuPreferences& gpu_preferences,
    const GpuDriverBugWorkarounds& workarounds,
    const GpuFeatureInfo& gpu_feature_info,
    SharedContextState* context_state,
    SharedImageManager* shared_image_manager,
    scoped_refptr<gpu::MemoryTracker> memory_tracker,
    bool is_for_display_compositor)
    : shared_image_manager_(shared_image_manager),
      context_state_(context_state),
      memory_type_tracker_(
          std::make_unique<MemoryTypeTracker>(std::move(memory_tracker))),
      is_for_display_compositor_(is_for_display_compositor),
      gr_context_type_(context_state_ ? context_state_->gr_context_type()
                                      : GrContextType::kNone),
      gpu_preferences_(gpu_preferences),
#if BUILDFLAG(IS_MAC)
      texture_target_for_io_surfaces_(GetTextureTargetForIOSurfaces()),
#endif
      workarounds_(workarounds) {
  copy_manager_ = base::MakeRefCounted<SharedImageCopyManager>();
  copy_manager_->AddStrategy(std::make_unique<SharedMemoryCopyStrategy>());
  copy_manager_->AddStrategy(std::make_unique<CPUReadbackUploadCopyStrategy>());

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
    bool use_passthrough = gpu_preferences.use_passthrough_cmd_decoder;
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
  // WebNN requires use of shared images for WebGPUInterop.
  const bool is_webnn_feature_enabled =
      (gpu_feature_info.status_values[GPU_FEATURE_TYPE_WEBNN] ==
       kGpuFeatureStatusEnabled);

  const bool enable_webnn_only_d3d_factory =
      is_webnn_feature_enabled && !IsD3DSharedImageSupported();

  if (IsD3DSharedImageSupported() || enable_webnn_only_d3d_factory) {
    auto d3d_factory = std::make_unique<D3DImageBackingFactory>(
        context_state_->GetD3D11Device(),
        shared_image_manager_->dxgi_shared_handle_manager(),
        context_state_->GetGLFormatCaps(), workarounds_,
        enable_webnn_only_d3d_factory);
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
        std::make_unique<ExternalVkImageBackingFactory>(
            context_state_,
            gpu_preferences_.enable_webgpu_on_vk_via_gl_interop);
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
        feature_info.get(), gpu_preferences_,
        context_state_->vk_context_provider());
    ahb_factory_ = ahb_factory.get();
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
  if (gr_context_type_ == GrContextType::kVulkan
#if BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
      /* We support GL context for WebGPU gl-vulkan interop (on linux).*/
      || gpu_preferences_.enable_webgpu_on_vk_via_gl_interop
#endif
  ) {
    auto external_vk_image_factory =
        std::make_unique<ExternalVkImageBackingFactory>(
            context_state_,
            gpu_preferences_.enable_webgpu_on_vk_via_gl_interop);
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
            texture_target_for_io_surfaces_
#else
            GL_TEXTURE_2D
#endif
        );
    factories_.push_back(std::move(iosurface_backing_factory));
  }
#endif

#if BUILDFLAG(USE_DAWN)
  // This factory will only be used with skia graphite dawn as of now. It is
  // currently not in use at all and will be soon enabled to be used with
  // CompoundSharedImage. It will be used when there is no other backing
  // which can support dawn representation. Hence it is added here at the end
  // in the list.
  if (gr_context_type_ == GrContextType::kGraphiteDawn) {
    factories_.push_back(std::make_unique<DawnImageBackingFactory>());
  }
#endif  // BUILDFLAG(USE_DAWN)
}

SharedImageFactory::~SharedImageFactory() {
  DCHECK(shared_images_.empty());
}

bool SharedImageFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    gpu::SurfaceHandle surface_handle,
    SharedImageUsageSet usage,
    std::string debug_label,
    std::optional<SharedImagePoolId> pool_id) {
  auto* factory = GetFactoryByUsage(usage, format, size,
                                    /*pixel_data=*/{}, gfx::EMPTY_BUFFER);
  if (!factory) {
    LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER, size, debug_label);
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

  return RegisterBacking(std::move(backing), std::move(pool_id));
}

// static
bool SharedImageFactory::IsNativeBufferSupported(
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    const gfx::GpuExtraInfo& gpu_extra_info) {
#if BUILDFLAG(IS_APPLE)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
      return format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kR_8 ||
             format == viz::SinglePlaneFormat::kRG_88 ||
             format == viz::SinglePlaneFormat::kR_16 ||
             format == viz::SinglePlaneFormat::kRG_1616 ||
             format == viz::SinglePlaneFormat::kRGBA_F16 ||
             format == viz::SinglePlaneFormat::kBGRA_1010102 ||
             format == viz::MultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kNV12A ||
             format == viz::MultiPlaneFormat::kP010;
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
      return false;
  }
  NOTREACHED();
#elif BUILDFLAG(IS_ANDROID)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kBGR_565;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED();
#elif BUILDFLAG(IS_OZONE)
  auto buffer_format = viz::SharedImageFormatToBufferFormat(format);
  return ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
      buffer_format, usage);
#elif BUILDFLAG(IS_WIN)
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
    case gfx::BufferUsage::SCANOUT:
      return format == viz::SinglePlaneFormat::kRGBA_8888 ||
             format == viz::SinglePlaneFormat::kRGBX_8888 ||
             format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::PROTECTED_SCANOUT:
    case gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE:
    case gfx::BufferUsage::SCANOUT_CAMERA_READ_WRITE:
    case gfx::BufferUsage::CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_VEA_CPU_READ:
    case gfx::BufferUsage::VEA_READ_CAMERA_AND_CPU_READ_WRITE:
    case gfx::BufferUsage::SCANOUT_FRONT_RENDERING:
      return false;
  }
  NOTREACHED();
#else
  return false;
#endif
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

  auto native_buffer_supported =
      IsNativeBufferSupported(format, buffer_usage, gpu_extra_info_);
  std::unique_ptr<SharedImageBacking> backing;
  if (native_buffer_supported) {
    auto* factory = GetFactoryByUsage(usage, format, size,
                                      /*pixel_data=*/{}, GetNativeBufferType());
    if (!factory) {
      LogGetFactoryFailed(usage, format, GetNativeBufferType(), size,
                          debug_label);
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
    if (SharedMemoryImageBackingFactory::IsBufferUsageSupported(buffer_usage) &&
        SharedMemoryImageBackingFactory::IsSizeValidForFormat(size, format)) {
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
        // Check if CompoundImageBacking can be created. CompoundImageBacking
        // holds
        // a shared memory buffer plus another GPU backing type to satisfy the
        // requirements.
        backing = CompoundImageBacking::Create(
            this, copy_manager(), mailbox, format, size, color_space,
            surface_origin, alpha_type, usage, debug_label, buffer_usage);
        use_compound = backing != nullptr;
      }

      if (!use_compound) {
        if (factory) {
          backing = factory->CreateSharedImage(
              mailbox, format, surface_handle, size, color_space,
              surface_origin, alpha_type, SharedImageUsageSet(usage),
              debug_label, IsSharedBetweenThreads(usage), buffer_usage);
        } else {
          LogGetFactoryFailed(usage, format, gfx::SHARED_MEMORY_BUFFER, size,
                              debug_label);
          return false;
        }
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

  SharedImageBackingFactory* const factory =
      GetFactoryByUsage(usage, format, size, data, gfx::EMPTY_BUFFER);
  if (!factory) {
    LogGetFactoryFailed(usage, format, gfx::EMPTY_BUFFER, size, debug_label);
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
    gfx::GpuMemoryBufferHandle buffer_handle,
    std::optional<SharedImagePoolId> pool_id) {
  gfx::GpuMemoryBufferType gmb_type = buffer_handle.type;

  bool use_compound = false;
  std::unique_ptr<SharedImageBacking> backing;
  SharedImageBackingFactory* factory = nullptr;
#if BUILDFLAG(IS_ANDROID)
  if (ahb_factory_ &&
      ahb_factory_->IsSupportedForMappableBuffer(usage, format, gmb_type)) {
    factory = ahb_factory_;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  if (!factory) {
    factory = GetFactoryByUsage(usage, format, size, {}, gmb_type);
  }
  if (!factory && gmb_type == gfx::SHARED_MEMORY_BUFFER &&
      !IsSharedBetweenThreads(usage)) {
    // Check if CompoundImageBacking can be created. CompoundImageBacking holds
    // a shared memory buffer plus another GPU backing type to satisfy the
    // requirements.
    backing = CompoundImageBacking::Create(
        this, copy_manager(), mailbox, buffer_handle.Clone(), format, size,
        color_space, surface_origin, alpha_type, usage, debug_label);
    use_compound = backing != nullptr;
  }

  if (!use_compound && !factory) {
    LogGetFactoryFailed(usage, format, gmb_type, size, debug_label);
    return false;
  }

  if (!use_compound) {
    backing = factory->CreateSharedImage(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        std::move(debug_label), IsSharedBetweenThreads(usage),
        std::move(buffer_handle));
  }

  if (backing) {
    DVLOG(1) << "CreateSharedImageWithBuffer[" << backing->GetName()
             << "] size=" << size.ToString()
             << " usage=" << CreateLabelForSharedImageUsage(usage)
             << " format=" << format.ToString()
             << " gmb_type=" << GmbTypeToString(gmb_type);
  }
  return RegisterBacking(std::move(backing), std::move(pool_id));
}

bool SharedImageFactory::UpdateSharedImage(const Mailbox& mailbox) {
  return UpdateSharedImage(mailbox, nullptr);
}

bool SharedImageFactory::UpdateSharedImage(
    const Mailbox& mailbox,
    std::unique_ptr<gfx::GpuFence> in_fence) {
  auto* shared_image = GetFactoryRef(mailbox);
  if (!shared_image) {
    LOG(ERROR) << "UpdateSharedImage: Could not find shared image mailbox";
    return false;
  }
  shared_image->Update(std::move(in_fence));
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
  auto* shared_image = GetFactoryRef(mailbox);
  if (!shared_image) {
    LOG(ERROR)
        << "SetSharedImagePurgeable: Could not find shared image mailbox";
    return false;
  }
  shared_image->SetPurgeable(purgeable);
  return true;
}

void SharedImageFactory::DestroyAllSharedImages(bool have_context) {
  if (!have_context) {
    for (auto& [_, shared_image] : shared_images_) {
      shared_image->OnContextLost();
    }
  }
  shared_images_.clear();
}

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::IsD3DSharedImageSupported() const {
  if (!context_state_) {
    return false;
  }

  return D3DImageBackingFactory::IsD3DSharedImageSupported(
      context_state_->GetD3D11Device().Get(), gpu_preferences_);
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
  vulkan_context_provider->GetVulkanImplementation()
      ->RegisterSysmemBufferCollection(
          device, std::move(service_handle), std::move(sysmem_token), format,
          usage, gfx::Size(), 0, register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

bool SharedImageFactory::CopyToGpuMemoryBuffer(const Mailbox& mailbox) {
  auto* shared_image = GetFactoryRef(mailbox);
  if (!shared_image) {
    DLOG(ERROR) << "CopyToGpuMemoryBuffer: Could not find shared image mailbox";
    return false;
  }
  return shared_image->CopyToGpuMemoryBuffer();
}

#if !BUILDFLAG(IS_ANDROID)
gfx::GpuMemoryBufferHandle
SharedImageFactory::CreateNativeGpuMemoryBufferHandle(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage) {
#if BUILDFLAG(IS_APPLE)
  return IOSurfaceImageBackingFactory::CreateGpuMemoryBufferHandle(size,
                                                                   format);
#elif BUILDFLAG(IS_OZONE)
  return OzoneImageBackingFactory::CreateGpuMemoryBufferHandle(
      shared_image_manager_->vulkan_context_provider(), size, format, usage);
#else
  return D3DImageBackingFactory::CreateGpuMemoryBufferHandle(
      shared_image_manager_->io_runner(), size, format, usage);
#endif
}
#endif

bool SharedImageFactory::CopyNativeBufferToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
#if BUILDFLAG(IS_WIN)
  return D3DImageBackingFactory::CopyNativeBufferToSharedMemoryAsync(
      std::move(buffer_handle), std::move(shared_memory));
#elif BUILDFLAG(IS_ANDROID)
  return AHardwareBufferImageBackingFactory::
      CopyNativeBufferToSharedMemoryAsync(std::move(buffer_handle),
                                          std::move(shared_memory));
#else
  return false;
#endif
}

#if BUILDFLAG(IS_WIN)
bool SharedImageFactory::CopyToGpuMemoryBufferAsync(
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  auto* shared_image = GetFactoryRef(mailbox);
  if (!shared_image) {
    DLOG(ERROR)
        << "CopyToGpuMemoryBufferAsync: Could not find shared image mailbox";
    return false;
  }
  shared_image->CopyToGpuMemoryBufferAsync(std::move(callback));
  return true;
}
#endif

bool SharedImageFactory::GetGpuMemoryBufferHandleInfo(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle& handle,
    gfx::BufferUsage& buffer_usage) {
  auto* shared_image = GetFactoryRef(mailbox);
  if (!shared_image) {
    LOG(ERROR)
        << "GetGpuMemoryBufferHandleInfo: Could not find shared image mailbox";
    return false;
  }
  shared_image->GetGpuMemoryBufferHandleInfo(handle, buffer_usage);
  return true;
}

bool SharedImageFactory::CreateSharedImagePool(
    const SharedImagePoolId& pool_id,
    mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote) {
  auto it = shared_image_pool_map_.find(pool_id);
  // Ensure that there is no pool already corresponding to the |pool_id|.
  if (it != shared_image_pool_map_.end()) {
    return false;
  }
  auto pool = std::make_unique<SharedImagePoolService>(
      pool_id, std::move(client_remote), this);
  shared_image_pool_map_.emplace(pool_id, std::move(pool));
  return true;
}

bool SharedImageFactory::DestroySharedImagePool(
    const SharedImagePoolId& pool_id) {
  // Ensure that there is a pool corresponding to the |pool_id|.
  return shared_image_pool_map_.erase(pool_id);
}

void SharedImageFactory::RegisterSharedImageBackingFactoryForTesting(
    SharedImageBackingFactory* factory) {
  backing_factory_for_testing_ = factory;
}

gpu::SharedImageCapabilities SharedImageFactory::MakeCapabilities() {
  gpu::SharedImageCapabilities shared_image_caps;
  shared_image_caps.supports_scanout_shared_images =
      shared_image_manager_->SupportsScanoutImages();

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
      IsNativeBufferSupported(viz::MultiPlaneFormat::kNV12,
                              gfx::BufferUsage::GPU_READ_CPU_READ_WRITE,
                              gpu_extra_info_);
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

  const bool display_compositor_on_another_thread =
      shared_image_manager_->display_context_on_another_thread();
  if (!context_state_) {
    shared_image_caps.disable_one_component_textures = false;
  } else if (context_state_->GrContextIsGL()) {
    shared_image_caps.disable_one_component_textures =
        display_compositor_on_another_thread &&
        workarounds_.avoid_one_component_egl_images;
  } else if (context_state_->GrContextIsVulkan() ||
             context_state_->IsGraphiteDawnVulkan()) {
    // Vulkan currently doesn't support single-component cross-thread shared
    // images for WebView.
    shared_image_caps.disable_one_component_textures =
        display_compositor_on_another_thread &&
        !context_state_->is_drdc_enabled();
  }

#if BUILDFLAG(IS_MAC)
  shared_image_caps.texture_target_for_io_surfaces =
      texture_target_for_io_surfaces_;
#endif

#if BUILDFLAG(IS_WIN)
  shared_image_caps.shared_image_d3d = IsD3DSharedImageSupported();
  shared_image_caps.shared_image_swap_chain =
      shared_image_caps.shared_image_d3d &&
      D3DImageBackingFactory::IsSwapChainSupported(
          gpu_preferences_, context_state_->dawn_context_provider());
#endif  // BUILDFLAG(IS_WIN)

  return shared_image_caps;
}

bool SharedImageFactory::HasSharedImage(const Mailbox& mailbox) const {
  return shared_images_.contains(mailbox);
}

base::WeakPtr<SharedImageFactory> SharedImageFactory::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void SharedImageFactory::SetGpuExtraInfo(
    const gfx::GpuExtraInfo& gpu_extra_info) {
  gpu_extra_info_ = gpu_extra_info;
}

bool SharedImageFactory::IsSharedBetweenThreads(
    gpu::SharedImageUsageSet usage) {
  // Ignore for mipmap usage.
  usage.RemoveAll(SHARED_IMAGE_USAGE_MIPMAP);

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
                                             const gfx::Size& size,
                                             const std::string& debug_label) {
  LOG(ERROR) << "Could not find SharedImageBackingFactory with params: usage: "
             << CreateLabelForSharedImageUsage(usage)
             << ", format: " << format.ToString()
             << ", share_between_threads: " << IsSharedBetweenThreads(usage)
             << ", gmb_type: " << GmbTypeToString(gmb_type)
             << ", size: " << size.ToString()
             << ", debug_label: " << debug_label;

  std::string new_debug_label = debug_label;
  // Get the debug label with Process Id for filtering crash reports by label as
  // key.
  if (debug_label.find("_Pid") != std::string::npos) {
    auto parts = base::RSplitStringOnce(debug_label, '_');
    new_debug_label = parts->first;
  }

#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/423037052): Handle offscreen canvas case for WebView where
  // we fail to find a shared image factory.
  // Suppress crashes due to this client for now.
  if (new_debug_label.find("CanvasResourceRasterGmb") != std::string::npos) {
    return;
  }
#endif
  SCOPED_CRASH_KEY_STRING64("SIFactory", "DebugLabel", new_debug_label);
  SCOPED_CRASH_KEY_STRING64("SIFactory", "Format", format.ToString());
  SCOPED_CRASH_KEY_NUMBER("SIFactory", "Usage", static_cast<uint32_t>(usage));
  SCOPED_CRASH_KEY_STRING64("SIFactory", "GMBType", GmbTypeToString(gmb_type));
  SCOPED_CRASH_KEY_STRING64("SIFactory", "Size", size.ToString());
  SCOPED_CRASH_KEY_BOOL("SIFactory", "SharedBwThreads",
                        IsSharedBetweenThreads(usage));
  // DumpWithoutCrashing to get crash reports for failure to find a shared image
  // backing factory.
  base::debug::DumpWithoutCrashing();
}

bool SharedImageFactory::RegisterBacking(
    std::unique_ptr<SharedImageBacking> backing,
    std::optional<SharedImagePoolId> pool_id) {
  if (!backing) {
    LOG(ERROR) << "CreateSharedImage: could not create backing.";
    return false;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());

  if (!shared_image) {
    LOG(ERROR) << "CreateSharedImage: could not register backing.";
    return false;
  }

  if (pool_id) {
    shared_image->SetSharedImagePoolId(pool_id.value());
  }

  gpu::Mailbox mailbox = shared_image->mailbox();
  shared_images_.emplace(std::move(mailbox), std::move(shared_image));
  return true;
}

bool SharedImageFactory::AddSecondaryReference(const gpu::Mailbox& mailbox) {
  if (shared_images_.contains(mailbox)) {
    LOG(ERROR) << "AddSecondaryReference: Can't have more than one reference.";
    return false;
  }

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->AddSecondaryReference(mailbox,
                                                   memory_type_tracker_.get());

  if (!shared_image) {
    return false;
  }

  shared_images_.emplace(mailbox, std::move(shared_image));
  return true;
}

SharedImageUsageSet SharedImageFactory::GetUsageForMailbox(
    const Mailbox& mailbox) {
  auto* shared_image = GetFactoryRef(mailbox);
  return shared_image ? shared_image->usage() : SharedImageUsageSet();
}

SharedImageRepresentationFactoryRef* SharedImageFactory::GetFactoryRef(
    const gpu::Mailbox& mailbox) const {
  auto it = shared_images_.find(mailbox);
  return it != shared_images_.end() ? it->second.get() : nullptr;
}

const scoped_refptr<SharedImageCopyManager>&
SharedImageFactory::copy_manager() {
  return copy_manager_;
}

SharedImageRepresentationFactory::SharedImageRepresentationFactory(
    SharedImageManager* manager,
    scoped_refptr<gpu::MemoryTracker> memory_tracker)
    : manager_(manager),
      memory_type_tracker_(
          std::make_unique<MemoryTypeTracker>(std::move(memory_tracker))) {}

SharedImageRepresentationFactory::~SharedImageRepresentationFactory() {
  DCHECK_EQ(0u, memory_type_tracker_->GetMemRepresented());
}

std::unique_ptr<GLTextureImageRepresentation>
SharedImageRepresentationFactory::ProduceGLTexture(const Mailbox& mailbox) {
  return manager_->ProduceGLTexture(mailbox, memory_type_tracker_.get());
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
SharedImageRepresentationFactory::ProduceGLTexturePassthrough(
    const Mailbox& mailbox) {
  return manager_->ProduceGLTexturePassthrough(mailbox,
                                               memory_type_tracker_.get());
}

std::unique_ptr<SkiaImageRepresentation>
SharedImageRepresentationFactory::ProduceSkia(
    const Mailbox& mailbox,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceSkia(mailbox, memory_type_tracker_.get(),
                               context_state);
}

std::unique_ptr<DawnImageRepresentation>
SharedImageRepresentationFactory::ProduceDawn(
    const Mailbox& mailbox,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    std::vector<wgpu::TextureFormat> view_formats,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceDawn(mailbox, memory_type_tracker_.get(), device,
                               backend_type, std::move(view_formats),
                               context_state);
}

std::unique_ptr<DawnBufferRepresentation>
SharedImageRepresentationFactory::ProduceDawnBuffer(
    const Mailbox& mailbox,
    const wgpu::Device& device,
    wgpu::BackendType backend_type,
    scoped_refptr<SharedContextState> context_state) {
  return manager_->ProduceDawnBuffer(mailbox, memory_type_tracker_.get(),
                                     device, backend_type, context_state);
}

std::unique_ptr<WebNNTensorRepresentation>
SharedImageRepresentationFactory::ProduceWebNNTensor(const Mailbox& mailbox) {
  return manager_->ProduceWebNNTensor(mailbox, memory_type_tracker_.get());
}

std::unique_ptr<OverlayImageRepresentation>
SharedImageRepresentationFactory::ProduceOverlay(const gpu::Mailbox& mailbox) {
  return manager_->ProduceOverlay(mailbox, memory_type_tracker_.get());
}

std::unique_ptr<MemoryImageRepresentation>
SharedImageRepresentationFactory::ProduceMemory(const gpu::Mailbox& mailbox) {
  return manager_->ProduceMemory(mailbox, memory_type_tracker_.get());
}

std::unique_ptr<RasterImageRepresentation>
SharedImageRepresentationFactory::ProduceRaster(const Mailbox& mailbox) {
  return manager_->ProduceRaster(mailbox, memory_type_tracker_.get());
}

#if BUILDFLAG(IS_ANDROID)
std::unique_ptr<LegacyOverlayImageRepresentation>
SharedImageRepresentationFactory::ProduceLegacyOverlay(
    const gpu::Mailbox& mailbox) {
  return manager_->ProduceLegacyOverlay(mailbox, memory_type_tracker_.get());
}
#endif

#if BUILDFLAG(ENABLE_VULKAN) && BUILDFLAG(IS_OZONE)
std::unique_ptr<VulkanImageRepresentation>
SharedImageRepresentationFactory::ProduceVulkan(
    const gpu::Mailbox& mailbox,
    gpu::VulkanDeviceQueue* vulkan_device_queue,
    gpu::VulkanImplementation& vulkan_impl,
    bool needs_detiling) {
  return manager_->ProduceVulkan(mailbox, memory_type_tracker_.get(),
                                 vulkan_device_queue, vulkan_impl,
                                 needs_detiling);
}
#endif

}  // namespace gpu
