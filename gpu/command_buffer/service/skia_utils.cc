// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/graphite_image_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/skia_limits.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextureCompressionType.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/GraphiteTypes.h"
#include "third_party/skia/include/gpu/graphite/Recorder.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/vk/VulkanTypes.h"
#endif

namespace gpu {

namespace {

struct FlushCleanupContext {
  std::vector<base::OnceClosure> cleanup_tasks;
};

void CleanupAfterSkiaFlush(void* context) {
  FlushCleanupContext* flush_context =
      static_cast<FlushCleanupContext*>(context);
  for (auto& task : flush_context->cleanup_tasks) {
    std::move(task).Run();
  }
  delete flush_context;
}

void CleanupAfterGraphiteRecording(void* context, skgpu::CallbackResult) {
  CleanupAfterSkiaFlush(context);
}

template <class T>
void DeleteSkObject(SharedContextState* context_state, sk_sp<T> sk_object) {
  DCHECK(sk_object && sk_object->unique());

  if (context_state->context_lost())
    return;
  DCHECK(!context_state->gr_context()->abandoned());

  if (!context_state->GrContextIsVulkan())
    return;

#if BUILDFLAG(ENABLE_VULKAN)
  auto* fence_helper =
      context_state->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](const sk_sp<GrDirectContext>& gr_context, sk_sp<T> sk_object,
         gpu::VulkanDeviceQueue* device_queue, bool is_lost) {},
      sk_ref_sp(context_state->gr_context()), std::move(sk_object)));
#endif
}

#if BUILDFLAG(ENABLE_VULKAN)
constexpr bool VkFormatNeedsYcbcrSampler(VkFormat format) {
  return format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM ||
         format == VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM ||
         format == VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
}
#endif

}  // namespace

GrContextOptions GetDefaultGrContextOptions() {
  // If you make any changes to the GrContext::Options here that could affect
  // text rendering, make sure to match the capabilities initialized in
  // GetCapabilities and ensuring these are also used by the
  // PaintOpBufferSerializer.
  GrContextOptions options;
  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);
  options.fDisableCoverageCountingPaths = true;
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;
  // TODO(junov, csmartdalton): Find a way to control fInternalMultisampleCount
  // in a more granular way.  For OOPR-Canvas we want 8, but for other purposes,
  // a texture atlas with sample count of 4 would be sufficient
  options.fInternalMultisampleCount = 8;

  options.fSuppressMipmapSupport =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableMipmapGeneration);

  // fSupportBilerpFromGlyphAtlas is needed for Raw Draw.
  options.fSupportBilerpFromGlyphAtlas = features::IsUsingRawDraw();

  return options;
}

skgpu::graphite::ContextOptions GetDefaultGraphiteContextOptions(
    const GpuDriverBugWorkarounds& workarounds) {
  // Use the default resource cache limits used for Ganesh which is 96 MB for
  // the resource cache and 8 MB for the glyph cache. These same values also get
  // used for the GPU main and Viz compositor recorders later and the resource
  // caches are not shared so don't use the large default value of 256 MB.
  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);
  skgpu::graphite::ContextOptions options;
  options.fGpuBudgetInBytes = max_resource_cache_bytes;
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;

  // msaa_is_slow_2 excludes new Intel >= Gen 11 GPUs. We're unconditionally
  // enabling MSAA on those GPUs for Graphite instead of looking at the
  // features::kEnableMSSAOnNewIntelGPUs experiment or gles2::MSAAIsSlow().
  if (workarounds.msaa_is_slow_2) {
    // For single-sampling, currently Graphite falls back to the CPU-based
    // RasterPathAtlas, which is still a little slow and buggy now.
    options.fInternalMultisampleCount = 1;
  }

  // Disable use of cached text uploads in Recordings to improve performance.
  // NOTE: Currently Recordings are played back in the order they were
  // created so use of this option is safe. Once Recordings are replayed or
  // are played out of sequence this option should no longer be used.
  options.fDisableCachedGlyphUploads = true;

  // Always emit labels in Skia. For Dawn, we have a toggle that controls
  // whether labels are emitted to the underlying backend, which is currently
  // only enabled on Windows or DCHECK builds on other platforms. For Metal,
  // the labels are only emitted under the SK_ENABLE_MTL_DEBUG_INFO define.
  options.fSetBackendLabels = true;

  return options;
}

