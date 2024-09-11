// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/android/ndk_audio_encoder.h"

#include <aaudio/AAudio.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaError.h>
#include <media/NdkMediaFormat.h>

#include <memory>
#include <optional>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_encoder.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/converting_audio_fifo.h"
#include "media/base/encoder_status.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/timestamp_constants.h"
#include "media/gpu/android/ndk_media_codec_wrapper.h"

#pragma clang attribute push DEFAULT_REQUIRES_ANDROID_API( \
    NDK_MEDIA_CODEC_MIN_API)

namespace media {

struct AMediaFormatDeleter {
  inline void operator()(AMediaFormat* ptr) const {
    if (ptr) {
      AMediaFormat_delete(ptr);
    }
  }
};

// AAC uses a frame size of 1024 samples.
constexpr int kAacFramesPerBuffer = 1024;

// Chosen since this offers high quality audio, while still saving some space.
// Apps might set a lower value for voice-only RTC applications, and a higher
// value for encoding music.
constexpr int kDefaultAacBitrate = 192000;

using MediaFormatPtr = std::unique_ptr<AMediaFormat, AMediaFormatDeleter>;

AudioEncoder::AacOutputFormat GetOutputFormat(
    const AudioEncoder::Options options) {
  return options.aac.value_or(AudioEncoder::AacOptions()).format;
}

MediaFormatPtr CreateAudioParams(const AudioEncoder::Options& options,
                                 std::string_view mime_type) {
  MediaFormatPtr result(AMediaFormat_new());

  AMediaFormat_setString(result.get(), AMEDIAFORMAT_KEY_MIME, mime_type.data());
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                        options.channels);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_SAMPLE_RATE,
                        options.sample_rate);

  // AMediaCodec uses signed 16 bits input by default.
  const int input_size =
      sizeof(int16_t) * kAacFramesPerBuffer * options.channels;

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_MAX_INPUT_SIZE,
                        input_size);

  // TODO(crbug.com/40259205) Consider adding HE-AAC profile support.

  if (options.bitrate_mode) {
    constexpr int32_t BITRATE_MODE_VBR = 1;
    constexpr int32_t BITRATE_MODE_CBR = 2;
    switch (*options.bitrate_mode) {
      case media::AudioEncoder::BitrateMode::kConstant:
        AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BITRATE_MODE,
                              BITRATE_MODE_CBR);
        break;
      case media::AudioEncoder::BitrateMode::kVariable:
        AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BITRATE_MODE,
                              BITRATE_MODE_VBR);
        break;
    }
  }

  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_BIT_RATE,
                        options.bitrate.value_or(kDefaultAacBitrate));

  auto format = GetOutputFormat(options);
  AMediaFormat_setInt32(result.get(), AMEDIAFORMAT_KEY_IS_ADTS,
                        format == AudioEncoder::AacOutputFormat::ADTS ? 1 : 0);

  return result;
}

NdkAudioEncoder::NdkAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> runner)
    : task_runner_(std::move(runner)) {}

NdkAudioEncoder::~NdkAudioEncoder() {
  ClearMediaCodec();
}

bool NdkAudioEncoder::CreateAndStartMediaCodec() {
  auto mime_type =
      MediaCodecUtil::CodecToAndroidMimeType(options_.codec, kSampleFormatS16);

  media_codec_ =
      NdkMediaCodecWrapper::CreateByMimeType(mime_type, this, task_runner_);

  if (!media_codec_) {
    LogError({EncoderStatus::Codes::kEncoderInitializationError,
              "Could not create AMediaCodec"});
    return false;
  }

  auto aac_format = CreateAudioParams(options_, mime_type);

  media_status_t status =
      AMediaCodec_configure(media_codec_->codec(), aac_format.get(), nullptr,
                            nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);

  if (status != AMEDIA_OK) {
    LogError({EncoderStatus::Codes::kEncoderInitializationError,
              base::StringPrintf("Could not create AMediaCodec. Status: %d",
                                 status)});
    return false;
  }

  status = media_codec_->Start();

  if (status != AMEDIA_OK) {
    LogError({EncoderStatus::Codes::kEncoderInitializationError,
              base::StringPrintf("Could not start AMediaCodec. Status: %d",
                                 status)});
    return false;
  }

  return true;
}

