// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/media_codec_audio_decoder.h"

#include <cmath>
#include <memory>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/android/media_codec_bridge_impl.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/media_serializers.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/ac3/ac3_util.h"
#include "media/formats/dts/dts_util.h"
#include "media/media_buildflags.h"

namespace media {

MediaCodecAudioDecoder::MediaCodecAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner),
      state_(STATE_UNINITIALIZED),
      is_passthrough_(false),
      sample_format_(kSampleFormatS16),
      channel_count_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      sample_rate_(0),
      media_crypto_context_(nullptr),
      pool_(base::MakeRefCounted<AudioBufferMemoryPool>()) {
  DVLOG(1) << __func__;
}

MediaCodecAudioDecoder::~MediaCodecAudioDecoder() {
  DVLOG(1) << __func__;

  codec_loop_.reset();

  // Cancel previously registered callback (if any).
  if (media_crypto_context_)
    media_crypto_context_->SetMediaCryptoReadyCB(base::NullCallback());

  ClearInputQueue(DecoderStatus::Codes::kAborted);
}

AudioDecoderType MediaCodecAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kMediaCodec;
}

void MediaCodecAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                        CdmContext* cdm_context,
                                        InitCB init_cb,
                                        const OutputCB& output_cb,
                                        const WaitingCB& waiting_cb) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_NE(state_, STATE_WAITING_FOR_MEDIA_CRYPTO);
  DCHECK(output_cb);
  DCHECK(waiting_cb);

  // Initialization and reinitialization should not be called during pending
  // decode.
  DCHECK(input_queue_.empty());
  ClearInputQueue(DecoderStatus::Codes::kAborted);

  if (state_ == STATE_ERROR) {
    DVLOG(1) << "Decoder is in error state.";
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // We can support only the codecs that MediaCodecBridge can decode.
  // TODO(xhwang): Get this list from MediaCodecBridge or just rely on
  // attempting to create one to determine whether the codec is supported.

  bool platform_codec_supported = false;
  is_passthrough_ = false;
  sample_format_ = config.target_output_sample_format();
  switch (config.codec()) {
    case AudioCodec::kVorbis:
    case AudioCodec::kFLAC:
    case AudioCodec::kAAC:
    case AudioCodec::kOpus:
      platform_codec_supported = true;
      break;
    case AudioCodec::kUnknown:
    case AudioCodec::kMP3:
    case AudioCodec::kPCM:
    case AudioCodec::kAMR_NB:
    case AudioCodec::kAMR_WB:
    case AudioCodec::kPCM_MULAW:
    case AudioCodec::kGSM_MS:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
    case AudioCodec::kPCM_ALAW:
    case AudioCodec::kALAC:
    case AudioCodec::kAC4:
    case AudioCodec::kIAMF:
      platform_codec_supported = false;
      break;
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
    case AudioCodec::kDTS:
    case AudioCodec::kDTSXP2:
    case AudioCodec::kDTSE:
    case AudioCodec::kMpegHAudio:
      is_passthrough_ = sample_format_ != kUnknownSampleFormat;
      // Check if MediaCodec Library supports decoding of the sample format.
      platform_codec_supported = MediaCodecUtil::CanDecode(config.codec());
      break;
  }

  // sample_format_ is stream type for pass-through. Otherwise sample_format_
  // should be set to kSampleFormatS16, which is what Android MediaCodec
  // supports for PCM decode.
  if (!is_passthrough_)
    sample_format_ = kSampleFormatS16;

  const bool is_codec_supported = platform_codec_supported || is_passthrough_;

  if (!is_codec_supported) {
    DVLOG(1) << "Unsupported codec " << GetCodecName(config.codec());
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kUnsupportedCodec);
    return;
  }

  config_ = config;

  // TODO(xhwang): Check whether base::BindPostTaskToCurrentDefault is needed
  // here.
  output_cb_ = base::BindPostTaskToCurrentDefault(output_cb);
  waiting_cb_ = base::BindPostTaskToCurrentDefault(waiting_cb);

  SetInitialConfiguration();

  if (config_.is_encrypted() && !media_crypto_) {
    if (!cdm_context || !cdm_context->GetMediaCryptoContext()) {
      LOG(ERROR) << "The stream is encrypted but there is no CdmContext or "
                    "MediaCryptoContext is not supported";
      SetState(STATE_ERROR);
      base::BindPostTaskToCurrentDefault(std::move(init_cb))
          .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
      return;
    }

    // Postpone initialization after MediaCrypto is available.
    // SetCdm uses init_cb in a method that's already bound to the current loop.
    SetState(STATE_WAITING_FOR_MEDIA_CRYPTO);
    SetCdm(cdm_context, std::move(init_cb));
    return;
  }

  if (!CreateMediaCodecLoop()) {
    base::BindPostTaskToCurrentDefault(std::move(init_cb))
        .Run(DecoderStatus::Codes::kFailed);
    return;
  }

  SetState(STATE_READY);
  base::BindPostTaskToCurrentDefault(std::move(init_cb))
      .Run(DecoderStatus::Codes::kOk);
}

