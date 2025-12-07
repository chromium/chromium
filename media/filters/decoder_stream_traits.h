// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
#define MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/moving_window.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log_properties.h"
#include "media/base/pipeline_status.h"
#include "media/base/sample_format.h"
#include "media/base/video_decoder.h"
#include "media/filters/audio_timestamp_validator.h"

namespace media {

class AudioBuffer;
class CdmContext;
class DemuxerStream;
class VideoDecoderConfig;
class VideoFrame;

template <DemuxerStream::Type StreamType>
class DecoderStreamTraits {};

enum class PostDecodeAction { DELIVER, DROP };

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::AUDIO> {
 public:
  using OutputType = AudioBuffer;
  using DecoderType = AudioDecoder;
  using DecoderConfigType = AudioDecoderConfig;
  using InitCB = AudioDecoder::InitCB;
  using OutputCB = AudioDecoder::OutputCB;

  static const MediaLogProperty kDecoderName =
      MediaLogProperty::kAudioDecoderName;
  static const MediaLogProperty kIsPlatformDecoder =
      MediaLogProperty::kIsPlatformAudioDecoder;
  static const MediaLogProperty kIsDecryptingDemuxerStream =
      MediaLogProperty::kIsAudioDecryptingDemuxerStream;

  static std::string ToString();
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  static scoped_refptr<OutputType> CreateEOSOutput();

  DecoderStreamTraits(MediaLog* media_log,
                      ChannelLayout initial_hw_layout,
                      SampleFormat initial_hw_sample_format);

  void ReportStatistics(const StatisticsCB& statistics_cb, int bytes_decoded);
  void SetIsPlatformDecoder(bool is_platform_decoder);
  void SetIsDecryptingDemuxerStream(bool is_dds);
  void SetEncryptionType(EncryptionType decryption_type);
  void InitializeDecoder(DecoderType* decoder,
                         const DecoderConfigType& config,
                         bool low_delay,
                         CdmContext* cdm_context,
                         InitCB init_cb,
                         const OutputCB& output_cb,
                         const WaitingCB& waiting_cb);
  void OnDecoderInitialized(DecoderType* decoder,
                            InitCB cb,
                            DecoderStatus status);
  DecoderConfigType GetDecoderConfig(DemuxerStream* stream);
  void OnDecode(const DecoderBuffer& buffer);
  PostDecodeAction OnDecodeDone(OutputType* buffer);
  void OnStreamReset(DemuxerStream* stream);
  void OnOutputReady(OutputType* output);

 private:
  void OnConfigChanged(const AudioDecoderConfig& config);

  // Validates encoded timestamps match decoded output duration. MEDIA_LOG warns
  // if timestamp gaps are detected. Sufficiently large gaps can lead to AV sync
  // drift.
  std::unique_ptr<AudioTimestampValidator> audio_ts_validator_;
  raw_ptr<MediaLog> media_log_;
  // HW layout at the time pipeline was started. Will not reflect possible
  // device changes.
  ChannelLayout initial_hw_layout_;
  // HW sample format at the time pipeline was started. Will not reflect
  // possible device changes.
  SampleFormat initial_hw_sample_format_;
  PipelineStatistics stats_;
  AudioDecoderConfig config_;

  base::WeakPtr<DecoderStreamTraits<DemuxerStream::AUDIO>> weak_this_;
  base::WeakPtrFactory<DecoderStreamTraits<DemuxerStream::AUDIO>> weak_factory_{
      this};
};

template <>
class MEDIA_EXPORT DecoderStreamTraits<DemuxerStream::VIDEO> {
 public:
  using OutputType = VideoFrame;
  using DecoderType = VideoDecoder;
  using DecoderConfigType = VideoDecoderConfig;
  using InitCB = VideoDecoder::InitCB;
  using OutputCB = VideoDecoder::OutputCB;
  static const MediaLogProperty kDecoderName =
      MediaLogProperty::kVideoDecoderName;
  static const MediaLogProperty kIsPlatformDecoder =
      MediaLogProperty::kIsPlatformVideoDecoder;
  static const MediaLogProperty kIsDecryptingDemuxerStream =
      MediaLogProperty::kIsVideoDecryptingDemuxerStream;

  static std::string ToString();
  static bool NeedsBitstreamConversion(DecoderType* decoder);
  static scoped_refptr<OutputType> CreateEOSOutput();

  explicit DecoderStreamTraits(MediaLog* media_log);

  DecoderConfigType GetDecoderConfig(DemuxerStream* stream);
  void ReportStatistics(const StatisticsCB& statistics_cb, int bytes_decoded);
  void SetIsPlatformDecoder(bool is_platform_decoder);
  void SetIsDecryptingDemuxerStream(bool is_dds);
  void SetEncryptionType(EncryptionType decryption_type);
  void InitializeDecoder(DecoderType* decoder,
                         const DecoderConfigType& config,
                         bool low_delay,
                         CdmContext* cdm_context,
                         InitCB init_cb,
                         const OutputCB& output_cb,
                         const WaitingCB& waiting_cb);
  void OnDecoderInitialized(DecoderType* decoder,
                            InitCB cb,
                            DecoderStatus status);
  void OnDecode(const DecoderBuffer& buffer);
  PostDecodeAction OnDecodeDone(OutputType* buffer);
  void OnStreamReset(DemuxerStream* stream);
  void OnOutputReady(OutputType* output);

 private:
  base::TimeDelta last_keyframe_timestamp_;
  base::MovingAverage<base::TimeDelta, base::TimeDelta>
      keyframe_distance_average_;

  // Tracks the duration of incoming packets over time.
  struct FrameMetadata {
    bool should_drop = false;
    base::TimeDelta duration = kNoTimestamp;
    base::TimeTicks decode_begin_time;
  };
  base::flat_map<base::TimeDelta, FrameMetadata> frame_metadata_;

  PipelineStatistics stats_;

  VideoTransformation transform_ = kNoTransformation;

  base::WeakPtr<DecoderStreamTraits<DemuxerStream::VIDEO>> weak_this_;
  base::WeakPtrFactory<DecoderStreamTraits<DemuxerStream::VIDEO>> weak_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_TRAITS_H_
