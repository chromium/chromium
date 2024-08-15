// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/win/media_foundation_audio_decoder.h"

#include <mfapi.h>
#include <mferror.h>
#include <stdint.h>
#include <wmcodecdsp.h>

#include "base/auto_reset.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/audio_sample_types.h"
#include "media/base/limits.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"

namespace media {

namespace {

bool CodecSupportsFloatOutput(AudioCodec codec) {
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  if (codec == AudioCodec::kAC3 || codec == AudioCodec::kEAC3) {
    return true;
  }
#endif
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (codec == AudioCodec::kAAC) {
    return true;
  }
#endif
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
  if (codec == AudioCodec::kAC4) {
    return true;
  }
#endif
  return false;
}

bool CodecSupportsFormat(const AudioDecoderConfig& config,
                         const WAVEFORMATEX& format) {
  if (config.channels() == format.nChannels &&
      config.samples_per_second() == static_cast<int>(format.nSamplesPerSec)) {
    return true;
  }

  // Sometimes HE-AAC configurations may be off by a factor of two, so allow
  // such cases -- they'll reconfigure upon first decoded frame.
  if (config.codec() == AudioCodec::kAAC &&
      2 * config.channels() == format.nChannels &&
      2 * config.samples_per_second() ==
          static_cast<int>(format.nSamplesPerSec)) {
    return true;
  }

  // For AC3/EAC3, we expect channel config changes, no need to compare channels
  // here.
  if ((config.codec() == AudioCodec::kAC3 ||
       config.codec() == AudioCodec::kEAC3) &&
      config.samples_per_second() == static_cast<int>(format.nSamplesPerSec)) {
    return true;
  }

  if (config.codec() == AudioCodec::kAC4) {
    return true;
  }

  return false;
}

std::optional<MFT_REGISTER_TYPE_INFO> GetTypeInfo(
    const AudioDecoderConfig& config) {
  switch (config.codec()) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    case AudioCodec::kDTSXP2:
      return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio, MFAudioFormat_DTS_UHD};
    case AudioCodec::kDTS:
    case AudioCodec::kDTSE:
      return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio, MFAudioFormat_DTS_RAW};
#endif
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    case AudioCodec::kAC3:
      return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio, MFAudioFormat_Dolby_AC3};
    case AudioCodec::kEAC3:
      return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio,
                                    MFAudioFormat_Dolby_DDPlus};
#endif
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case AudioCodec::kAAC:
      if (config.profile() == AudioCodecProfile::kXHE_AAC &&
          base::win::GetVersion() >= base::win::Version::WIN11_22H2) {
        return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio, MFAudioFormat_AAC};
      }
      [[fallthrough]];
#endif
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    case AudioCodec::kAC4:
      return MFT_REGISTER_TYPE_INFO{MFMediaType_Audio, MFAudioFormat_Dolby_AC4};
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    default:
      return std::nullopt;
  }
}

bool PopulateInputSample(IMFSample* sample, const DecoderBuffer& input) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  HRESULT hr = sample->GetBufferByIndex(0, &buffer);
  RETURN_ON_HR_FAILURE(hr, "Failed to get buffer from sample", false);

  DWORD max_length = 0;
  DWORD current_length = 0;
  uint8_t* destination_ptr = nullptr;
  hr = buffer->Lock(&destination_ptr, &max_length, &current_length);
  RETURN_ON_HR_FAILURE(hr, "Failed to lock buffer", false);
  // SAFETY: IMFMediaBuffer::Lock returns a pointer that points to at least
  // `max_length` many bytes.
  // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock
  auto destination = UNSAFE_BUFFERS(base::span(destination_ptr, max_length));

  RETURN_ON_FAILURE(!current_length, "Input length is zero", false);
  RETURN_ON_FAILURE(input.size() <= max_length, "Input length is too long",
                    false);
  destination.copy_prefix_from(input);

  hr = buffer->SetCurrentLength(input.size());
  RETURN_ON_HR_FAILURE(hr, "Failed to set buffer length", false);

  hr = buffer->Unlock();
  RETURN_ON_HR_FAILURE(hr, "Failed to unlock buffer", false);

  RETURN_ON_HR_FAILURE(
      sample->SetSampleTime(input.timestamp().InNanoseconds() / 100),
      "Failed to set input timestamp", false);
  RETURN_ON_HR_FAILURE(
      sample->SetSampleDuration(input.duration().InNanoseconds() / 100),
      "Failed to set input duration", false);
  return true;
}

