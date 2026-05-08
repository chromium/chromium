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
#include "base/rand_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
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
#include "media/filters/symphonia_glue.rs.h"

namespace media {

namespace {

// PCM specific property for the maximum number of frames per packet. This
// value covers up to ~85ms of audio per chunk.
constexpr int kDefaultMaxFramesPerPcmPacket = 4096;

// We sample 1% of Symphonia related errors for dumping.
static constexpr double kSampleRate = 0.01;

DecoderStatus MaybeDumpError(DecoderStatus&& status) {
  // TODO(crbug.com/491162892): remove temporary DUMP_WILL_BE_CHECK once
  // Symphonia is sufficiently stable.
  if (base::RandDouble() < kSampleRate) {
    DUMP_WILL_BE_CHECK(false) << status << ": " << status.message();
  }
  return std::move(status);
}

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
  out.sample_rate = config.samples_per_second();

  // Symphonia needs to know the max frames per packet for PCM decoding, which
  // is not something we know directly in Chrome. Set a safe limit here.
  // If this limit is violated, Symphonia will return a DecodeError and audio
  // decoding will fail. FFMpeg does not have this restriction because it
  // dynamically derives the frames needed from the AVPacket size itself.
  out.max_frames_per_packet =
      IsPcm(config.codec()) ? kDefaultMaxFramesPerPcmPacket : 0;
  return out;
}

// Helper to create a SymphoniaPacket from a DecoderBuffer.
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
    CHECK_GT(buffer.size(), 0u);
    packet.data = rust::Slice<const uint8_t>(
        buffer.empty() ? nullptr : base::to_address(buffer.begin()),
        buffer.size());
    packet.timestamp_us =
        (buffer.timestamp() - first_frame_timestamp.value()).InMicroseconds();
    packet.duration_us = buffer.duration().InMicroseconds();
  }
  return packet;
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
  }
  NOTREACHED();
}

DecoderStatus ToDecoderStatus(SymphoniaInitResult& result) {
  switch (result.status) {
    case SymphoniaInitStatus::Ok:
      return OkStatus();
    case SymphoniaInitStatus::InvalidConfig:
    case SymphoniaInitStatus::XiphVorbisUnpackError:
      return MaybeDumpError(
          {DecoderStatus::Codes::kUnsupportedConfig, result.error_str.c_str()});
    case SymphoniaInitStatus::UnsupportedCodec:
    case SymphoniaInitStatus::SymphoniaUnsupported:
      return MaybeDumpError({
          DecoderStatus::Codes::kUnsupportedCodec,
          result.error_str.c_str(),
      });
    case SymphoniaInitStatus::DecoderError:
      return MaybeDumpError({
          DecoderStatus::Codes::kFailedToCreateDecoder,
          result.error_str.c_str(),
      });
    case SymphoniaInitStatus::SymphoniaDecodeError:
      return MaybeDumpError({
          DecoderStatus::Codes::kMalformedBitstream,
          result.error_str.c_str(),
      });
    case SymphoniaInitStatus::SymphoniaIoError:
      return MaybeDumpError({
          DecoderStatus::Codes::kDecoderStreamDemuxerError,
          result.error_str.c_str(),
      });
    case SymphoniaInitStatus::SymphoniaLimitError:
      return MaybeDumpError({
          DecoderStatus::Codes::kFailed,
          result.error_str.c_str(),
      });
    case SymphoniaInitStatus::kMaxValue:
      NOTREACHED();
  }
}

DecoderStatus ToDecoderStatus(SymphoniaDecodeResult& result) {
  switch (result.status) {
    case SymphoniaDecodeStatus::Ok:
      return OkStatus();
    case SymphoniaDecodeStatus::InvalidDecoderState:
      return MaybeDumpError({
          DecoderStatus::Codes::kNotInitialized,
          result.error_str.c_str(),
      });
    case SymphoniaDecodeStatus::DecodeError:
      return MaybeDumpError({
          DecoderStatus::Codes::kMalformedBitstream,
          result.error_str.c_str(),
      });
    case SymphoniaDecodeStatus::IoError:
      return MaybeDumpError({
          DecoderStatus::Codes::kDecoderStreamDemuxerError,
          result.error_str.c_str(),
      });
    case SymphoniaDecodeStatus::Unsupported:
      return MaybeDumpError({
          DecoderStatus::Codes::kUnsupportedCodec,
          result.error_str.c_str(),
      });
    case SymphoniaDecodeStatus::InsufficentData:
    case SymphoniaDecodeStatus::InvalidDecodedBufferSampleFormat:
    case SymphoniaDecodeStatus::UnexpectedEndOfStream:
    case SymphoniaDecodeStatus::ResetRequired:
    case SymphoniaDecodeStatus::SeekError:
    case SymphoniaDecodeStatus::Error:
      return MaybeDumpError({
          DecoderStatus::Codes::kFailed,
          result.error_str.c_str(),
      });
    case SymphoniaDecodeStatus::kMaxValue:
      NOTREACHED();
  }
}