void NdkAudioEncoder::Initialize(const Options& options,
                                 OutputCB output_callback,
                                 EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));

  // Check for `fifo_` instead of `media_codec_`, as `media_codec_` is reset
  // during a flush.
  if (fifo_) {
    LogAndReportError({EncoderStatus::Codes::kEncoderInitializeTwice,
                       "Encoder initialized twice"},
                      std::move(done_cb));
    return;
  }

  if (options.codec != AudioCodec::kAAC) {
    LogAndReportError({EncoderStatus::Codes::kEncoderInitializationError,
                       "NdkAudioEncoder only supports AAC"},
                      std::move(done_cb));
    return;
  }

  options_ = options;

  if (!CreateAndStartMediaCodec()) {
    ReportPendingError(std::move(done_cb));
    return;
  }

  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_callback));

  output_params_.Reset(
      AudioParameters::Format::AUDIO_PCM_LINEAR,
      ChannelLayoutConfig(GuessChannelLayout(options_.channels),
                          options_.channels),
      options_.sample_rate, kAacFramesPerBuffer);

  // `fifo_` will upmix/downmix and repacketize inputs to make sure there are
  // the correct number of channels and samples per buffer, without resampling.
  fifo_ = std::make_unique<ConvertingAudioFifo>(output_params_, output_params_);

  input_timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(options_.sample_rate);

  output_timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(options_.sample_rate);

  ReportOk(std::move(done_cb));
}

void NdkAudioEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                             base::TimeTicks capture_time,
                             EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));

  if (error_occurred_) {
    ReportPendingError(std::move(done_cb));
    return;
  }

  if (flush_state_ == FlushState::kNeedsMediaCodec) {
    CHECK(!media_codec_);

    if (!CreateAndStartMediaCodec()) {
      ReportPendingError(std::move(done_cb));
      return;
    }

    flush_state_ = FlushState::kNone;
  }

  if (!media_codec_) {
    LogAndReportError(EncoderStatus::Codes::kEncoderInitializeNeverCompleted,
                      std::move(done_cb));
    return;
  }

  if (flush_state_ != FlushState::kNone) {
    CHECK(pending_flush_cb_);
    LogAndReportError({EncoderStatus::Codes::kEncoderFailedFlush,
                       "Received Encode() before Flush() completed."},
                      std::move(pending_flush_cb_));

    ReportPendingError(std::move(done_cb));
    return;
  }

  if (!input_timestamp_tracker_->base_timestamp()) {
    input_timestamp_tracker_->SetBaseTimestamp(capture_time -
                                               base::TimeTicks());
    output_timestamp_tracker_->SetBaseTimestamp(capture_time -
                                                base::TimeTicks());
  }

  fifo_->Push(std::move(audio_bus));

  FeedAllInputs();

  if (error_occurred_) {
    ReportPendingError(std::move(done_cb));
  } else {
    ReportOk(std::move(done_cb));
  }
}

void NdkAudioEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));

  if (error_occurred_) {
    ReportPendingError(std::move(done_cb));
    return;
  }

  // We should have been initialized already.
  if (!fifo_) {
    LogAndReportError({EncoderStatus::Codes::kEncoderInitializeNeverCompleted,
                       "Cannot flush uninitialized encoder."},
                      std::move(done_cb));
    return;
  }

  if (flush_state_ != FlushState::kNone) {
    LogAndReportError({EncoderStatus::Codes::kEncoderIllegalState,
                       "Cannot start new Flush() before first one completes."},
                      std::move(done_cb));
    return;
  }

  // Nothing to flush if we never fed input to the encoder.
  if (!input_timestamp_tracker_->base_timestamp()) {
    ReportOk(std::move(done_cb));
    return;
  }

  CHECK(!pending_flush_cb_);
  pending_flush_cb_ = std::move(done_cb);

  flush_state_ = FlushState::kFlushingInputs;

  fifo_->Flush();

  FeedAllInputs();
}

void NdkAudioEncoder::FeedAllInputs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  while (InputReady()) {
    FeedInput(fifo_->PeekOutput());
    fifo_->PopOutput();
  }

  if (error_occurred_ && pending_flush_cb_) {
    flush_state_ = FlushState::kNone;
    ReportPendingError(std::move(pending_flush_cb_));
    return;
  }

  // When we have fed all inputs, send an EOS to `media_codec_`.
  if (flush_state_ == FlushState::kFlushingInputs) {
    MaybeFeedEos();
  }
}

void NdkAudioEncoder::MaybeFeedEos() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(flush_state_, FlushState::kFlushingInputs);
  CHECK(!error_occurred_);
  CHECK(pending_flush_cb_);

  // Don't send EOS until all inputs have been fed.
  if (fifo_->HasOutput()) {
    return;
  }

  // We don't have a buffer to send an EOS yet.
  if (!media_codec_->HasInput()) {
    return;
  }

  FeedEos();
}