bool MediaCodecAudioDecoder::CreateMediaCodecLoop() {
  DVLOG(1) << __func__ << ": config:" << config_.AsHumanReadableString();

  codec_loop_.reset();
  const base::android::JavaRef<jobject>& media_crypto =
      media_crypto_ ? *media_crypto_ : nullptr;
  std::unique_ptr<MediaCodecBridge> audio_codec_bridge(
      MediaCodecBridgeImpl::CreateAudioDecoder(
          config_, media_crypto,
          base::BindPostTaskToCurrentDefault(
              base::BindRepeating(&MediaCodecAudioDecoder::PumpMediaCodecLoop,
                                  weak_factory_.GetWeakPtr()))));
  if (!audio_codec_bridge) {
    DLOG(ERROR) << __func__ << " failed: cannot create MediaCodecBridge";
    return false;
  }

  codec_loop_ = std::make_unique<MediaCodecLoop>(
      base::android::BuildInfo::GetInstance()->sdk_int(), this,
      std::move(audio_codec_bridge),
      scoped_refptr<base::SingleThreadTaskRunner>());

  return true;
}

void MediaCodecAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                    DecodeCB decode_cb) {
  DecodeCB bound_decode_cb =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  if (!DecoderBuffer::DoSubsamplesMatch(*buffer)) {
    std::move(bound_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp) {
    DVLOG(2) << __func__ << " " << buffer->AsHumanReadableString()
             << ": no timestamp, skipping this buffer";
    std::move(bound_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  // Note that we transition to STATE_ERROR if |codec_loop_| does.
  if (state_ == STATE_ERROR) {
    // We get here if an error happens in DequeueOutput() or Reset().
    DVLOG(2) << __func__ << " " << buffer->AsHumanReadableString()
             << ": Error state, returning decode error for all buffers";
    ClearInputQueue(DecoderStatus::Codes::kFailed);
    std::move(bound_decode_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  DCHECK(codec_loop_);

  DVLOG(3) << __func__ << " " << buffer->AsHumanReadableString();

  DCHECK_EQ(state_, STATE_READY) << " unexpected state " << AsString(state_);

  // AudioDecoder requires that "Only one decode may be in flight at any given
  // time".
  DCHECK(input_queue_.empty());

  input_queue_.push_back(
      std::make_pair(std::move(buffer), std::move(bound_decode_cb)));

  codec_loop_->ExpectWork();
}

void MediaCodecAudioDecoder::Reset(base::OnceClosure closure) {
  DVLOG(2) << __func__;

  ClearInputQueue(DecoderStatus::Codes::kAborted);

  // Flush if we can, otherwise completely recreate and reconfigure the codec.
  bool success = codec_loop_->TryFlush();

  // If the flush failed, then we have to re-create the codec.
  if (!success)
    success = CreateMediaCodecLoop();

  timestamp_helper_->Reset();

  SetState(success ? STATE_READY : STATE_ERROR);

  task_runner_->PostTask(FROM_HERE, std::move(closure));
}

bool MediaCodecAudioDecoder::NeedsBitstreamConversion() const {
  // An AAC stream needs to be converted as ADTS stream.
  DCHECK_NE(config_.codec(), AudioCodec::kUnknown);
  return config_.codec() == AudioCodec::kAAC;
}

void MediaCodecAudioDecoder::SetCdm(CdmContext* cdm_context, InitCB init_cb) {
  DVLOG(1) << __func__;
  DCHECK(cdm_context) << "No CDM provided";
  DCHECK(cdm_context->GetMediaCryptoContext());

  media_crypto_context_ = cdm_context->GetMediaCryptoContext();

  // CdmContext will always post the registered callback back to this thread.
  event_cb_registration_ = cdm_context->RegisterEventCB(base::BindRepeating(
      &MediaCodecAudioDecoder::OnCdmContextEvent, weak_factory_.GetWeakPtr()));

  // The callback will be posted back to this thread via
  // base::BindPostTaskToCurrentDefault.
  media_crypto_context_->SetMediaCryptoReadyCB(
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&MediaCodecAudioDecoder::OnMediaCryptoReady,
                         weak_factory_.GetWeakPtr(), std::move(init_cb))));
}

void MediaCodecAudioDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DVLOG(1) << __func__;

  if (event != CdmContext::Event::kHasAdditionalUsableKey)
    return;

  // We don't register |codec_loop_| directly with the DRM bridge, since it's
  // subject to replacement.
  if (codec_loop_)
    codec_loop_->OnKeyAdded();
}

void MediaCodecAudioDecoder::OnMediaCryptoReady(
    InitCB init_cb,
    JavaObjectPtr media_crypto,
    bool /*requires_secure_video_codec*/) {
  DVLOG(1) << __func__;

  DCHECK(state_ == STATE_WAITING_FOR_MEDIA_CRYPTO);
  DCHECK(media_crypto);

  if (media_crypto->is_null()) {
    LOG(ERROR) << "MediaCrypto is not available, can't play encrypted stream.";
    SetState(STATE_UNINITIALIZED);
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  media_crypto_ = std::move(media_crypto);

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!codec_loop_);

  // After receiving |media_crypto_| we can configure MediaCodec.
  if (!CreateMediaCodecLoop()) {
    SetState(STATE_UNINITIALIZED);
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  SetState(STATE_READY);
  std::move(init_cb).Run(DecoderStatus::Codes::kOk);
}

bool MediaCodecAudioDecoder::IsAnyInputPending() const {
  if (state_ != STATE_READY)
    return false;

  return !input_queue_.empty();
}

MediaCodecLoop::InputData MediaCodecAudioDecoder::ProvideInputData() {
  DVLOG(3) << __func__;

  const DecoderBuffer* decoder_buffer = input_queue_.front().first.get();

  MediaCodecLoop::InputData input_data;
  if (decoder_buffer->end_of_stream()) {
    input_data.is_eos = true;
  } else {
    input_data.memory = static_cast<const uint8_t*>(decoder_buffer->data());
    input_data.length = decoder_buffer->size();
    const DecryptConfig* decrypt_config = decoder_buffer->decrypt_config();
    if (decrypt_config) {
      input_data.key_id = decrypt_config->key_id();
      input_data.iv = decrypt_config->iv();
      input_data.subsamples = decrypt_config->subsamples();
      input_data.encryption_scheme = decrypt_config->encryption_scheme();
      input_data.encryption_pattern = decrypt_config->encryption_pattern();
    }
    input_data.presentation_time = decoder_buffer->timestamp();
  }

  // We do not pop |input_queue_| here.  MediaCodecLoop may refer to data that
  // it owns until OnInputDataQueued is called.

  return input_data;
}

void MediaCodecAudioDecoder::OnInputDataQueued(bool success) {
  // If this is an EOS buffer, then wait to call back until we are notified that
  // it has been processed via OnDecodedEos().  If the EOS was not queued
  // successfully, then we do want to signal error now since there is no queued
  // EOS to process later.
  if (input_queue_.front().first->end_of_stream() && success)
    return;

  std::move(input_queue_.front().second)
      .Run(success ? DecoderStatus::Codes::kOk : DecoderStatus::Codes::kFailed);
  input_queue_.pop_front();
}

void MediaCodecAudioDecoder::ClearInputQueue(DecoderStatus decode_status) {
  DVLOG(2) << __func__;

  for (auto& entry : input_queue_)
    std::move(entry.second).Run(decode_status);

  input_queue_.clear();
}

void MediaCodecAudioDecoder::SetState(State new_state) {
  DVLOG(3) << __func__ << ": " << AsString(state_) << "->"
           << AsString(new_state);
  state_ = new_state;
}

void MediaCodecAudioDecoder::OnCodecLoopError() {
  // If the codec transitions into the error state, then so should we.
  SetState(STATE_ERROR);
  ClearInputQueue(DecoderStatus::Codes::kFailed);
}

bool MediaCodecAudioDecoder::OnDecodedEos(
    const MediaCodecLoop::OutputBuffer& out) {
  DVLOG(2) << __func__ << " pts:" << out.pts;

  // Rarely, we seem to get multiple EOSes or, possibly, unsolicited ones from
  // MediaCodec.  Just transition to the error state.
  // https://crbug.com/818866
  if (!input_queue_.size() || !input_queue_.front().first->end_of_stream()) {
    LOG(WARNING) << "MCAD received unexpected eos";
    return false;
  }

  // If we've transitioned into the error state, then we don't really know what
  // to do.  If we transitioned because of OnCodecError, then all of our
  // buffers have been returned anyway.  Otherwise, it's unclear.  Note that
  // MCL does not call us back after OnCodecError(), since it stops decoding.
  // So, we shouldn't be in that state.  So, just DCHECK here.
  DCHECK_NE(state_, STATE_ERROR);

  std::move(input_queue_.front()).second.Run(DecoderStatus::Codes::kOk);
  input_queue_.pop_front();

  return true;
}

bool MediaCodecAudioDecoder::OnDecodedFrame(
    const MediaCodecLoop::OutputBuffer& out) {
  DVLOG(3) << __func__ << " pts:" << out.pts;

  DCHECK_NE(out.size, 0U);
  DCHECK_NE(out.index, MediaCodecLoop::kInvalidBufferIndex);
  DCHECK(codec_loop_);
  MediaCodecBridge* media_codec = codec_loop_->GetCodec();
  DCHECK(media_codec);

  // For proper |frame_count| calculation we need to use the actual number
  // of channels which can be different from |config_| value.
  DCHECK_GT(channel_count_, 0);

  size_t frame_count = 1;
  scoped_refptr<AudioBuffer> audio_buffer;

  if (is_passthrough_) {
    audio_buffer = AudioBuffer::CreateBitstreamBuffer(
        sample_format_, channel_layout_, channel_count_, sample_rate_,
        frame_count, out.size, pool_);

    MediaCodecResult result = media_codec->CopyFromOutputBuffer(
        out.index, out.offset, audio_buffer->channel_data()[0], out.size);

    if (!result.is_ok()) {
      media_codec->ReleaseOutputBuffer(out.index, false);
      return false;
    }

    if (config_.codec() == AudioCodec::kAC3) {
      frame_count = Ac3Util::ParseTotalAc3SampleCount(
          audio_buffer->channel_data()[0], out.size);
    } else if (config_.codec() == AudioCodec::kEAC3) {
      frame_count = Ac3Util::ParseTotalEac3SampleCount(
          audio_buffer->channel_data()[0], out.size);
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    } else if (config_.codec() == AudioCodec::kDTS) {
      frame_count = media::dts::ParseTotalSampleCount(
          audio_buffer->channel_data()[0], out.size, AudioCodec::kDTS);
      DVLOG(2) << ": DTS Frame Count = " << frame_count;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    } else {
      NOTREACHED_IN_MIGRATION() << "Unsupported passthrough format.";
    }

    // Create AudioOutput buffer based on current parameters.
    audio_buffer = AudioBuffer::CreateBitstreamBuffer(
        sample_format_, channel_layout_, channel_count_, sample_rate_,
        frame_count, out.size, pool_);
  } else {
    // Android MediaCodec can only output 16bit PCM audio.
    const int bytes_per_frame = sizeof(uint16_t) * channel_count_;
    frame_count = out.size / bytes_per_frame;

    // Create AudioOutput buffer based on current parameters.
    audio_buffer = AudioBuffer::CreateBuffer(sample_format_, channel_layout_,
                                             channel_count_, sample_rate_,
                                             frame_count, pool_);
  }

  // Copy data into AudioBuffer.
  CHECK_LE(out.size, audio_buffer->data_size());

  MediaCodecResult result = media_codec->CopyFromOutputBuffer(
      out.index, out.offset, audio_buffer->channel_data()[0], out.size);

  // Release MediaCodec output buffer.
  media_codec->ReleaseOutputBuffer(out.index, false);

  if (!result.is_ok()) {
    return false;
  }

  // Calculate and set buffer timestamp.
  if (!timestamp_helper_->base_timestamp()) {
    // Clamp the base timestamp to zero.
    timestamp_helper_->SetBaseTimestamp(std::max(base::TimeDelta(), out.pts));
  }

  audio_buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  timestamp_helper_->AddFrames(frame_count);

  // Call the |output_cb_|.
  output_cb_.Run(audio_buffer);

  return true;
}

void MediaCodecAudioDecoder::OnWaiting(WaitingReason reason) {
  DVLOG(2) << __func__;
  waiting_cb_.Run(reason);
}

bool MediaCodecAudioDecoder::OnOutputFormatChanged() {
  DVLOG(2) << __func__;
  MediaCodecBridge* media_codec = codec_loop_->GetCodec();

  // Note that if we return false to transition |codec_loop_| to the error
  // state, then we'll also transition to the error state when it notifies us.

  int new_sampling_rate = 0;
  MediaCodecResult result =
      media_codec->GetOutputSamplingRate(&new_sampling_rate);
  if (!result.is_ok()) {
    DLOG(ERROR) << "GetOutputSamplingRate failed, result: "
                << MediaSerialize(result);
    return false;
  }
  if (new_sampling_rate != sample_rate_) {
    DVLOG(1) << __func__ << ": detected sample rate change: " << sample_rate_
             << " -> " << new_sampling_rate;

    sample_rate_ = new_sampling_rate;

    std::optional<base::TimeDelta> base_timestamp;
    if (timestamp_helper_->base_timestamp()) {
      base_timestamp = timestamp_helper_->GetTimestamp();
    }
    timestamp_helper_ = std::make_unique<AudioTimestampHelper>(sample_rate_);
    if (base_timestamp) {
      timestamp_helper_->SetBaseTimestamp(*base_timestamp);
    }
  }

  int new_channel_count = 0;
  result = media_codec->GetOutputChannelCount(&new_channel_count);
  if (!result.is_ok() || !new_channel_count) {
    DLOG(ERROR) << "GetOutputChannelCount failed, result: "
                << MediaSerialize(result);
    return false;
  }

  if (new_channel_count != channel_count_) {
    DVLOG(1) << __func__
             << ": detected channel count change: " << channel_count_ << " -> "
             << new_channel_count;
    channel_count_ = new_channel_count;
    channel_layout_ = GuessChannelLayout(channel_count_);
  }

  return true;
}

void MediaCodecAudioDecoder::SetInitialConfiguration() {
  // Guess the channel count from |config_| in case OnOutputFormatChanged
  // that delivers the true count is not called before the first data arrives.
  // It seems upon certain input errors a codec may substitute silence and
  // not call OnOutputFormatChanged in this case.
  channel_layout_ = config_.channel_layout();
  channel_count_ = ChannelLayoutToChannelCount(channel_layout_);

  sample_rate_ = config_.samples_per_second();
  timestamp_helper_ = std::make_unique<AudioTimestampHelper>(sample_rate_);
}

void MediaCodecAudioDecoder::PumpMediaCodecLoop() {
  codec_loop_->ExpectWork();
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

// static
const char* MediaCodecAudioDecoder::AsString(State state) {
  switch (state) {
    RETURN_STRING(STATE_UNINITIALIZED);
    RETURN_STRING(STATE_WAITING_FOR_MEDIA_CRYPTO);
    RETURN_STRING(STATE_READY);
    RETURN_STRING(STATE_ERROR);
  }
  NOTREACHED_IN_MIGRATION() << "Unknown state " << state;
  return nullptr;
}

#undef RETURN_STRING

}  // namespace media