// A templated ExternalMemory implementation that wraps and owns a rust::Box<T>,
// automatically deriving the span from the box's `data` member (expected to be
// a contiguous buffer like rust::Vec<uint8_t>).
template <typename T>
class BoxedMemory : public AudioBuffer::ExternalMemory {
 public:
  explicit BoxedMemory(rust::Box<T> box)
      : ExternalMemory(box->data), box_(std::move(box)) {}
  ~BoxedMemory() override = default;

 private:
  rust::Box<T> box_;
};

// Helper function to automatically deduce the template argument T from
// rust::Box<T>.
template <typename T>
std::unique_ptr<BoxedMemory<T>> WrapBoxedMemory(rust::Box<T> box) {
  return std::make_unique<BoxedMemory<T>>(std::move(box));
}

}  // namespace

SymphoniaAudioDecoder::SymphoniaAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    ExecutionMode mode)
    : task_runner_(std::move(task_runner)), media_log_(media_log), mode_(mode) {
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

  // Symphonia's PCM decoder requires a pre-configured max frames per packet.
  // If an incoming packet yields more frames than the capacity, the decode
  // will fail out of bounds. Since we don't know the max chunk bounds in
  // advance, we lazily grow it here by tearing down and recreating the wrapper
  // handle if necessary.
  if (!is_eos && IsPcm(config_.codec())) {
    const int bytes_per_frame =
        config_.channels() *
        GetBytesPerSample(config_.codec(), config_.sample_format());
    const int frames_in_buffer = buffer->size() / bytes_per_frame;

    if (frames_in_buffer > kDefaultMaxFramesPerPcmPacket) {
      base::UmaHistogramCounts100000("Media.Audio.Symphonia.OversizedPcmPacket",
                                     frames_in_buffer);
      std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
      return;
    }
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

  // The first frame only has a valid timestamp if it is not EOS.
  if (!first_frame_timestamp_.has_value() && !buffer.end_of_stream()) {
    first_frame_timestamp_ = buffer.timestamp();
  }

  SymphoniaDecodeResult result = symphonia_decoder_.value()->decode(
      ToSymphoniaPacket(buffer, first_frame_timestamp_));

  // Record status for every decode attempt.
  if (result.status != SymphoniaDecodeStatus::Ok) {
    base::UmaHistogramEnumeration("Media.Audio.Symphonia.DecodeError",
                                  result.status);
  }

  // The Symphonia glue will return an empty buffer if end of stream is reached.
  if (result.buffer->data.empty()) {
    // The stream end was unexpected, which is not as severe of an error as the
    // other potential cases logged below.
    if (result.status == SymphoniaDecodeStatus::UnexpectedEndOfStream) {
      MEDIA_LOG(WARNING, media_log_) << "Reached an unexpected end of stream.";
    }

    // Even if we didn't decode a frame, we should still send the packet
    // to the discard helper for caching.
    if (!buffer.end_of_stream()) {
      const bool processed = discard_helper_->ProcessBuffers(
          AudioDiscardHelper::TimeInfo::FromBuffer(buffer), nullptr);
      DCHECK(!processed);
    }

    return DecoderStatus::Codes::kOk;
  }
  // Sanity check: if Symphonia thinks things are OK and returned a valid
  // buffer, then the input buffer should definitely not have been end of
  // stream.
  CHECK(!buffer.end_of_stream());

  if (result.status != SymphoniaDecodeStatus::Ok) {
    MEDIA_LOG(ERROR, media_log_)
        << "Symphonia error occurred: " << result.error_str.c_str();
    return ToDecoderStatus(result);
  }

  // TODO(crbug.com/40074653): similar to FFMPEG audio decoder, add support
  // for midstream channel and sample rate changes.

  // Convert the Symphonia buffer to a media::AudioBuffer, using the original
  // timestamp.
  const base::TimeDelta timestamp = buffer.timestamp();
  scoped_refptr<AudioBuffer> decoded_audio =
      ToMediaAudioBuffer(std::move(result.buffer), timestamp);
  CHECK(decoded_audio);

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
    rust::Box<SymphoniaAudioBuffer> symphonia_buffer,
    base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const SampleFormat sample_format =
      ToSampleFormat(symphonia_buffer->sample_format);
  const int channel_count = symphonia_buffer->channel_count;
  const int sample_rate = symphonia_buffer->sample_rate;
  const int num_frames = symphonia_buffer->num_frames;

  const bool count_changed = channel_count != config_.channels();
  const auto layout = count_changed
                          ? ChannelMaskToLayout(symphonia_buffer->channel_mask)
                          : config_.channel_layout();

  auto external_memory = WrapBoxedMemory(std::move(symphonia_buffer));

  return AudioBuffer::CreateFromExternalMemory(
      sample_format, layout, channel_count, sample_rate, num_frames, timestamp,
      std::move(external_memory));
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

// The Symphonia audio decoder implementation currently needs the same discard
// help as FFMPEG does.
void SymphoniaAudioDecoder::ResetTimestampState(
    const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      config.samples_per_second(), config.codec_delay(),
      /*delayed_discard=*/false);
  discard_helper_->Reset(config.codec_delay());
}

}  // namespace media
