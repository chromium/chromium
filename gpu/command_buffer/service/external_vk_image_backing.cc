// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_backing.h"

#include <utility>
#include <vector>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"

#if defined(OS_LINUX) && BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/external_vk_image_dawn_representation.h"
#endif

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(OS_LINUX)
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#endif

namespace gpu {

namespace {

static const struct {
  GLenum gl_format;
  GLenum gl_type;
  GLuint bytes_per_pixel;
} kFormatTable[] = {
    {GL_RGBA, GL_UNSIGNED_BYTE, 4},                // RGBA_8888
    {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2},       // RGBA_4444
    {GL_BGRA, GL_UNSIGNED_BYTE, 4},                // BGRA_8888
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // ALPHA_8
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // LUMINANCE_8
    {GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2},          // RGB_565
    {GL_BGR, GL_UNSIGNED_SHORT_5_6_5, 2},          // BGR_565
    {GL_ZERO, GL_ZERO, 0},                         // ETC1
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // RED_8
    {GL_RG, GL_UNSIGNED_BYTE, 2},                  // RG_88
    {GL_RED, GL_HALF_FLOAT_OES, 2},                // LUMINANCE_F16
    {GL_RGBA, GL_HALF_FLOAT_OES, 8},               // RGBA_F16
    {GL_RED, GL_UNSIGNED_SHORT, 2},                // R16_EXT
    {GL_RGBA, GL_UNSIGNED_BYTE, 4},                // RGBX_8888
    {GL_BGRA, GL_UNSIGNED_BYTE, 4},                // BGRX_8888
    {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4},  // RGBX_1010102
    {GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, 4},  // BGRX_1010102
    {GL_ZERO, GL_ZERO, 0},                         // YVU_420
    {GL_ZERO, GL_ZERO, 0},                         // YUV_420_BIPLANAR
    {GL_ZERO, GL_ZERO, 0},                         // P010
};
static_assert(base::size(kFormatTable) == (viz::RESOURCE_FORMAT_MAX + 1),
              "kFormatTable does not handle all cases.");

GrVkImageInfo CreateGrVkImageInfo(
    VkImage image,
    VkFormat vk_format,
    VkDeviceMemory memory,
    size_t memory_size,
    bool use_protected_memory,
    const GrVkYcbcrConversionInfo& gr_ycbcr_info) {
  GrVkAlloc alloc(memory, 0 /* offset */, memory_size, 0 /* flags */);
  return GrVkImageInfo(
      image, alloc, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED,
      vk_format, 1 /* levelCount */, VK_QUEUE_FAMILY_IGNORED,
      use_protected_memory ? GrProtected::kYes : GrProtected::kNo,
      gr_ycbcr_info);
}

VkResult CreateVkImage(SharedContextState* context_state,
                       VkFormat format,
                       const gfx::Size& size,
                       bool is_transfer_dst,
                       bool is_external,
                       bool use_protected_memory,
                       VkImage* image) {
  VkExternalMemoryImageCreateInfoKHR external_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .handleTypes = context_state->vk_context_provider()
                         ->GetVulkanImplementation()
                         ->GetExternalImageHandleType(),
  };