void DumpBackgroundGraphiteMemoryStatistics(
    const skgpu::graphite::Context* context,
    const skgpu::graphite::Recorder* recorder,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  static constexpr char kNamePurgeableSize[] = "purgeable_size";

  std::string context_dump_name =
      base::StringPrintf("skia/gpu_resources/graphite_context_0x%" PRIXPTR,
                         reinterpret_cast<uintptr_t>(context));
  MemoryAllocatorDump* context_dump =
      pmd->CreateAllocatorDump(context_dump_name);
  context_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          context->currentBudgetedBytes());
  context_dump->AddScalar(kNamePurgeableSize, MemoryAllocatorDump::kUnitsBytes,
                          context->currentPurgeableBytes());

  std::string recorder_dump_name = base::StringPrintf(
      "skia/gpu_resources/gpu_main_graphite_recorder_0x%" PRIXPTR,
      reinterpret_cast<uintptr_t>(recorder));
  MemoryAllocatorDump* recorder_dump =
      pmd->CreateAllocatorDump(recorder_dump_name);
  recorder_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                           MemoryAllocatorDump::kUnitsBytes,
                           recorder->currentBudgetedBytes());
  recorder_dump->AddScalar(kNamePurgeableSize, MemoryAllocatorDump::kUnitsBytes,
                           recorder->currentPurgeableBytes());

  // The ImageProvider's bytes are not included in the recorder's budgeted
  // bytes as they are owned by Chrome, so dump them separately.
  const auto* image_provider = static_cast<const gpu::GraphiteImageProvider*>(
      recorder->clientImageProvider());
  std::string image_provider_dump_name = base::StringPrintf(
      "skia/gpu_resources/gpu_main_graphite_image_provider_0x%" PRIXPTR,
      reinterpret_cast<uintptr_t>(image_provider));
  MemoryAllocatorDump* image_provider_dump =
      pmd->CreateAllocatorDump(image_provider_dump_name);
  image_provider_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                                 MemoryAllocatorDump::kUnitsBytes,
                                 image_provider->CurrentSizeInBytes());
}

GLuint GetGrGLBackendTextureFormat(
    const gles2::FeatureInfo* feature_info,
    GLenum gl_storage_format,
    sk_sp<GrContextThreadSafeProxy> gr_context_thread_safe) {
  // TODO(hitawala): Internalize the skia version specifics to a
  // SharedImageFormat util function after getting the TextureStorageFormat.
  const gl::GLVersionInfo* version_info = &feature_info->gl_version_info();
  GLuint internal_format =
      gl::GetInternalFormat(version_info, gl_storage_format);

  // Use R8 and R16F when using later GLs where ALPHA8, LUMINANCE8, ALPHA16F and
  // LUMINANCE16F are deprecated
  if (feature_info->gl_version_info().NeedsLuminanceAlphaEmulation()) {
    switch (internal_format) {
      case GL_ALPHA8_EXT:
      case GL_LUMINANCE8:
        internal_format = GL_R8_EXT;
        break;
      case GL_ALPHA16F_EXT:
      case GL_LUMINANCE16F_EXT:
        internal_format = GL_R16F_EXT;
        break;
    }
  }

  // Map ETC1 to ETC2 type depending on conversion by skia
  if (gl_storage_format == GL_ETC1_RGB8_OES) {
    GrGLFormat gr_gl_format = GrBackendFormats::AsGLFormat(
        gr_context_thread_safe->compressedBackendFormat(
            SkTextureCompressionType::kETC1_RGB8));
    if (gr_gl_format == GrGLFormat::kCOMPRESSED_ETC1_RGB8) {
      internal_format = GL_ETC1_RGB8_OES;
    } else if (gr_gl_format == GrGLFormat::kCOMPRESSED_RGB8_ETC2) {
      internal_format = GL_COMPRESSED_RGB8_ETC2;
    }
  }

  return internal_format;
}

