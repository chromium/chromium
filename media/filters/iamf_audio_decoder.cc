// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/iamf_audio_decoder.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/sample_format.h"
#include "media/base/timestamp_constants.h"
#include "third_party/iamf_tools/src/iamf/include/iamf_tools/iamf_decoder_factory.h"
#include "third_party/iamf_tools/src/iamf/include/iamf_tools/iamf_tools_api_types.h"

namespace media {

using RequestedMix = iamf_tools::api::RequestedMix;
using OutputLayout = iamf_tools::api::OutputLayout;
using OutputSampleType = iamf_tools::api::OutputSampleType;
using IamfDecoderFactory = iamf_tools::api::IamfDecoderFactory;
using ChannelOrdering = iamf_tools::api::ChannelOrdering;
constexpr SampleFormat kOutputSampleFormat = kSampleFormatS32;

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(IamfMixMode)
enum class IamfMixMode {
  kNoMix = 0,
  kDownmix = 1,
  kUpmix = 2,
  kMaxValue = kUpmix,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:IamfMixMode)

DecoderStatus ToDecoderStatus(const iamf_tools::api::IamfStatus& status) {
  if (status.ok()) {
    return DecoderStatus::Codes::kOk;
  }
  return {DecoderStatus::Codes::kFailed, status.error_message};
}
}  // namespace

IamfAudioDecoder::IamfAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log,
    ExecutionMode mode)
    : task_runner_(std::move(task_runner)),
      media_log_(MediaLog::CloneSafely(media_log)),
      mode_(mode),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  CHECK(media_log_);
  if (mode_ == ExecutionMode::kAsynchronous) {
    CHECK(task_runner_);
  }
}

IamfAudioDecoder::~IamfAudioDecoder() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (decoded_duration_.has_value()) {
    base::UmaHistogramLongTimes("Media.Audio.Iamf.UsageTime",
                                decoded_duration_.value());
  }
}

AudioDecoderType IamfAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kIamf;
}

void IamfAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                  CdmContext* /*cdm_context*/,
                                  InitCB init_cb,
                                  const OutputCB& output_cb,
                                  const WaitingCB& /*waiting_cb*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindCallbackIfNeeded(std::move(init_cb));

  if (config.codec() != AudioCodec::kIAMF) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec)
                 .WithData("codec", config.codec()));
    return;
  }

  bound_init_cb = base::BindOnce(
      [](InitCB cb, DecoderStatus status) {
        base::UmaHistogramEnumeration("Media.Audio.Iamf.InitStatus",
                                      status.code());
        std::move(cb).Run(std::move(status));
      },
      std::move(bound_init_cb));

  if (config.is_encrypted()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (config.extra_data().empty()) {
    std::move(bound_init_cb)
        .Run(DecoderStatus(DecoderStatus::Codes::kUnsupportedConfig,
                           "IamfAudioDecoder requires extra_data descriptors"));
    return;
  }

  const DecoderStatus configure_result = ConfigureDecoder(config);
  if (!configure_result.is_ok()) {
    std::move(bound_init_cb).Run(std::move(configure_result));
    return;
  }

  if (!VerifyStreamParameters()) {
    MEDIA_LOG(ERROR, media_log_) << "Stream parameters verification failed.";
    std::move(bound_init_cb).Run(DecoderStatus::Codes::kUnsupportedConfig);
    return;
  }

  timestamp_helper_ =
      std::make_unique<AudioTimestampHelper>(output_sample_rate_);
  DVLOG(1) << "TimestampHelper initialized with sample rate "
           << output_sample_rate_;

  config_ = config;
  output_cb_ = BindCallbackIfNeeded(output_cb);
  state_ = DecoderState::kNormal;
  base::UmaHistogramCounts100("Media.Audio.Iamf.InputChannelCount",
                              config_.channels());

  IamfMixMode mix_mode;
  if (config_.channels() > output_layout_config_.channels()) {
    mix_mode = IamfMixMode::kDownmix;
  } else if (config_.channels() < output_layout_config_.channels()) {
    mix_mode = IamfMixMode::kUpmix;
  } else {
    mix_mode = IamfMixMode::kNoMix;
  }
  base::UmaHistogramEnumeration("Media.Audio.Iamf.MixMode", mix_mode);

  std::move(bound_init_cb).Run(DecoderStatus::Codes::kOk);
  DVLOG(3) << __func__ << ": successfully initialized IAMF audio decoder...";
}

void IamfAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                              DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(state_, DecoderState::kUninitialized);
  CHECK(decode_cb);
  DecodeCB decode_cb_bound = BindCallbackIfNeeded(std::move(decode_cb));

  switch (state_) {
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

void IamfAudioDecoder::DecodeBuffer(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb_bound) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, DecoderState::kNormal);

  const bool is_eos = buffer->end_of_stream();
  if (!is_eos && buffer->timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without a timestamp.";
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!is_eos && buffer->is_encrypted()) {
    DLOG(ERROR) << "Encrypted buffer not supported";
    state_ = DecoderState::kError;
    std::move(decode_cb_bound)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (!is_eos && !timestamp_helper_->base_timestamp()) {
    DVLOG(1) << "Setting base timestamp to "
             << buffer->timestamp().InMilliseconds() << " ms";
    timestamp_helper_->SetBaseTimestamp(buffer->timestamp());
  }

  const DecoderStatus status = IamfDecode(*buffer);
  if (!status.is_ok()) {
    state_ = DecoderState::kError;
    std::move(decode_cb_bound).Run(std::move(status));
    return;
  }

  if (is_eos) {
    DVLOG(1) << "Decode finished (EOS received).";
    state_ = DecoderState::kDecodeFinished;
  }

  std::move(decode_cb_bound).Run(DecoderStatus::Codes::kOk);
}

void IamfAudioDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (iamf_decoder_) {
    auto result = iamf_decoder_->Reset();
    if (result.ok()) {
      DVLOG(1) << "Reset succeeded.";
      state_ = DecoderState::kNormal;
    } else {
      MEDIA_LOG(ERROR, media_log_) << "Reset failed: " << result.error_message;
      state_ = DecoderState::kError;
    }
  }

  if (timestamp_helper_) {
    timestamp_helper_->Reset();
  }

  if (mode_ == ExecutionMode::kAsynchronous) {
    task_runner_->PostTask(FROM_HERE, std::move(closure));
  } else {
    std::move(closure).Run();
  }
}

DecoderStatus IamfAudioDecoder::IamfDecode(const DecoderBuffer& buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (buffer.end_of_stream()) {
    auto result = iamf_decoder_->SignalEndOfDecoding();
    if (!result.ok()) {
      MEDIA_LOG(ERROR, media_log_)
          << "Failed to signal end of stream to IAMF decoder: "
          << result.error_message;
      return ToDecoderStatus(result);
    }
    return DrainTemporalUnits();
  }

  auto result = iamf_decoder_->Decode(base::span(buffer).data(), buffer.size());

  if (!result.ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Decode failed: " << result.error_message;
    return ToDecoderStatus(result);
  }

  return DrainTemporalUnits();
}

DecoderStatus IamfAudioDecoder::DrainTemporalUnits() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  while (iamf_decoder_->IsTemporalUnitAvailable()) {
    scoped_refptr<AudioBuffer> result = AudioBuffer::CreateBuffer(
        kOutputSampleFormat, output_layout_config_.channel_layout(),
        output_layout_config_.channels(), output_sample_rate_, frame_size_,
        pool_);
    if (!result) {
      return DecoderStatus::Codes::kFailed;
    }

    size_t bytes_written = 0;
    auto status = iamf_decoder_->GetOutputTemporalUnit(
        result->channels()[0].data(), result->data_size(), bytes_written);

    if (!status.ok()) {
      MEDIA_LOG(ERROR, media_log_)
          << "GetOutputTemporalUnit failed: " << status.error_message;
      return ToDecoderStatus(status);
    }

    if (bytes_written == 0) {
      break;
    }

    const size_t bytes_per_frame =
        SampleFormatToBytesPerChannel(result->sample_format()) *
        result->channel_count();
    // We do not expect partial frames to be returned by the IAMF decoder.
    CHECK_EQ(bytes_written % bytes_per_frame, 0u);
    const size_t decoded_frames = bytes_written / bytes_per_frame;
    if (decoded_frames < static_cast<size_t>(result->frame_count())) {
      result->TrimEnd(result->frame_count() - decoded_frames);
    }
    result->set_timestamp(timestamp_helper_->GetTimestamp());

    timestamp_helper_->AddFrames(decoded_frames);
    decoded_duration_ =
        decoded_duration_.value_or(base::TimeDelta()) + result->duration();
    output_cb_.Run(result);
  }

  return DecoderStatus::Codes::kOk;
}

DecoderStatus IamfAudioDecoder::ConfigureDecoder(
    const AudioDecoderConfig& config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(config.IsValidConfig());
  CHECK(!config.is_encrypted());

  ChannelLayoutConfig layout_config;

  if (config.target_output_channel_layout().channel_layout() !=
          CHANNEL_LAYOUT_NONE &&
      config.target_output_channel_layout().channel_layout() !=
          CHANNEL_LAYOUT_UNSUPPORTED) {
    layout_config = config.target_output_channel_layout();
    MEDIA_LOG(DEBUG, media_log_)
        << "Using target HW layouts: "
        << ChannelLayoutToString(layout_config.channel_layout()) << " w/ "
        << layout_config.channels() << " channels.";
  } else {
    layout_config = config.channel_layout_config();
    MEDIA_LOG(DEBUG, media_log_)
        << "Using config layouts: "
        << ChannelLayoutToString(layout_config.channel_layout()) << " w/ "
        << layout_config.channels() << " channels.";
  }

  RequestedMix requested_mix{.output_layout = ConvertMediaLayoutToIamfLayout(
                                 layout_config, media_log_.get())};

  // While this says "Android", this follows the same order as Windows, which is
  // the order that we set our output channels to.
  IamfDecoderFactory::Settings settings{
      .requested_mix = requested_mix,
      .channel_ordering = ChannelOrdering::kOrderingForAndroid};

  iamf_decoder_ = IamfDecoderFactory::CreateFromDescriptors(
      settings, config.extra_data().data(), config.extra_data().size());
  if (!iamf_decoder_) {
    MEDIA_LOG(ERROR, media_log_) << "Decoder creation failed.";
    return DecoderStatus::Codes::kFailedToCreateDecoder;
  }
  return DecoderStatus::Codes::kOk;
}

bool IamfAudioDecoder::VerifyStreamParameters() {
  const auto sample_type = iamf_decoder_->GetOutputSampleType();
  if (sample_type != OutputSampleType::kInt32LittleEndian) {
    MEDIA_LOG(ERROR, media_log_)
        << "IAMF decoder created with unexpected sample type: "
        << static_cast<int>(sample_type);
    return false;
  }

  int channels = 0;
  if (!iamf_decoder_->GetNumberOfOutputChannels(channels).ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get number of output channels.";
    return false;
  }

  if (!iamf_decoder_->GetSampleRate(output_sample_rate_).ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get sample rate.";
    return false;
  }

  if (output_sample_rate_ < static_cast<uint32_t>(limits::kMinSampleRate) ||
      output_sample_rate_ > static_cast<uint32_t>(limits::kMaxSampleRate)) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported sample rate: " << output_sample_rate_;
    return false;
  }

  iamf_tools::api::SelectedMix selected_mix;
  if (!iamf_decoder_->GetOutputMix(selected_mix).ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get output mix.";
    return false;
  }

  constexpr int kMaxIamfOutputLayout =
      static_cast<int>(iamf_tools::api::OutputLayout::kIAMF_Binaural);
  base::UmaHistogramExactLinear("Media.Audio.Iamf.OutputLayout",
                                static_cast<int>(selected_mix.output_layout),
                                kMaxIamfOutputLayout + 1);

  ChannelLayoutConfig layout_config =
      ConvertIamfLayout(selected_mix.output_layout, media_log_.get());
  if (layout_config.channel_layout() == CHANNEL_LAYOUT_UNSUPPORTED) {
    MEDIA_LOG(ERROR, media_log_)
        << "Unsupported IAMF channel layout: "
        << static_cast<int>(selected_mix.output_layout);
    return false;
  }

  if (layout_config.channel_layout() == CHANNEL_LAYOUT_DISCRETE) {
    if (channels <= 0 || channels > limits::kMaxChannels) {
      MEDIA_LOG(ERROR, media_log_)
          << "Invalid discrete channel count: " << channels;
      return false;
    }
    layout_config = ChannelLayoutConfig(CHANNEL_LAYOUT_DISCRETE, channels);
  } else if (channels != layout_config.channels()) {
    MEDIA_LOG(ERROR, media_log_)
        << "Inconsistent channel count " << channels << " for layout "
        << ChannelLayoutToString(layout_config.channel_layout());
    return false;
  }

  output_layout_config_ = layout_config;

  if (!iamf_decoder_->GetFrameSize(frame_size_).ok()) {
    MEDIA_LOG(ERROR, media_log_) << "Failed to get frame size.";
    return false;
  }

  if (frame_size_ == 0 ||
      frame_size_ > static_cast<uint32_t>(limits::kMaxSamplesPerPacket)) {
    MEDIA_LOG(ERROR, media_log_) << "Invalid frame size: " << frame_size_;
    return false;
  }

  return true;
}

ChannelLayoutConfig IamfAudioDecoder::ConvertIamfLayout(
    const OutputLayout& iamf_layout,
    MediaLog* media_log) {
  switch (iamf_layout) {
    case OutputLayout::kIAMF_SoundSystemExtension_0_1_0:
      return ChannelLayoutConfig::Mono();
    case OutputLayout::kItu2051_SoundSystemA_0_2_0:
    case OutputLayout::kIAMF_Binaural:
      return ChannelLayoutConfig::Stereo();
    case OutputLayout::kItu2051_SoundSystemB_0_5_0:
      return ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1>();
    case OutputLayout::kItu2051_SoundSystemI_0_7_0:
      return ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1>();
    case OutputLayout::kItu2051_SoundSystemD_4_5_0:
      return ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_5_1_4>();
    case OutputLayout::kItu2051_SoundSystemJ_4_7_0:
      return ChannelLayoutConfig::FromLayout<CHANNEL_LAYOUT_7_1_4>();
    default:
      if (media_log) {
        MEDIA_LOG(WARNING, media_log)
            << "Unsupported IAMF layout: " << static_cast<int>(iamf_layout);
      } else {
        DLOG(WARNING) << "Unsupported IAMF layout: "
                      << static_cast<int>(iamf_layout);
      }
      return ChannelLayoutConfig::FromLayout(CHANNEL_LAYOUT_UNSUPPORTED);
  }
}

OutputLayout IamfAudioDecoder::ConvertMediaLayoutToIamfLayout(
    const ChannelLayoutConfig& layout_config,
    MediaLog* media_log) {
  const ChannelLayout media_layout = layout_config.channel_layout();
  const int channel_count = layout_config.channels();

  switch (media_layout) {
    case CHANNEL_LAYOUT_MONO:
      return OutputLayout::kIAMF_SoundSystemExtension_0_1_0;
    case CHANNEL_LAYOUT_STEREO:
      return OutputLayout::kItu2051_SoundSystemA_0_2_0;
    case CHANNEL_LAYOUT_5_1:
      return OutputLayout::kItu2051_SoundSystemB_0_5_0;
    case CHANNEL_LAYOUT_7_1:
      return OutputLayout::kItu2051_SoundSystemI_0_7_0;
    case CHANNEL_LAYOUT_5_1_4:
      return OutputLayout::kItu2051_SoundSystemD_4_5_0;
    case CHANNEL_LAYOUT_7_1_4:
      return OutputLayout::kItu2051_SoundSystemJ_4_7_0;
    case CHANNEL_LAYOUT_DISCRETE: {
      if (channel_count == 12) {
        return OutputLayout::kItu2051_SoundSystemJ_4_7_0;
      }
      if (channel_count == 10) {
        return OutputLayout::kItu2051_SoundSystemD_4_5_0;
      }
      [[fallthrough]];
    }
    default:
      if (media_log) {
        MEDIA_LOG(WARNING, media_log)
            << "Unsupported media layout: " << static_cast<int>(media_layout)
            << " w/ " << channel_count << " channels."
            << " Falling back to Stereo.";
      } else {
        DLOG(WARNING) << "Unsupported media layout: "
                      << static_cast<int>(media_layout) << " w/ "
                      << channel_count << " channels."
                      << " Falling back to Stereo.";
      }
      return OutputLayout::kItu2051_SoundSystemA_0_2_0;
  }
}

}  // namespace media
