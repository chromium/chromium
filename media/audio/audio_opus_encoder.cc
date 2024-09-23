// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/audio_opus_encoder.h"

#include <utility>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_encoder.h"
#include "media/base/channel_mixer.h"
#include "media/base/converting_audio_fifo.h"
#include "media/base/encoder_status.h"
#include "media/base/timestamp_constants.h"

namespace media {

namespace {

// Opus preferred sampling rate for encoding. This is also the one WebM likes
// to have: https://wiki.xiph.org/MatroskaOpus.
constexpr int kOpusPreferredSamplingRate = 48000;

// For Opus, 20ms is the suggested default.
constexpr base::TimeDelta kDefaultOpusBufferDuration = base::Milliseconds(20);

// Deletes the libopus encoder instance pointed to by |encoder_ptr|.
inline void OpusEncoderDeleter(OpusEncoder* encoder_ptr) {
  opus_encoder_destroy(encoder_ptr);
}

base::TimeDelta GetFrameDuration(
    const std::optional<AudioEncoder::OpusOptions> opus_options) {
  return opus_options.has_value() ? opus_options.value().frame_duration
                                  : kDefaultOpusBufferDuration;
}

AudioParameters CreateInputParams(const AudioEncoder::Options& options,
                                  base::TimeDelta frame_duration) {
  const int frames_per_buffer =
      AudioTimestampHelper::TimeToFrames(frame_duration, options.sample_rate);
  AudioParameters result(media::AudioParameters::AUDIO_PCM_LINEAR,
                         {media::CHANNEL_LAYOUT_DISCRETE, options.channels},
                         options.sample_rate, frames_per_buffer);
  return result;
}

// Creates the audio parameters of the converted audio format that Opus prefers,
// which will be used as the input to the libopus encoder.
AudioParameters CreateOpusCompatibleParams(const AudioParameters& params,
                                           base::TimeDelta frame_duration) {
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
  const int frames_per_buffer =
      AudioTimestampHelper::TimeToFrames(frame_duration, used_rate);

  AudioParameters result(
      AudioParameters::AUDIO_PCM_LOW_LATENCY,
      ChannelLayoutConfig::Guess(std::min(params.channels(), 2)), used_rate,
      frames_per_buffer);
  return result;
}

}  // namespace


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
  const base::TimeDelta frame_duration = GetFrameDuration(options_.opus);
  input_params_ = CreateInputParams(options, frame_duration);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  converted_params_ = CreateOpusCompatibleParams(input_params_, frame_duration);
  if (!converted_params_.IsValid()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  fifo_ =
      std::make_unique<ConvertingAudioFifo>(input_params_, converted_params_);

  timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(converted_params_.sample_rate());
  buffer_.resize(converted_params_.channels() *
                 converted_params_.frames_per_buffer());
  auto status_or_encoder = CreateOpusEncoder(options.opus);
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

  if (!timestamp_tracker_->base_timestamp()) {
    timestamp_tracker_->SetBaseTimestamp(capture_time - base::TimeTicks());
  }

  fifo_->Push(std::move(audio_bus));
  fifo_has_data_ = true;
  DrainFifoOutput();

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

  if (fifo_has_data_) {
    int32_t encoder_delay = 0;
    opus_encoder_ctl(opus_encoder_.get(), OPUS_GET_LOOKAHEAD(&encoder_delay));

    // Add enough silence to the queue to guarantee that all audible frames will
    // be output from the encoder.
    if (encoder_delay) {
      int encoder_delay_in_input_frames = std::ceil(
          static_cast<double>(encoder_delay) * input_params_.sample_rate() /
          converted_params_.sample_rate());

      auto silent_delay = AudioBus::Create(input_params_.channels(),
                                           encoder_delay_in_input_frames);
      silent_delay->Zero();
      fifo_->Push(std::move(silent_delay));
    }

    fifo_->Flush();
    DrainFifoOutput();
    fifo_has_data_ = false;
  }

  timestamp_tracker_->Reset();
  if (current_done_cb_) {
    // Is |current_done_cb_| is null, it means OnFifoOutput() has already
    // reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioOpusEncoder::DrainFifoOutput() {
  while (fifo_->HasOutput()) {
    DoEncode(fifo_->PeekOutput());
    fifo_->PopOutput();
  }
}

void AudioOpusEncoder::DoEncode(const AudioBus* audio_bus) {
  audio_bus->ToInterleaved<Float32SampleTypeTraits>(audio_bus->frames(),
                                                    buffer_.data());
  // We already reported an error. Don't attempt to encode any further inputs.
  if (!current_done_cb_)
    return;

  auto result = opus_encode_float(opus_encoder_.get(), buffer_.data(),
                                  converted_params_.frames_per_buffer(),
                                  encoding_buffer_.data(), kOpusMaxDataBytes);

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
    std::optional<CodecDescription> desc;
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

    EncodedAudioBuffer encoded_buffer(
        converted_params_,
        base::HeapArray<uint8_t>::CopiedFrom(
            base::span(encoding_buffer_).first(encoded_data_size)),
        ts, duration);
    output_cb_.Run(std::move(encoded_buffer), desc);
  }
  timestamp_tracker_->AddFrames(converted_params_.frames_per_buffer());
}

// Creates and returns the libopus encoder instance. Returns nullptr if the
// encoder creation fails.
EncoderStatus::Or<OwnedOpusEncoder> AudioOpusEncoder::CreateOpusEncoder(
    const std::optional<AudioEncoder::OpusOptions>& opus_options) {
  int opus_result;
  OwnedOpusEncoder encoder(
      opus_encoder_create(converted_params_.sample_rate(),
                          converted_params_.channels(), OPUS_APPLICATION_AUDIO,
                          &opus_result),
      OpusEncoderDeleter);

  if (opus_result < 0 || !encoder) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf(
            "Couldn't init Opus encoder: %s, sample rate: %d, channels: %d",
            opus_strerror(opus_result), converted_params_.sample_rate(),
            converted_params_.channels()));
  }

  const int bitrate =
      options_.bitrate.has_value() ? options_.bitrate.value() : OPUS_AUTO;
  if (opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(bitrate)) != OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus bitrate: %d", bitrate));
  }

  if (options_.bitrate_mode.has_value()) {
    bool vbr_enabled =
        options_.bitrate_mode == media::AudioEncoder::BitrateMode::kVariable;

    if (opus_encoder_ctl(encoder.get(), OPUS_SET_VBR(vbr_enabled ? 1 : 0)) !=
        OPUS_OK) {
      return EncoderStatus(
          EncoderStatus::Codes::kEncoderInitializationError,
          base::StringPrintf("Failed to set Opus bitrateMode: %d",
                             vbr_enabled));
    }
  }

  // The remaining parameters are all purely optional.
  if (!opus_options.has_value()) {
    return encoder;
  }

  const unsigned int complexity = opus_options.value().complexity;
  DCHECK_LE(complexity, 10u);
  if (opus_encoder_ctl(encoder.get(), OPUS_SET_COMPLEXITY(complexity)) !=
      OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus complexity: %d", complexity));
  }

  const unsigned int packet_loss_perc = opus_options.value().packet_loss_perc;
  DCHECK_LE(packet_loss_perc, 100u);
  if (opus_encoder_ctl(encoder.get(), OPUS_SET_PACKET_LOSS_PERC(
                                          packet_loss_perc)) != OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus packetlossperc: %d",
                           packet_loss_perc));
  }

  const unsigned int use_in_band_fec =
      opus_options.value().use_in_band_fec ? 1 : 0;
  if (opus_encoder_ctl(encoder.get(), OPUS_SET_INBAND_FEC(use_in_band_fec)) !=
      OPUS_OK) {
    return EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                         base::StringPrintf("Failed to set Opus inband FEC: %d",
                                            use_in_band_fec));
  }

  unsigned int opus_signal;
  switch (opus_options.value().signal) {
    case AudioEncoder::OpusSignal::kAuto:
      opus_signal = OPUS_AUTO;
      break;
    case AudioEncoder::OpusSignal::kMusic:
      opus_signal = OPUS_SIGNAL_MUSIC;
      break;
    case AudioEncoder::OpusSignal::kVoice:
      opus_signal = OPUS_SIGNAL_VOICE;
      break;
  }

  if (opus_encoder_ctl(encoder.get(), OPUS_SET_SIGNAL(opus_signal)) !=
      OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus signal hint: %d", opus_signal));
  }

  const unsigned int use_dtx = opus_options.value().use_dtx ? 1 : 0;
  if (opus_encoder_ctl(encoder.get(), OPUS_SET_DTX(use_dtx)) != OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus DTX: %d", use_dtx));
  }

  unsigned int opus_application;
  switch (opus_options.value().application) {
    case AudioEncoder::OpusApplication::kVoip:
      opus_application = OPUS_APPLICATION_VOIP;
      break;
    case AudioEncoder::OpusApplication::kAudio:
      opus_application = OPUS_APPLICATION_AUDIO;
      break;
    case AudioEncoder::OpusApplication::kLowDelay:
      opus_application = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
      break;
  }

  if (opus_encoder_ctl(encoder.get(), OPUS_SET_APPLICATION(opus_application)) !=
      OPUS_OK) {
    return EncoderStatus(
        EncoderStatus::Codes::kEncoderInitializationError,
        base::StringPrintf("Failed to set Opus application hint: %d",
                           opus_application));
  }

  return encoder;
}

}  // namespace media