bool GetGrBackendTexture(const gles2::FeatureInfo* feature_info,
                         GLenum target,
                         const gfx::Size& size,
                         GLuint service_id,
                         GLenum gl_storage_format,
                         sk_sp<GrContextThreadSafeProxy> gr_context_thread_safe,
                         GrBackendTexture* gr_texture) {
  if (target != GL_TEXTURE_2D && target != GL_TEXTURE_RECTANGLE_ARB &&
      target != GL_TEXTURE_EXTERNAL_OES) {
    LOG(ERROR) << "GetGrBackendTexture: invalid texture target.";
    return false;
  }

  GrGLTextureInfo texture_info;
  texture_info.fID = service_id;
  texture_info.fTarget = target;
  texture_info.fFormat = GetGrGLBackendTextureFormat(
      feature_info, gl_storage_format, gr_context_thread_safe);
  *gr_texture = GrBackendTextures::MakeGL(size.width(), size.height(),
                                          skgpu::Mipmapped::kNo, texture_info);
  return true;
}

void AddCleanupTaskForSkiaFlush(base::OnceClosure task,
                                GrFlushInfo* flush_info) {
  FlushCleanupContext* context;
  if (!flush_info->fFinishedProc) {
    CHECK(!flush_info->fFinishedContext);
    flush_info->fFinishedProc = &CleanupAfterSkiaFlush;
    context = new FlushCleanupContext();
    flush_info->fFinishedContext = context;
  } else {
    CHECK_EQ(flush_info->fFinishedProc, &CleanupAfterSkiaFlush);
    CHECK(flush_info->fFinishedContext);
    context = static_cast<FlushCleanupContext*>(flush_info->fFinishedContext);
  }
  context->cleanup_tasks.push_back(std::move(task));
}

void AddCleanupTaskForGraphiteRecording(
    base::OnceClosure task,
    skgpu::graphite::InsertRecordingInfo* info) {
  FlushCleanupContext* context;
  if (!info->fFinishedProc) {
    CHECK(!info->fFinishedContext);
    info->fFinishedProc = &CleanupAfterGraphiteRecording;
    context = new FlushCleanupContext();
    info->fFinishedContext = context;
  } else {
    CHECK_EQ(info->fFinishedProc, &CleanupAfterGraphiteRecording);
    CHECK(info->fFinishedContext);
    context = static_cast<FlushCleanupContext*>(info->fFinishedContext);
  }
  context->cleanup_tasks.push_back(std::move(task));
}

void AddVulkanCleanupTaskForSkiaFlush(
    viz::VulkanContextProvider* context_provider,
    GrFlushInfo* flush_info) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (context_provider) {
    auto task = context_provider->GetDeviceQueue()
                    ->GetFenceHelper()
                    ->CreateExternalCallback();
    if (task)
      AddCleanupTaskForSkiaFlush(std::move(task), flush_info);
  }
#endif
}

void DeleteGrBackendTexture(SharedContextState* context_state,
                            GrBackendTexture* backend_texture) {
  DCHECK(backend_texture && backend_texture->isValid());

  if (context_state->context_lost())
    return;
  DCHECK(!context_state->gr_context()->abandoned());

  if (!context_state->GrContextIsVulkan()) {
    DCHECK(context_state->gr_context());
    context_state->gr_context()->deleteBackendTexture(
        std::move(*backend_texture));
    return;
  }

#if BUILDFLAG(ENABLE_VULKAN)
  auto* fence_helper =
      context_state->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](const sk_sp<GrDirectContext>& gr_context,
         GrBackendTexture backend_texture, gpu::VulkanDeviceQueue* device_queue,
         bool is_lost) {
        if (!gr_context->abandoned())
          gr_context->deleteBackendTexture(std::move(backend_texture));
      },
      sk_ref_sp(context_state->gr_context()), std::move(*backend_texture)));
#endif
}

void DeleteSkImage(SharedContextState* context_state, sk_sp<SkImage> sk_image) {
  DeleteSkObject(context_state, std::move(sk_image));
}

void DeleteSkSurface(SharedContextState* context_state,
                     sk_sp<SkSurface> sk_surface) {
  DeleteSkObject(context_state, std::move(sk_surface));
}

