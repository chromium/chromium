// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_opus_encoder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/channel_mixer.h"
#include "media/base/converting_audio_fifo.h"
#include "media/base/encoder_status.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// Recommended value for opus_encode_float(), according to documentation in
// third_party/opus/src/include/opus.h, so that the Opus encoder does not
// degrade the audio due to memory constraints, and is independent of the
// duration of the encoded buffer.
constexpr int kOpusMaxDataBytes = 4000;

// Opus preferred sampling rate for encoding. This is also the one WebM likes
// to have: https://wiki.xiph.org/MatroskaOpus.
constexpr int kOpusPreferredSamplingRate = 48000;

// For Opus, we try to encode 60ms, the maximum Opus buffer, for quality
// reasons.
constexpr int kOpusPreferredBufferDurationMs = 60;

// Deletes the libopus encoder instance pointed to by |encoder_ptr|.
inline void OpusEncoderDeleter(OpusEncoder* encoder_ptr) {
  opus_encoder_destroy(encoder_ptr);
}

AudioParameters CreateInputParams(const AudioEncoder::Options& options) {
  const int frames_per_buffer = options.sample_rate *
                                kOpusPreferredBufferDurationMs /
                                base::Time::kMillisecondsPerSecond;
  AudioParameters result(media::AudioParameters::AUDIO_PCM_LINEAR,
                         {media::CHANNEL_LAYOUT_DISCRETE, options.channels},
                         options.sample_rate, frames_per_buffer);
  return result;
}

// Creates the audio parameters of the converted audio format that Opus prefers,
// which will be used as the input to the libopus encoder.
AudioParameters CreateOpusCompatibleParams(const AudioParameters& params) {
  // third_party/libopus supports up to 2 channels (see implementation of
  // opus_encoder_create()): force |converted_params| to at most those.
  // Also, the libopus encoder can accept sample rates of 8, 12, 16, 24, and the
  // default preferred 48 kHz. If the input sample rate is anything else, we'll
  // use 48 kHz.
  const int input_rate = params.sample_rate();
  const int used_rate = (input_rate == 8000 || input_rate == 12000 ||
                         input_rate == 16000 || input_rate == 24000)
                            ? input_rate
                            : kOpusPreferredSamplingRate;
  const int frames_per_buffer = used_rate * kOpusPreferredBufferDurationMs /
                                base::Time::kMillisecondsPerSecond;

  AudioParameters result(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(std::min(params.channels(), 2)), used_rate,
      frames_per_buffer);
  return result;
}

}  // namespace

// TODO: Remove after switching to C++17
constexpr int AudioOpusEncoder::kMinBitrate;

AudioOpusEncoder::AudioOpusEncoder()
    : opus_encoder_(nullptr, OpusEncoderDeleter) {}

void AudioOpusEncoder::Initialize(const Options& options,
                                  OutputCB output_callback,
                                  EncoderStatusCB done_cb) {
  DCHECK(output_callback);
  DCHECK(done_cb);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (opus_encoder_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  if (options.codec != AudioCodec::kOpus) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  options_ = options;
  input_params_ = CreateInputParams(options);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  converted_params_ = CreateOpusCompatibleParams(input_params_);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  // Unretained is safe here, because |this| owns |fifo_|.
  fifo_ = std::make_unique<ConvertingAudioFifo>(
      input_params_, converted_params_,
      base::BindRepeating(&AudioOpusEncoder::OnFifoOutput,
                          base::Unretained(this)));

  timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(converted_params_.sample_rate());
  buffer_.resize(converted_params_.channels() *
                 converted_params_.frames_per_buffer());
  auto status_or_encoder = CreateOpusEncoder();
  if (!status_or_encoder.has_value()) {
    std::move(done_cb).Run(std::move(status_or_encoder).error());
    return;
  }

  opus_encoder_ = std::move(status_or_encoder).value();

  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_callback));
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

AudioOpusEncoder::~AudioOpusEncoder() = default;

