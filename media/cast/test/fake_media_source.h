// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A fake media source that generates video and audio frames to a cast
// sender.
// This class can transcode a WebM file using FFmpeg. It can also
// generate an animation and audio of fixed frequency.

#ifndef MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_
#define MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_

#include <stdint.h>

#include "base/containers/queue.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/cast/cast_config.h"
#include "media/filters/audio_renderer_algorithm.h"
#include "media/filters/ffmpeg_demuxer.h"

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;

namespace media {

class AudioBus;
class AudioConverter;
class AudioFifo;
class AudioTimestampHelper;
class FFmpegGlue;
class FFmpegDecodingLoop;
class InMemoryUrlProtocol;
class VideoFrame;

struct ScopedPtrAVFreeContext;

namespace cast {

class AudioFrameInput;
class VideoFrameInput;
class TestAudioBusFactory;

class FakeMediaSource : public media::AudioConverter::InputCallback {
 public:
  // |task_runner| is to schedule decoding tasks.
  // |clock| is used by this source but is not owned.
  // |audio_config| is the desired audio config.
  // |video_config| is the desired video config.
  // |keep_frames| is true if all VideoFrames are saved in a queue.
  FakeMediaSource(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  const base::TickClock* clock,
                  const FrameSenderConfig& audio_config,
                  const FrameSenderConfig& video_config,
                  bool keep_frames);
  ~FakeMediaSource() final;

  // Transcode this file as the source of video and audio frames.
  // If |final_fps| is non zero then the file is played at the desired rate.
  void SetSourceFile(const base::FilePath& video_file, int final_fps);

  // Set to true to randomly change the frame size at random points in time.
  // Only applies when SetSourceFile() is not used.
  void SetVariableFrameSizeMode(bool enabled);

  void Start(scoped_refptr<AudioFrameInput> audio_frame_input,
             scoped_refptr<VideoFrameInput> video_frame_input);

  const FrameSenderConfig& get_video_config() const { return video_config_; }

  scoped_refptr<media::VideoFrame> PopOldestInsertedVideoFrame();

 private:
  bool is_transcoding_audio() const { return audio_stream_index_ >= 0; }
  bool is_transcoding_video() const { return video_stream_index_ >= 0; }

  void SendNextFrame();
  void SendNextFakeFrame();

  void UpdateNextFrameSize();

  // Return true if a frame was sent.
  bool SendNextTranscodedVideo(base::TimeDelta elapsed_time);

  // Return true if a frame was sent.
  bool SendNextTranscodedAudio(base::TimeDelta elapsed_time);

  // Helper methods to compute timestamps for the frame number specified.
  base::TimeDelta VideoFrameTime(int frame_number);

  base::TimeDelta ScaleTimestamp(base::TimeDelta timestamp);

  base::TimeDelta AudioFrameTime(int frame_number);

  // Go to the beginning of the stream.
  void Rewind();

  // Call FFmpeg to fetch one packet.
  ScopedAVPacket DemuxOnePacket(bool* audio);

  void DecodeAudio(ScopedAVPacket packet);
  bool OnNewAudioFrame(AVFrame* frame);
  void DecodeVideo(ScopedAVPacket packet);
  bool OnNewVideoFrame(AVFrame* frame);
  void Decode(bool decode_audio);

  // media::AudioConverter::InputCallback implementation.
  double ProvideInput(media::AudioBus* output_bus,
                      uint32_t frames_delayed) final;

  AVStream* av_audio_stream();
  AVStream* av_video_stream();

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const media::AudioParameters output_audio_params_;
  const FrameSenderConfig video_config_;
  const bool keep_frames_;
  bool variable_frame_size_mode_;
  gfx::Size current_frame_size_;
  base::TimeTicks next_frame_size_change_time_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;
  uint8_t synthetic_count_;
  const base::TickClock* const clock_;  // Not owned by this class.

  // Time when the stream starts.
  base::TimeTicks start_time_;

  // The following three members are used only for fake frames.
  int audio_frame_count_;  // Each audio frame is exactly 10ms.
  int video_frame_count_;
  std::unique_ptr<TestAudioBusFactory> audio_bus_factory_;

  base::MemoryMappedFile file_data_;
  std::unique_ptr<InMemoryUrlProtocol> protocol_;
  std::unique_ptr<FFmpegGlue> glue_;
  AVFormatContext* av_format_context_;

  int audio_stream_index_;
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> av_audio_context_;
  std::unique_ptr<FFmpegDecodingLoop> audio_decoding_loop_;
  AudioParameters source_audio_params_;
  double playback_rate_;

  int video_stream_index_;
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> av_video_context_;
  std::unique_ptr<FFmpegDecodingLoop> video_decoding_loop_;
  int video_frame_rate_numerator_;
  int video_frame_rate_denominator_;

  // These are used for audio resampling.
  std::unique_ptr<media::AudioConverter> audio_converter_;
  std::unique_ptr<media::AudioFifo> audio_fifo_;
  std::unique_ptr<media::AudioBus> audio_fifo_input_bus_;
  media::AudioRendererAlgorithm audio_algo_;

  // Track the timestamp of audio sent to the receiver.
  std::unique_ptr<media::AudioTimestampHelper> audio_sent_ts_;

  base::queue<scoped_refptr<VideoFrame>> video_frame_queue_;
  base::queue<scoped_refptr<VideoFrame>> inserted_video_frame_queue_;
  int64_t video_first_pts_;
  bool video_first_pts_set_;
  base::TimeDelta last_video_frame_timestamp_;

  base::queue<AudioBus*> audio_bus_queue_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeMediaSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMediaSource);
};

}  // namespace cast
}  // namespace media

#endif // MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_