void NdkAudioEncoder::FeedEos() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(flush_state_, FlushState::kFlushingInputs);

  size_t capacity = 0;
  const size_t buffer_idx = media_codec_->TakeInput();

  uint8_t* buffer_ptr =
      AMediaCodec_getInputBuffer(media_codec_->codec(), buffer_idx, &capacity);

  if (!buffer_ptr) {
    LogAndReportError({EncoderStatus::Codes::kEncoderFailedFlush,
                       "Unable to get input buffer during flush"},
                      std::move(pending_flush_cb_));
    return;
  }

  const auto timestamp_us =
      input_timestamp_tracker_->GetTimestamp().InMicroseconds();

  media_status_t status = AMediaCodec_queueInputBuffer(
      media_codec_->codec(), buffer_idx, /*offset=*/0, /*size=*/0, timestamp_us,
      AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM);

  flush_state_ = FlushState::kPendingEOS;

  if (status != AMEDIA_OK) {
    LogAndReportError(
        {EncoderStatus::Codes::kEncoderFailedFlush,
         base::StringPrintf("Error queueing EOS input buffer: status=%d",
                            status)},
        std::move(pending_flush_cb_));
  }
}

bool NdkAudioEncoder::InputReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !error_occurred_ && media_codec_->HasInput() && fifo_->HasOutput();
}

void NdkAudioEncoder::FeedInput(const AudioBus* audio_bus) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(InputReady());
  CHECK(!error_occurred_);

  const size_t buffer_idx = media_codec_->TakeInput();

  size_t capacity = 0;
  uint8_t* buffer_ptr =
      AMediaCodec_getInputBuffer(media_codec_->codec(), buffer_idx, &capacity);

  if (!buffer_ptr) {
    LogError({EncoderStatus::Codes::kEncoderFailedEncode,
              "Unable to get input buffer"});
    return;
  }

  const size_t bytes_per_frame =
      audio_bus->channels() * SampleFormatToBytesPerChannel(kSampleFormatS16);
  const size_t total_bytes = bytes_per_frame * audio_bus->frames();

  if (capacity < total_bytes) {
    LogError({EncoderStatus::Codes::kEncoderFailedEncode,
              base::StringPrintf(
                  "Input capacity too small: needed=%zu, capacity=%zu",
                  total_bytes, capacity)});
    return;
  }

  // MediaCodec uses signed 16bit PCM encoding by default.
  // Configuring the encoder to use float PCM did not work in tests.
  audio_bus->ToInterleaved<SignedInt16SampleTypeTraits>(
      audio_bus->frames(), reinterpret_cast<int16_t*>(buffer_ptr));

  CHECK_EQ(audio_bus->frames(), kAacFramesPerBuffer);
  const auto timestamp_us =
      input_timestamp_tracker_->GetTimestamp().InMicroseconds();
  input_timestamp_tracker_->AddFrames(audio_bus->frames());

  media_status_t status =
      AMediaCodec_queueInputBuffer(media_codec_->codec(), buffer_idx,
                                   /*offset=*/0, total_bytes, timestamp_us,
                                   /*flags=*/0);

  if (status != AMEDIA_OK) {
    LogError(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Error queueing input buffer: status=%d", status)});
  }
}

void NdkAudioEncoder::CompleteFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(flush_state_, FlushState::kPendingEOS);

  input_timestamp_tracker_->Reset();
  output_timestamp_tracker_->Reset();

  ClearMediaCodec();
  flush_state_ = FlushState::kNeedsMediaCodec;

  ReportOk(std::move(pending_flush_cb_));
}

void NdkAudioEncoder::ClearMediaCodec() {
  if (!media_codec_) {
    return;
  }

  media_codec_->Stop();
  media_codec_.reset();
}

bool NdkAudioEncoder::DrainConfig() {
  CHECK(media_codec_->HasOutput());

  NdkMediaCodecWrapper::OutputInfo output_buffer = media_codec_->PeekOutput();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;

  // Check whether the first buffer in the queue contains config data.
  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG) == 0) {
    return false;
  }

  // We already have the info we need from `output_buffer`
  std::ignore = media_codec_->TakeOutput();

  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_->codec(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    LogError({EncoderStatus::Codes::kEncoderFailedEncode,
              "Can't obtain config output buffer from media codec"});
    return false;
  }

  const size_t mc_buffer_size = base::checked_cast<size_t>(mc_buffer_info.size);

  if (mc_buffer_info.offset + mc_buffer_size > capacity) {
    LogError(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Invalid config output buffer layout."
                            "offset: %d size: %zu capacity: %zu",
                            mc_buffer_info.offset, mc_buffer_size, capacity)});
    return false;
  }

  const uint8_t* data_start = buf_data + mc_buffer_info.offset;

  if (GetOutputFormat(options_) == AudioEncoder::AacOutputFormat::ADTS) {
    NullMediaLog null_log;
    if (!aac_config_parser_.Parse(base::make_span(data_start, mc_buffer_size),
                                  &null_log)) {
      LogError({EncoderStatus::Codes::kInvalidOutputBuffer,
                "Could not parse output config"});
      return false;
    }
  } else {
    // Output format is AudioEncoder::AacOutputFormat::AAC
    codec_desc_.assign(data_start, data_start + mc_buffer_size);
  }

  AMediaCodec_releaseOutputBuffer(media_codec_->codec(),
                                  output_buffer.buffer_index, false);

  return true;
}

