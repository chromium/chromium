// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/buildflag.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format_utils.h"

namespace gpu {

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return viz::SinglePlaneSharedImageFormatToBufferFormat(format);
  }

  if (format == viz::MultiPlaneFormat::kYV12) {
    return gfx::BufferFormat::YVU_420;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    return gfx::BufferFormat::YUV_420_BIPLANAR;
  } else if (format == viz::MultiPlaneFormat::kNV12A) {
    return gfx::BufferFormat::YUVA_420_TRIPLANAR;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    return gfx::BufferFormat::P010;
  }
  NOTREACHED();
  return gfx::BufferFormat::RGBA_8888;
}

SkYUVAInfo::PlaneConfig ToSkYUVAPlaneConfig(viz::SharedImageFormat format) {
  switch (format.plane_config()) {
    case viz::SharedImageFormat::PlaneConfig::kY_V_U:
      return SkYUVAInfo::PlaneConfig::kY_V_U;
    case viz::SharedImageFormat::PlaneConfig::kY_UV:
      return SkYUVAInfo::PlaneConfig::kY_UV;
    case viz::SharedImageFormat::PlaneConfig::kY_UV_A:
      return SkYUVAInfo::PlaneConfig::kY_UV_A;
  }
}

SkYUVAInfo::Subsampling ToSkYUVASubsampling(viz::SharedImageFormat format) {
  switch (format.subsampling()) {
    case viz::SharedImageFormat::Subsampling::k420:
      return SkYUVAInfo::Subsampling::k420;
  }
}

GLFormatDesc ToGLFormatDescExternalSampler(viz::SharedImageFormat format) {
  DCHECK(format.is_multi_plane());
  DCHECK(format.PrefersExternalSampler());
  const GLenum ext_format = format.HasAlpha() ? GL_RGBA : GL_RGB;
  GLFormatDesc gl_format;
  gl_format.data_type = GL_NONE;
  gl_format.data_format = ext_format;
  gl_format.image_internal_format = ext_format;
  gl_format.storage_internal_format = ext_format;
  gl_format.target = GL_TEXTURE_EXTERNAL_OES;
  return gl_format;
}

GLFormatDesc ToGLFormatDesc(viz::SharedImageFormat format,
                            int plane_index,
                            bool use_angle_rgbx_format) {
  GLFormatDesc gl_format;
  gl_format.data_type = GLDataType(format);
  gl_format.data_format = GLDataFormat(format, plane_index);
  gl_format.image_internal_format = GLInternalFormat(format, plane_index);
  gl_format.storage_internal_format =
      TextureStorageFormat(format, use_angle_rgbx_format, plane_index);
  gl_format.target = GL_TEXTURE_2D;
  return gl_format;
}

GLenum GLDataType(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return viz::GLDataType(format.resource_format());
  }

  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return GL_UNSIGNED_BYTE;
    case viz::SharedImageFormat::ChannelFormat::k10:
      return GL_UNSIGNED_SHORT;
    case viz::SharedImageFormat::ChannelFormat::k16:
      return GL_UNSIGNED_SHORT;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return GL_HALF_FLOAT_OES;
  }
}

GLenum GLDataFormat(viz::SharedImageFormat format, int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    return viz::GLDataFormat(format.resource_format());
  }

  // For multiplanar formats without external sampler, GL formats are per plane.
  // For single channel planes Y, U, V, A return GL_RED_EXT.
  // For 2 channel plane UV return GL_RG_EXT.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  return num_channels == 2 ? GL_RG_EXT : GL_RED_EXT;
}

