// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_opus_encoder.h"

#include <array>
#include <utility>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_sample_types.h"
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

constexpr base::TimeDelta kMinFrameDuration = base::Microseconds(2500);

// Keep this in reverse order as `ComputeCompatibleFrameDuration` grabs the
// largest compatible frame greedily.
constexpr std::array<base::TimeDelta, 6> kStandardDurations = {
    base::Milliseconds(60), base::Milliseconds(40), base::Milliseconds(20),
    base::Milliseconds(10), base::Milliseconds(5),  base::Microseconds(2500)};

// Deletes the libopus encoder instance pointed to by |encoder_ptr|.
inline void OpusEncoderDeleter(OpusEncoder* encoder_ptr) {
  opus_encoder_destroy(encoder_ptr);
}

inline void OpusRepacketizerDeleter(OpusRepacketizer* repacketizer_ptr) {
  opus_repacketizer_destroy(repacketizer_ptr);
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

// Check if the duration is explicitly 2.5ms, 5, 10, 20, 40, 60.
bool IsStandardFrameDuration(base::TimeDelta frame_duration) {
  return std::ranges::find(kStandardDurations, frame_duration) !=
         kStandardDurations.end();
}

// Checks that we are only giving Valid Frame durations.
bool IsValidFrameDuration(base::TimeDelta frame_duration) {
  return frame_duration <= base::Milliseconds(120) &&
         frame_duration >= kMinFrameDuration &&
         (frame_duration % kMinFrameDuration).is_zero();
}

// Finds an appropriate standard duration based on the given `frame_duration`
// The current implementation is merely finding the largest standard duration
// that is divisible by the given frame duration.
base::TimeDelta ComputeCompatibleFrameDuration(base::TimeDelta frame_duration) {
  auto is_divisible = [frame_duration](base::TimeDelta duration) {
    return (frame_duration % duration).is_zero();
  };

  auto result = std::ranges::find_if(kStandardDurations, is_divisible);
  return *result;
}

}  // namespace

AudioOpusEncoder::AudioOpusEncoder()
    : opus_encoder_(nullptr, OpusEncoderDeleter),
      opus_repacketizer_(nullptr, OpusRepacketizerDeleter) {}

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

  final_frame_duration_ = GetFrameDuration(options_.opus);
  auto intermediate_frame_duration = final_frame_duration_;
  if (!IsValidFrameDuration(final_frame_duration_)) {
    std::move(done_cb).Run(
        EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                      "Invalid frame duration."));
    return;
  }

  if (!IsStandardFrameDuration(final_frame_duration_)) {
    // If it is a valid duration but not one of the "standard" durations, we
    // will need to use the repacketizer.
    intermediate_frame_duration =
        ComputeCompatibleFrameDuration(final_frame_duration_);

    max_packets_in_repacketizer_ =
        final_frame_duration_ / intermediate_frame_duration;

    pending_packets_.resize(max_packets_in_repacketizer_);
    for (auto& v : pending_packets_) {
      v = EncodingBuffer::Uninit(kOpusMaxDataBytes);
    }

    opus_repacketizer_ = OwnedOpusRepacketizer(opus_repacketizer_create(),
                                               OpusRepacketizerDeleter);
    if (!opus_repacketizer_) {
      std::move(done_cb).Run(
          EncoderStatus(EncoderStatus::Codes::kEncoderInitializationError,
                        "Failed to create Opus repacketizer."));
      return;
    }
  }

  input_params_ = CreateInputParams(options, final_frame_duration_);
  if (!input_params_.IsValid()) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  // We use `intermediate_params_` to correctly initialize the FIFO to handle
  // the compatible frame durations. In the case of no repacketizer, this will
  // be the same as `converted_params_`.
  auto intermediate_params =
      CreateOpusCompatibleParams(input_params_, intermediate_frame_duration);
  DCHECK(intermediate_params.IsValid());

  intermediate_frame_count_ = intermediate_params.frames_per_buffer();

  fifo_ =
      std::make_unique<ConvertingAudioFifo>(input_params_, intermediate_params);

  converted_params_ =
      CreateOpusCompatibleParams(input_params_, final_frame_duration_);
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
  static constexpr auto kExtraDataTemplate = std::to_array<const uint8_t>(
      {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd',
       1,           // offset 8, version, always 1
       0,           // offset 9, channel count
       0, 0,        // offset 10, pre-skip
       0, 0, 0, 0,  // offset 12, original input sample rate in Hz
       0, 0, 0});

  extra_data.assign(kExtraDataTemplate.begin(), kExtraDataTemplate.end());

  // Save number of channels
  base::CheckedNumeric<uint8_t> channels(converted_params_.channels());
  base::SpanWriter extra_data_writer(base::as_writable_byte_span(extra_data));
  extra_data_writer.Skip<9u>();
  extra_data_writer.WriteU8NativeEndian(channels.ValueOrDefault(0));
  // Number of samples to skip from the start of the decoder's output.
  // Real data begins this many samples late. These samples need to be skipped
  // only at the very beginning of the audio stream, NOT at beginning of each
  // decoded output.
  base::CheckedNumeric<uint16_t> samples_to_skip_safe = 0;
  if (opus_encoder_) {
    int32_t samples_to_skip = 0;

    static_assert(sizeof(int32_t) == sizeof(opus_int32));
    opus_encoder_ctl(opus_encoder_.get(),
                     // SAFETY: In `OPUS_GET_LOOKAHEAD`, we check the pointer
                     // type. We require a pointer of type `opus_int32`. Our
                     // static assertion ensures that it is safe.
                     UNSAFE_BUFFERS(OPUS_GET_LOOKAHEAD(&samples_to_skip)));
    samples_to_skip_safe = samples_to_skip;
  }
  extra_data_writer.WriteU16NativeEndian(
      samples_to_skip_safe.ValueOrDefault(0));

  // Save original sample rate
  base::CheckedNumeric<uint16_t> sample_rate = input_params_.sample_rate();
  extra_data_writer.WriteU16NativeEndian(
      sample_rate.ValueOrDefault(uint16_t{kOpusPreferredSamplingRate}));
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
    static_assert(sizeof(int32_t) == sizeof(opus_int32));
    opus_encoder_ctl(opus_encoder_.get(),
                     // SAFETY: In `OPUS_GET_LOOKAHEAD`, we check the pointer
                     // type. We require a pointer of type `opus_int32`. Our
                     // static assertion ensures that it is safe.
                     UNSAFE_BUFFERS(OPUS_GET_LOOKAHEAD(&encoder_delay)));

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

    // Add silence in order to flush remaining frames in repacketizer as a
    // full packet.
    if (packets_in_repacketizer_ > 0) {
      waiting_for_output_ = true;
      auto silent_padding = AudioBus::Create(converted_params_.channels(),
                                             intermediate_frame_count_);
      silent_padding->Zero();
      while (waiting_for_output_ && current_done_cb_) {
        DoEncode(silent_padding.get());
      }
    }
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