int GetBytesPerFrame(AudioCodec codec) {
  switch (codec) {
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    // DTS Sound Unbound MFT v1.3 supports 24-bit PCM output only
    case AudioCodec::kDTS:
    case AudioCodec::kDTSE:
    case AudioCodec::kDTSXP2:
      return 3;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    default:
      return 4;
  }
}

}  // namespace

// static
std::unique_ptr<MediaFoundationAudioDecoder>
MediaFoundationAudioDecoder::Create() {
  return InitializeMediaFoundation()
             ? std::make_unique<MediaFoundationAudioDecoder>()
             : nullptr;
}

MediaFoundationAudioDecoder::MediaFoundationAudioDecoder() = default;

MediaFoundationAudioDecoder::~MediaFoundationAudioDecoder() = default;

AudioDecoderType MediaFoundationAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kMediaFoundation;
}

void MediaFoundationAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                             CdmContext* cdm_context,
                                             InitCB init_cb,
                                             const OutputCB& output_cb,
                                             const WaitingCB& waiting_cb) {
  if (config.is_encrypted()) {
    std::move(init_cb).Run(
        DecoderStatus(DecoderStatus::Codes::kUnsupportedEncryptionMode,
                      "MFT Codec does not support encrypted content"));
    return;
  }

  config_ = config;
  output_cb_ = output_cb;

  base::BindPostTaskToCurrentDefault(std::move(init_cb))
      .Run(CreateDecoder()
               ? DecoderStatus(OkStatus())
               : DecoderStatus(DecoderStatus::Codes::kUnsupportedCodec));
}

void MediaFoundationAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                         DecodeCB decode_cb) {
  DecodeCB decode_cb_bound =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (buffer->end_of_stream()) {
    switch (decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0)) {
      case S_OK: {
        OutputStatus rc;
        do {
          rc = PumpOutput(PumpState::kNormal);
        } while (rc == OutputStatus::kSuccess);
        // Return kOk if more input is needed since this is end of stream
        std::move(decode_cb_bound)
            .Run(rc == OutputStatus::kFailed ? DecoderStatus::Codes::kFailed
                                             : DecoderStatus::Codes::kOk);
        return;
      }
      case MF_E_TRANSFORM_TYPE_NOT_SET:
        std::move(decode_cb_bound)
            .Run(DecoderStatus::Codes::kPlatformDecodeFailure);
        return;
      default:
        std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
        return;
    }
  }

  if (buffer->is_encrypted()) {
    DLOG(ERROR) << "Encrypted buffer not supported";
    std::move(decode_cb_bound)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  if (buffer->timestamp() == kNoTimestamp) {
    DLOG(ERROR) << "Received a buffer without timestamps!";
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kMissingTimestamp);
    return;
  }

  if (has_reset_) {
    ResetTimestampState();
    has_reset_ = false;
  }

  auto sample = CreateEmptySampleWithBuffer(buffer->size(), 0);
  if (!sample) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!PopulateInputSample(sample.Get(), *buffer)) {
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
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
    std::move(decode_cb_bound).Run(rc);
    return;
  }

  current_buffer_time_info_ = buffer->time_info();

  bool decoded_frame_this_loop = false;
  OutputStatus rc;
  do {
    rc = PumpOutput(PumpState::kNormal);
    if (rc == OutputStatus::kNeedMoreInput)
      break;
    if (rc == OutputStatus::kFailed) {
      std::move(decode_cb_bound).Run(DecoderStatus::Codes::kFailed);
      return;
    }
    decoded_frame_this_loop = true;
  } while (rc == OutputStatus::kSuccess);

  // Even if we didn't decode a frame this loop, we should still send the packet
  // to the discard helper for caching.
  if (!decoded_frame_this_loop && !buffer->end_of_stream()) {
    const bool result =
        discard_helper_->ProcessBuffers(current_buffer_time_info_, nullptr);
    DCHECK(!result);
  }

  std::move(decode_cb_bound).Run(OkStatus());
}

void MediaFoundationAudioDecoder::Reset(base::OnceClosure reset_cb) {
  has_reset_ = true;
  auto hr = decoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  if (hr != S_OK) {
    DLOG(ERROR) << "Reset failed with \"" << PrintHr(hr) << "\"";
  }
  base::BindPostTaskToCurrentDefault(std::move(reset_cb)).Run();
}

bool MediaFoundationAudioDecoder::NeedsBitstreamConversion() const {
  // DTS does not require any header/bit stream conversion
  return false;
}