GLenum GLInternalFormat(viz::SharedImageFormat format, int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    // In GLES2, the internal format must match the texture format. (It no
    // longer is true in GLES3, however it still holds for the BGRA extension.)
    // GL_EXT_texture_norm16 follows GLES3 semantics and only exposes a sized
    // internal format (GL_R16_EXT).
    if (format == viz::SinglePlaneFormat::kR_16) {
      return GL_R16_EXT;
    } else if (format == viz::SinglePlaneFormat::kRG_1616) {
      return GL_RG16_EXT;
    } else if (format == viz::SinglePlaneFormat::kETC1) {
      return GL_ETC1_RGB8_OES;
    } else if (format == viz::SinglePlaneFormat::kRGBA_1010102 ||
               format == viz::SinglePlaneFormat::kBGRA_1010102) {
      return GL_RGB10_A2_EXT;
    }
    return GLDataFormat(format);
  }

  // For multiplanar formats without external sampler, GL formats are per plane.
  // For single channel 8-bit planes Y, U, V, A return GL_RED_EXT.
  // For single channel 10/16-bit planes Y,  U, V, A return GL_R16_EXT.
  // For 2 channel plane 8-bit UV return GL_RG_EXT.
  // For 2 channel plane 10/16-bit UV return GL_RG16_EXT.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? GL_RG_EXT : GL_RED_EXT;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
      return num_channels == 2 ? GL_RG16_EXT : GL_R16_EXT;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? GL_RG16F_EXT : GL_R16F_EXT;
  }
}

GLenum TextureStorageFormat(viz::SharedImageFormat format,
                            bool use_angle_rgbx_format,
                            int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane()) {
    return viz::TextureStorageFormat(format.resource_format(),
                                     use_angle_rgbx_format);
  }

  // For multiplanar formats without external sampler, GL formats are per plane.
  // For single channel 8-bit planes Y, U, V, A return GL_R8_EXT.
  // For single channel 10/16-bit planes Y,  U, V, A return GL_R16_EXT.
  // For 2 channel plane 8-bit UV return GL_RG8_EXT.
  // For 2 channel plane 10/16-bit UV return GL_RG16_EXT.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  switch (format.channel_format()) {
    case viz::SharedImageFormat::ChannelFormat::k8:
      return num_channels == 2 ? GL_RG8_EXT : GL_R8_EXT;
    case viz::SharedImageFormat::ChannelFormat::k10:
    case viz::SharedImageFormat::ChannelFormat::k16:
      return num_channels == 2 ? GL_RG16_EXT : GL_R16_EXT;
    case viz::SharedImageFormat::ChannelFormat::k16F:
      return num_channels == 2 ? GL_RG16F_EXT : GL_R16F_EXT;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
bool HasVkFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return viz::HasVkFormat(format.resource_format());
  } else if (format == viz::MultiPlaneFormat::kYV12 ||
             format == viz::MultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kP010) {
    return true;
  }

  return false;
}

VkFormat ToVkFormat(viz::SharedImageFormat format, int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));

  if (format.is_single_plane()) {
    return viz::ToVkFormat(format.resource_format());
  }

  // The following SharedImageFormat constants have PrefersExternalSampler()
  // false so they create a separate VkImage per plane and return the single
  // planar equivalents.
  if (format == viz::MultiPlaneFormat::kYV12) {
    // Based on VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM.
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::MultiPlaneFormat::kNV12) {
    // Based on VK_FORMAT_G8_B8R8_2PLANE_420_UNORM.
    return plane_index == 0 ? VK_FORMAT_R8_UNORM : VK_FORMAT_R8G8_UNORM;
  } else if (format == viz::MultiPlaneFormat::kP010) {
    // Based on VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16 but using
    // 16bit unorm plane formats as they are class compatible and more widely
    // supported.
    return plane_index == 0 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R16G16_UNORM;
  }

  NOTREACHED();
  return VK_FORMAT_UNDEFINED;
}
#endif

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format) {
  if (format == viz::SinglePlaneFormat::kRGBA_8888 ||
      format == viz::SinglePlaneFormat::kRGBX_8888) {
    return wgpu::TextureFormat::RGBA8Unorm;
  } else if (format == viz::SinglePlaneFormat::kBGRA_8888 ||
             format == viz::SinglePlaneFormat::kBGRX_8888) {
    return wgpu::TextureFormat::BGRA8Unorm;
  } else if (format == viz::SinglePlaneFormat::kR_8 ||
             format == viz::SinglePlaneFormat::kALPHA_8 ||
             format == viz::SinglePlaneFormat::kLUMINANCE_8) {
    return wgpu::TextureFormat::R8Unorm;
  } else if (format == viz::SinglePlaneFormat::kRG_88) {
    return wgpu::TextureFormat::RG8Unorm;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return wgpu::TextureFormat::RGBA16Float;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return wgpu::TextureFormat::RGB10A2Unorm;
  } else if (format == viz::LegacyMultiPlaneFormat::kNV12 ||
             format == viz::MultiPlaneFormat::kNV12) {
    return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
  }

  // TODO(crbug.com/1175525): Add R8BG8A8Triplanar420Unorm format for dawn.
  // TODO(crbug.com/1445450): Add support for other multiplane formats.

  NOTREACHED();
  return wgpu::TextureFormat::Undefined;
}

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format,
                                 int plane_index) {
  CHECK(format.is_multi_plane() || format.IsLegacyMultiplanar() ||
        (plane_index == 0));

  wgpu::TextureFormat wgpu_format = ToDawnFormat(format);
  if (wgpu_format == wgpu::TextureFormat::R8BG8Biplanar420Unorm) {
    // kNV12 creates a separate image per plane and returns the single planar
    // equivalents.
    // TODO(crbug.com/1449108): The above reasoning does not hold unilaterally
    // on Android, and this function will need more information to determine the
    // correct operation to take on that platform.
#if BUILDFLAG(IS_ANDROID)
    CHECK(false);
#endif
    return plane_index == 0 ? wgpu::TextureFormat::R8Unorm
                            : wgpu::TextureFormat::RG8Unorm;
  }
  return wgpu_format;
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format) {
  return static_cast<WGPUTextureFormat>(ToDawnFormat(format));
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format, int plane_index) {
  return static_cast<WGPUTextureFormat>(ToDawnFormat(format, plane_index));
}

