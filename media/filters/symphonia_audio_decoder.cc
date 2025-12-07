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
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
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

// TODO(crbug.com/40074653): consider using a view type instead where possible.
rust::Vec<uint8_t> ToRustVec(base::span<const uint8_t> data) {
  rust::Vec<uint8_t> vec;
  vec.reserve(data.size());
  for (const uint8_t value : data) {
    vec.push_back(value);
  }
  return vec;
}

// Currently the Symphonia decoder only has FLAC audio support enabled. This
// will be expanded in the future.
SymphoniaAudioCodec ToSymphoniaCodec(AudioCodec codec) {
  CHECK_EQ(AudioCodec::kFLAC, codec);
  return SymphoniaAudioCodec::Flac;
}

// Helper to create a SymphoniaDecoderConfig from an AudioDecoderConfig.
SymphoniaDecoderConfig ToSymphoniaConfig(const AudioDecoderConfig& config) {
  SymphoniaDecoderConfig symphonia_config;
  symphonia_config.codec = ToSymphoniaCodec(config.codec());
  symphonia_config.extra_data = ToRustVec(config.extra_data());
  return symphonia_config;
}

// Helper to create a SymphoniaPacket from a DecoderBuffer.
SymphoniaPacket ToSymphoniaPacket(
    const DecoderBuffer& buffer,
    std::optional<base::TimeDelta> first_frame_timestamp) {
  SymphoniaPacket packet;
  if (buffer.end_of_stream()) {
    // Represent EOS as an empty data vector.
    packet.data = rust::Vec<uint8_t>();

    // EOS buffers do not have a valid timestamp or duration.
    packet.timestamp_us = 0;
    packet.duration_us = 0;
  } else {
    CHECK_GT(buffer.size(), 0u);
    packet.data = ToRustVec(buffer);
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
    MEDIA_LOG(ERROR, media_log_)
        << "SymphoniaAudioDecoder does not currently support encrypted content";
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!IsCodecSupported(config.codec())) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported codec: " << GetCodecName(config.codec());
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec)
                 .WithData("codec", config.codec()));
    return;
  }

  // Symphonia does not currently support any of the specific audio codec
  // profiles.
  if (config.profile() != AudioCodecProfile::kUnknown) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported profile: " << GetProfileName(config.profile());
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedProfile)
                 .WithData("profile", config.profile()));
    return;
  }

  if (!ConfigureDecoder(config)) {
    // ConfigureDecoder logs the specific error.
    std::move(bound_init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
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
  if (!SymphoniaDecode(*buffer)) {
    // SymphoniaDecode logs the error.
    state_ = DecoderState::kError;
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
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
  // Currently, only FLAC audio is supported.
  return codec == AudioCodec::kFLAC;
}

bool SymphoniaAudioDecoder::SymphoniaDecode(const DecoderBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The first frame only has a valid timestamp if it is not EOS.
  if (!first_frame_timestamp_.has_value() && !buffer.end_of_stream()) {
    first_frame_timestamp_ = buffer.timestamp();
  }

  SymphoniaDecodeResult result = symphonia_decoder_.value()->decode(
      ToSymphoniaPacket(buffer, first_frame_timestamp_));

  // The Symphonia glue will return an empty buffer if end of stream is reached.
  if (result.buffer->data.empty()) {
    // The stream end was unexpected, which is not as severe of an error as the
    // other potential cases logged below.
    if (result.status == SymphoniaDecodeStatus::UnexpectedEndOfStream) {
      MEDIA_LOG(WARNING, media_log_) << "Reached an unexpected end of stream.";
    }
    return true;
  }
  // Sanity check: if Symphonia thinks things are OK and returned a valid
  // buffer, then the input buffer should definitely not have been end of
  // stream.
  CHECK(!buffer.end_of_stream());

  if (result.status != SymphoniaDecodeStatus::Ok) {
    MEDIA_LOG(ERROR, media_log_)
        << "Symphonia error occurred: " << result.error_str.c_str();
    return false;
  }

  // TODO(crbug.com/40074653): similar to FFMPEG audio decoder, add support
  // for midstream channel and sample rate changes.

  // Convert the Symphonia buffer to a media::AudioBuffer, using the original
  // timestamp.
  const base::TimeDelta timestamp = buffer.timestamp();
  scoped_refptr<AudioBuffer> decoded_audio =
      ToMediaAudioBuffer(*result.buffer, timestamp);
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

  return true;
}

scoped_refptr<AudioBuffer> SymphoniaAudioDecoder::ToMediaAudioBuffer(
    const SymphoniaAudioBuffer& symphonia_buffer,
    base::TimeDelta timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create the AudioBuffer.
  // TODO(crbug.com/40074653): long term we want a WrapOrCopy implementation,
  // since we own the Symphonia audio buffer.
  const uint8_t* data = symphonia_buffer.data.data();
  return AudioBuffer::CopyFrom(
      ToSampleFormat(symphonia_buffer.sample_format), config_.channel_layout(),
      config_.channels(), config_.samples_per_second(),
      symphonia_buffer.num_frames, &data, timestamp, pool_);
}

void SymphoniaAudioDecoder::ReleaseSymphoniaResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  symphonia_decoder_.reset();
}

bool SymphoniaAudioDecoder::ConfigureDecoder(const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config.IsValidConfig());
  CHECK(!config.is_encrypted());

  // Release existing decoder resources if necessary.
  ReleaseSymphoniaResources();

  // Codec support is determined by the rust implementation, and will return
  // an error as an initialization result if the codec is not supported.
  const SymphoniaDecoderConfig symphonia_config = ToSymphoniaConfig(config);
  SymphoniaInitResult result = init_symphonia_decoder(symphonia_config);
  if (result.status != SymphoniaInitStatus::Ok) {
    MEDIA_LOG(ERROR, media_log_)
        << "Could not initialize Symphonia audio decoder: "
        << result.error_str.c_str();
    state_ = DecoderState::kUninitialized;
    return false;
  }

  symphonia_decoder_ = std::move(result.decoder);
  ResetTimestampState(config);
  return true;
}

// TODO(crbug.com/40074653): determine if Symphonia needs the same discard
// help as FFMPEG does.
void SymphoniaAudioDecoder::ResetTimestampState(
    const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Symphonia should handle codec delay internally, so we pass zero here.
  const int codec_delay = 0;
  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      config.samples_per_second(), codec_delay,
      config.codec() == AudioCodec::kVorbis);  // Vorbis needs special handling?
  discard_helper_->Reset(codec_delay);
}

}  // namespace media