  auto usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (is_transfer_dst)
    usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  VkImageCreateInfo create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = is_external ? &external_info : nullptr,
      .flags = use_protected_memory ? VK_IMAGE_CREATE_PROTECTED_BIT : 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent = {size.width(), size.height(), 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkDevice device =
      context_state->vk_context_provider()->GetDeviceQueue()->GetVulkanDevice();
  return vkCreateImage(device, &create_info, nullptr, image);
}

uint32_t FindMemoryTypeIndex(SharedContextState* context_state,
                             const VkMemoryRequirements& requirements,
                             VkMemoryPropertyFlags flags) {
  VkPhysicalDevice physical_device = context_state->vk_context_provider()
                                         ->GetDeviceQueue()
                                         ->GetVulkanPhysicalDevice();
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);
  constexpr uint32_t kInvalidTypeIndex = 32;
  for (uint32_t i = 0; i < kInvalidTypeIndex; i++) {
    if (((1u << i) & requirements.memoryTypeBits) == 0)
      continue;
    if ((properties.memoryTypes[i].propertyFlags & flags) != flags)
      continue;
    return i;
  }
  NOTREACHED();
  return kInvalidTypeIndex;
}

class ScopedPixelStore {
 public:
  ScopedPixelStore(gl::GLApi* api, GLenum name, GLint value)
      : api_(api), name_(name), value_(value) {
    api_->glGetIntegervFn(name_, &old_value_);
    if (value_ != old_value_)
      api->glPixelStoreiFn(name_, value_);
  }
  ~ScopedPixelStore() {
    if (value_ != old_value_)
      api_->glPixelStoreiFn(name_, old_value_);
  }

 private:
  gl::GLApi* const api_;
  const GLenum name_;
  const GLint value_;
  GLint old_value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPixelStore);
};

