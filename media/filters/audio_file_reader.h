// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_FILE_READER_H_
#define MEDIA_FILTERS_AUDIO_FILE_READER_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "media/base/audio_codecs.h"
#include "media/base/media_export.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/filters/ffmpeg_glue.h"

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVStream;

namespace base { class TimeDelta; }

namespace media {

class AudioBus;
class FFmpegURLProtocol;

class MEDIA_EXPORT AudioFileReader {
 public:
  // Audio file data will be read using the given protocol.
  // The AudioFileReader does not take ownership of |protocol| and
  // simply maintains a weak reference to it.
  explicit AudioFileReader(FFmpegURLProtocol* protocol);
  virtual ~AudioFileReader();

  // Open() reads the audio data format so that the sample_rate(),
  // channels(), GetDuration(), and GetNumberOfFrames() methods can be called.
  // It returns |true| on success.
  bool Open();
  void Close();

  // After a call to Open(), attempts to decode the data of |packets_to_read|,
  // updating |decodedAudioPackets| with each decoded packet in order.
  // The caller must convert these packets into one complete set of
  // decoded audio data.  The audio data will be decoded as
  // floating-point linear PCM with a nominal range of -1.0 -> +1.0.
  // Returns the number of sample-frames actually read which will
  // always be the total size of all the frames in
  // |decodedAudioPackets|.
  // If |packets_to_read| is std::numeric_limits<int>::max(), decodes the entire
  // data.
  int Read(std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
           int packets_to_read = std::numeric_limits<int>::max());

  // These methods can be called once Open() has been called.
  int channels() const { return channels_; }
  int sample_rate() const { return sample_rate_; }

  // Returns true if (an estimated) duration of the audio data is
  // known.  Must be called after Open();
  bool HasKnownDuration() const;

  // Please note that GetDuration() and GetNumberOfFrames() attempt to be
  // accurate, but are only estimates.  For some encoded formats, the actual
  // duration of the file can only be determined once all the file data has been
  // read. The Read() method returns the actual number of sample-frames it has
  // read.
  base::TimeDelta GetDuration() const;
  int GetNumberOfFrames() const;

  // The methods below are helper methods which allow AudioFileReader to double
  // as a test utility for demuxing audio files.
  // --------------------------------------------------------------------------

  // Similar to Open() but does not initialize the decoder.
  bool OpenDemuxerForTesting();

  // Returns true if a packet could be demuxed from the first audio stream in
  // the file, |output_packet| will contain the demuxed packet then.
  bool ReadPacketForTesting(AVPacket* output_packet);

  // Seeks to the given point and returns true if successful.  |seek_time| will
  // be converted to the stream's time base automatically.
  bool SeekForTesting(base::TimeDelta seek_time);

  const AVStream* GetAVStreamForTesting() const;
  const AVCodecContext* codec_context_for_testing() const {
    return codec_context_.get();
  }

 private:
  bool OpenDemuxer();
  bool OpenDecoder();
  bool ReadPacket(AVPacket* output_packet);
  bool OnNewFrame(int* total_frames,
                  std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
                  AVFrame* frame);

  // Destruct |glue_| after |codec_context_|.
  std::unique_ptr<FFmpegGlue> glue_;
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  int stream_index_;
  FFmpegURLProtocol* protocol_;
  AudioCodec audio_codec_;
  int channels_;
  int sample_rate_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_;

  DISALLOW_COPY_AND_ASSIGN(AudioFileReader);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_FILE_READER_H_