bool MediaFoundationAudioDecoder::CreateDecoder() {
  auto type_info = GetTypeInfo(config_);

  // This shouldn't be possible outside of tests since production code will use
  // the MediaFoundationAudioDecoder::Create() which enforces this.
  if (!type_info || !InitializeMediaFoundation()) {
    return false;
  }

  // Find the decoder factory.
  //
  // Note: It'd be nice if there was an asynchronous MFT (to avoid the need
  // for a codec pump), but alas MFT_ENUM_FLAG_ASYNC_MFT returns no matches :(
  base::win::ScopedCoMem<IMFActivate*> acts;
  UINT32 acts_num = 0;
  MFTEnumEx(MFT_CATEGORY_AUDIO_DECODER,
            MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT |
                MFT_ENUM_FLAG_SORTANDFILTER,
            &type_info.value(), nullptr, &acts, &acts_num);
  if (acts_num < 1) {
    return false;
  }

  // Create the decoder from the factory. Activate the first MFT object.
  RETURN_ON_HR_FAILURE(acts[0]->ActivateObject(IID_PPV_ARGS(&decoder_)),
                       "Failed to activate MFT", false);

  // Release all activated and unactivated object after creating the decoder
  for (UINT32 curr_act = 0; curr_act < acts_num; ++curr_act) {
    acts[curr_act]->Release();
  }

  Microsoft::WRL::ComPtr<IMFMediaType> input_type;
  auto hr = E_NOTIMPL;
  if (config_.codec() == AudioCodec::kAAC) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    hr = GetAacAudioType(config_, &input_type);
#endif
  } else {
    hr = GetDefaultAudioType(config_, &input_type);
  }

  RETURN_ON_HR_FAILURE(hr, "Failed to create IMFMediaType for input data",
                       false);
  RETURN_ON_HR_FAILURE(decoder_->SetInputType(0, input_type.Get(), 0),
                       "Failed to set input type for IMFTransform", false);

  return ConfigureOutput();
}

bool MediaFoundationAudioDecoder::ConfigureOutput() {
  // Reset sample staging buffer before configure output, in case stream
  // configuration changed.
  output_sample_.Reset();
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
    // Configuration specific to DTS Sound Unbound MFT v1.3.0
    // DTS-CA 5.1 (6 channels)
    constexpr uint32_t DTS_5_1 = 2;
    // DTS:X P2 5.1 (6 channels) or 5.1.4 (downmix to 6 channels)
    constexpr uint32_t DTSX_5_1_DOWNMIX = 3;

    if ((out_subtype == MFAudioFormat_PCM) &&
        ((config_.codec() == AudioCodec::kDTS && i == DTS_5_1) ||
         (config_.codec() == AudioCodec::kDTSE && i == DTS_5_1) ||
         (config_.codec() == AudioCodec::kDTSXP2 && i == DTSX_5_1_DOWNMIX))) {
      RETURN_ON_HR_FAILURE(decoder_->SetOutputType(0, output_type.Get(), 0),
                           "Failed to set output type IMFTransform", false);

      RETURN_ON_HR_FAILURE(
          output_type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channel_count_),
          "Failed to get output channel count", false);

      MFT_OUTPUT_STREAM_INFO info = {0};
      RETURN_ON_HR_FAILURE(decoder_->GetOutputStreamInfo(0, &info),
                           "Failed to get output stream info", false);

      if (channel_count_ == 6) {
        output_sample_ =
            CreateEmptySampleWithBuffer(info.cbSize, info.cbAlignment);
        RETURN_ON_FAILURE(!!output_sample_, "Failed to create staging sample",
                          false);
      }
    }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

    if (CodecSupportsFloatOutput(config_.codec()) &&
        out_subtype == MFAudioFormat_Float) {
      base::win::ScopedCoMem<WAVEFORMATEX> wave_format;
      UINT32 wave_format_size;
      RETURN_ON_HR_FAILURE(
          MFCreateWaveFormatExFromMFMediaType(output_type.Get(), &wave_format,
                                              &wave_format_size),
          "Failed to get waveformat for media type", false);
      if (CodecSupportsFormat(config_, *wave_format)) {
        RETURN_ON_HR_FAILURE(decoder_->SetOutputType(0, output_type.Get(), 0),
                             "Failed to set output type IMFTransform", false);

        MFT_OUTPUT_STREAM_INFO info = {0};
        RETURN_ON_HR_FAILURE(decoder_->GetOutputStreamInfo(0, &info),
                             "Failed to get output stream info", false);

        output_sample_ =
            CreateEmptySampleWithBuffer(info.cbSize, info.cbAlignment);
        RETURN_ON_FAILURE(!!output_sample_, "Failed to create staging sample",
                          false);

        channel_count_ = wave_format->nChannels;
      }
    }

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

    const auto current_sample_rate = sample_rate_;
    RETURN_ON_HR_FAILURE(
        output_type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sample_rate_),
        "Failed to get output sample rate", false);

    RETURN_ON_FAILURE(
        channel_count_ > 0 && channel_count_ <= limits::kMaxChannels,
        "Channel count is not supported", false);

    RETURN_ON_FAILURE(sample_rate_ >= limits::kMinSampleRate &&
                          sample_rate_ <= limits::kMaxSampleRate,
                      "Sample rate is not supported", false);

    if (current_sample_rate != sample_rate_) {
      ResetTimestampState();
    }
    decoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
    return true;
  }

  return false;
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
    if (!ConfigureOutput()) {
      return OutputStatus::kFailed;
    }

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
  uint8_t* destination_ptr = nullptr;
  RETURN_ON_HR_FAILURE(
      output_buffer->Lock(&destination_ptr, NULL, &current_length),
      "Failed to lock output buffer", OutputStatus::kFailed);
  // SAFETY: IMFMediaBuffer::Lock returns a pointer that points to at least
  // `current_length` many bytes (and up to a larger max, which we discard).
  // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock
  auto destination =
      UNSAFE_BUFFERS(base::span(destination_ptr, current_length));

  // Output is always configured to be interleaved float.
  int sample_byte_len = GetBytesPerFrame(config_.codec());
  size_t frames = (current_length / sample_byte_len / channel_count_);
  RETURN_ON_FAILURE(frames > 0u, "Invalid output buffer size",
                    OutputStatus::kFailed);

  if (!pool_)
    pool_ = base::MakeRefCounted<AudioBufferMemoryPool>();

  scoped_refptr<AudioBuffer> audio_buffer;