base::Optional<WGPUTextureFormat> GetWGPUFormat(viz::ResourceFormat format) {
  switch (format) {
    case viz::RED_8:
    case viz::ALPHA_8:
    case viz::LUMINANCE_8:
      return WGPUTextureFormat_R8Unorm;
    case viz::RG_88:
      return WGPUTextureFormat_RG8Unorm;
    case viz::RGBA_8888:
      return WGPUTextureFormat_RGBA8Unorm;
    case viz::BGRA_8888:
      return WGPUTextureFormat_BGRA8Unorm;
    default:
      return {};
  }
}

}  // namespace

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::Create(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    base::span<const uint8_t> pixel_data,
    bool using_gmb) {
  VkDevice device =
      context_state->vk_context_provider()->GetDeviceQueue()->GetVulkanDevice();
  VkFormat vk_format = ToVkFormat(format);
  VkImage image;
  bool is_external = context_state->support_vulkan_external_object();
  bool is_transfer_dst = using_gmb || !pixel_data.empty() || !is_external;
  if (context_state->vk_context_provider()
          ->GetVulkanImplementation()
          ->enforce_protected_memory()) {
    usage |= SHARED_IMAGE_USAGE_PROTECTED;
  }
  VkResult result =
      CreateVkImage(context_state, vk_format, size, is_transfer_dst,
                    is_external, usage & SHARED_IMAGE_USAGE_PROTECTED, &image);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create external VkImage: " << result;
    return nullptr;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, image, &requirements);

  if (!requirements.memoryTypeBits) {
    DLOG(ERROR)
        << "Unable to find appropriate memory type for external VkImage";
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  VkExportMemoryAllocateInfoKHR external_info = {
      .sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR,
      .handleTypes = context_state->vk_context_provider()
                         ->GetVulkanImplementation()
                         ->GetExternalImageHandleType(),
  };

  VkMemoryAllocateInfo mem_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = is_external ? &external_info : nullptr,
      .allocationSize = requirements.size,
      .memoryTypeIndex = FindMemoryTypeIndex(
          context_state, requirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VkDeviceMemory memory;
  // TODO(crbug.com/932286): Allocating a separate piece of memory for every
  // VkImage might have too much overhead. It is recommended that one large
  // VkDeviceMemory be sub-allocated to multiple VkImages instead.
  result = vkAllocateMemory(device, &mem_alloc_info, nullptr, &memory);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to allocate memory for external VkImage: " << result;
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  result = vkBindImageMemory(device, image, memory, 0);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind memory to external VkImage: " << result;
    vkFreeMemory(device, memory, nullptr);
    vkDestroyImage(device, image, nullptr);
    return nullptr;
  }

  auto backing = base::WrapUnique(new ExternalVkImageBacking(
      mailbox, format, size, color_space, usage, context_state, image, memory,
      requirements.size, vk_format, command_pool, GrVkYcbcrConversionInfo(),
      GetWGPUFormat(format), mem_alloc_info.memoryTypeIndex));

  if (!pixel_data.empty()) {
    backing->WritePixels(
        pixel_data.size(), 0,
        base::BindOnce([](const void* data, size_t size,
                          void* buffer) { memcpy(buffer, data, size); },
                       pixel_data.data(), pixel_data.size()));
  }

  return backing;
}

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::CreateFromGMB(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  auto* vulkan_implementation =
      context_state->vk_context_provider()->GetVulkanImplementation();
  auto resource_format = viz::GetResourceFormat(buffer_format);
  if (vulkan_implementation->CanImportGpuMemoryBuffer(handle.type)) {
    VkDevice vk_device = context_state->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetVulkanDevice();
    VkImage vk_image = VK_NULL_HANDLE;
    VkImageCreateInfo vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
    VkDeviceSize memory_size = 0;
    base::Optional<VulkanYCbCrInfo> ycbcr_info;

    if (!vulkan_implementation->CreateImageFromGpuMemoryHandle(
            vk_device, std::move(handle), size, &vk_image, &vk_image_info,
            &vk_device_memory, &memory_size, &ycbcr_info)) {
      DLOG(ERROR) << "Failed to create VkImage from GpuMemoryHandle.";
      return nullptr;
    }

    VkFormat expected_format = ToVkFormat(resource_format);
    if (expected_format != vk_image_info.format) {
      DLOG(ERROR) << "BufferFormat doesn't match the buffer ";
      vkFreeMemory(vk_device, vk_device_memory, nullptr);
      vkDestroyImage(vk_device, vk_image, nullptr);
      return nullptr;
    }

    GrVkYcbcrConversionInfo gr_ycbcr_info =
        CreateGrVkYcbcrConversionInfo(context_state->vk_context_provider()
                                          ->GetDeviceQueue()
                                          ->GetVulkanPhysicalDevice(),
                                      vk_image_info.tiling, ycbcr_info);

    return base::WrapUnique(new ExternalVkImageBacking(
        mailbox, resource_format, size, color_space, usage, context_state,
        vk_image, vk_device_memory, memory_size, vk_image_info.format,
        command_pool, gr_ycbcr_info, GetWGPUFormat(resource_format), {}));
  }

  if (gfx::NumberOfPlanesForLinearBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return nullptr;
  }

  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
    return nullptr;

  int32_t width_in_bytes = 0;
  if (!viz::ResourceSizes::MaybeWidthInBytes(size.width(), resource_format,
                                             &width_in_bytes)) {
    DLOG(ERROR) << "ResourceSizes::MaybeWidthInBytes() failed.";
    return nullptr;
  }

  if (handle.stride < width_in_bytes) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return nullptr;
  }

  auto bits_per_pixel = viz::BitsPerPixel(resource_format);
  switch (bits_per_pixel) {
    case 64:
    case 32:
    case 16:
      if (handle.stride % (bits_per_pixel / 8) != 0) {
        DLOG(ERROR) << "Invalid GMB stride.";
        return nullptr;
      }
      break;
    case 8:
    case 4:
      break;
    case 12:
      // We are not supporting YVU420 and YUV_420_BIPLANAR format.
    default:
      NOTREACHED();
      return nullptr;
  }

  if (!handle.region.IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return nullptr;
  }

  base::CheckedNumeric<size_t> checked_size = handle.stride;
  checked_size *= size.height();
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  // Minimize the amount of address space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  size_t memory_offset =
      handle.offset % base::SysInfo::VMAllocationGranularity();
  size_t map_offset =
      base::SysInfo::VMAllocationGranularity() *
      (handle.offset / base::SysInfo::VMAllocationGranularity());
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  auto shared_memory_mapping = handle.region.MapAt(
      static_cast<off_t>(map_offset), checked_size.ValueOrDie());

  if (!shared_memory_mapping.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return nullptr;
  }

  auto backing = Create(context_state, command_pool, mailbox, resource_format,
                        size, color_space, usage, base::span<const uint8_t>(),
                        true /* using_gmb */);
  if (!backing)
    return nullptr;

  backing->InstallSharedMemory(std::move(shared_memory_mapping), handle.stride,
                               memory_offset);
  return backing;
}

