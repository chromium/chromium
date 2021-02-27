// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_opus_encoder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/status.h"
#include "media/base/status_codes.h"

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
                         media::CHANNEL_LAYOUT_DISCRETE, options.sample_rate,
                         frames_per_buffer);
  result.set_channels_for_discrete(options.channels);
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

  AudioParameters result(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                         GuessChannelLayout(std::min(params.channels(), 2)),
                         used_rate, frames_per_buffer);
  return result;
}

// During this object's lifetime, it will use its |audio_bus_| to provide input
// to its |converter_|.
class ScopedConverterInputProvider : public AudioConverter::InputCallback {
 public:
  ScopedConverterInputProvider(AudioConverter* converter,
                               const AudioBus* audio_bus)
      : converter_(converter), audio_bus_(audio_bus) {
    DCHECK(converter_);
    DCHECK(audio_bus_);
    converter_->AddInput(this);
  }
  ScopedConverterInputProvider(const ScopedConverterInputProvider&) = delete;
  ScopedConverterInputProvider& operator=(const ScopedConverterInputProvider&) =
      delete;
  ~ScopedConverterInputProvider() override { converter_->RemoveInput(this); }

  // AudioConverted::InputCallback:
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override {
    audio_bus_->CopyTo(audio_bus);
    return 1.0f;
  }

 private:
  AudioConverter* const converter_;
  const AudioBus* const audio_bus_;
};

}  // namespace

AudioOpusEncoder::AudioOpusEncoder()
    : opus_encoder_(nullptr, OpusEncoderDeleter) {}

void AudioOpusEncoder::Initialize(const Options& options,
                                  OutputCB output_callback,
                                  StatusCB done_cb) {
  DCHECK(!output_callback.is_null());
  DCHECK(!done_cb.is_null());

  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (opus_encoder_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeTwice);
    return;
  }

  options_ = options;
  input_params_ = CreateInputParams(options);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializationError);
    return;
  }

  converted_params_ = CreateOpusCompatibleParams(input_params_);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializationError);
    return;
  }

  converter_ =
      std::make_unique<AudioConverter>(input_params_, converted_params_,
                                       /*disable_fifo=*/false);
  fifo_ = std::make_unique<AudioPushFifo>(base::BindRepeating(
      &AudioOpusEncoder::OnFifoOutput, base::Unretained(this)));
  converted_audio_bus_ = AudioBus::Create(
      converted_params_.channels(), converted_params_.frames_per_buffer());
  buffer_.resize(converted_params_.channels() *
                 converted_params_.frames_per_buffer());
  auto status_or_encoder = CreateOpusEncoder();
  if (status_or_encoder.has_error()) {
    std::move(done_cb).Run(std::move(status_or_encoder).error());
    return;
  }

  opus_encoder_ = std::move(status_or_encoder).value();
  converter_->PrimeWithSilence();
  fifo_->Reset(converter_->GetMaxInputFramesRequested(
      converted_params_.frames_per_buffer()));

  output_cb_ = BindToCurrentLoop(std::move(output_callback));
  std::move(done_cb).Run(OkStatus());
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
                              StatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(audio_bus->channels(), input_params_.channels());
  DCHECK(!capture_time.is_null());
  DCHECK(!done_cb.is_null());

  current_done_cb_ = BindToCurrentLoop(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(current_done_cb_)
        .Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  DLOG_IF(ERROR, !last_capture_time_.is_null() &&
                     ((capture_time - last_capture_time_).InSecondsF() >
                      1.5f * audio_bus->frames() / input_params_.sample_rate()))
      << "Possibly frames were skipped, which may result in inaccurate "
         "timestamp calculation.";

  last_capture_time_ = capture_time;

  // If |capture_time| - |audio_bus|'s duration is not equal to the expected
  // timestamp for the next audio sample. It means there is a gap/overlap,
  // if it's big enough we flash and start anew, otherwise we ignore it.
  auto capture_ts = ComputeTimestamp(audio_bus->frames(), capture_time);
  auto existing_buffer_duration = AudioTimestampHelper::FramesToTime(
      fifo_->queued_frames(), input_params_.sample_rate());
  auto end_of_existing_buffer_ts = next_timestamp_ + existing_buffer_duration;
  base::TimeDelta gap = (capture_ts - end_of_existing_buffer_ts).magnitude();
  constexpr base::TimeDelta max_gap = base::TimeDelta::FromMilliseconds(1);
  if (gap > max_gap) {
    DLOG(ERROR) << "Large gap in sound. Forced flush."
                << " Gap/overlap duration: " << gap
                << " capture_ts: " << capture_ts
                << " next_timestamp_: " << next_timestamp_
                << " existing_buffer_duration: " << existing_buffer_duration
                << " end_of_existing_buffer_ts: " << end_of_existing_buffer_ts;
    FlushInternal();
    next_timestamp_ = capture_ts;
  }

  // The |fifo_| won't trigger OnFifoOutput() until we have enough frames
  // suitable for the converter.
  fifo_->Push(*audio_bus);
  if (!current_done_cb_.is_null()) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(OkStatus());
  }
}

