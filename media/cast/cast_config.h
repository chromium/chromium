// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_CONFIG_H_
#define MEDIA_CAST_CAST_CONFIG_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "media/base/audio_codecs.h"
#include "media/base/video_codecs.h"

namespace media {
class VideoEncodeAccelerator;

namespace cast {

// Desired end-to-end latency.
constexpr base::TimeDelta kDefaultTargetPlayoutDelay = base::Milliseconds(400);

enum SuggestedDefaults {
  // Audio encoder bitrate.  Zero means "auto," which asks the encoder to select
  // a bitrate that dynamically adjusts to the content.  Otherwise, a constant
  // bitrate is used.
  kDefaultAudioEncoderBitrate = 0,

  // Suggested default audio sampling rate.
  kDefaultAudioSamplingRate = 48000,

  // RTP timebase for media remoting RTP streams.
  kRemotingRtpTimebase = 90000,

  // Suggested default maximum video frame rate.
  kDefaultMaxFrameRate = 30,

  // Suggested minimum and maximum video bitrates for general-purpose use (up to
  // 1080p, 30 FPS).
  kDefaultMinVideoBitrate = 300000,
  kDefaultMaxVideoBitrate = 5000000,

  // Minimum and Maximum VP8 quantizer in default configuration.
  kDefaultMaxQp = 63,
  kDefaultMinQp = 4,

  kDefaultMaxCpuSaverQp = 25,

  // Number of video buffers in default configuration (applies only to certain
  // external codecs).
  kDefaultNumberOfVideoBuffers = 1,
};

// These parameters are only for video encoders.
struct VideoCodecParams {
  VideoCodecParams();
  explicit VideoCodecParams(VideoCodec codec);
  VideoCodecParams(const VideoCodecParams& other);
  VideoCodecParams(VideoCodecParams&& other);
  VideoCodecParams& operator=(const VideoCodecParams& other);
  VideoCodecParams& operator=(VideoCodecParams&& other);
  ~VideoCodecParams();

  VideoCodec codec = VideoCodec::kUnknown;

  // When true, allows use of VideoCodec::kUnknown.  When false,
  // VideoCodec::kUnknown is not supported.
  bool enable_fake_codec_for_tests = false;

  int max_qp = kDefaultMaxQp;
  int min_qp = kDefaultMinQp;

  // The maximum |min_quantizer| set to the encoder when CPU is constrained.
  // This is a trade-off between higher resolution with lower encoding quality
  // and lower resolution with higher encoding quality. The set value indicates
  // the maximum quantizer that the encoder might produce better quality video
  // at this resolution than lowering resolution with similar CPU usage and
  // smaller quantizer. The set value has to be between |min_qp| and |max_qp|.
  // Suggested value range: [4, 30]. It is only used by software VP8 codec.
  int max_cpu_saver_qp = kDefaultMaxCpuSaverQp;

  // This field is used differently by various encoders.
  //
  // It defaults to 1.
  //
  // For VP8, this field is ignored.
  //
  // For H.264 on Mac or iOS, it controls the max number of frames the encoder
  // may hold before emitting a frame. A larger window may allow higher encoding
  // efficiency at the cost of latency and memory. Set to 0 to let the encoder
  // choose a suitable value for the platform and other encoding settings.
  int max_number_of_video_buffers_used = kDefaultNumberOfVideoBuffers;

  int number_of_encode_threads = 1;
};

// These parameters are only for audio encoders.
struct AudioCodecParams {
  AudioCodec codec = AudioCodec::kUnknown;
};

struct FrameSenderConfig {
  FrameSenderConfig();
  FrameSenderConfig(uint32_t sender_ssrc,
                    uint32_t receiver_ssrc,
                    base::TimeDelta min_playout_delay,
                    base::TimeDelta max_playout_delay,
                    bool use_hardware_encoder,
                    int rtp_timebase,
                    int channels,
                    int max_bitrate,
                    int min_bitrate,
                    int start_bitrate,
                    double max_frame_rate,
                    std::optional<VideoCodecParams> video_codec_params,
                    std::optional<AudioCodecParams> audio_codec_params);
  FrameSenderConfig(const FrameSenderConfig& other);
  FrameSenderConfig(FrameSenderConfig&& other);
  FrameSenderConfig& operator=(const FrameSenderConfig& other);
  FrameSenderConfig& operator=(FrameSenderConfig&& other);
  ~FrameSenderConfig();

  // The sender's SSRC identifier.
  uint32_t sender_ssrc = 0;

  // The receiver's SSRC identifier.
  uint32_t receiver_ssrc = 0;

  // The total amount of time between a frame's capture/recording on the sender
  // and its playback on the receiver (i.e., shown to a user).  This should be
  // set to a value large enough to give the system sufficient time to encode,
  // transmit/retransmit, receive, decode, and render; given its run-time
  // environment (sender/receiver hardware performance, network conditions,
  // etc.).
  //
  // All three delays are set to the same value due to adaptive latency
  // being disabled in Chrome.
  // TODO(crbug.com/40238532): re-enable adaptive playout dleay.
  base::TimeDelta min_playout_delay = kDefaultTargetPlayoutDelay;
  base::TimeDelta max_playout_delay = kDefaultTargetPlayoutDelay;

  bool is_audio() const { return audio_codec_params.has_value(); }
  bool is_video() const { return video_codec_params.has_value(); }
  bool is_remoting() const {
    return (audio_codec_params &&
            audio_codec() == media::AudioCodec::kUnknown) ||
           (video_codec_params && video_codec() == media::VideoCodec::kUnknown);
  }

  // If true, use an external HW encoder rather than the built-in
  // software-based one.
  bool use_hardware_encoder = false;

  // RTP timebase: The number of RTP units advanced per one second.  For audio,
  // this is the sampling rate.  For video, by convention, this is 90 kHz.
  int rtp_timebase = 0;

  // Number of channels.  For audio, this is normally 2.  For video, this must
  // be 1 as Cast does not have support for stereoscopic video.
  int channels = 0;

  // For now, only fixed bitrate is used for audio encoding. So for audio,
  // |max_bitrate| is used, and the other two will be overridden if they are not
  // equal to |max_bitrate|.
  int max_bitrate = 0;
  int min_bitrate = 0;
  int start_bitrate = 0;

  double max_frame_rate = kDefaultMaxFrameRate;

  // These are codec specific parameters for video streams only.
  std::optional<VideoCodecParams> video_codec_params;
  VideoCodec video_codec() const { return video_codec_params->codec; }

  // These are codec specific parameters for audio streams only.
  std::optional<AudioCodecParams> audio_codec_params;
  AudioCodec audio_codec() const { return audio_codec_params->codec; }
};

typedef base::OnceCallback<void(scoped_refptr<base::SingleThreadTaskRunner>,
                                std::unique_ptr<media::VideoEncodeAccelerator>)>
    ReceiveVideoEncodeAcceleratorCallback;
typedef base::RepeatingCallback<void(ReceiveVideoEncodeAcceleratorCallback)>
    CreateVideoEncodeAcceleratorCallback;
typedef base::OnceCallback<void(base::UnsafeSharedMemoryRegion)>
    ReceiveVideoEncodeMemoryCallback;

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_CONFIG_H_