ExternalVkImageBacking::ExternalVkImageBacking(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    VkImage image,
    VkDeviceMemory memory,
    size_t memory_size,
    VkFormat vk_format,
    VulkanCommandPool* command_pool,
    const GrVkYcbcrConversionInfo& ycbcr_info,
    base::Optional<WGPUTextureFormat> wgpu_format,
    base::Optional<uint32_t> memory_type_index)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         memory_size,
                         false /* is_thread_safe */),
      context_state_(context_state),
      backend_texture_(size.width(),
                       size.height(),
                       CreateGrVkImageInfo(image,
                                           vk_format,
                                           memory,
                                           memory_size,
                                           usage & SHARED_IMAGE_USAGE_PROTECTED,
                                           ycbcr_info)),
      command_pool_(command_pool),
      wgpu_format_(wgpu_format),
      memory_type_index_(memory_type_index) {}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  DCHECK(!backend_texture_.isValid());
}

bool ExternalVkImageBacking::BeginAccess(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles,
    bool is_gl) {
  if (readonly && !reads_in_progress_) {
    UpdateContent(kInVkImage);
    if (texture_)
      UpdateContent(kInGLTexture);
  }
  return BeginAccessInternal(readonly, semaphore_handles);
}

void ExternalVkImageBacking::EndAccess(bool readonly,
                                       SemaphoreHandle semaphore_handle,
                                       bool is_gl) {
  EndAccessInternal(readonly, std::move(semaphore_handle));
  if (!readonly) {
    if (use_separate_gl_texture()) {
      latest_content_ = is_gl ? kInGLTexture : kInVkImage;
    } else {
      latest_content_ = kInVkImage | kInGLTexture;
    }
  }
}

bool ExternalVkImageBacking::IsCleared() const {
  return is_cleared_;
}

void ExternalVkImageBacking::SetCleared() {
  is_cleared_ = true;
}

void ExternalVkImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  latest_content_ = kInSharedMemory;
  SetCleared();
}

void ExternalVkImageBacking::Destroy() {
  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);

  auto* fence_helper = context_state()
                           ->vk_context_provider()
                           ->GetDeviceQueue()
                           ->GetFenceHelper();
  fence_helper->EnqueueImageCleanupForSubmittedWork(image_info.fImage,
                                                    image_info.fAlloc.fMemory);
  backend_texture_ = GrBackendTexture();

  if (texture_) {
    // Ensure that a context is current before removing the ref and calling
    // glDeleteTextures.
    if (!gl::GLContext::GetCurrent())
      context_state()->MakeCurrent(nullptr, true /* need_gl */);
    texture_->RemoveLightweightRef(have_context());
  }
}

bool ExternalVkImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // It is not safe to produce a legacy mailbox because it would bypass the
  // synchronization between Vulkan and GL that is implemented in the
  // representation classes.
  return false;
}

std::unique_ptr<SharedImageRepresentationDawn>
ExternalVkImageBacking::ProduceDawn(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker,
                                    WGPUDevice wgpuDevice) {
#if defined(OS_LINUX) && BUILDFLAG(USE_DAWN)
  if (!wgpu_format_) {
    DLOG(ERROR) << "Format not supported for Dawn";
    return nullptr;
  }

  if (!memory_type_index_) {
    DLOG(ERROR) << "No type index info provided";
    return nullptr;
  }

  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);

  int memory_fd = GetMemoryFd(image_info);
  if (memory_fd < 0) {
    return nullptr;
  }

  return std::make_unique<ExternalVkImageDawnRepresentation>(
      manager, this, tracker, wgpuDevice, wgpu_format_.value(), memory_fd,
      image_info.fAlloc.fSize, memory_type_index_.value());
