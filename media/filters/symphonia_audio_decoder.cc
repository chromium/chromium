// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/symphonia_audio_decoder.h"

#include <stdint.h>

#include <functional>
#include <memory>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/expected.h"
#include "base/types/to_address.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/base/limits.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/sample_format.h"
#include "media/base/timestamp_constants.h"
#include "media/filters/symphonia_decoder_bridge.rs.h"

namespace media {

namespace {

SymphoniaAudioCodec ToSymphoniaCodec(AudioCodec codec,
                                     SampleFormat sample_format) {
  switch (codec) {
    case AudioCodec::kUnknown:
      return SymphoniaAudioCodec::Unknown;
    case AudioCodec::kFLAC:
      return SymphoniaAudioCodec::Flac;
    case AudioCodec::kMP3:
      return SymphoniaAudioCodec::Mp3;
    case AudioCodec::kPCM:
      switch (sample_format) {
        case SampleFormat::kUnknownSampleFormat:
          return SymphoniaAudioCodec::Unknown;
        case SampleFormat::kSampleFormatF32:
          return SymphoniaAudioCodec::PcmF32;
        case SampleFormat::kSampleFormatPlanarF32:
          return SymphoniaAudioCodec::PcmF32Planar;
        case SampleFormat::kSampleFormatS16:
          return SymphoniaAudioCodec::PcmS16;
        case SampleFormat::kSampleFormatPlanarS16:
          return SymphoniaAudioCodec::PcmS16Planar;
        case SampleFormat::kSampleFormatS24:
          return SymphoniaAudioCodec::PcmS24;
        case SampleFormat::kSampleFormatS32:
          return SymphoniaAudioCodec::PcmS32;
        case SampleFormat::kSampleFormatPlanarS32:
          return SymphoniaAudioCodec::PcmS32Planar;
        case SampleFormat::kSampleFormatU8:
          return SymphoniaAudioCodec::PcmU8;
        case SampleFormat::kSampleFormatPlanarU8:
          return SymphoniaAudioCodec::PcmU8Planar;
        default:
          return SymphoniaAudioCodec::Unknown;
      }
    case AudioCodec::kPCM_ALAW:
      return SymphoniaAudioCodec::PcmAlaw;
    case AudioCodec::kPCM_MULAW:
      return SymphoniaAudioCodec::PcmMulaw;
    case AudioCodec::kPCM_S16BE:
      return SymphoniaAudioCodec::PcmS16be;
    case AudioCodec::kPCM_S24BE:
      return SymphoniaAudioCodec::PcmS24be;
    case AudioCodec::kVorbis:
      return SymphoniaAudioCodec::Vorbis;
    default:
      NOTREACHED();
  }
}

bool IsPcm(AudioCodec codec) {
  return codec == AudioCodec::kPCM || codec == AudioCodec::kPCM_MULAW ||
         codec == AudioCodec::kPCM_S16BE || codec == AudioCodec::kPCM_S24BE ||
         codec == AudioCodec::kPCM_ALAW;
}

constexpr int GetBytesPerSample(AudioCodec codec, SampleFormat sample_format) {
  // Other than this special case, where Chrome pads 24-bit samples into 32-bit
  // containers, the number of bytes per sample is the same as the bytes per
  // channel. The padding is corrected on the output side in the rust glue
  // code when creating the `symphonia::core::audio::AudioBuffer`.
  // TODO(crbug.com/493720049): as a cleanup, handle the S24 case better.
  if (sample_format == kSampleFormatS24 || codec == AudioCodec::kPCM_S24BE) {
    return 3;
  }
  return SampleFormatToBytesPerChannel(sample_format);
}

// Helper to create a SymphoniaDecoderConfig from an AudioDecoderConfig.
SymphoniaDecoderConfig ToSymphoniaConfig(const AudioDecoderConfig& config) {
  SymphoniaDecoderConfig out;
  out.codec = ToSymphoniaCodec(config.codec(), config.sample_format());

  const auto& extra = config.extra_data();
  out.extra_data = rust::Slice<const uint8_t>(extra.data(), extra.size());
  out.bytes_per_sample =
      GetBytesPerSample(config.codec(), config.sample_format());
  out.channel_mask = ChannelLayoutToMask(config.channel_layout());
  out.channel_count = base::checked_cast<uint16_t>(config.channels());
  out.sample_rate = config.samples_per_second();
  return out;
}



SampleFormat ToSampleFormat(SymphoniaSampleFormat value) {
  switch (value) {
    case SymphoniaSampleFormat::Unknown:
      return SampleFormat::kUnknownSampleFormat;
    case SymphoniaSampleFormat::U8:
      return SampleFormat::kSampleFormatU8;
    case SymphoniaSampleFormat::S16:
      return SampleFormat::kSampleFormatS16;
    case SymphoniaSampleFormat::S24:
      return SampleFormat::kSampleFormatS24;
    case SymphoniaSampleFormat::S32:
      return SampleFormat::kSampleFormatS32;
    case SymphoniaSampleFormat::F32:
      return SampleFormat::kSampleFormatF32;
    case SymphoniaSampleFormat::PlanarF32:
      return SampleFormat::kSampleFormatPlanarF32;
  }
  NOTREACHED();
}

DecoderStatus ToDecoderStatus(SymphoniaInitResult& result) {
  const char* message = result.error_str.c_str();
  switch (result.status) {
    using enum DecoderStatus::Codes;
    case SymphoniaInitStatus::Ok:
      return OkStatus();
    case SymphoniaInitStatus::InvalidConfig:
    case SymphoniaInitStatus::XiphVorbisUnpackError:
      return {kUnsupportedConfig, message};
    case SymphoniaInitStatus::UnsupportedCodec:
    case SymphoniaInitStatus::SymphoniaUnsupported:
      return {kUnsupportedCodec, message};
    case SymphoniaInitStatus::DecoderError:
      return {kFailedToCreateDecoder, message};
    case SymphoniaInitStatus::SymphoniaDecodeError:
      return {kMalformedBitstream, message};
    case SymphoniaInitStatus::SymphoniaIoError:
      return {kDecoderStreamDemuxerError, message};
    case SymphoniaInitStatus::SymphoniaLimitError:
      return {kFailed, message};
    case SymphoniaInitStatus::kMaxValue:
      NOTREACHED();
  }
}

DecoderStatus ToDecoderStatus(SymphoniaDecodeResult& result) {
  const char* message = result.error_str.c_str();
  switch (result.status) {
    using enum DecoderStatus::Codes;
    case SymphoniaDecodeStatus::Ok:
      return OkStatus();
    case SymphoniaDecodeStatus::InvalidDecoderState:
      return {kNotInitialized, message};
    case SymphoniaDecodeStatus::DecodeError:
      return {kMalformedBitstream, message};
    case SymphoniaDecodeStatus::IoError:
      return {kDecoderStreamDemuxerError, message};
    case SymphoniaDecodeStatus::Unsupported:
      return {kUnsupportedCodec, message};
    case SymphoniaDecodeStatus::InsufficentData:
    case SymphoniaDecodeStatus::InvalidDecodedBufferSampleFormat:
    case SymphoniaDecodeStatus::UnexpectedEndOfStream:
    case SymphoniaDecodeStatus::ResetRequired:
    case SymphoniaDecodeStatus::SeekError:
    case SymphoniaDecodeStatus::Error:
      return {kFailed, message};
    case SymphoniaDecodeStatus::kMaxValue:
      NOTREACHED();
  }
}

bool IsValidChannelLayout(ChannelLayout layout, int channel_count) {
  return layout != CHANNEL_LAYOUT_UNSUPPORTED &&
         layout != CHANNEL_LAYOUT_NONE &&
         (layout == CHANNEL_LAYOUT_DISCRETE ||
          ChannelLayoutToChannelCount(layout) == channel_count);
}

// Resolves the channel layout for a decoded frame, taking into account any
// midstream changes to channel count.
ChannelLayout ResolveChannelLayout(ChannelLayout current_layout,
                                   int current_channels,
                                   uint32_t channel_mask,
                                   int new_channels) {
  if (new_channels == current_channels &&
      IsValidChannelLayout(current_layout, current_channels)) {
    return current_layout;
  }
  ChannelLayout layout = ChannelMaskToLayout(channel_mask);
  if (IsValidChannelLayout(layout, new_channels)) {
    return layout;
  }
  if (current_layout == CHANNEL_LAYOUT_DISCRETE) {
    return CHANNEL_LAYOUT_DISCRETE;
  }
  layout = GuessChannelLayout(new_channels);
  return (layout != CHANNEL_LAYOUT_UNSUPPORTED) ? layout
                                                : CHANNEL_LAYOUT_DISCRETE;
}

// Returns true if the decoded audio buffer parameters fall within valid limits.
bool IsValidDecodedParameters(int channels,
                              int sample_rate,
                              size_t num_frames,
                              SampleFormat sample_format) {
  return channels > 0 && channels <= limits::kMaxChannels &&
         sample_rate >= limits::kMinSampleRate &&
         sample_rate <= limits::kMaxSampleRate &&
         num_frames <= static_cast<size_t>(limits::kMaxSamplesPerPacket) &&
         sample_format != kUnknownSampleFormat;
}

}  // namespace

SymphoniaPacket ToSymphoniaPacket(
    const DecoderBuffer& buffer,
    std::optional<base::TimeDelta> first_frame_timestamp) {
  SymphoniaPacket packet;
  if (buffer.end_of_stream()) {
    // Represent EOS as an empty data vector.
    packet.data = rust::Slice<const uint8_t>();

    // EOS buffers do not have a valid timestamp or duration.
    packet.timestamp_us = 0;
    packet.duration_us = 0;
  } else {
    packet.data = rust::Slice<const uint8_t>(
        buffer.empty() ? nullptr : base::to_address(buffer.begin()),
        buffer.size());
    const base::TimeDelta first_timestamp =
        first_frame_timestamp.value_or(buffer.timestamp());
    packet.timestamp_us =
        (buffer.timestamp() - first_timestamp).InMicroseconds();
    packet.duration_us = buffer.duration().InMicroseconds();
  }
  return packet;
}

SymphoniaAudioDecoder::SymphoniaAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    ExecutionMode mode)
    : task_runner_(std::move(task_runner)),
      media_log_(MediaLog::CloneSafely(media_log)),
      mode_(mode) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (mode_ == ExecutionMode::kAsynchronous) {
    CHECK(task_runner_);
  }
}