#if BUILDFLAG(ENABLE_VULKAN)
GrVkImageInfo CreateGrVkImageInfo(VulkanImage* image,
                                  const viz::SharedImageFormat& si_format,
                                  const gfx::ColorSpace& color_space) {
  DCHECK(image);
  VkPhysicalDevice physical_device =
      image->device_queue()->GetVulkanPhysicalDevice();
  skgpu::VulkanYcbcrConversionInfo gr_ycbcr_info =
      CreateVulkanYcbcrConversionInfo(physical_device, image->image_tiling(),
                                      image->format(), si_format, color_space,
                                      image->ycbcr_info());
  skgpu::VulkanAlloc alloc;
  alloc.fMemory = image->device_memory();
  alloc.fOffset = 0;
  alloc.fSize = image->device_size();
  alloc.fFlags = 0;

  bool is_protected = image->flags() & VK_IMAGE_CREATE_PROTECTED_BIT;
  GrVkImageInfo image_info;
  image_info.fImage = image->image();
  image_info.fAlloc = alloc;
  // TODO(hitawala, https://crbug.com/1310028): Skia assumes that all VkImages
  // with DRM modifier extensions are only for reads. When using Vulkan with
  // OzoneImageBackings on Skia, when importing buffer we create SkSurface and
  // write to it which fails. To fix this, we add checks for tiling with DRM
  // modifiers and set it to optimal. This will be removed once skia adds write
  // support.
  if (image->image_tiling() == VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT &&
      (image->format() == VK_FORMAT_R8G8B8A8_UNORM ||
       image->format() == VK_FORMAT_R8G8B8_UNORM ||
       image->format() == VK_FORMAT_B8G8R8A8_UNORM ||
       image->format() == VK_FORMAT_B8G8R8_UNORM ||
       image->format() == VK_FORMAT_R8_UNORM ||
       image->format() == VK_FORMAT_R8G8_UNORM ||
       image->format() == VK_FORMAT_R5G6B5_UNORM_PACK16)) {
    image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
  } else {
    image_info.fImageTiling = image->image_tiling();
  }
  image_info.fImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.fFormat = image->format();
  image_info.fImageUsageFlags = image->usage();
  image_info.fSampleCount = 1;
  image_info.fLevelCount = 1;
  image_info.fCurrentQueueFamily = image->queue_family_index();
  image_info.fProtected = is_protected ? GrProtected::kYes : GrProtected::kNo;
  image_info.fYcbcrConversionInfo = gr_ycbcr_info;

  // Skia currently requires all wrapped VkImages to have transfer src and dst
  // usage. Note, that driver _should_ advertise transfer support if any usage
  // is supported, but spec hasn't updated yet:
  // https://github.com/KhronosGroup/Vulkan-Docs/issues/1223#issuecomment-1379078493
  image_info.fImageUsageFlags |=
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  return image_info;
}