wgpu::TextureUsage GetSupportedDawnTextureUsage(viz::SharedImageFormat format,
                                                bool is_yuv_plane) {
  wgpu::TextureUsage usage =
      wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopySrc;
  // The below usages are not supported for multiplanar formats in Dawn.
  // TODO(crbug.com/1451784): Use read/write intent instead of format to get
  // correct usages. This needs support in Skia to loosen TextureUsage
  // validation. Alternatively, add support in Dawn for multiplanar formats to
  // be Renderable.
  if (format.is_single_plane() && !format.IsLegacyMultiplanar() &&
      !is_yuv_plane) {
    usage |= wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopyDst;
  }
  return usage;
}

WGPUTextureUsage GetSupportedWGPUTextureUsage(viz::SharedImageFormat format,
                                              bool is_yuv_plane) {
  return static_cast<WGPUTextureUsage>(
      GetSupportedDawnTextureUsage(format, is_yuv_plane));
}

skgpu::graphite::TextureInfo GetGraphiteTextureInfo(
    GrContextType gr_context_type,
    viz::SharedImageFormat format,
    int plane_index,
    bool is_yuv_plane,
    bool mipmapped) {
  if (gr_context_type == GrContextType::kGraphiteMetal) {
#if BUILDFLAG(SKIA_USE_METAL)
    return GetGraphiteMetalTextureInfo(format, plane_index, is_yuv_plane,
                                       mipmapped);
#endif
  } else {
    CHECK_EQ(gr_context_type, GrContextType::kGraphiteDawn);
#if BUILDFLAG(SKIA_USE_DAWN)
    return GetGraphiteDawnTextureInfo(format, plane_index, is_yuv_plane,
                                      mipmapped);
#endif
  }
  NOTREACHED_NORETURN();
}

#if BUILDFLAG(SKIA_USE_DAWN)
skgpu::graphite::DawnTextureInfo GetGraphiteDawnTextureInfo(
    viz::SharedImageFormat format,
    int plane_index,
    bool is_yuv_plane,
    bool mipmapped) {
  skgpu::graphite::DawnTextureInfo dawn_texture_info;
  wgpu::TextureFormat wgpu_format = ToDawnFormat(format, plane_index);
  if (wgpu_format != wgpu::TextureFormat::Undefined) {
    dawn_texture_info.fSampleCount = 1;
    dawn_texture_info.fFormat = wgpu_format;
    dawn_texture_info.fUsage =
        GetSupportedDawnTextureUsage(format, is_yuv_plane);
    dawn_texture_info.fMipmapped =
        mipmapped ? skgpu::Mipmapped::kYes : skgpu::Mipmapped::kNo;
  }
  return dawn_texture_info;
}
#endif

}  // namespace gpu
