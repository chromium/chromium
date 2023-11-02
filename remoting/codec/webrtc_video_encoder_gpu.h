// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_codecs.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {

// A WebrtcVideoEncoder implementation utilizing the VideoEncodeAccelerator
// framework to do hardware-accelerated encoding. Due to threading requirements
// when using the VEA on Windows, WebrtcVideoEncoderGpu uses an inner 'core'
// class which is run on a dedicated thread.
class WebrtcVideoEncoderGpu : public WebrtcVideoEncoder {
 public:
  struct Profile {
    gfx::Size resolution;
    int frame_rate;  // Always > 0
  };

  static std::unique_ptr<WebrtcVideoEncoder> CreateForH264();
  static bool IsSupportedByH264(const Profile& profile);

  ~WebrtcVideoEncoderGpu() override;
  WebrtcVideoEncoderGpu(const WebrtcVideoEncoderGpu&) = delete;
  WebrtcVideoEncoderGpu& operator=(const WebrtcVideoEncoderGpu&) = delete;

  // WebrtcVideoEncoder interface.
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              WebrtcVideoEncoder::EncodeCallback done) override;

 private:
  class Core;
  std::unique_ptr<Core> core_;

  explicit WebrtcVideoEncoderGpu(media::VideoCodecProfile codec_profile);

  // GPU operations are performed on this task runner. This is necessary as the
  // MF VEA must be created and destroyed on a specific thread and not a
  // sequence.
  // TODO(joedow): If we start supporting H.264 on non-Windows platforms and
  // they do not have this requirement, we can refactor this such that the
  // Windows impl runs on a dedicated thread and other platforms do not.
  scoped_refptr<base::SingleThreadTaskRunner> hw_encode_task_runner_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_
