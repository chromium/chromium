// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/opus_audio_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>

#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/decoder_buffer.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/common/opus_constants.h"
#include "third_party/opus/src/include/opus.h"
#include "third_party/opus/src/include/opus_multistream.h"

namespace media {

void OpusMSDecoderDeleter::operator()(OpusMSDecoder* ptr) const {
  if (ptr) {
    opus_multistream_decoder_destroy(ptr);
  }
}

namespace {

// Maximum packet size used in Xiph's opusdec and FFmpeg's libopusdec.
static constexpr int kMaxOpusOutputPacketSizeSamples = 960 * 6;

void RemapOpusChannelLayout(base::span<const uint8_t> opus_mapping,
                            int num_channels,
                            base::span<uint8_t> channel_layout) {
  CHECK_LE(num_channels, OPUS_MAX_VORBIS_CHANNELS);
  CHECK_EQ(static_cast<size_t>(num_channels), channel_layout.size());

  // Reorder the channels to produce the same ordering as FFmpeg, which is
  // what the pipeline expects.
  base::span<const uint8_t> vorbis_layout_offset =
      base::span(kOpusVorbisChannelMap)[num_channels - 1];
  for (int channel = 0; channel < num_channels; ++channel) {
    channel_layout[channel] = opus_mapping[vorbis_layout_offset[channel]];
  }
}

struct OpusExtraData {
  int channels = 0;
  uint16_t skip_samples = 0;
  int channel_mapping = 0;
  int num_streams = 0;
  int num_coupled = 0;
  int16_t gain_db = 0;

  // Initialize with the default opus channel layout.
  std::array<uint8_t, OPUS_MAX_VORBIS_CHANNELS> stream_map = {0, 1};
};

std::optional<OpusExtraData> ParseOpusExtraData(
    base::span<const uint8_t> data,
    const AudioDecoderConfig& config) {
  // WebCodecs allows developers to omit the extradata (description) for Opus
  // streams with 1 or 2 channels. In these cases, we manually construct a
  // default OpusExtraData block. We cannot do this for > 2 channels since the
  // channel mapping is unpredictable.
  if (data.empty() &&
      config.channels() <= OPUS_MAX_CHANNELS_WITH_DEFAULT_LAYOUT) {
    OpusExtraData extra_data;
    extra_data.channels = config.channels();
    extra_data.skip_samples = config.codec_delay();
    extra_data.num_streams = 1;
    extra_data.num_coupled = config.channels() > 1 ? 1 : 0;
    return extra_data;
  }

  if (data.size() < OPUS_EXTRADATA_SIZE) {
    return std::nullopt;
  }
  base::SpanReader reader(data);
  if (!reader.Skip(OPUS_EXTRADATA_CHANNELS_OFFSET)) {
    return std::nullopt;
  }

  uint8_t channels;
  if (!reader.ReadU8BigEndian(channels)) {
    return std::nullopt;
  }

  OpusExtraData extra_data;
  extra_data.channels = channels;
  if (extra_data.channels <= 0 ||
      extra_data.channels > OPUS_MAX_VORBIS_CHANNELS) {
    DLOG(ERROR) << "invalid channel count in extra data: "
                << extra_data.channels;
    return std::nullopt;
  }

  if (!reader.ReadU16LittleEndian(extra_data.skip_samples)) {
    return std::nullopt;
  }

  if (!reader.Skip(OPUS_EXTRADATA_GAIN_OFFSET - reader.num_read())) {
    return std::nullopt;
  }

  if (!reader.ReadI16LittleEndian(extra_data.gain_db)) {
    return std::nullopt;
  }

  uint8_t channel_mapping;
  if (!reader.ReadU8BigEndian(channel_mapping)) {
    return std::nullopt;
  }
  extra_data.channel_mapping = channel_mapping;

  if (!extra_data.channel_mapping) {
    if (extra_data.channels > OPUS_MAX_CHANNELS_WITH_DEFAULT_LAYOUT) {
      return std::nullopt;
    }

    extra_data.num_streams = 1;
    extra_data.num_coupled = config.channels() > 1 ? 1 : 0;
    return extra_data;
  }

  uint8_t num_streams;
  if (!reader.ReadU8BigEndian(num_streams)) {
    return std::nullopt;
  }
  extra_data.num_streams = num_streams;

  uint8_t num_coupled;
  if (!reader.ReadU8BigEndian(num_coupled)) {
    return std::nullopt;
  }
  extra_data.num_coupled = num_coupled;

  if (extra_data.num_streams + extra_data.num_coupled != extra_data.channels) {
    DVLOG(1) << "Inconsistent channel mapping.";
  }

  for (int i = 0; i < extra_data.channels; ++i) {
    uint8_t stream_map_value;
    if (!reader.ReadU8BigEndian(stream_map_value)) {
      DLOG(ERROR)
          << "Invalid stream map; insufficient data for current channel "
          << "count: " << extra_data.channels;
      return std::nullopt;
    }
    extra_data.stream_map[i] = stream_map_value;
  }
  return extra_data;
}

}  // namespace

OpusAudioDecoder::OpusAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ExecutionMode mode)
    : task_runner_(std::move(task_runner)),
      mode_(mode),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  if (mode_ == ExecutionMode::kAsynchronous) {
    CHECK(task_runner_);
  }
}