AudioOpusEncoder::CodecDescription AudioOpusEncoder::PrepareExtraData() {
  CodecDescription extra_data;
  // RFC #7845  Ogg Encapsulation for the Opus Audio Codec
  // https://tools.ietf.org/html/rfc7845
  static const uint8_t kExtraDataTemplate[19] = {
      'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
      1,                 // offset 8, version, always 1
      0,                 // offset 9, channel count
      0,   0,            // offset 10, pre-skip
      0,   0,   0,   0,  // offset 12, original input sample rate in Hz
      0,   0,   0};

  extra_data.assign(kExtraDataTemplate,
                    kExtraDataTemplate + sizeof(kExtraDataTemplate));

  // Save number of channels
  base::CheckedNumeric<uint8_t> channels(converted_params_.channels());
  if (channels.IsValid())
    extra_data.data()[9] = channels.ValueOrDie();

  // Number of samples to skip from the start of the decoder's output.
  // Real data begins this many samples late. These samples need to be skipped
  // only at the very beginning of the audio stream, NOT at beginning of each
  // decoded output.
  if (opus_encoder_) {
    int32_t samples_to_skip = 0;

    opus_encoder_ctl(opus_encoder_.get(), OPUS_GET_LOOKAHEAD(&samples_to_skip));
    base::CheckedNumeric<uint16_t> samples_to_skip_safe = samples_to_skip;
    if (samples_to_skip_safe.IsValid())
      *reinterpret_cast<uint16_t*>(extra_data.data() + 10) =
          samples_to_skip_safe.ValueOrDie();
  }

  // Save original sample rate
  base::CheckedNumeric<uint16_t> sample_rate = input_params_.sample_rate();
  uint16_t* sample_rate_ptr =
      reinterpret_cast<uint16_t*>(extra_data.data() + 12);
  if (sample_rate.IsValid())
    *sample_rate_ptr = sample_rate.ValueOrDie();
  else
    *sample_rate_ptr = uint16_t{kOpusPreferredSamplingRate};
  return extra_data;
}

void AudioOpusEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks capture_time,
                              EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb);

  current_done_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(current_done_cb_)
        .Run(EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  DCHECK(timestamp_tracker_);

  if (timestamp_tracker_->base_timestamp() == kNoTimestamp)
    timestamp_tracker_->SetBaseTimestamp(capture_time - base::TimeTicks());

  // This might synchronously call OnFifoOutput().
  fifo_->Push(std::move(audio_bus));

  if (current_done_cb_) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioOpusEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK(done_cb);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  current_done_cb_ = std::move(done_cb);

  fifo_->Flush();

  timestamp_tracker_->SetBaseTimestamp(kNoTimestamp);
  if (current_done_cb_) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioOpusEncoder::OnFifoOutput(AudioBus* audio_bus) {
  audio_bus->ToInterleaved<Float32SampleTypeTraits>(audio_bus->frames(),
                                                    buffer_.data());
  // We already reported an error. Don't attempt to encode any further inputs.
  if (!current_done_cb_)
    return;

  std::unique_ptr<uint8_t[]> encoded_data(new uint8_t[kOpusMaxDataBytes]);
  auto result = opus_encode_float(opus_encoder_.get(), buffer_.data(),
                                  converted_params_.frames_per_buffer(),
                                  encoded_data.get(), kOpusMaxDataBytes);

  if (result < 0) {
    DCHECK(current_done_cb_);
    std::move(current_done_cb_)
        .Run(EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                           opus_strerror(result)));
    return;
  }

  size_t encoded_data_size = result;
  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  if (encoded_data_size > 1) {
    absl::optional<CodecDescription> desc;
    if (need_to_emit_extra_data_) {
      desc = PrepareExtraData();
      need_to_emit_extra_data_ = false;
    }

    auto ts = base::TimeTicks() + timestamp_tracker_->GetTimestamp();

    auto duration = timestamp_tracker_->GetFrameDuration(
        converted_params_.frames_per_buffer());

    // `timestamp_tracker_` will return base::TimeDelta() if the timestamps
    // overflow.
    if (duration.is_zero()) {
      DCHECK(current_done_cb_);
      std::move(current_done_cb_)
          .Run(EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                             "Invalid computed duration."));
      return;
    }

    EncodedAudioBuffer encoded_buffer(converted_params_,
                                      std::move(encoded_data),
                                      encoded_data_size, ts, duration);
    output_cb_.Run(std::move(encoded_buffer), desc);
  }
  timestamp_tracker_->AddFrames(converted_params_.frames_per_buffer());
}

// Creates and returns the libopus encoder instance. Returns nullptr if the
// encoder creation fails.
EncoderStatus::Or<OwnedOpusEncoder> AudioOpusEncoder::CreateOpusEncoder() {
  int opus_result;
  OwnedOpusEncoder encoder(
      opus_encoder_create(converted_params_.sample_rate(),
                          converted_params_.channels(), OPUS_APPLICATION_AUDIO,
                          &opus_result),
      OpusEncoderDeleter);

  if (opus_result < 0) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf(
            "Couldn't init Opus encoder: %s, sample rate: %d, channels: %d",
            opus_strerror(opus_result), converted_params_.sample_rate(),
            converted_params_.channels()));
  }

  int bitrate =
      options_.bitrate.has_value() ? options_.bitrate.value() : OPUS_AUTO;
  if (encoder &&
      opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus bitrate: %d", bitrate));
  }

  return encoder;
}

}  // namespace media