#else  // !defined(OS_LINUX) || !BUILDFLAG(USE_DAWN)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexture>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  if (!(usage() & SHARED_IMAGE_USAGE_GLES2)) {
    DLOG(ERROR) << "The backing is not created with GLES2 usage.";
    return nullptr;
  }

#if defined(OS_FUCHSIA)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#elif defined(OS_LINUX)
  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);
  if (!texture_) {
    gl::GLApi* api = gl::g_current_gl_context;
    GLuint memory_object = 0;
    if (!use_separate_gl_texture()) {
      int memory_fd = GetMemoryFd(image_info);
      if (memory_fd < 0) {
        return nullptr;
      }

      api->glCreateMemoryObjectsEXTFn(1, &memory_object);
      api->glImportMemoryFdEXTFn(memory_object, image_info.fAlloc.fSize,
                                 GL_HANDLE_TYPE_OPAQUE_FD_EXT, memory_fd);
    }

    GLuint internal_format = viz::TextureStorageFormat(format());
    GLint old_texture_binding = 0;
    api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &old_texture_binding);
    GLuint texture_service_id;
    api->glGenTexturesFn(1, &texture_service_id);
    api->glBindTextureFn(GL_TEXTURE_2D, texture_service_id);
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (use_separate_gl_texture()) {
      api->glTexStorage2DEXTFn(GL_TEXTURE_2D, 1, internal_format,
                               size().width(), size().height());
    } else {
      DCHECK(memory_object);
      if (internal_format == GL_BGRA8_EXT) {
        // BGRA8 internal format is not well supported, so use RGBA8 instead.
        api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, GL_RGBA8, size().width(),
                                    size().height(), memory_object, 0);
        api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
        api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
      } else {
        api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, internal_format,
                                    size().width(), size().height(),
                                    memory_object, 0);
      }
    }
    texture_ = new gles2::Texture(texture_service_id);
    texture_->SetLightweightRef();
    texture_->SetTarget(GL_TEXTURE_2D, 1);
    texture_->sampler_state_.min_filter = GL_LINEAR;
    texture_->sampler_state_.mag_filter = GL_LINEAR;
    texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (is_cleared_)
      cleared_rect = gfx::Rect(size());

    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());
    texture_->SetLevelInfo(GL_TEXTURE_2D, 0, internal_format, size().width(),
                           size().height(), 1, 0, gl_format, gl_type,
                           cleared_rect);
    texture_->SetImmutable(true, true);

    api->glBindTextureFn(GL_TEXTURE_2D, old_texture_binding);
  }
  return std::make_unique<ExternalVkImageGlRepresentation>(
      manager, this, tracker, texture_, texture_->service_id());
#else  // !defined(OS_LINUX) && !defined(OS_FUCHSIA)
#error Unsupported OS
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Passthrough command decoder is not currently used on Linux.
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
ExternalVkImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  // This backing type is only used when vulkan is enabled, so SkiaRenderer
  // should also be using Vulkan.
  DCHECK_EQ(context_state_, context_state.get());
  DCHECK(context_state->GrContextIsVulkan());
  return std::make_unique<ExternalVkImageSkiaRepresentation>(manager, this,
                                                             tracker);
}

#ifdef OS_LINUX
int ExternalVkImageBacking::GetMemoryFd(const GrVkImageInfo& image_info) {
  VkMemoryGetFdInfoKHR get_fd_info;
  get_fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
  get_fd_info.pNext = nullptr;
  get_fd_info.memory = image_info.fAlloc.fMemory;
  get_fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT_KHR;

  int memory_fd = -1;
  vkGetMemoryFdKHR(device(), &get_fd_info, &memory_fd);
  if (memory_fd < 0) {
    DLOG(ERROR) << "Unable to extract file descriptor out of external VkImage";
  }
  return memory_fd;
}
#endif