void NdkAudioEncoder::DrainOutput() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error_occurred_) {
    return;
  }

  if (!media_codec_->HasOutput()) {
    return;
  }

  if (DrainConfig()) {
    return;
  }

  NdkMediaCodecWrapper::OutputInfo output_buffer = media_codec_->TakeOutput();
  AMediaCodecBufferInfo& mc_buffer_info = output_buffer.info;

  // The current output buffer should be data, and not a config.
  CHECK_EQ(mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG, 0u);

  if ((mc_buffer_info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) != 0) {
    CompleteFlush();
    return;
  }

  size_t capacity = 0;
  uint8_t* buf_data = AMediaCodec_getOutputBuffer(
      media_codec_->codec(), output_buffer.buffer_index, &capacity);

  if (!buf_data) {
    LogError({EncoderStatus::Codes::kEncoderFailedEncode,
              "Unable to get output buffer"});
    return;
  }

  const size_t mc_buffer_size = base::checked_cast<size_t>(mc_buffer_info.size);
  const int32_t mc_buffer_offset = mc_buffer_info.offset;
  if (mc_buffer_size + mc_buffer_offset > capacity) {
    LogError(
        {EncoderStatus::Codes::kEncoderFailedEncode,
         base::StringPrintf("Invalid output buffer layout."
                            "offset: %d size: %zu capacity: %zu",
                            mc_buffer_info.offset, mc_buffer_size, capacity)});
    return;
  }

  auto output_format = GetOutputFormat(options_);


  auto mc_data = base::make_span(buf_data + mc_buffer_offset, mc_buffer_size);
  base::HeapArray<uint8_t> output_data;

  if (output_format == AudioEncoder::AacOutputFormat::ADTS) {
    int adts_header_size = 0;
    output_data =
        aac_config_parser_.CreateAdtsFromEsds(mc_data, &adts_header_size);
    if (output_data.empty()) {
      AMediaCodec_releaseOutputBuffer(media_codec_->codec(),
                                      output_buffer.buffer_index, false);
      LogError({EncoderStatus::Codes::kFormatConversionError,
                "Unable to convert to ADTS"});
      return;
    }

  } else {
    output_data = base::HeapArray<uint8_t>::CopiedFrom(mc_data);
  }

  AMediaCodec_releaseOutputBuffer(media_codec_->codec(),
                                  output_buffer.buffer_index, false);

  const auto timestamp =
      output_timestamp_tracker_->GetTimestamp() + base::TimeTicks();
  output_timestamp_tracker_->AddFrames(kAacFramesPerBuffer);

  std::optional<CodecDescription> desc;
  if (!codec_desc_.empty()) {
    desc = codec_desc_;
    codec_desc_.clear();
  }

  output_cb_.Run(
      EncodedAudioBuffer(
          output_params_, std::move(output_data), timestamp,
          output_timestamp_tracker_->GetFrameDuration(kAacFramesPerBuffer)),
      desc);
}

void NdkAudioEncoder::OnInputAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FeedAllInputs();
}

void NdkAudioEncoder::OnOutputAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DrainOutput();
}

void NdkAudioEncoder::OnError(media_status_t error) {
  LogError({EncoderStatus::Codes::kEncoderFailedEncode,
            base::StringPrintf("MediaCodec async error:%d", error)});
}

void NdkAudioEncoder::LogAndReportError(EncoderStatus status,
                                        EncoderStatusCB done_cb) {
  LogError(status);
  ReportPendingError(std::move(done_cb));
}

void NdkAudioEncoder::LogError(EncoderStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!status.is_ok());
  LOG(ERROR) << "ReportError(): code=" << static_cast<int>(status.code())
             << ", message=" << status.message();
  if (!error_occurred_) {
    error_occurred_ = true;
    pending_error_status_ = status;
  }
}

void NdkAudioEncoder::ReportPendingError(EncoderStatusCB done_cb) {
  CHECK(error_occurred_);

  // Already reported error.
  if (!pending_error_status_) {
    std::move(done_cb).Run({EncoderStatus::Codes::kEncoderIllegalState,
                            "Encoder already reported error"});
    return;
  }

  std::move(done_cb).Run(*pending_error_status_);
  pending_error_status_ = std::nullopt;
}

void NdkAudioEncoder::ReportOk(EncoderStatusCB done_cb) {
  CHECK(!error_occurred_);
  CHECK(!pending_error_status_);

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

}  // namespace media
#pragma clang attribute pop