base::span<uint8_t> AudioOpusEncoder::GetEncoderDestination() {
  if (!opus_repacketizer_) {
    return encoding_buffer_;
  }

  // Get the next free packet.
  CHECK_LT(packets_in_repacketizer_, max_packets_in_repacketizer_);
  return pending_packets_[packets_in_repacketizer_];
}

void AudioOpusEncoder::DoEncode(const AudioBus* audio_bus) {
  audio_bus->ToInterleaved<Float32SampleTypeTraits>(audio_bus->frames(),
                                                    buffer_.data());
  // We already reported an error. Don't attempt to encode any further inputs.
  if (!current_done_cb_)
    return;

  auto buffer = GetEncoderDestination();

  auto result = opus_encode_float(opus_encoder_.get(), buffer_.data(),
                                  intermediate_frame_count_, buffer.data(),
                                  kOpusMaxDataBytes);

  if (result < 0) {
    DCHECK(current_done_cb_);
    std::move(current_done_cb_)
        .Run(EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                           opus_strerror(result)));
    return;
  } else if (result <= 1) {
    // If |result| in {0,1}, do nothing; the documentation says that a return
    // value of zero or one means the packet does not need to be transmitted.
    return;
  }

  if (!opus_repacketizer_) {
    EmitEncodedBuffer(result);
    return;
  }

  int status =
      opus_repacketizer_cat(opus_repacketizer_.get(), buffer.data(), result);

  if (status != OPUS_OK) {
    DCHECK(current_done_cb_);
    std::move(current_done_cb_)
        .Run(EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                           "Failed to add to Repacketizer."));
    return;
  }

  ++packets_in_repacketizer_;

  if (packets_in_repacketizer_ == max_packets_in_repacketizer_) {
    auto encoded_data_size = opus_repacketizer_out(
        opus_repacketizer_.get(), encoding_buffer_.data(), kOpusMaxDataBytes);
    if (encoded_data_size < 0) {
      std::move(current_done_cb_)
          .Run(EncoderStatus(EncoderStatus::Codes::kEncoderFailedEncode,
                             "Failed to read from Repacketizer."));
      return;
    }
    EmitEncodedBuffer(encoded_data_size);
    opus_repacketizer_init(opus_repacketizer_.get());
    packets_in_repacketizer_ = 0;
  }
  DCHECK_LT(packets_in_repacketizer_, max_packets_in_repacketizer_);
}

void AudioOpusEncoder::EmitEncodedBuffer(size_t encoded_data_size) {
  std::optional<CodecDescription> desc;
  if (need_to_emit_extra_data_) {
    desc = PrepareExtraData();
    need_to_emit_extra_data_ = false;
  }

  auto ts = base::TimeTicks() + timestamp_tracker_->GetTimestamp();

  EncodedAudioBuffer encoded_buffer(
      converted_params_,
      base::HeapArray<uint8_t>::CopiedFrom(
          base::span(encoding_buffer_).first(encoded_data_size)),
      ts, final_frame_duration_);
  output_cb_.Run(std::move(encoded_buffer), desc);
  timestamp_tracker_->AddFrames(AudioTimestampHelper::TimeToFrames(
      final_frame_duration_, converted_params_.sample_rate()));
  waiting_for_output_ = false;
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
