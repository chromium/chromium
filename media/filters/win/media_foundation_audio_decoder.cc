// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mfapi.h>
#include <mferror.h>
#include <stdint.h>
#include <wmcodecdsp.h>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_sample_types.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/filters/win/media_foundation_audio_decoder.h"
#include "media/filters/win/media_foundation_utils.h"

namespace media {

namespace {

bool PopulateInputSample(IMFSample* sample, const DecoderBuffer& input) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from sample", false);

  DWORD max_length = 0;
  DWORD current_length = 0;
  uint8_t* destination = nullptr;
  hr = buffer->Lock(&destination, &max_length, &current_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to lock buffer", false);

  RETURN_ON_FAILURE(!current_length, "Input length is zero", false);
  RETURN_ON_FAILURE(input.data_size() <= max_length, "Input length is too long",
                    false);
  memcpy(destination, input.data(), input.data_size());

  hr = buffer->SetCurrentLength(input.data_size());
  RETURN_ON_HR_FAILURE(hr, "Failed to set buffer length", false);

  hr = buffer->Unlock();
  RETURN_ON_HR_FAILURE(hr, "Failed to unlock buffer", false);

  RETURN_ON_HR_FAILURE(
      sample->SetSampleTime(input.timestamp().InNanoseconds() / 100),
      "Failed to set input timestamp", false);
  return true;
}

}  // namespace

// static
std::unique_ptr<MediaFoundationAudioDecoder>
MediaFoundationAudioDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return InitializeMediaFoundation()
             ? std::make_unique<MediaFoundationAudioDecoder>(
                   std::move(task_runner))
             : nullptr;
}

MediaFoundationAudioDecoder::MediaFoundationAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  task_runner_ = task_runner;
}

MediaFoundationAudioDecoder::~MediaFoundationAudioDecoder() {}

AudioDecoderType MediaFoundationAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kMediaFoundation;
}

void MediaFoundationAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                             CdmContext* cdm_context,
                                             InitCB init_cb,
                                             const OutputCB& output_cb,
                                             const WaitingCB& waiting_cb) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  if (config.codec() != AudioCodec::kDTS &&
      config.codec() != AudioCodec::kDTSXP2) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec,
                      "MFT Codec does not support DTS content"));
    return;
  }
#else  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
#error "MediaFoundationAudioDecoder requires proprietary codecs and DTS audio"
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

  // FIXME: MFT will need to be signed by a Microsoft Certificate
  //        to support a secured chain of custody on Windows.
  if (config.is_encrypted()) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kUnsupportedEncryptionMode,
                      "MFT Codec does not support encrypted content"));
    return;
  }

  // This shouldn't be possible outside of tests since production code will use
  // the Create() method above.
  if (!InitializeMediaFoundation()) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kMediaFoundationNotAvailable,
                      "Unable to initialize Microsoft Media Foundation"));
    return;
  }

  config_ = config;
  output_cb_ = output_cb;

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  if (config.codec() == AudioCodec::kDTS ||
      config.codec() == AudioCodec::kDTSXP2) {
    std::move(init_cb).Run(
        CreateDecoder()
            ? DecoderStatus(OkStatus())
            : DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec,
                            "MFT Codec does not support DTS content"));
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
}

void MediaFoundationAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                         DecodeCB decode_cb) {
  if (buffer->end_of_stream()) {
    switch (decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0)) {
      case S_OK: {
        OutputStatus rc;
        do {
          rc = PumpOutput(PumpState::kNormal);
        } while (rc == OutputStatus::kSuccess);
        // Return kOk if more input is needed since this is end of stream
        std::move(decode_cb).Run(rc == OutputStatus::kFailed
                                     ? DecoderStatus::Codes::kFailed
                                     : DecoderStatus::Codes::kOk);
        return;
      }
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        std::move(decode_cb).Run(DecoderStatus::Codes::kPlatformDecodeFailure);
        return;
      default:
        std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
        return;
    }
  }

  if (buffer->timestamp() == kNoTimestamp) {
    DVLOG(1) << "Received a buffer without timestamps!";
    std::move(decode_cb).Run(DecoderStatus::Codes::kMissingTimestamp);
    return;
  }

  if (timestamp_helper_->base_timestamp() == kNoTimestamp || has_reset_) {
    has_reset_ = false;
    timestamp_helper_->SetBaseTimestamp(buffer->timestamp());
  }

  auto sample = CreateEmptySampleWithBuffer(buffer->data_size(), 0);
  if (!sample) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!PopulateInputSample(sample.Get(), *buffer)) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  auto hr = decoder_->ProcessInput(0, sample.Get(), 0);
  if (hr != S_OK && hr != MF_E_NOTACCEPTING) {
    DecoderStatus::Codes rc;
    switch (hr) {
      case MF_E_NO_SAMPLE_DURATION:
        rc = DecoderStatus::Codes::kDecoderStreamInErrorState;
        break;
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        rc = DecoderStatus::Codes::kPlatformDecodeFailure;
        break;
      case MF_E_NO_SAMPLE_TIMESTAMP:
        rc = DecoderStatus::Codes::kMissingTimestamp;
        break;
      default:
        rc = DecoderStatus::Codes::kFailed;
        break;
    }
    // Drop remaining samples on error, no need to call PumpOutput
    std::move(decode_cb).Run(rc);
    return;
  }

  OutputStatus rc;
  do {
    rc = PumpOutput(PumpState::kNormal);
    if (rc == OutputStatus::kNeedMoreInput)
      break;
    if (rc == OutputStatus::kFailed) {
      std::move(decode_cb).Run(DecoderStatus::Codes::kFailed);
      return;
    }
  } while (rc == OutputStatus::kSuccess);
  std::move(decode_cb).Run(OkStatus());
}

void MediaFoundationAudioDecoder::Reset(base::OnceClosure reset_cb) {
  has_reset_ = true;
  auto hr = decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  if (hr != S_OK) {
    DLOG(ERROR) << "Reset failed with \"" << PrintHr(hr) << "\"";
  }
  std::move(reset_cb).Run();
}

bool MediaFoundationAudioDecoder::NeedsBitstreamConversion() const {
  // DTS does not require any header/bit stream conversion
  return false;
}

bool MediaFoundationAudioDecoder::CreateDecoder() {
  // Find the decoder factory.
  //
  // Note: It'd be nice if there was an asynchronous DTS MFT (to avoid the need
  // for a codec pump), but alas MFT_ENUM_FLAG_ASYNC_MFT returns no matches :(
  MFT_REGISTER_TYPE_INFO type_info;
  switch (config_.codec()) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    case AudioCodec::kDTSXP2:
      type_info = {MFMediaType_Audio, MFAudioFormat_DTS_UHD};
      break;
    case AudioCodec::kDTS:
      type_info = {MFMediaType_Audio, MFAudioFormat_DTS_RAW};
      break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    default:
      type_info = {MFMediaType_Audio, MFAudioFormat_Base};
  }

  base::win::ScopedCoMem<IMFActivate*> acts;
  UINT32 acts_num = 0;
  ::MFTEnumEx(MFT_CATEGORY_AUDIO_DECODER,
              MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT |
                  MFT_ENUM_FLAG_SORTANDFILTER,
              &type_info, NULL, &acts, &acts_num);

  if (acts_num < 1)
    return false;

  // Create the decoder from the factory.
  // Activate the first MFT object
  RETURN_ON_HR_FAILURE(acts[0]->ActivateObject(IID_PPV_ARGS(&decoder_)),
                       "Failed to activate DTS MFT", false);
  // Release all activated and unactivated object after creating the decoder
  for (UINT32 curr_act = 0; curr_act < acts_num; ++curr_act)
    acts[curr_act]->Release();

  // Configure DTS input.
  Microsoft::WRL::ComPtr<IMFMediaType> input_type;
  RETURN_ON_HR_FAILURE(GetDefaultAudioType(config_, &input_type),
                       "Failed to create IMFMediaType for input data", false);

  RETURN_ON_HR_FAILURE(decoder_->SetInputType(0, input_type.Get(), 0),
                       "Failed to set input type for IMFTransform", false);

  return ConfigureOutput();
}