void AudioOpusEncoder::Flush(StatusCB done_cb) {
  DCHECK(!done_cb.is_null());

  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(done_cb).Run(StatusCode::kEncoderInitializeNeverCompleted);
    return;
  }

  current_done_cb_ = BindToCurrentLoop(std::move(done_cb));
  FlushInternal();
  if (!current_done_cb_.is_null()) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(OkStatus());
  }
}

void AudioOpusEncoder::FlushInternal() {
  // This is needed to correctly compute the timestamp, since the number of
  // frames of |output_bus| provided to OnFifoOutput() will always be equal to
  // the full frames_per_buffer(), as the fifo's Flush() will pad the remaining
  // empty frames with zeros.
  number_of_flushed_frames_ = fifo_->queued_frames();
  fifo_->Flush();
  number_of_flushed_frames_.reset();
}

void AudioOpusEncoder::OnFifoOutput(const AudioBus& output_bus,
                                    int frame_delay) {
  // Provides input to the converter from |output_bus| within this scope only.
  ScopedConverterInputProvider provider(converter_.get(), &output_bus);
  converter_->Convert(converted_audio_bus_.get());
  converted_audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
      converted_audio_bus_->frames(), buffer_.data());

  std::unique_ptr<uint8_t[]> encoded_data(new uint8_t[kOpusMaxDataBytes]);
  auto result = opus_encode_float(opus_encoder_.get(), buffer_.data(),
                                  converted_params_.frames_per_buffer(),
                                  encoded_data.get(), kOpusMaxDataBytes);

  if (result < 0 && !current_done_cb_.is_null()) {
    std::move(current_done_cb_)
        .Run(Status(StatusCode::kEncoderFailedEncode, opus_strerror(result)));
    return;
  }

  auto encoded_duration = AudioTimestampHelper::FramesToTime(
      number_of_flushed_frames_.value_or(output_bus.frames()),
      input_params_.sample_rate());

  size_t encoded_data_size = result;
  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  if (encoded_data_size > 1) {
    base::Optional<CodecDescription> desc;
    if (need_to_emit_extra_data_) {
      desc = PrepareExtraData();
      need_to_emit_extra_data_ = false;
    }
    output_cb_.Run(
        EncodedAudioBuffer(converted_params_, std::move(encoded_data),
                           encoded_data_size, next_timestamp_),
        desc);
  }
  next_timestamp_ += encoded_duration;
}

base::TimeTicks AudioOpusEncoder::ComputeTimestamp(
    int num_frames,
    base::TimeTicks capture_time) const {
  return capture_time - AudioTimestampHelper::FramesToTime(
                            num_frames, input_params_.sample_rate());
}

// Creates and returns the libopus encoder instance. Returns nullptr if the
// encoder creation fails.
StatusOr<OwnedOpusEncoder> AudioOpusEncoder::CreateOpusEncoder() {
  int opus_result;
  OwnedOpusEncoder encoder(
      opus_encoder_create(converted_params_.sample_rate(),
                          converted_params_.channels(), OPUS_APPLICATION_AUDIO,
                          &opus_result),
      OpusEncoderDeleter);

  if (opus_result < 0) {
    return Status(
        StatusCode::kEncoderInitializationError,
        base::StringPrintf(
            "Couldn't init Opus encoder: %s, sample rate: %d, channels: %d",
            opus_strerror(opus_result), converted_params_.sample_rate(),
            converted_params_.channels()));
  }

  int bitrate =
      options_.bitrate.has_value() ? options_.bitrate.value() : OPUS_AUTO;
  if (encoder &&
      opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    return Status(
        StatusCode::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus bitrate: %d", bitrate));
  }

  return encoder;
}

}  // namespace media
