// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_FILE_READER_H_
#define MEDIA_FILTERS_AUDIO_FILE_READER_H_

#include <limits>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/base/media_util.h"
#include "media/ffmpeg/ffmpeg_deleters.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_glue.h"

struct AVCodecContext;
struct AVPacket;
struct AVStream;

namespace base {
class TimeDelta;
}  // namespace base

namespace media {

class AudioBus;
class FFmpegURLProtocol;
class ScopedAVPacket;

class MEDIA_EXPORT AudioFileReader {
 public:
  // Audio file data will be read using the given protocol.
  // The AudioFileReader does not take ownership of `protocol` and
  // simply maintains a weak reference to it.
  explicit AudioFileReader(FFmpegURLProtocol* protocol);

  AudioFileReader(const AudioFileReader&) = delete;
  AudioFileReader& operator=(const AudioFileReader&) = delete;

  virtual ~AudioFileReader();

  // Open() reads the audio data format so that the sample_rate(),
  // channels(), GetDuration(), and GetNumberOfFrames() methods can be called.
  // It returns `true` on success.
  bool Open();
  void Close();

  // After a call to Open(), attempts to decode up to `packets_to_read` amount
  // of packets, updating `decoded_audio_packets` with each decoded packet in
  // order.  The caller must convert these packets into one complete set of
  // decoded audio data.  The audio data will be decoded as
  // floating-point linear PCM with a nominal range of -1.0 -> +1.0.
  //
  // Returns the number of sample-frames read, i.e. the combined size of all
  // frames in `decoded_audio_packets`.
  size_t Read(std::vector<std::unique_ptr<AudioBus>>* decoded_audio_packets,
              int packets_to_read = std::numeric_limits<int>::max());

  // These methods can be called once Open() has been called and `config_` has
  // been populated.
  int channels() const { return config_->channels(); }
  int sample_rate() const { return config_->samples_per_second(); }

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
  // the file, `output_packet` will contain the demuxed packet then.
  bool ReadPacketForTesting(AVPacket* output_packet);

  const AVStream* GetAVStreamForTesting() const { return stream(); }
  const AVCodecContext* codec_context_for_testing() const {
    return codec_context_.get();
  }

 private:
  bool OpenDemuxer();
  bool OpenDecoder();
  bool ReadPacket(AVPacket* output_packet);
  bool DecodePacket(const ScopedAVPacket& packet);
  void OnOutput(scoped_refptr<AudioBuffer> buffer);
  bool IsMp3File();

  const AVStream* stream() const;

  // Destruct `glue_` after `codec_context_`.
  std::unique_ptr<FFmpegGlue> glue_;
  std::unique_ptr<AudioDecoder> decoder_;
  std::unique_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;

  raw_ptr<FFmpegURLProtocol, DanglingUntriaged> protocol_;

  // Set once the demuxer is opened.
  std::optional<int> stream_index_;

  // Set once the decoder is opened.
  std::optional<AudioDecoderConfig> config_;

  media::NullMediaLog media_log_;

  // Last timestamp starts at a valid value of zero.
  base::TimeDelta last_packet_timestamp_;
  base::TimeDelta last_packet_duration_ = kNoTimestamp;

  // Used in `OnOutput` to report errors that should cause the entire Read() to
  // to stop.
  bool on_output_error_ = false;

  // Temporary pointer to the vector of audio buses for the current Read() call.
  raw_ptr<std::vector<std::unique_ptr<AudioBus>>> decoded_audio_packets_ =
      nullptr;
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_FILE_READER_H_