SymphoniaAudioDecoder::~SymphoniaAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ReleaseSymphoniaResources();
}

AudioDecoderType SymphoniaAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kSymphonia;
}

void SymphoniaAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                       CdmContext* /* cdm_context */,
                                       InitCB init_cb,
                                       const OutputCB& output_cb,
                                       const WaitingCB& /* waiting_cb */) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindCallbackIfNeeded(std::move(init_cb));
  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!IsCodecSupported(config.codec())) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec)
                 .WithData("codec", config.codec()));
    return;
  }

  // Symphonia does not currently support any of the specific audio codec
  // profiles.
  if (config.profile() != AudioCodecProfile::kUnknown) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedProfile)
                 .WithData("profile", config.profile()));
    return;
  }

  const auto configure_result = ConfigureDecoder(config);
  if (!configure_result.is_ok()) {
    std::move(bound_init_cb).Run(std::move(configure_result));
    return;
  }

  // Success!
  config_ = config;
  output_cb_ = BindCallbackIfNeeded(output_cb);
  state_ = DecoderState::kNormal;
  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
  DVLOG(3) << __func__
           << ": successfully initialized Symphonia audio decoder...";
}

void SymphoniaAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                   DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(state_, DecoderState::kUninitialized);
  CHECK(decode_cb);
  DecodeCB decode_cb_bound = BindCallbackIfNeeded(std::move(decode_cb));

  switch (state_) {
    // If the decoder is uninitialized at this point, that's a developer error.
    case DecoderState::kUninitialized:
      NOTREACHED();

    case DecoderState::kError:
      std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
      return;

    case DecoderState::kDecodeFinished:
      std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
      return;

    case DecoderState::kNormal:
      DecodeBuffer(std::move(buffer), std::move(decode_cb_bound));
      break;
  }
}

void SymphoniaAudioDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer,
                                         DecodeCB decode_cb_bound) {
  const bool is_eos = buffer->end_of_stream();
  if (!is_eos && buffer->timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without a timestamp.";
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!is_eos && buffer->is_encrypted()) {
    state_ = DecoderState::kError;
    std::move(decode_cb_bound)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // Pass the buffer to the Symphonia decoder.
  const DecoderStatus status = SymphoniaDecode(*buffer);
  if (!status.is_ok()) {
    state_ = DecoderState::kError;
    std::move(decode_cb_bound).Run(std::move(status));
    return;
  }

  // If we processed the EOS buffer, transition state.
  if (is_eos) {
    state_ = DecoderState::kDecodeFinished;
  }

  std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
}

void SymphoniaAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ReleaseSymphoniaResources();
  ConfigureDecoder(config_);  // Re-create the decoder instance.

  state_ = DecoderState::kNormal;
  consecutive_error_count_ = 0;
  ResetTimestampState(config_);

  if (mode_ == ExecutionMode::kAsynchronous) {
    task_runner_->PostTask(FROM_HERE, std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

// static
bool SymphoniaAudioDecoder::IsCodecSupported(AudioCodec codec) {
  if (codec == AudioCodec::kFLAC) {
    return base::FeatureList::IsEnabled(kSymphoniaAudioDecoding);
  }
  if (codec == AudioCodec::kMP3) {
    return base::FeatureList::IsEnabled(kSymphoniaMp3Decoding);
  }
  if (IsPcm(codec)) {
    return base::FeatureList::IsEnabled(kSymphoniaPcmDecoding);
  }
  if (codec == AudioCodec::kVorbis) {
    return base::FeatureList::IsEnabled(kSymphoniaVorbisDecoding);
  }
  return false;
}

DecoderStatus SymphoniaAudioDecoder::SymphoniaDecode(
    const DecoderBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // EOS buffers are markers and contain no audio payload to decode.
  if (buffer.end_of_stream()) {
    return DecoderStatus::Codes::kOk;
  }

  // An empty buffer has no payload to decode.
  if (buffer.empty()) {
    const bool processed = discard_helper_->ProcessBuffers(
        AudioDiscardHelper::TimeInfo::FromBuffer(buffer), nullptr);
    CHECK(!processed);
    return DecoderStatus::Codes::kOk;
  }

  // The first frame only has a valid timestamp if it is not EOS.
  if (!first_frame_timestamp_.has_value()) {
    first_frame_timestamp_ = buffer.timestamp();
  }

  SymphoniaDecodeResult result = symphonia_decoder_.value()->decode(
      ToSymphoniaPacket(buffer, first_frame_timestamp_));

  if (result.status != SymphoniaDecodeStatus::Ok) {
    base::UmaHistogramEnumeration("Media.Audio.Symphonia.DecodeError",
                                  result.status);
    switch (result.status) {
      case SymphoniaDecodeStatus::DecodeError:
      case SymphoniaDecodeStatus::UnexpectedEndOfStream:
        // Forbid back-to-back decode errors to prevent runaway packet drops and
        // excessive A/V desync.
        if (++consecutive_error_count_ > 1) {
          MEDIA_LOG(ERROR, media_log_)
              << "Stopping playback due to consecutive audio buffer decoding "
                 "failures: "
              << result.error_str.c_str() << ", at "
              << buffer.AsHumanReadableString();
          return ToDecoderStatus(result);
        }
        LIMITED_MEDIA_LOG(DEBUG, media_log_, num_decode_errors_, 5)
            << "Dropping audio buffer which failed decoding: "
            << result.error_str.c_str() << ", at "
            << buffer.AsHumanReadableString();
        break;
      default:
        MEDIA_LOG(ERROR, media_log_)
            << "Symphonia error occurred: " << result.error_str.c_str();
        return ToDecoderStatus(result);
    }
  }

  // If 0 frames were decoded (either due to a non-fatal decode error or an
  // empty frame), forward the buffer metadata to the discard helper for
  // caching.
  if (result.buffer.num_frames == 0) {
    const bool processed = discard_helper_->ProcessBuffers(
        AudioDiscardHelper::TimeInfo::FromBuffer(buffer), nullptr);
    CHECK(!processed);
    return DecoderStatus::Codes::kOk;
  }
  // Sanity check: if Symphonia thinks things are OK and returned a valid
  // buffer, then the input buffer should definitely not have been end of
  // stream.
  CHECK(!buffer.end_of_stream());

  const int channels = base::checked_cast<int>(result.buffer.channel_count);
  const int sample_rate = base::checked_cast<int>(result.buffer.sample_rate);
  const size_t num_frames = result.buffer.num_frames;
  const SampleFormat sample_format =
      ToSampleFormat(result.buffer.sample_format);

  if (!IsValidDecodedParameters(channels, sample_rate, num_frames,
                                sample_format)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Invalid decoded buffer parameters: channels=" << channels
        << ", sample_rate=" << sample_rate << ", num_frames=" << num_frames
        << ", sample_format=" << SampleFormatToString(sample_format);
    return DecoderStatus::Codes::kFailed;
  }

  const ChannelLayout channel_layout =
      ResolveChannelLayout(config_.channel_layout(), config_.channels(),
                           result.buffer.channel_mask, channels);
  if (!IsValidChannelLayout(channel_layout, channels)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported channel layout for " << channels << " channels.";
    return DecoderStatus::Codes::kFailed;
  }

  MaybeUpdateConfig(buffer, sample_rate,
                    ChannelLayoutConfig(channel_layout, channels));

  // Convert the Symphonia buffer to a media::AudioBuffer, using the original
  // timestamp.
  const base::TimeDelta timestamp = buffer.timestamp();
  scoped_refptr<AudioBuffer> decoded_audio =
      ToMediaAudioBuffer(result.buffer, timestamp);
  if (!decoded_audio) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to allocate decoded audio buffer.";
    return DecoderStatus::Codes::kFailed;
  }

  // Process potential discards.
  const bool processed = discard_helper_->ProcessBuffers(
      AudioDiscardHelper::TimeInfo::FromBuffer(buffer), decoded_audio.get());

  // Output the frame if it wasn't discarded.
  if (processed) {
    VLOG(3) << __func__ << ": processed buffer with "
            << decoded_audio->frame_count() << " frames...";
    output_cb_.Run(std::move(decoded_audio));
  }

  return DecoderStatus::Codes::kOk;
}

scoped_refptr<AudioBuffer> SymphoniaAudioDecoder::ToMediaAudioBuffer(
    const SymphoniaAudioBuffer& symphonia_buffer,
    base::TimeDelta timestamp) {
  const SampleFormat sample_format =
      ToSampleFormat(symphonia_buffer.sample_format);
  const int channel_count =
      base::checked_cast<int>(symphonia_buffer.channel_count);
  const int sample_rate = base::checked_cast<int>(symphonia_buffer.sample_rate);
  const int num_frames = base::checked_cast<int>(symphonia_buffer.num_frames);

  scoped_refptr<AudioBuffer> decoded_audio =
      AudioBuffer::CreateBuffer(sample_format, config_.channel_layout(),
                                channel_count, sample_rate, num_frames, pool_);
  if (!decoded_audio) {
    return nullptr;
  }

  if (IsPlanar(sample_format)) {
    for (int ch = 0; ch < channel_count; ++ch) {
      auto channel_span = decoded_audio->planar_channel(ch);
      const bool copied = symphonia_decoder_.value()->copy_decoded_channel(
          ch, rust::Slice<uint8_t>(channel_span.data(), channel_span.size()));
      if (!copied) {
        MEDIA_LOG(ERROR, media_log_)
            << "Failed to copy planar decoded audio channel " << ch;
        return nullptr;
      }
    }
  } else {
    auto data_span = decoded_audio->interleaved_data();
    const bool copied = symphonia_decoder_.value()->copy_decoded_samples(
        rust::Slice<uint8_t>(data_span.data(), data_span.size()));
    if (!copied) {
      MEDIA_LOG(ERROR, media_log_)
          << "Failed to copy interleaved decoded audio samples";
      return nullptr;
    }
  }

  decoded_audio->set_timestamp(timestamp);
  return decoded_audio;
}

// static
SymphoniaPacket SymphoniaAudioDecoder::ToSymphoniaPacketForTesting(
    const DecoderBuffer& buffer,
    std::optional<base::TimeDelta> first_frame_timestamp) {
  return ToSymphoniaPacket(buffer, first_frame_timestamp);
}

void SymphoniaAudioDecoder::ReleaseSymphoniaResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  symphonia_decoder_.reset();
}

DecoderStatus SymphoniaAudioDecoder::ConfigureDecoder(
    const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config.IsValidConfig());
  CHECK(!config.is_encrypted());

  // Release existing decoder resources if necessary.
  ReleaseSymphoniaResources();

  // Codec support is determined by the rust implementation, and will return
  // an error as an initialization result if the codec is not supported.
  const SymphoniaDecoderConfig symphonia_config = ToSymphoniaConfig(config);
  SymphoniaInitResult result = init_symphonia_decoder(symphonia_config);
  // Record status for every initialization attempt.
  base::UmaHistogramEnumeration("Media.Audio.Symphonia.InitStatus",
                                result.status);
  if (result.status != SymphoniaInitStatus::Ok) {
    MEDIA_LOG(ERROR, media_log_)
        << "Could not initialize Symphonia audio decoder: "
        << result.error_str.c_str();
    state_ = DecoderState::kUninitialized;
    return ToDecoderStatus(result);
  }

  ResetTimestampState(config);
  symphonia_decoder_ = std::move(result.decoder);
  return DecoderStatus::Codes::kOk;
}