AudioDecoderType OpusAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kOpus;
}

void OpusAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* /* cdm_context */,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (config.is_encrypted()) {
    BindCallbackIfNeeded(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  config_ = config;
  output_cb_ = BindCallbackIfNeeded(output_cb);

  if (!ConfigureDecoder()) {
    BindCallbackIfNeeded(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  BindCallbackIfNeeded(std::move(init_cb)).Run(DecoderStatus::Codes::kOk);
}

void OpusAudioDecoder::Decode(scoped_refptr<DecoderBuffer> input,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!decode_cb.is_null());
  CHECK(input.get());

  // Libopus does not buffer output. Decoding is complete when an end of stream
  // input buffer is received.
  if (input->end_of_stream()) {
    BindCallbackIfNeeded(std::move(decode_cb)).Run(DecoderStatus::Codes::kOk);
    return;
  }

  if (input->is_encrypted()) {
    DLOG(ERROR) << "Encrypted buffer not supported";
    BindCallbackIfNeeded(std::move(decode_cb))
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (input->timestamp() == kNoTimestamp) {
    DLOG(ERROR) << "Received a buffer without timestamps!";
    BindCallbackIfNeeded(std::move(decode_cb))
        .Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // Allocate a buffer for the output samples.
  const bool result = DecodeBuffer(input);
  BindCallbackIfNeeded(std::move(decode_cb))
      .Run(result ? DecoderStatus::Codes::kOk : DecoderStatus::Codes::kFailed);
}

void OpusAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  opus_multistream_decoder_ctl(opus_decoder_.get(), OPUS_RESET_STATE);
  ResetTimestampState();

  BindCallbackIfNeeded(std::move(closure)).Run();
}

OpusAudioDecoder::~OpusAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!opus_decoder_) {
    return;
  }

  opus_multistream_decoder_ctl(opus_decoder_.get(), OPUS_RESET_STATE);
  opus_decoder_.reset();
}

bool OpusAudioDecoder::ConfigureDecoder() {
  if (config_.codec() != AudioCodec::kOpus) {
    DVLOG(1) << "Codec must be kOpus.";
    return false;
  }

  if (!config_.IsValidConfig() ||
      config_.channels() > OPUS_MAX_VORBIS_CHANNELS) {
    DLOG(ERROR) << "Invalid or unsupported audio stream -"
                << " codec: " << config_.codec()
                << " channel count: " << config_.channels()
                << " channel layout: " << config_.channel_layout()
                << " bytes per channel: " << config_.bytes_per_channel()
                << " samples per second: " << config_.samples_per_second();
    return false;
  }

  CHECK(!config_.is_encrypted());

  opus_decoder_.reset();

  const std::optional<OpusExtraData> extra_data =
      ParseOpusExtraData(config_.extra_data(), config_);
  if (!extra_data) {
    DLOG(ERROR) << "Failed to parse opus extra data";
    return false;
  }

  if (config_.codec_delay() < 0) {
    DLOG(ERROR) << "Invalid file. Incorrect value for codec delay: "
                << config_.codec_delay();
    return false;
  }

  if (config_.codec_delay() != extra_data->skip_samples) {
    base::UmaHistogramCounts1000(
        "Media.Audio.Decode.OpusCodecDelayMismatch",
        std::abs(config_.codec_delay() -
                 static_cast<int>(extra_data->skip_samples)));

    // Overwrite the config with the skip samples value from extra data.
    config_.Initialize(config_.codec(), config_.sample_format(),
                       {config_.channel_layout(), config_.channels()},
                       config_.samples_per_second(), config_.extra_data(),
                       config_.encryption_scheme(), config_.seek_preroll(),
                       extra_data->skip_samples);
  }

  std::array<uint8_t, OPUS_MAX_VORBIS_CHANNELS> channel_mapping = {0, 1};

  if (config_.channels() > OPUS_MAX_CHANNELS_WITH_DEFAULT_LAYOUT) {
    CHECK(extra_data->channel_mapping);
    RemapOpusChannelLayout(extra_data->stream_map, config_.channels(),
                           base::span(channel_mapping)
                               .first(static_cast<size_t>(config_.channels())));
  }

  // Init Opus.
  int status = OPUS_INVALID_STATE;
  opus_decoder_.reset(opus_multistream_decoder_create(
      config_.samples_per_second(), config_.channels(), extra_data->num_streams,
      extra_data->num_coupled, channel_mapping.data(), &status));
  if (!opus_decoder_ || status != OPUS_OK) {
    DLOG(ERROR) << "opus_multistream_decoder_create failed status="
                << opus_strerror(status);
    return false;
  }

  status = opus_multistream_decoder_ctl(opus_decoder_.get(),
                                        OPUS_SET_GAIN(extra_data->gain_db));
  if (status != OPUS_OK) {
    DLOG(ERROR) << "Failed to set OPUS header gain; status="
                << opus_strerror(status);
    return false;
  }

  ResetTimestampState();
  return true;
}

void OpusAudioDecoder::ResetTimestampState() {
  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      config_.samples_per_second(), 0, false);
  discard_helper_->Reset(
      config_.should_discard_decoder_delay() ? config_.codec_delay() : 0);
}

