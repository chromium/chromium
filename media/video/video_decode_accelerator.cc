// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_accelerator.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/logging.h"
#include "media/base/video_util.h"

namespace media {

VideoDecodeAccelerator::Config::Config() = default;
VideoDecodeAccelerator::Config::Config(const Config& config) = default;

VideoDecodeAccelerator::Config::Config(VideoCodecProfile video_codec_profile)
    : profile(video_codec_profile) {}

VideoDecodeAccelerator::Config::~Config() = default;

std::string VideoDecodeAccelerator::Config::AsHumanReadableString() const {
  std::ostringstream s;
  s << "profile: " << GetProfileName(profile) << " encrypted? "
    << (is_encrypted() ? "true" : "false");
  return s.str();
}

void VideoDecodeAccelerator::Client::NotifyInitializationComplete(
    DecoderStatus status) {
  NOTREACHED() << "By default deferred initialization is not supported.";
}

void VideoDecodeAccelerator::Client::ProvidePictureBuffersWithVisibleRect(
    uint32_t requested_num_of_buffers,
    VideoPixelFormat format,
    uint32_t textures_per_buffer,
    const gfx::Size& dimensions,
    const gfx::Rect& visible_rect,
    uint32_t texture_target) {
  if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
    // In ALLOCATE mode and when the texture target is GL_TEXTURE_EXTERNAL_OES,
    // |video_decode_accelerator_| is responsible for allocating storage for the
    // result of the decode. However, we are responsible for creating the GL
    // texture to which we'll attach the decoded image. The decoder needs the
    // buffer to be of size = coded size, but for the purposes of working with a
    // graphics API (e.g., GL), the client does not need to know about the
    // non-visible area. For example, for a 360p H.264 video with a visible
    // rectangle of 0,0,640x360, the coded size is 640x368, but the GL texture
    // that the client uses should be only 640x360. Therefore, we pass
    // |visible_rect|.size() here as the requested size of the picture buffers.
    //
    // Note that we use GetRectSizeFromOrigin() to handle the unusual case in
    // which the visible rectangle does not start at (0, 0). For example, for an
    // H.264 video with a visible rectangle of 2,2,635x360, the coded size is
    // 640x360, but the GL texture that the client uses should be 637x362. This
    // is because we want the texture to include the non-visible area to the
    // left and on the top of the visible rectangle so that the compositor can
    // calculate the UV coordinates to omit the non-visible area.
    ProvidePictureBuffers(requested_num_of_buffers, format, textures_per_buffer,
                          GetRectSizeFromOrigin(visible_rect), texture_target);
  } else {
    ProvidePictureBuffers(requested_num_of_buffers, format, textures_per_buffer,
                          dimensions, texture_target);
  }
}

gpu::SharedImageStub* VideoDecodeAccelerator::Client::GetSharedImageStub()
    const {
  return nullptr;
}

CommandBufferHelper* VideoDecodeAccelerator::Client::GetCommandBufferHelper()
    const {
  return nullptr;
}

VideoDecodeAccelerator::~VideoDecodeAccelerator() = default;

void VideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    int32_t bitstream_id) {
  NOTREACHED() << "By default DecoderBuffer is not supported.";
}

bool VideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  // Implementations in the process that VDA runs in must override this.
  LOG(FATAL) << "This may only be called in the same process as VDA impl.";
  return false;
}

void VideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  NOTREACHED() << "Buffer import not supported.";
}

void VideoDecodeAccelerator::SetOverlayInfo(const OverlayInfo& overlay_info) {
  NOTREACHED() << "Overlays are not supported.";
}

GLenum VideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_RGBA;
}

bool VideoDecodeAccelerator::SupportsSharedImagePictureBuffers() const {
  return false;
}

VideoDecodeAccelerator::TextureAllocationMode
VideoDecodeAccelerator::GetSharedImageTextureAllocationMode() const {
  return VideoDecodeAccelerator::TextureAllocationMode::kAllocateGLTextures;
}

VideoDecodeAccelerator::SupportedProfile::SupportedProfile()
    : profile(media::VIDEO_CODEC_PROFILE_UNKNOWN), encrypted_only(false) {}

VideoDecodeAccelerator::SupportedProfile::~SupportedProfile() = default;

VideoDecodeAccelerator::Capabilities::Capabilities() : flags(NO_FLAGS) {}

VideoDecodeAccelerator::Capabilities::Capabilities(const Capabilities& other) =
    default;

VideoDecodeAccelerator::Capabilities::~Capabilities() = default;

std::string VideoDecodeAccelerator::Capabilities::AsHumanReadableString()
    const {
  std::ostringstream s;
  s << "[";
  for (const SupportedProfile& sp : supported_profiles) {
    s << " " << GetProfileName(sp.profile) << ": " << sp.min_resolution.width()
      << "x" << sp.min_resolution.height() << "->" << sp.max_resolution.width()
      << "x" << sp.max_resolution.height();
  }
  s << "]";
  return s.str();
}

} // namespace media

namespace std {

void default_delete<media::VideoDecodeAccelerator>::operator()(
    media::VideoDecodeAccelerator* vda) const {
  vda->Destroy();
}

}  // namespace std