bool MediaFoundationAudioDecoder::ConfigureOutput() {
  // Configure audio output.
  Microsoft::WRL::ComPtr<IMFMediaType> output_type;
  for (uint32_t i = 0;
       SUCCEEDED(decoder_->GetOutputAvailableType(0, i, &output_type)); ++i) {
    GUID out_type;
    RETURN_ON_HR_FAILURE(output_type->GetGUID(MF_MT_MAJOR_TYPE, &out_type),
                         "Failed to get output main type", false);
    GUID out_subtype;
    RETURN_ON_HR_FAILURE(output_type->GetGUID(MF_MT_SUBTYPE, &out_subtype),
                         "Failed to get output subtype", false);

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    if (config_.codec() == AudioCodec::kDTS ||
        config_.codec() == AudioCodec::kDTSXP2) {
      // Configuration specific to DTS Sound Unbound MFT v1.3.0
      // DTS-CA 5.1 (6 channels)
      constexpr uint32_t DTS_5_1 = 2;
      // DTS:X P2 5.1 (6 channels) or 5.1.4 (downmix to 6 channels)
      constexpr uint32_t DTSX_5_1_DOWNMIX = 3;
      if ((out_subtype == MFAudioFormat_PCM && i == DTS_5_1 &&
           config_.codec() == AudioCodec::kDTS) ||
          (out_subtype == MFAudioFormat_PCM && i == DTSX_5_1_DOWNMIX &&
           config_.codec() == AudioCodec::kDTSXP2)) {
        RETURN_ON_HR_FAILURE(decoder_->SetOutputType(0, output_type.Get(), 0),
                             "Failed to set output type IMFTransform", false);

        MFT_OUTPUT_STREAM_INFO info = {0};
        RETURN_ON_HR_FAILURE(decoder_->GetOutputStreamInfo(0, &info),
                             "Failed to get output stream info", false);

        output_sample_ =
            CreateEmptySampleWithBuffer(info.cbSize, info.cbAlignment);
        RETURN_ON_FAILURE(!!output_sample_, "Failed to create staging sample",
                          false);

        RETURN_ON_HR_FAILURE(
            output_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channel_count_),
            "Failed to get output channel count", false);

        if (channel_count_ != 6) {
          output_type.Reset();
          continue;
        }
      }
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    if (!output_sample_) {
      output_type.Reset();
      continue;
    }

    // Check the optional channel mask argument.
    ChannelConfig mask = 0u;
    auto hr = output_type->GetUINT32(MF_MT_AUDIO_CHANNEL_MASK, &mask);
    if (hr == MF_E_ATTRIBUTENOTFOUND) {
      channel_layout_ = GuessChannelLayout(channel_count_);
    } else {
      RETURN_ON_HR_FAILURE(hr, "Failed to get output channel mask", false);
      channel_layout_ = ChannelConfigToChannelLayout(mask);

      RETURN_ON_FAILURE(static_cast<uint32_t>(ChannelLayoutToChannelCount(
                            channel_layout_)) == channel_count_ ||
                            channel_layout_ == CHANNEL_LAYOUT_DISCRETE,
                        "Channel layout and channel count don't match", false);
    }

    RETURN_ON_HR_FAILURE(
        output_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate_),
        "Failed to get output sample rate", false);

    RETURN_ON_FAILURE(
        channel_count_ > 0 && channel_count_ <= limits::kMaxChannels,
        "Channel count is not supported", false);

    RETURN_ON_FAILURE(sample_rate_ >= limits::kMinSampleRate &&
                          sample_rate_ <= limits::kMaxSampleRate,
                      "Sample rate is not supported", false);

    timestamp_helper_ = std::make_unique<AudioTimestampHelper>(sample_rate_);
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    return true;
  }
  RETURN_ON_HR_FAILURE(decoder_->SetOutputType(0, output_type.Get(), 0),
                       "Failed to set output type IMFTransform", false);
  return false;
}

