// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/video_decode_accelerator.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
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
  NOTREACHED_IN_MIGRATION()
      << "By default deferred initialization is not supported.";
}

VideoDecodeAccelerator::~VideoDecodeAccelerator() = default;

void VideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    int32_t bitstream_id) {
  NOTREACHED_IN_MIGRATION() << "By default DecoderBuffer is not supported.";
}

bool VideoDecodeAccelerator::TryToSetupDecodeOnSeparateSequence(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner) {
  // Implementations in the process that VDA runs in must override this.
  NOTREACHED() << "This may only be called in the same process as VDA impl.";
}

void VideoDecodeAccelerator::ImportBufferForPicture(
    int32_t picture_buffer_id,
    VideoPixelFormat pixel_format,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) {
  NOTREACHED_IN_MIGRATION() << "Buffer import not supported.";
}

void VideoDecodeAccelerator::SetOverlayInfo(const OverlayInfo& overlay_info) {
  NOTREACHED_IN_MIGRATION() << "Overlays are not supported.";
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