void ExternalVkImageBacking::InstallSharedMemory(
    base::WritableSharedMemoryMapping shared_memory_mapping,
    size_t stride,
    size_t memory_offset) {
  DCHECK(!shared_memory_mapping_.IsValid());
  DCHECK(shared_memory_mapping.IsValid());
  shared_memory_mapping_ = std::move(shared_memory_mapping);
  stride_ = stride;
  memory_offset_ = memory_offset;
  Update(nullptr);
}

void ExternalVkImageBacking::UpdateContent(uint32_t content_flags) {
  // Only support one backing for now.
  DCHECK(content_flags == kInVkImage || content_flags == kInGLTexture ||
         content_flags == kInSharedMemory);

  if ((latest_content_ & content_flags) == content_flags)
    return;

  if (content_flags == kInGLTexture && !use_separate_gl_texture())
    content_flags = kInVkImage;

  if (content_flags == kInVkImage) {
    if (latest_content_ & kInSharedMemory) {
      if (!shared_memory_mapping_.IsValid())
        return;
      auto pixel_data =
          shared_memory_mapping_.GetMemoryAsSpan<const uint8_t>().subspan(
              memory_offset_);
      if (!WritePixels(
              pixel_data.size(), stride_,
              base::BindOnce([](const void* data, size_t size,
                                void* buffer) { memcpy(buffer, data, size); },
                             pixel_data.data(), pixel_data.size()))) {
        return;
      }
      latest_content_ |=
          use_separate_gl_texture() ? kInVkImage : kInVkImage | kInGLTexture;
      return;
    }
    if ((latest_content_ & kInGLTexture) && use_separate_gl_texture()) {
      CopyPixelsFromGLTextureToVkImage();
      latest_content_ |= kInVkImage;
      return;
    }
  } else if (content_flags == kInGLTexture) {
    DCHECK(use_separate_gl_texture());
    if (latest_content_ & kInSharedMemory) {
      CopyPixelsFromShmToGLTexture();
    } else if (latest_content_ & kInVkImage) {
      NOTIMPLEMENTED_LOG_ONCE();
    }
  } else if (content_flags == kInSharedMemory) {
    // TODO(penghuang): read pixels back from VkImage to shared memory GMB, if
    // this feature is needed.
    NOTIMPLEMENTED_LOG_ONCE();
  }
}

