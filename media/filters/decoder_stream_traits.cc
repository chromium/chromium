// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/decoder_stream_traits.h"

#include <limits>
#include <memory>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"

namespace media {

// Audio decoder stream traits implementation.

// static
std::string DecoderStreamTraits<DemuxerStream::AUDIO>::ToString() {
  return "audio";
}

// static
bool DecoderStreamTraits<DemuxerStream::AUDIO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

// static
scoped_refptr<DecoderStreamTraits<DemuxerStream::AUDIO>::OutputType>
DecoderStreamTraits<DemuxerStream::AUDIO>::CreateEOSOutput() {
  return OutputType::CreateEOSBuffer();
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::SetIsPlatformDecoder(
    bool is_platform_decoder) {
  stats_.audio_pipeline_info.is_platform_decoder = is_platform_decoder;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::SetIsDecryptingDemuxerStream(
    bool is_dds) {
  stats_.audio_pipeline_info.has_decrypting_demuxer_stream = is_dds;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::SetEncryptionType(
    EncryptionType encryption_type) {
  stats_.audio_pipeline_info.encryption_type = encryption_type;
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderStreamTraits(
    MediaLog* media_log,
    ChannelLayout initial_hw_layout,
    SampleFormat initial_hw_sample_format)
    : media_log_(media_log),
      initial_hw_layout_(initial_hw_layout),
      initial_hw_sample_format_(initial_hw_sample_format) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

DecoderStreamTraits<DemuxerStream::AUDIO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::AUDIO>::GetDecoderConfig(
    DemuxerStream* stream) {
  auto config = stream->audio_decoder_config();
  // Demuxer is not aware of hw layout, so we set it here.
  config.set_target_output_channel_layout(initial_hw_layout_);
  config.set_target_output_sample_format(initial_hw_sample_format_);
  return config;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  stats_.audio_bytes_decoded = bytes_decoded;
  statistics_cb.Run(stats_);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool /* low_delay */,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb) {
  DCHECK(config.IsValidConfig());

  if (config_.IsValidConfig() && !config_.Matches(config))
    OnConfigChanged(config);
  config_ = config;

  stats_.audio_pipeline_info.decoder_type = AudioDecoderType::kUnknown;
  // Both |this| and |decoder| are owned by a DecoderSelector and will stay
  // alive at least until |init_cb| is finished executing.
  decoder->Initialize(
      config, cdm_context,
      base::BindOnce(
          &DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecoderInitialized,
          weak_this_, base::Unretained(decoder), std::move(init_cb)),
      output_cb, waiting_cb);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecoderInitialized(
    DecoderType* decoder,
    InitCB cb,
    DecoderStatus result) {
  if (result.is_ok())
    stats_.audio_pipeline_info.decoder_type = decoder->GetDecoderType();
  std::move(cb).Run(result);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  // Stream is likely being seeked to a new timestamp, so make new validator to
  // build new timestamp expectations.
  audio_ts_validator_ = std::make_unique<AudioTimestampValidator>(
      stream->audio_decoder_config(), media_log_);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecode(
    const DecoderBuffer& buffer) {
  audio_ts_validator_->CheckForTimestampGap(buffer);
}

PostDecodeAction DecoderStreamTraits<DemuxerStream::AUDIO>::OnDecodeDone(
    OutputType* buffer) {
  audio_ts_validator_->RecordOutputDuration(*buffer);
  return PostDecodeAction::DELIVER;
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnConfigChanged(
    const DecoderConfigType& config) {
  // Reset validator with the latest config. Also ensures that we do not attempt
  // to match timestamps across config boundaries.
  audio_ts_validator_ =
      std::make_unique<AudioTimestampValidator>(config, media_log_);
}

void DecoderStreamTraits<DemuxerStream::AUDIO>::OnOutputReady(
    OutputType* buffer) {}

// Video decoder stream traits implementation.

// static
std::string DecoderStreamTraits<DemuxerStream::VIDEO>::ToString() {
  return "video";
}

// static
bool DecoderStreamTraits<DemuxerStream::VIDEO>::NeedsBitstreamConversion(
    DecoderType* decoder) {
  return decoder->NeedsBitstreamConversion();
}

// static
scoped_refptr<DecoderStreamTraits<DemuxerStream::VIDEO>::OutputType>
DecoderStreamTraits<DemuxerStream::VIDEO>::CreateEOSOutput() {
  return OutputType::CreateEOSFrame();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::SetIsPlatformDecoder(
    bool is_platform_decoder) {
  stats_.video_pipeline_info.is_platform_decoder = is_platform_decoder;
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::SetIsDecryptingDemuxerStream(
    bool is_dds) {
  stats_.video_pipeline_info.has_decrypting_demuxer_stream = is_dds;
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::SetEncryptionType(
    EncryptionType encryption_type) {
  stats_.video_pipeline_info.encryption_type = encryption_type;
}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderStreamTraits(
    MediaLog* media_log)
    // Randomly selected number of samples to keep.
    : keyframe_distance_average_(16) {
  weak_this_ = weak_factory_.GetWeakPtr();
}

DecoderStreamTraits<DemuxerStream::VIDEO>::DecoderConfigType
DecoderStreamTraits<DemuxerStream::VIDEO>::GetDecoderConfig(
    DemuxerStream* stream) {
  return stream->video_decoder_config();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::ReportStatistics(
    const StatisticsCB& statistics_cb,
    int bytes_decoded) {
  stats_.video_bytes_decoded = bytes_decoded;

  if (keyframe_distance_average_.Count()) {
    stats_.video_keyframe_distance_average = keyframe_distance_average_.Mean();
  } else {
    // Before we have enough keyframes to calculate the average distance, we
    // will assume the average keyframe distance is infinitely large.
    stats_.video_keyframe_distance_average = base::TimeDelta::Max();
  }

  statistics_cb.Run(stats_);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::InitializeDecoder(
    DecoderType* decoder,
    const DecoderConfigType& config,
    bool low_delay,
    CdmContext* cdm_context,
    InitCB init_cb,
    const OutputCB& output_cb,
    const WaitingCB& waiting_cb) {
  DCHECK(config.IsValidConfig());
  stats_.video_pipeline_info.decoder_type = VideoDecoderType::kUnknown;
  transform_ = config.video_transformation();
  // |decoder| is owned by a DecoderSelector and will stay
  // alive at least until |init_cb| is finished executing.
  decoder->Initialize(
      config, low_delay, cdm_context,
      base::BindOnce(
          &DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecoderInitialized,
          weak_this_, base::Unretained(decoder), std::move(init_cb)),
      output_cb, waiting_cb);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecoderInitialized(
    DecoderType* decoder,
    InitCB cb,
    DecoderStatus result) {
  if (result.is_ok()) {
    stats_.video_pipeline_info.decoder_type = decoder->GetDecoderType();
    DVLOG(2) << stats_.video_pipeline_info.decoder_type;
  } else {
    DVLOG(2) << "Decoder initialization failed.";
  }
  std::move(cb).Run(result);
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnStreamReset(
    DemuxerStream* stream) {
  DCHECK(stream);
  last_keyframe_timestamp_ = base::TimeDelta();
  frame_metadata_.clear();
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecode(
    const DecoderBuffer& buffer) {
  if (buffer.end_of_stream()) {
    last_keyframe_timestamp_ = base::TimeDelta();
    return;
  }

  frame_metadata_[buffer.timestamp()] = {
      buffer.discard_padding().first == kInfiniteDuration,  // should_drop
      buffer.duration(),                                    // duration
      base::TimeTicks::Now(),                               // decode_begin_time
  };

  if (!buffer.is_key_frame())
    return;

  base::TimeDelta current_frame_timestamp = buffer.timestamp();
  if (last_keyframe_timestamp_.is_zero()) {
    last_keyframe_timestamp_ = current_frame_timestamp;
    return;
  }

  const base::TimeDelta frame_distance =
      current_frame_timestamp - last_keyframe_timestamp_;
  last_keyframe_timestamp_ = current_frame_timestamp;
  keyframe_distance_average_.AddSample(frame_distance);
}

PostDecodeAction DecoderStreamTraits<DemuxerStream::VIDEO>::OnDecodeDone(
    OutputType* buffer) {
  auto it = frame_metadata_.find(buffer->timestamp());

  // If the frame isn't in |frame_metadata_| it probably was erased below on a
  // previous cycle. We could drop these, but today our video algorithm will put
  // them back into sorted order or drop the frame if a later frame has already
  // been rendered.
  if (it == frame_metadata_.end())
    return PostDecodeAction::DELIVER;

  // Add a timestamp here to enable buffering delay measurements down the line.
  buffer->metadata().decode_begin_time = it->second.decode_begin_time;
  buffer->metadata().decode_end_time = base::TimeTicks::Now();

  auto action = it->second.should_drop ? PostDecodeAction::DROP
                                       : PostDecodeAction::DELIVER;

  // Provide duration information to help the rendering algorithm on the very
  // first and very last frames.
  if (it->second.duration != kNoTimestamp)
    buffer->metadata().frame_duration = it->second.duration;

  // We erase from the beginning onward to our target frame since frames should
  // be returned in presentation order. It's possible to accumulate entries in
  // this queue if playback begins at a non-keyframe; those frames may never be
  // returned from the decoder.
  frame_metadata_.erase(frame_metadata_.begin(), it + 1);
  return action;
}

void DecoderStreamTraits<DemuxerStream::VIDEO>::OnOutputReady(
    OutputType* buffer) {
  buffer->metadata().transformation = transform_;

  if (!buffer->metadata().decode_begin_time.has_value())
    return;

  // Tag buffer with elapsed time since creation.
  buffer->metadata().processing_time =
      base::TimeTicks::Now() - *buffer->metadata().decode_begin_time;
}

}  // namespace media
