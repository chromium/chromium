// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"

namespace gpu {

int BitsPerPixel(viz::SharedImageFormat format) {
  return viz::BitsPerPixel(format.resource_format());
}

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    return viz::BufferFormat(format.resource_format());
  }

  if (format == viz::MultiPlaneFormat::kYVU_420) {
    return gfx::BufferFormat::YVU_420;
  } else if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
    return gfx::BufferFormat::YUV_420_BIPLANAR;
  } else if (format == viz::MultiPlaneFormat::kYUVA_420_TRIPLANAR) {
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
    return viz::GLInternalFormat(format.resource_format());
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
  } else if (format == viz::MultiPlaneFormat::kYVU_420 ||
             format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR ||
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
  if (format == viz::MultiPlaneFormat::kYVU_420) {
    // Based on VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM.
    return VK_FORMAT_R8_UNORM;
  } else if (format == viz::MultiPlaneFormat::kYUV_420_BIPLANAR) {
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

// TODO (hitawala): Add support for multiplanar formats.
wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane()) {
    switch (format.resource_format()) {
      case viz::ResourceFormat::RGBA_8888:
      case viz::ResourceFormat::RGBX_8888:
        return wgpu::TextureFormat::RGBA8Unorm;
      case viz::ResourceFormat::BGRA_8888:
      case viz::ResourceFormat::BGRX_8888:
        return wgpu::TextureFormat::BGRA8Unorm;
      case viz::ResourceFormat::RED_8:
      case viz::ResourceFormat::ALPHA_8:
      case viz::ResourceFormat::LUMINANCE_8:
        return wgpu::TextureFormat::R8Unorm;
      case viz::ResourceFormat::RG_88:
        return wgpu::TextureFormat::RG8Unorm;
      case viz::ResourceFormat::RGBA_F16:
        return wgpu::TextureFormat::RGBA16Float;
      case viz::ResourceFormat::RGBA_1010102:
        return wgpu::TextureFormat::RGB10A2Unorm;
      case viz::ResourceFormat::YUV_420_BIPLANAR:
        return wgpu::TextureFormat::R8BG8Biplanar420Unorm;
      // TODO(crbug.com/1175525): Add a R8BG8A8Triplanar420Unorm
      // format for dawn.
      case viz::ResourceFormat::YUVA_420_TRIPLANAR:
      case viz::ResourceFormat::RGBA_4444:
      case viz::ResourceFormat::RGB_565:
      case viz::ResourceFormat::BGR_565:
      case viz::ResourceFormat::R16_EXT:
      case viz::ResourceFormat::RG16_EXT:
      case viz::ResourceFormat::BGRA_1010102:
      case viz::ResourceFormat::YVU_420:
      case viz::ResourceFormat::ETC1:
      case viz::ResourceFormat::LUMINANCE_F16:
      case viz::ResourceFormat::P010:
        break;
    }
    return wgpu::TextureFormat::Undefined;
  }
  NOTREACHED();
  return wgpu::TextureFormat::Undefined;
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format) {
  return static_cast<WGPUTextureFormat>(ToDawnFormat(format));
}

}  // namespace gpu