bool ExternalVkImageBacking::WritePixels(size_t data_size,
                                         size_t stride,
                                         FillBufferCallback callback) {
  DCHECK(stride == 0 || size().height() * stride <= data_size);
  VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = data_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkBuffer stage_buffer = VK_NULL_HANDLE;
  // TODO: Consider reusing stage_buffer and stage_memory, if allocation causes
  // performance issue.
  VkResult result = vkCreateBuffer(device(), &buffer_create_info,
                                   nullptr /* pAllocator */, &stage_buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateBuffer() failed." << result;
    return false;
  }

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device(), stage_buffer, &memory_requirements);

  VkMemoryAllocateInfo memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = memory_requirements.size,
      .memoryTypeIndex =
          FindMemoryTypeIndex(context_state_, memory_requirements,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),

  };
  VkDeviceMemory stage_memory = VK_NULL_HANDLE;
  result = vkAllocateMemory(device(), &memory_allocate_info,
                            nullptr /* pAllocator */, &stage_memory);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkAllocateMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    return false;
  }

  result = vkBindBufferMemory(device(), stage_buffer, stage_memory,
                              0 /* memoryOffset */);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkBindBufferMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }

  void* buffer = nullptr;
  result = vkMapMemory(device(), stage_memory, 0 /* memoryOffset */, data_size,
                       0, &buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkMapMemory() failed. " << result;
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }

  std::move(callback).Run(buffer);
  vkUnmapMemory(device(), stage_memory);

  std::vector<gpu::SemaphoreHandle> handles;
  if (!BeginAccessInternal(false /* readonly */, &handles)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    vkDestroyBuffer(device(), stage_buffer, nullptr /* pAllocator */);
    vkFreeMemory(device(), stage_memory, nullptr /* pAllocator */);
    return false;
  }

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer);
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    GrVkImageInfo image_info;
    bool success = backend_texture_.getVkImageInfo(&image_info);
    DCHECK(success);
    if (image_info.fImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      command_buffer->TransitionImageLayout(
          image_info.fImage, image_info.fImageLayout,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      backend_texture_.setVkImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }
    uint32_t buffer_width =
        stride ? stride * 8 / BitsPerPixel(format()) : size().width();
    command_buffer->CopyBufferToImage(stage_buffer, image_info.fImage,
                                      buffer_width, size().height(),
                                      size().width(), size().height());
  }

  if (!need_sychronization()) {
    DCHECK(handles.empty());
    command_buffer->Submit(0, nullptr, 0, nullptr);
    EndAccessInternal(false /* readonly */, SemaphoreHandle());

    auto* fence_helper = context_state_->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(command_buffer));
    fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                       stage_memory);

    return true;
  }

  std::vector<VkSemaphore> begin_access_semaphores;
  begin_access_semaphores.reserve(handles.size() + 1);
  for (auto& handle : handles) {
    VkSemaphore semaphore = vulkan_implementation()->ImportSemaphoreHandle(
        device(), std::move(handle));
    begin_access_semaphores.emplace_back(semaphore);
  }

  VkSemaphore end_access_semaphore =
      vulkan_implementation()->CreateExternalSemaphore(device());
  command_buffer->Submit(begin_access_semaphores.size(),
                         begin_access_semaphores.data(), 1,
                         &end_access_semaphore);

  auto end_access_semphore_handle = vulkan_implementation()->GetSemaphoreHandle(
      device(), end_access_semaphore);
  EndAccessInternal(false /* readonly */,
                    std::move(end_access_semphore_handle));

  auto* fence_helper =
      context_state_->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buffer));
  begin_access_semaphores.emplace_back(end_access_semaphore);
  fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
      begin_access_semaphores);
  fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                     stage_memory);

  return true;
}

void ExternalVkImageBacking::CopyPixelsFromGLTextureToVkImage() {
  DCHECK(use_separate_gl_texture());
  DCHECK(texture_);

  DCHECK_GE(format(), 0);
  DCHECK_LE(format(), viz::RESOURCE_FORMAT_MAX);
  auto gl_format = kFormatTable[format()].gl_format;
  auto gl_type = kFormatTable[format()].gl_type;
  auto bytes_per_pixel = kFormatTable[format()].bytes_per_pixel;

  if (gl_format == GL_ZERO) {
    NOTREACHED() << "Not supported resource format=" << format();
    return;
  }

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!gl::GLContext::GetCurrent() &&
      !context_state_->MakeCurrent(nullptr, true /* needs_gl */))
    return;

  gl::GLApi* api = gl::g_current_gl_context;
  GLuint framebuffer;
  GLint old_framebuffer;
  api->glGetIntegervFn(GL_READ_FRAMEBUFFER_BINDING, &old_framebuffer);
  api->glGenFramebuffersEXTFn(1, &framebuffer);
  api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, framebuffer);
  api->glFramebufferTexture2DEXTFn(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture_->service_id(), 0);
  GLenum status = api->glCheckFramebufferStatusEXTFn(GL_READ_FRAMEBUFFER);
  DCHECK_EQ(status, static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE))
      << "CheckFramebufferStatusEXT() failed.";

  base::CheckedNumeric<size_t> checked_size = bytes_per_pixel;
  checked_size *= size().width();
  checked_size *= size().height();
  DCHECK(checked_size.IsValid());

  ScopedPixelStore pack_row_length(api, GL_PACK_ROW_LENGTH, 0);
  ScopedPixelStore pack_skip_pixels(api, GL_PACK_SKIP_PIXELS, 0);
  ScopedPixelStore pack_skip_rows(api, GL_PACK_SKIP_ROWS, 0);
  ScopedPixelStore pack_aligment(api, GL_PACK_ALIGNMENT, 1);

  WritePixels(checked_size.ValueOrDie(), 0,
              base::BindOnce(
                  [](gl::GLApi* api, const gfx::Size& size, GLenum format,
                     GLenum type, void* buffer) {
                    api->glReadPixelsFn(0, 0, size.width(), size.height(),
                                        format, type, buffer);
                    DCHECK_EQ(api->glGetErrorFn(),
                              static_cast<GLenum>(GL_NO_ERROR));
                  },
                  api, size(), gl_format, gl_type));
  api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, old_framebuffer);
  api->glDeleteFramebuffersEXTFn(1, &framebuffer);
}

