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

// TODO (hitawala): Add support for multiplanar formats.

int BitsPerPixel(viz::SharedImageFormat format) {
  return viz::BitsPerPixel(format.resource_format());
}

gfx::BufferFormat ToBufferFormat(viz::SharedImageFormat format) {
  return viz::BufferFormat(format.resource_format());
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

bool GLSupportsFormat(viz::SharedImageFormat format) {
  if (format.is_single_plane())
    return viz::GLSupportsFormat(format.resource_format());
  // No support for multiplanar formats.
  return false;
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
  if (format.is_single_plane())
    return viz::GLDataType(format.resource_format());

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
  if (format.is_single_plane())
    return viz::GLDataFormat(format.resource_format());

  // For multiplanar formats without external sampler, GL formats are per plane.
  // For single channel planes Y, U, V, A return GL_RED_EXT.
  // For 2 channel plane UV return GL_RG_EXT.
  int num_channels = format.NumChannelsInPlane(plane_index);
  DCHECK_LE(num_channels, 2);
  return num_channels == 2 ? GL_RG_EXT : GL_RED_EXT;
}

GLenum GLInternalFormat(viz::SharedImageFormat format, int plane_index) {
  DCHECK(format.IsValidPlaneIndex(plane_index));
  if (format.is_single_plane())
    return viz::GLInternalFormat(format.resource_format());

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
  if (format.is_single_plane())
    return viz::TextureStorageFormat(format.resource_format(),
                                     use_angle_rgbx_format);

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
  return viz::HasVkFormat(format.resource_format());
}

VkFormat ToVkFormat(viz::SharedImageFormat format) {
  return viz::ToVkFormat(format.resource_format());
}
#endif

wgpu::TextureFormat ToDawnFormat(viz::SharedImageFormat format) {
  return viz::ToDawnFormat(format.resource_format());
}

WGPUTextureFormat ToWGPUFormat(viz::SharedImageFormat format) {
  return viz::ToWGPUFormat(format.resource_format());
}

}  // namespace gpu
