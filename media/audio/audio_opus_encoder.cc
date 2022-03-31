// Copyright 2020 The Chromium Authors. All rights reserved.
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
#include "media/base/bind_to_current_loop.h"
#include "media/base/channel_mixer.h"
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

}  // namespace

class AudioOpusEncoder::InputFramesFifo final
    : public AudioConverter::InputCallback {
 public:
  explicit InputFramesFifo(AudioConverter* converter) : converter_(converter) {
    DCHECK(converter_);
    converter_->AddInput(this);
  }

  ~InputFramesFifo() override { converter_->RemoveInput(this); }

  // Releases all |inputs_| and resets internal state.
  void Clear() {
    inputs_.clear();
    total_frames_ = 0;
    front_frame_index_ = 0;
    is_flushing_ = false;
  }

  // Add an input into the FIFO.
  void Push(std::unique_ptr<AudioBus> input_bus) {
    DCHECK(!is_flushing_);

    total_frames_ += input_bus->frames();
    inputs_.emplace_back(std::move(input_bus));
  }

  // Allows underflowing and filling data requests with silence.
  void Flush() {
    DCHECK(!is_flushing_);
    is_flushing_ = true;
  }

  int frames() { return total_frames_; }
  bool is_flushing() { return is_flushing_; }

  // AudioConverted::InputCallback:
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override {
    int frames_needed = audio_bus->frames();
    int frames_written = 0;

    // If we aren't flushing, this should only be called if we have enough
    // frames to completey satisfy the request.
    DCHECK(is_flushing_ || total_frames_ >= frames_needed);

    // Write until we've fulfilled the request or run out of frames.
    while (frames_written < frames_needed && total_frames_) {
      const AudioBus* front = inputs_.front().get();

      int frames_in_front = front->frames() - front_frame_index_;
      int frames_to_write =
          std::min(frames_needed - frames_written, frames_in_front);

      front->CopyPartialFramesTo(front_frame_index_, frames_to_write,
                                 frames_written, audio_bus);

      frames_written += frames_to_write;
      front_frame_index_ += frames_to_write;
      total_frames_ -= frames_to_write;

      if (front_frame_index_ == front->frames()) {
        // We exhausted all frames in the front buffer, remove it.
        inputs_.pop_front();
        front_frame_index_ = 0;
      }
    }

    // We should only run out of frames if we're flushing.
    if (frames_written != frames_needed) {
      DCHECK(is_flushing_);
      DCHECK(!total_frames_);
      DCHECK(!inputs_.size());
      audio_bus->ZeroFramesPartial(frames_written,
                                   frames_needed - frames_written);
    }

    return 1.0f;
  }

 private:
  bool is_flushing_ = false;

  // Index to the first unused frame in |inputs_.front()|.
  int front_frame_index_ = 0;

  // How many frames are in |inputs_|.
  int total_frames_ = 0;

  const raw_ptr<AudioConverter> converter_;
  base::circular_deque<std::unique_ptr<AudioBus>> inputs_;
};

// TODO: Remove after switching to C++17
constexpr int AudioOpusEncoder::kMinBitrate;

AudioOpusEncoder::AudioOpusEncoder()
    : opus_encoder_(nullptr, OpusEncoderDeleter) {}

void AudioOpusEncoder::Initialize(const Options& options,
                                  OutputCB output_callback,
                                  EncoderStatusCB done_cb) {
  DCHECK(output_callback);
  DCHECK(done_cb);

  done_cb = BindToCurrentLoop(std::move(done_cb));
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

  converter_ =
      std::make_unique<AudioConverter>(input_params_, converted_params_,
                                       /*disable_fifo=*/false);
  timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(converted_params_.sample_rate());
  fifo_ = std::make_unique<InputFramesFifo>(converter_.get());
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
  min_input_frames_needed_ = converter_->GetMaxInputFramesRequested(
      converted_params_.frames_per_buffer());

  output_cb_ = BindToCurrentLoop(std::move(output_callback));
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

std::unique_ptr<AudioBus> AudioOpusEncoder::EnsureExpectedChannelCount(
    std::unique_ptr<AudioBus> audio_bus) {
  // No mixing required.
  if (audio_bus->channels() == input_params_.channels())
    return audio_bus;

  const int& incoming_channels = audio_bus->channels();
  if (!mixer_ || mixer_input_params_.channels() != incoming_channels) {
    // Both the format and the sample rate are unused for mixing, but we still
    // need to pass a value below.
    mixer_input_params_.Reset(input_params_.format(),
                              GuessChannelLayout(incoming_channels),
                              incoming_channels, input_params_.sample_rate());

    mixer_ = std::make_unique<ChannelMixer>(mixer_input_params_, input_params_);
  }

  auto mixed_bus =
      AudioBus::Create(input_params_.channels(), audio_bus->frames());

  mixer_->Transform(audio_bus.get(), mixed_bus.get());

  return mixed_bus;
}

void AudioOpusEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                              base::TimeTicks capture_time,
                              EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb);

  current_done_cb_ = BindToCurrentLoop(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(current_done_cb_)
        .Run(EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  DCHECK(timestamp_tracker_);

  if (timestamp_tracker_->base_timestamp() == kNoTimestamp)
    timestamp_tracker_->SetBaseTimestamp(capture_time - base::TimeTicks());

  fifo_->Push(EnsureExpectedChannelCount(std::move(audio_bus)));

  while (fifo_->frames() >= min_input_frames_needed_ && current_done_cb_)
    OnEnoughInputFrames();

  if (current_done_cb_) {
    // Is |current_done_cb_| is null, it means OnEnoughInputFrames() has already
    // reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioOpusEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK(done_cb);

  done_cb = BindToCurrentLoop(std::move(done_cb));
  if (!opus_encoder_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  current_done_cb_ = std::move(done_cb);

  // Start filling partially fulfilled ProvideInput() calls with silence.
  fifo_->Flush();

  while (fifo_->frames())
    OnEnoughInputFrames();

  // Clear any excess buffered silence. Re-prime the |converter_|, otherwise
  // the value of |min_input_frames_needed_| might be wrong for the first calls
  // to Convert().
  converter_->Reset();
  converter_->PrimeWithSilence();

  // Clear the flushing flag.
  fifo_->Clear();

  timestamp_tracker_->SetBaseTimestamp(kNoTimestamp);
  if (current_done_cb_) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioOpusEncoder::OnEnoughInputFrames() {
  // We should have enough frames to satisfy all request, or be in the process
  // of flushing the |fifo_|.
  DCHECK(fifo_->frames() >= min_input_frames_needed_ || fifo_->is_flushing());

  // Provides input to the converter from |output_bus| within this scope only.
  converter_->Convert(converted_audio_bus_.get());
  converted_audio_bus_->ToInterleaved<Float32SampleTypeTraits>(
      converted_audio_bus_->frames(), buffer_.data());

  std::unique_ptr<uint8_t[]> encoded_data(new uint8_t[kOpusMaxDataBytes]);
  auto result = opus_encode_float(opus_encoder_.get(), buffer_.data(),
                                  converted_params_.frames_per_buffer(),
                                  encoded_data.get(), kOpusMaxDataBytes);

  if (result < 0 && current_done_cb_) {
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