void SymphoniaAudioDecoder::MaybeUpdateConfig(
    const DecoderBuffer& buffer,
    int sample_rate,
    const ChannelLayoutConfig& layout_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool is_sample_rate_change =
      sample_rate != config_.samples_per_second();
  const bool is_config_change =
      is_sample_rate_change || layout_config != config_.channel_layout_config();
  if (!is_config_change) {
    return;
  }

  MEDIA_LOG(DEBUG, media_log_)
      << "Detected midstream configuration change"
      << " PTS:" << buffer.timestamp().InMicroseconds()
      << " Sample Rate: " << sample_rate << " vs "
      << config_.samples_per_second() << ", ChannelLayout: "
      << ChannelLayoutToString(layout_config.channel_layout()) << " vs "
      << ChannelLayoutToString(config_.channel_layout())
      << ", Channels: " << layout_config.channels() << " vs "
      << config_.channels();

  const bool should_discard_decoder_delay =
      config_.should_discard_decoder_delay();
  config_.Initialize(config_.codec(), config_.sample_format(), layout_config,
                     sample_rate, config_.extra_data(),
                     config_.encryption_scheme(), config_.seek_preroll(),
                     config_.codec_delay());
  if (!should_discard_decoder_delay) {
    config_.disable_discard_decoder_delay();
  }

  if (is_sample_rate_change) {
    ResetTimestampState(config_);
  }
}

// The Symphonia audio decoder implementation currently needs the same discard
// help as FFMPEG does.
void SymphoniaAudioDecoder::ResetTimestampState(
    const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      config.samples_per_second(), config.codec_delay(),
      /*delayed_discard=*/false);
  discard_helper_->Reset(config.codec_delay());
  first_frame_timestamp_.reset();
}

}  // namespace media