void ExternalVkImageBacking::CopyPixelsFromShmToGLTexture() {
  DCHECK(use_separate_gl_texture());
  DCHECK(texture_);

  DCHECK_GE(format(), 0);
  DCHECK_LE(format(), viz::RESOURCE_FORMAT_MAX);
  auto gl_format = kFormatTable[format()].gl_format;
  auto gl_type = kFormatTable[format()].gl_type;
  auto bytes_per_pixel = kFormatTable[format()].bytes_per_pixel;

  if (gl_format == GL_ZERO) {
    NOTREACHED() << "Not supported resource format=" << format();
    return;
  }

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!gl::GLContext::GetCurrent() &&
      !context_state_->MakeCurrent(nullptr, true /* needs_gl */))
    return;

  gl::GLApi* api = gl::g_current_gl_context;
  GLint old_texture;
  api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &old_texture);
  api->glBindTextureFn(GL_TEXTURE_2D, texture_->service_id());

  base::CheckedNumeric<size_t> checked_size = bytes_per_pixel;
  checked_size *= size().width();
  checked_size *= size().height();
  DCHECK(checked_size.IsValid());

  auto pixel_data =
      shared_memory_mapping_.GetMemoryAsSpan<const uint8_t>().subspan(
          memory_offset_);
  api->glTexSubImage2DFn(GL_TEXTURE_2D, 0, 0, 0, size().width(),
                         size().height(), gl_format, gl_type,
                         pixel_data.data());
  DCHECK_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));
  api->glBindTextureFn(GL_TEXTURE_2D, old_texture);
}

bool ExternalVkImageBacking::BeginAccessInternal(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles) {
  DCHECK(semaphore_handles);
  DCHECK(semaphore_handles->empty());
  if (is_write_in_progress_) {
    DLOG(ERROR) << "Unable to begin read or write access because another write "
                   "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    DLOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    DLOG_IF(ERROR, reads_in_progress_)
        << "Concurrent reading may cause problem.";
    ++reads_in_progress_;
    // If a shared image is read repeatedly without any write access,
    // |read_semaphore_handles_| will never be consumed and released, and then
    // chrome will run out of file descriptors. To avoid this problem, we wait
    // on read semaphores for readonly access too. And in most cases, a shared
    // image is only read from one vulkan device queue, so it should not have
    // performance impact.
    // TODO(penghuang): avoid waiting on read semaphores.
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();

    // A semaphore will become unsignaled, when it has been signaled and waited,
    // so it is not safe to reuse it.
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  } else {
    is_write_in_progress_ = true;
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  }
  return true;
}

void ExternalVkImageBacking::EndAccessInternal(
    bool readonly,
    SemaphoreHandle semaphore_handle) {
  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
  } else {
    DCHECK(is_write_in_progress_);
    is_write_in_progress_ = false;
  }

  if (need_sychronization()) {
    DCHECK(semaphore_handle.is_valid());
    if (readonly) {
      read_semaphore_handles_.push_back(std::move(semaphore_handle));
    } else {
      DCHECK(!write_semaphore_handle_.is_valid());
      DCHECK(read_semaphore_handles_.empty());
      write_semaphore_handle_ = std::move(semaphore_handle);
    }
  } else {
    DCHECK(!semaphore_handle.is_valid());
  }
}

}  // namespace gpu
