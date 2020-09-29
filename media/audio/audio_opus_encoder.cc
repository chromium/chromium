// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_opus_encoder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
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

// The amount of Frames in a 60 ms buffer @ 48000 samples/second.
constexpr int kOpusPreferredFramesPerBuffer =
    kOpusPreferredSamplingRate * kOpusPreferredBufferDurationMs /
    base::Time::kMillisecondsPerSecond;

// Deletes the libopus encoder instance pointed to by |encoder_ptr|.
inline void OpusEncoderDeleter(OpusEncoder* encoder_ptr) {
  opus_encoder_destroy(encoder_ptr);
}

// Adjusts the given input |params| to have a frames-per-buffer value that
// matches that of the FIFO which buffers the input audio before sending it to
// the converter.
AudioParameters AdjustInputParamsForOpus(const AudioParameters& params) {
  auto adjusted_params = params;
  adjusted_params.set_frames_per_buffer(params.sample_rate() *
                                        kOpusPreferredBufferDurationMs /
                                        base::Time::kMillisecondsPerSecond);
  return adjusted_params;
}

// Creates the audio parameters of the converted audio format that Opus prefers,
// which will be used as the input to the libopus encoder.
AudioParameters CreateOpusInputParams(const AudioParameters& input_params) {
  // third_party/libopus supports up to 2 channels (see implementation of
  // opus_encoder_create()): force |converted_params| to at most those.
  // Also, the libopus encoder can accept sample rates of 8, 12, 16, 24, and the
  // default preferred 48 kHz. If the input sample rate is anything else, we'll
  // use 48 kHz.
  const int input_rate = input_params.sample_rate();
  const int used_rate = (input_rate == 8000 || input_rate == 12000 ||
                         input_rate == 16000 || input_rate == 24000)
                            ? input_rate
                            : kOpusPreferredSamplingRate;
  const int frames_per_buffer = used_rate * kOpusPreferredBufferDurationMs /
                                base::Time::kMillisecondsPerSecond;

  const auto converted_params =
      AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                      GuessChannelLayout(std::min(input_params.channels(), 2)),
                      used_rate, frames_per_buffer);
  DVLOG(1) << "|input_params|:" << input_params.AsHumanReadableString()
           << " -->|converted_params|:"
           << converted_params.AsHumanReadableString();
  DCHECK(converted_params.IsValid());

  return converted_params;
}

// Creates and returns the libopus encoder instance. Returns nullptr if the
// encoder creation fails.
OwnedOpusEncoder CreateOpusEncoder(
    const AudioEncoder::StatusCB& status_callback,
    const AudioParameters& params,
    int32_t bitrate) {
  int opus_result;
  OwnedOpusEncoder encoder(
      opus_encoder_create(params.sample_rate(), params.channels(),
                          OPUS_APPLICATION_AUDIO, &opus_result),
      OpusEncoderDeleter);

  if (opus_result < 0) {
    status_callback.Run(Status(
        StatusCode::kEncoderInitializationError,
        base::StringPrintf(
            "Couldn't init Opus encoder: %s, sample rate: %d, channels: %d",
            opus_strerror(opus_result), params.sample_rate(),
            params.channels())));
  }

  if (encoder &&
      opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    status_callback.Run(
        Status(StatusCode::kEncoderInitializationError,
               base::StringPrintf("Failed to set Opus bitrate: %d", bitrate)));
    encoder.reset();
  }

  return encoder;
}

// Tries to encode |in_data|'s |num_samples| into |out_data|. |out_data| is
// always resized to |kOpusMaxDataBytes| but only filled with |*out_size| actual
// encoded data if encoding was successful. Returns true if encoding is
// successful, in which case |*out_size| is guaranteed to be > 1. Returns false
// if an error occurs or the packet does not need to be transmitted.
// |status_callback| will be used to report any errors.
bool DoEncode(const AudioEncoder::StatusCB& status_callback,
              OpusEncoder* opus_encoder,
              float* in_data,
              int num_samples,
              std::unique_ptr<uint8_t[]>* out_data,
              size_t* out_size) {
  DCHECK(opus_encoder);
  DCHECK(in_data);
  DCHECK(out_data);
  DCHECK(out_size);
  DCHECK_LE(num_samples, kOpusPreferredFramesPerBuffer);

  out_data->reset(new uint8_t[kOpusMaxDataBytes]);
  const opus_int32 result = opus_encode_float(
      opus_encoder, in_data, num_samples, out_data->get(), kOpusMaxDataBytes);

  if (result > 1) {
    // TODO(ajose): Investigate improving this. http://crbug.com/547918
    *out_size = result;
    return true;
  }

  // If |result| in {0,1}, do nothing; the documentation says that a return
  // value of zero or one means the packet does not need to be transmitted.
  // Otherwise, we have an error.
  if (result < 0) {
    status_callback.Run(
        Status(StatusCode::kEncoderFailedEncode, opus_strerror(result)));
  }

  return false;
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

AudioOpusEncoder::AudioOpusEncoder(const AudioParameters& input_params,
                                   EncodeCB encode_callback,
                                   StatusCB status_callback,
                                   int32_t opus_bitrate)
    : AudioEncoder(AdjustInputParamsForOpus(input_params),
                   std::move(encode_callback),
                   std::move(status_callback)),
      bits_per_second_(opus_bitrate > 0 ? opus_bitrate : OPUS_AUTO),
      converted_params_(CreateOpusInputParams(audio_input_params())),
      converter_(audio_input_params(),
                 converted_params_,
                 /*disable_fifo=*/false),
      fifo_(base::BindRepeating(&AudioOpusEncoder::OnFifoOutput,
                                base::Unretained(this))),
      converted_audio_bus_(
          AudioBus::Create(converted_params_.channels(),
                           converted_params_.frames_per_buffer())),
      buffer_(converted_params_.channels() *
              converted_params_.frames_per_buffer()),
      opus_encoder_(CreateOpusEncoder(this->status_callback(),
                                      converted_params_,
                                      bits_per_second_)) {
  converter_.PrimeWithSilence();
  fifo_.Reset(converter_.GetMaxInputFramesRequested(
      converted_params_.frames_per_buffer()));
}

AudioOpusEncoder::~AudioOpusEncoder() = default;

void AudioOpusEncoder::EncodeAudioImpl(const AudioBus& audio_bus,
                                       base::TimeTicks capture_time) {
  // Initializing the opus encoder may have failed.
  if (!opus_encoder_)
    return;

  // The |fifo_| won't trigger OnFifoOutput() until we have enough frames
  // suitable for the converter.
  fifo_.Push(audio_bus);
}

void AudioOpusEncoder::OnFifoOutput(const AudioBus& output_bus,
                                    int frame_delay) {
  // Provides input to the converter from |output_bus| within this scope only.
  ScopedConverterInputProvider provider(&converter_, &output_bus);
  converter_.Convert(converted_audio_bus_.get());
  converted_audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
      converted_audio_bus_->frames(), buffer_.data());

  std::unique_ptr<uint8_t[]> encoded_data;
  size_t encoded_data_size;
  if (DoEncode(status_callback(), opus_encoder_.get(), buffer_.data(),
               converted_params_.frames_per_buffer(), &encoded_data,
               &encoded_data_size)) {
    DCHECK_GT(encoded_data_size, 1u);
    encode_callback().Run(EncodedAudioBuffer(
        converted_params_, std::move(encoded_data), encoded_data_size,
        ComputeTimestamp(output_bus.frames(), last_capture_time())));
  }
}

}  // namespace media