#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
  // DTS Sound Unbound MFT v1.3.0 outputs 24-bit PCM samples, and will
  // be converted to 32-bit float
  if (config_.codec() == AudioCodec::kDTS ||
      config_.codec() == AudioCodec::kDTSE ||
      config_.codec() == AudioCodec::kDTSXP2) {
    audio_buffer =
        AudioBuffer::CreateBuffer(kSampleFormatF32, channel_layout_,
                                  channel_count_, sample_rate_, frames, pool_);
    auto channel_data = base::SpanWriter<uint8_t>(
        // TODO(crbug.com/40284755): channel_data() should be an array of spans,
        // not unbounded pointers. This span is constructed unsoundly.
        UNSAFE_TODO(base::span(audio_buffer->channel_data()[0u],
                               frames * channel_count_ * 4u)));
    for (uint64_t i = 0; i < frames; i++) {
      for (uint64_t ch = 0; ch < channel_count_; ch++) {
        auto a = static_cast<int8_t>(destination[0u]);
        auto b = static_cast<int8_t>(destination[1u]);
        auto c = static_cast<int8_t>(destination[2u]);
        int32_t pcmi = (int32_t{a} << 8) & 0xff00;
        pcmi |= (int32_t{b} << 16) & 0xff0000;
        pcmi |= (int32_t{c} << 24) & 0xff000000;
        destination = destination.subspan(3u);
        CHECK(channel_data.Write(base::byte_span_from_ref(
            SignedInt32SampleTypeTraits::ToFloat(pcmi))));
      }
    }
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)

  if (CodecSupportsFloatOutput(config_.codec())) {
    audio_buffer = AudioBuffer::CopyFrom(
        kSampleFormatF32, channel_layout_, channel_count_, sample_rate_, frames,
        // Sample format `kSampleFormatF32` is not planar, so it only reads from
        // the first pointer in the data array. Thus we give it a pointer to the
        // `destination_ptr` and it won't go past it.
        &destination_ptr, base::TimeDelta(), pool_);
  }

  RETURN_ON_FAILURE(!!audio_buffer, "Failed to create output buffer",
                    OutputStatus::kFailed);

  // Important to reset length to 0 since we reuse a same output buffer
  output_buffer->SetCurrentLength(0);
  output_buffer->Unlock();

  if (discard_helper_->ProcessBuffers(current_buffer_time_info_,
                                      audio_buffer.get())) {
    base::BindPostTaskToCurrentDefault(output_cb_).Run(std::move(audio_buffer));
  }

  return OutputStatus::kSuccess;
}

void MediaFoundationAudioDecoder::ResetTimestampState() {
  discard_helper_ =
      std::make_unique<AudioDiscardHelper>(sample_rate_, config_.codec_delay(),
                                           /*delayed_discard=*/true);
  discard_helper_->Reset(config_.codec_delay());
}

}  // namespace media
