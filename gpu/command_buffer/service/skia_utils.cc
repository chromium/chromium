// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/skia_utils.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "gpu/config/skia_limits.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTextureCompressionType.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContextThreadSafeProxy.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "third_party/skia/include/gpu/ganesh/vk/GrVkBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "third_party/skia/include/gpu/graphite/GraphiteTypes.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
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
  skgpu::graphite::ContextOptions options;
  size_t max_resource_cache_bytes;
  size_t glyph_cache_max_texture_bytes;
  DetermineGrCacheLimitsFromAvailableMemory(&max_resource_cache_bytes,
                                            &glyph_cache_max_texture_bytes);
  options.fGlyphCacheTextureMaximumBytes = glyph_cache_max_texture_bytes;

  // Disable multisampled antialiasing when it's slow if the relevant
  // base::Feature is enabled.
  // NOTE: `workarounds.msaa_is_slow` is true on all Intel devices.
  // gpu::gles2::MSAAIsSlow() will return true on Intel devices unless the
  // features::kEnableMSAAOnNewIntelGPUs base::Feature is enabled. If rolling
  // out single-sampling for Graphite, we should consider whether to tie the
  // rollout to the features::kEnableMSSAOnNewIntelGPUs experiment.
  if (workarounds.msaa_is_slow &&
      base::FeatureList::IsEnabled(features::kDisableSlowMSAAInGraphite)) {
    options.fInternalMultisampleCount = 1;
  }

  return options;
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

  bool use_version_es2 = false;
#if BUILDFLAG(IS_ANDROID)
  use_version_es2 = base::FeatureList::IsEnabled(features::kUseGles2ForOopR);
#endif

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

  // We tell Skia to use es2 which does not have GL_R8_EXT
  if (feature_info->gl_version_info().is_es3 && use_version_es2) {
    if (internal_format == GL_R8_EXT)
      internal_format = GL_LUMINANCE8;
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
GrVkImageInfo CreateGrVkImageInfo(VulkanImage* image) {
  DCHECK(image);
  VkPhysicalDevice physical_device =
      image->device_queue()->GetVulkanPhysicalDevice();
  GrVkYcbcrConversionInfo gr_ycbcr_info = CreateGrVkYcbcrConversionInfo(
      physical_device, image->image_tiling(), image->ycbcr_info());
  GrVkAlloc alloc;
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
       image->format() == VK_FORMAT_R8G8_UNORM)) {
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

  return image_info;
}

GrVkYcbcrConversionInfo CreateGrVkYcbcrConversionInfo(
    VkPhysicalDevice physical_device,
    VkImageTiling tiling,
    const absl::optional<VulkanYCbCrInfo>& ycbcr_info) {
  if (!ycbcr_info)
    return GrVkYcbcrConversionInfo();

  VkFormat vk_format = static_cast<VkFormat>(ycbcr_info->image_format);
  VkFormatFeatureFlags format_features =
      static_cast<VkFormatFeatureFlags>(ycbcr_info->format_features);

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

  GrVkYcbcrConversionInfo gr_ycbcr_info;
  gr_ycbcr_info.fFormat = vk_format;
  gr_ycbcr_info.fExternalFormat = ycbcr_info->external_format;
  gr_ycbcr_info.fYcbcrModel = static_cast<VkSamplerYcbcrModelConversion>(
      ycbcr_info->suggested_ycbcr_model);
  gr_ycbcr_info.fYcbcrRange =
      static_cast<VkSamplerYcbcrRange>(ycbcr_info->suggested_ycbcr_range);
  gr_ycbcr_info.fXChromaOffset =
      static_cast<VkChromaLocation>(ycbcr_info->suggested_xchroma_offset),
  gr_ycbcr_info.fYChromaOffset =
      static_cast<VkChromaLocation>(ycbcr_info->suggested_ychroma_offset),
  gr_ycbcr_info.fChromaFilter = chroma_filter;
  gr_ycbcr_info.fForceExplicitReconstruction = false;
  gr_ycbcr_info.fFormatFeatures = format_features;

  return gr_ycbcr_info;
}

#endif  // BUILDFLAG(ENABLE_VULKAN)

bool ShouldVulkanSyncCpuForSkiaSubmit(
    viz::VulkanContextProvider* context_provider) {
#if BUILDFLAG(ENABLE_VULKAN)
  if (context_provider) {
    const absl::optional<uint32_t>& sync_cpu_memory_limit =
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
