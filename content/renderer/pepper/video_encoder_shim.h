// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_
#define CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/video/video_encode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gfx {
class Size;
}

namespace content {

class PepperVideoEncoderHost;

// This class is a shim to wrap a media::cast::SoftwareVideoEncoder so that it
// can be used by PepperVideoEncoderHost in place of a
// media::VideoEncodeAccelerator. This class should be constructed, used, and
// destructed on the main (render) thread.
class VideoEncoderShim : public media::VideoEncodeAccelerator {
 public:
  explicit VideoEncoderShim(PepperVideoEncoderHost* host);
  ~VideoEncoderShim() override;

  // media::VideoEncodeAccelerator implementation.
  media::VideoEncodeAccelerator::SupportedProfiles GetSupportedProfiles()
      override;
  bool Initialize(const media::VideoEncodeAccelerator::Config& config,
                  media::VideoEncodeAccelerator::Client* client) override;
  void Encode(scoped_refptr<media::VideoFrame> frame,
              bool force_keyframe) override;
  void UseOutputBitstreamBuffer(media::BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(uint32_t bitrate,
                                       uint32_t framerate) override;
  void Destroy() override;

 private:
  class EncoderImpl;

  void OnRequireBitstreamBuffers(unsigned int input_count,
                                 const gfx::Size& input_coded_size,
                                 size_t output_buffer_size);
  void OnBitstreamBufferReady(scoped_refptr<media::VideoFrame> frame,
                              int32_t bitstream_buffer_id,
                              size_t payload_size,
                              bool key_frame);
  void OnNotifyError(media::VideoEncodeAccelerator::Error error);

  std::unique_ptr<EncoderImpl> encoder_impl_;

  PepperVideoEncoderHost* host_;

  // Task doing the encoding.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  base::WeakPtrFactory<VideoEncoderShim> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VideoEncoderShim);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_VIDEO_ENCODER_SHIM_H_