int GetBytesPerFrame(AudioCodec codec) {
  switch (codec) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    // DTS Sound Unbound MFT v1.3 supports 24-bit PCM output only
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
      return 3;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    default:
      return 4;
  }
}

MediaFoundationAudioDecoder::OutputStatus
MediaFoundationAudioDecoder::PumpOutput(PumpState pump_state) {
  // Unlike video, the audio MFT requires that we provide the output sample
  // instead of allocating it for us.
  MFT_OUTPUT_DATA_BUFFER output_data_buffer = {0};
  output_data_buffer.pSample = output_sample_.Get();

  DWORD status = 0;
  auto hr = decoder_->ProcessOutput(0, 1, &output_data_buffer, &status);
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    DVLOG(3) << __func__ << "More input needed to decode outputs.";
    return OutputStatus::kNeedMoreInput;
  }

  if (hr == MF_E_TRANSFORM_STREAM_CHANGE &&
      pump_state != PumpState::kStreamChange) {
    if (!ConfigureOutput())
      return OutputStatus::kFailed;

    DVLOG(1) << "New config: ch=" << channel_count_ << ", sr=" << sample_rate_
             << " (" << config_.AsHumanReadableString() << ")";
    PumpOutput(PumpState::kStreamChange);
    return OutputStatus::kStreamChange;
  }

  RETURN_ON_HR_FAILURE(hr, "Failed to process output", OutputStatus::kFailed);

  // Unused, but must be released.
  IMFCollection* events = output_data_buffer.pEvents;
  if (events)
    events->Release();

  Microsoft::WRL::ComPtr<IMFMediaBuffer> output_buffer;
  RETURN_ON_HR_FAILURE(
      output_sample_->ConvertToContiguousBuffer(&output_buffer),
      "Failed to map sample into a contiguous output buffer",
      OutputStatus::kFailed);

  DWORD current_length = 0;
  uint8_t* destination = nullptr;
  RETURN_ON_HR_FAILURE(output_buffer->Lock(&destination, NULL, &current_length),
                       "Failed to lock output buffer", OutputStatus::kFailed);

  // Output is always configured to be interleaved float.
  int sample_byte_len = GetBytesPerFrame(config_.codec());
  size_t frames = (current_length / sample_byte_len / channel_count_);
  RETURN_ON_FAILURE(frames > 0u, "Invalid output buffer size",
                    OutputStatus::kFailed);

  if (!pool_)
    pool_ = base::MakeRefCounted<AudioBufferMemoryPool>();

  auto audio_buffer =
      AudioBuffer::CreateBuffer(kSampleFormatF32, channel_layout_,
                                channel_count_, sample_rate_, frames, pool_);
  audio_buffer->set_timestamp(timestamp_helper_->GetTimestamp());

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  // DTS Sound Unbound MFT v1.3.0 outputs 24-bit PCM samples, and will
  // be converted to 32-bit float
  if (config_.codec() == AudioCodec::kDTS ||
      config_.codec() == AudioCodec::kDTSXP2) {
    float* channel_data =
        reinterpret_cast<float*>(audio_buffer->channel_data()[0]);
    int8_t* pcm24 = reinterpret_cast<int8_t*>(destination);
    for (uint64_t i = 0; i < frames; i++) {
      for (uint64_t ch = 0; ch < channel_count_; ch++) {
        int32_t pcmi = (*pcm24++ << 8) & 0xff00;
        pcmi |= (*pcm24++ << 16) & 0xff0000;
        pcmi |= (*pcm24++ << 24) & 0xff000000;
        *channel_data++ = SignedInt32SampleTypeTraits::ToFloat(pcmi);
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

  timestamp_helper_->AddFrames(frames);

  output_buffer->Unlock();

  output_cb_.Run(std::move(audio_buffer));
  return OutputStatus::kSuccess;
}

}  // namespace media