GPU_GLES2_EXPORT skgpu::VulkanYcbcrConversionInfo
CreateVulkanYcbcrConversionInfo(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    VkFormat format,
    const viz::SharedImageFormat& si_format,
    const gfx::ColorSpace& color_space,
    const std::optional<VulkanYCbCrInfo>& ycbcr_info) {
  auto valid_ycbcr_info = ycbcr_info;
  if (!valid_ycbcr_info) {
    if (!VkFormatNeedsYcbcrSampler(format)) {
      return skgpu::VulkanYcbcrConversionInfo();
    }

    // YCbCr sampler is required.
    VkSamplerYcbcrModelConversion ycbcr_model =
        (color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::BT709)
            ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
            : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
    VkSamplerYcbcrRange ycbcr_range =
        (color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL)
            ? VK_SAMPLER_YCBCR_RANGE_ITU_FULL
            : VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;

    valid_ycbcr_info.emplace(format, 0, ycbcr_model, ycbcr_range,
                             VK_CHROMA_LOCATION_COSITED_EVEN,
                             VK_CHROMA_LOCATION_COSITED_EVEN, 0);
  }

  VkFormat vk_format = static_cast<VkFormat>(valid_ycbcr_info->image_format);
  VkFormatFeatureFlags format_features =
      static_cast<VkFormatFeatureFlags>(valid_ycbcr_info->format_features);

  // |format_features| is expected to be set for external images. For regular
  // (non-external) images it may be set to 0. In that case we need to get the
  // value from Vulkan.
  if (format_features == 0) {
    DCHECK_NE(vk_format, 0);
    VkFormatProperties format_props = {};

    // vkGetPhysicalDeviceFormatProperties() is safe to call on any thread.
    vkGetPhysicalDeviceFormatProperties(physical_device, vk_format,
                                        &format_props);
    format_features = (tiling == VK_IMAGE_TILING_LINEAR)
                          ? format_props.linearTilingFeatures
                          : format_props.optimalTilingFeatures;
  }

  // As per the spec here [1], if the format does not support
  // VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT,
  // chromaFilter must be VK_FILTER_NEAREST.
  // [1] -
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkSamplerYcbcrConversionCreateInfo.html.
  VkFilter chroma_filter =
      (format_features &
       VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT)
          ? VK_FILTER_LINEAR
          : VK_FILTER_NEAREST;

  skgpu::VulkanYcbcrConversionInfo gr_ycbcr_info;
  gr_ycbcr_info.fFormat = vk_format;
  gr_ycbcr_info.fExternalFormat = valid_ycbcr_info->external_format;
  gr_ycbcr_info.fYcbcrModel = static_cast<VkSamplerYcbcrModelConversion>(
      valid_ycbcr_info->suggested_ycbcr_model);
  gr_ycbcr_info.fYcbcrRange =
      static_cast<VkSamplerYcbcrRange>(valid_ycbcr_info->suggested_ycbcr_range);
  gr_ycbcr_info.fXChromaOffset =
      static_cast<VkChromaLocation>(valid_ycbcr_info->suggested_xchroma_offset),
  gr_ycbcr_info.fYChromaOffset =
      static_cast<VkChromaLocation>(valid_ycbcr_info->suggested_ychroma_offset),
  gr_ycbcr_info.fChromaFilter = chroma_filter;
  gr_ycbcr_info.fForceExplicitReconstruction = false;
  gr_ycbcr_info.fFormatFeatures = format_features;

  if (!gr_ycbcr_info.fExternalFormat &&
      (si_format.is_multi_plane() &&
       si_format.plane_config() ==
           viz::SharedImageFormat::PlaneConfig::kY_V_U)) {
    switch (vk_format) {
      case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
      case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
      case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        gr_ycbcr_info.fComponents.r = VK_COMPONENT_SWIZZLE_B;
        gr_ycbcr_info.fComponents.b = VK_COMPONENT_SWIZZLE_R;
        break;
      default:
        break;
    }
  }

  return gr_ycbcr_info;
}

#endif  // BUILDFLAG(ENABLE_VULKAN)

bool ShouldVulkanSyncCpuForSkiaSubmit(
    viz::VulkanContextProvider* context_provider) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (context_provider) {
    const std::optional<uint32_t>& sync_cpu_memory_limit =
        context_provider->GetSyncCpuMemoryLimit();
    if (sync_cpu_memory_limit.has_value()) {
      uint64_t total_allocated_bytes =
          gpu::vma::GetTotalAllocatedAndUsedMemory(
              context_provider->GetDeviceQueue()->vma_allocator())
              .first;
      if (total_allocated_bytes > sync_cpu_memory_limit.value()) {
        return true;
      }
    }
  }
#endif
  return false;
}

uint64_t GrBackendTextureTracingID(const GrBackendTexture& backend_texture) {
  switch (backend_texture.backend()) {
    case GrBackendApi::kOpenGL: {
      GrGLTextureInfo tex_info;
      if (GrBackendTextures::GetGLTextureInfo(backend_texture, &tex_info)) {
        return tex_info.fID;
      }
      break;
    }
#if BUILDFLAG(ENABLE_VULKAN)
    case GrBackendApi::kVulkan: {
      GrVkImageInfo image_info;
      if (GrBackendTextures::GetVkImageInfo(backend_texture, &image_info)) {
        return reinterpret_cast<uint64_t>(image_info.fImage);
      }
      break;
    }
#endif
    default:
      break;
  }
  return 0;
}

}  // namespace gpu
