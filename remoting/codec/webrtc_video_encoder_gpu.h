// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_

#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "media/video/video_encode_accelerator.h"
#include "remoting/codec/encoder_bitrate_filter.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "remoting/codec/webrtc_video_encoder_selector.h"

namespace remoting {

// A WebrtcVideoEncoder implementation utilizing the VideoEncodeAccelerator
// framework to do hardware-accelerated encoding.
// A brief explanation of how this class is initialized:
// 1. An instance of WebrtcVideoEncoderGpu is created using a static method, for
//      example CreateForH264(). The state at this point is UNINITIALIZED.
// 2. On the first encode call, the WebrtcVideoEncoder saves the incoming
//      DesktopFrame's dimensions and thunks the encode. Before returning, it
//      calls BeginInitialization().
// 3. In BeginInitialization(), the WebrtcVideoEncoderGpu constructs the
//      VideoEncodeAccelerator using the saved dimensions from the DesktopFrame.
//      If the VideoEncodeAccelerator is constructed
//      successfully, the state is then INITIALIZING. If not, the state is
//      INIITALIZATION_ERROR.
// 4. Some time later, the VideoEncodeAccelerator sets itself up and is ready
//      to encode. At this point, it calls our WebrtcVideoEncoderGpu's
//      RequireBitstreamBuffers() method. Once bitstream buffers are allocated,
//      the state is INITIALIZED.
class WebrtcVideoEncoderGpu : public WebrtcVideoEncoder,
                              public media::VideoEncodeAccelerator::Client {
 public:
  static std::unique_ptr<WebrtcVideoEncoder> CreateForH264();
  static bool IsSupportedByH264(
      const WebrtcVideoEncoderSelector::Profile& profile);

  ~WebrtcVideoEncoderGpu() override;

  // WebrtcVideoEncoder interface.
  void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
              const FrameParams& params,
              WebrtcVideoEncoder::EncodeCallback done) override;

  // VideoEncodeAccelerator::Client interface.
  void RequireBitstreamBuffers(unsigned int input_count,
                               const gfx::Size& input_coded_size,
                               size_t output_buffer_size) override;
  void BitstreamBufferReady(
      int32_t bitstream_buffer_id,
      const media::BitstreamBufferMetadata& metadata) override;
  void NotifyError(media::VideoEncodeAccelerator::Error error) override;

 private:
  enum State { UNINITIALIZED, INITIALIZING, INITIALIZED, INITIALIZATION_ERROR };

  struct OutputBuffer {
    base::UnsafeSharedMemoryRegion region;
    base::WritableSharedMemoryMapping mapping;

    bool IsValid();
  };

  explicit WebrtcVideoEncoderGpu(media::VideoCodecProfile codec_profile);

  void BeginInitialization();

  void UseOutputBitstreamBufferId(int32_t bitstream_buffer_id);

  void RunAnyPendingEncode();

  State state_;

  // Only after the first encode request do we know how large the incoming
  // frames will be. Thus, we initialize after the first encode request,
  // postponing the encode until the encoder has been initialized.
  base::OnceClosure pending_encode_;

  std::unique_ptr<media::VideoEncodeAccelerator> video_encode_accelerator_;

  base::TimeDelta previous_timestamp_;

  media::VideoCodecProfile codec_profile_;

  // Shared memory with which the VEA transfers output to WebrtcVideoEncoderGpu
  std::vector<std::unique_ptr<OutputBuffer>> output_buffers_;

  // TODO(gusss): required_input_frame_count_ is currently unused; evaluate
  // whether or not it's actually needed. This variable represents the number of
  // frames needed by the VEA before it can start producing output.
  // This may be important in the future, as the current frame scheduler for CRD
  // encodes one frame at a time - it will not send the next frame in until the
  // previous frame has been returned. It may need to be "tricked" into sending
  // in a number of start frames.
  // However, initial tests showed that, even if the VEA requested a number of
  // initial frames, it still encoded and returned the first frame before
  // getting the second frame. This may be platform-dependent - these tests were
  // done with the MediaFoundationVideoEncodeAccelerator for Windows.
  unsigned int required_input_frame_count_;

  gfx::Size input_coded_size_;
  gfx::Size input_visible_size_;

  size_t output_buffer_size_;

  base::flat_map<base::TimeDelta, WebrtcVideoEncoder::EncodeCallback>
      callbacks_;

  EncoderBitrateFilter bitrate_filter_;

  base::WeakPtrFactory<WebrtcVideoEncoderGpu> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebrtcVideoEncoderGpu);
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_GPU_H_