bool OpusAudioDecoder::DecodeBuffer(const scoped_refptr<DecoderBuffer>& input) {
  // Allocate a buffer for the output samples.
  scoped_refptr<AudioBuffer> output_buffer = AudioBuffer::CreateBuffer(
      kSampleFormatF32, config_.channel_layout(), config_.channels(),
      config_.samples_per_second(), kMaxOpusOutputPacketSizeSamples, pool_);

  float* float_output_buffer =
      reinterpret_cast<float*>(output_buffer->channel_data()[0]);

  auto input_span = base::span(*input);
  const int frames_decoded = opus_multistream_decode_float(
      opus_decoder_.get(), input_span.data(), input_span.size(),
      float_output_buffer, output_buffer->frame_count(), 0);

  if (frames_decoded < 0) {
    DLOG(ERROR) << "opus_multistream_decode failed for"
                << " timestamp: " << input->timestamp().InMicroseconds()
                << " us, duration: " << input->duration().InMicroseconds()
                << " us, packet size: " << input_span.size() << " bytes with"
                << " status: " << opus_strerror(frames_decoded);
    return false;
  }

  // Trim off any extraneous allocation.
  CHECK_LE(frames_decoded, output_buffer->frame_count());
  const int trim_frames = output_buffer->frame_count() - frames_decoded;
  if (trim_frames > 0) {
    output_buffer->TrimEnd(trim_frames);
  }

  // Handles discards and timestamping. If the data is insufficient, return
  // early and don't output the buffer.
  if (!discard_helper_->ProcessBuffers(
          AudioDiscardHelper::TimeInfo::FromBuffer(*input),
          output_buffer.get())) {
    return true;
  }

  output_cb_.Run(std::move(output_buffer));
  return true;
}

}  // namespace media
