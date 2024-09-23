// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/mac/audio_toolbox_audio_decoder.h"

#include <optional>

#include "base/apple/osstatus_logging.h"
#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/limits.h"
#include "media/base/mac/channel_layout_util_mac.h"
#include "media/base/media_log.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp4/es_descriptor.h"
#include "media/media_buildflags.h"

namespace media {

namespace {

bool CanUseAudioToolbox(const AudioDecoderConfig& config) {
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
  // AC3 is available on macOS 10.2 or later, and E-AC3 is available on
  // macOS 10.11 or later, so this is always true.
  if (config.codec() == AudioCodec::kEAC3 ||
      config.codec() == AudioCodec::kAC3) {
    return true;
  }
#endif
  // We use AudioToolbox for decoding xHE-AAC content and that's available on
  // macOS 10.15 or higher.
  return config.profile() == AudioCodecProfile::kXHE_AAC;
}

struct InputData {
  raw_ptr<DecoderBuffer> buffer = nullptr;
  AudioStreamPacketDescription packet = {};
};

// Special error code we use to differentiate real errors from end of buffer.
constexpr OSStatus kNoMoreDataError = -12345;

// Callback used to provide input data to the AudioConverter.
OSStatus ProvideInputCallback(AudioConverterRef decoder,
                              UInt32* num_packets,
                              AudioBufferList* buffer_list,
                              AudioStreamPacketDescription** packets,
                              void* user_data) {
  auto* input_data = reinterpret_cast<InputData*>(user_data);
  if (!input_data->buffer || input_data->buffer->end_of_stream()) {
    *num_packets = 0;
    return kNoMoreDataError;
  }

  *num_packets = buffer_list->mNumberBuffers = 1;
  buffer_list->mBuffers[0].mNumberChannels = 0;
  buffer_list->mBuffers[0].mDataByteSize = input_data->buffer->size();

  // No const version of this API unfortunately, so we need const_cast().
  buffer_list->mBuffers[0].mData =
      const_cast<uint8_t*>(input_data->buffer->data());

  if (packets)
    *packets = &input_data->packet;

  // This ensures that if this callback is called again, we'll exit via the
  // kNoMoreDataError path above.
  input_data->buffer = nullptr;
  return noErr;
}

}  // namespace

// static
AudioConverterRef
AudioToolboxAudioDecoder::ScopedAudioConverterRefTraits::Retain(
    AudioConverterRef converter) {
  NOTREACHED_IN_MIGRATION() << "Only compatible with ASSUME policy";
  return converter;
}

// static
void AudioToolboxAudioDecoder::ScopedAudioConverterRefTraits::Release(
    AudioConverterRef converter) {
  const auto result = AudioConverterDispose(converter);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "AudioConverterDispose() failed";
}

AudioToolboxAudioDecoder::AudioToolboxAudioDecoder(
    std::unique_ptr<MediaLog> media_log)
    : media_log_(std::move(media_log)) {}

AudioToolboxAudioDecoder::~AudioToolboxAudioDecoder() = default;

AudioDecoderType AudioToolboxAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kAudioToolbox;
}

void AudioToolboxAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& waiting_cb) {
  if (!CanUseAudioToolbox(config)) {
    DLOG(WARNING)
        << "Only xHE-AAC/AC3/E-AC3 decoding is supported by this decoder.";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedCodec);
    return;
  }

  if (config.is_encrypted()) {
    DLOG(WARNING) << "Encrypted decoding not supported by platform decoder.";
    std::move(init_cb).Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  // This decoder supports re-initialization.
  decoder_.reset();

  output_cb_ = output_cb;
  base::BindPostTaskToCurrentDefault(std::move(init_cb))
      .Run(CreateDecoder(config)
               ? DecoderStatus::Codes::kOk
               : DecoderStatus::Codes::kFailedToCreateDecoder);
}

void AudioToolboxAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  DecodeCB decode_cb_bound =
      base::BindPostTaskToCurrentDefault(std::move(decode_cb));

  // Make sure we are notified if https://crbug.com/49709 returns. Issue also
  // occurs with some damaged files.
  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp) {
    DLOG(ERROR) << "Received a buffer without timestamps!";
    std::move(decode_cb_bound).Run(DecoderStatus::Codes::kMissingTimestamp);
    return;
  }

  if (!buffer->end_of_stream() && buffer->is_encrypted()) {
    DLOG(ERROR) << "Encrypted buffer not supported";
    std::move(decode_cb_bound)
        .Run(DecoderStatus::Codes::kUnsupportedEncryptionMode);
    return;
  }

  InputData input_data;
  input_data.buffer = buffer.get();
  if (!buffer->end_of_stream())
    input_data.packet.mDataByteSize = buffer->size();

  // Must be filled in each time in case AudioConverterFillComplexBuffer()
  // modified it during a previous call.
  output_buffer_list_->mNumberBuffers = output_bus_->channels();
  for (int i = 0; i < output_bus_->channels(); ++i) {
    output_buffer_list_->mBuffers[i].mNumberChannels = 1;
    output_buffer_list_->mBuffers[i].mDataByteSize =
        output_bus_->frames() * sizeof(float);
    output_buffer_list_->mBuffers[i].mData = output_bus_->channel(i);
  }

  // Decodes |num_frames| of encoded data into |output_bus_| by calling the
  // ProvideInputCallback to fill an AudioBufferList that points into
  // |input_data|. See media::AudioConverter for a similar mechanism.
  UInt32 num_frames = output_bus_->frames();
  auto result = AudioConverterFillComplexBuffer(
      decoder_.get(), ProvideInputCallback, &input_data, &num_frames,
      output_buffer_list_.get(), nullptr);

  if (result == kNoMoreDataError && !num_frames) {
    std::move(decode_cb_bound).Run(OkStatus());
    return;
  }

  if (result != noErr && result != kNoMoreDataError) {
    OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
        << "AudioConverterFillComplexBuffer() failed";
    std::move(decode_cb_bound)
        .Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  auto output_buffer =
      AudioBuffer::CopyFrom(channel_layout_, sample_rate_, buffer->timestamp(),
                            output_bus_.get(), pool_);

  if (num_frames != static_cast<UInt32>(output_bus_->frames()))
    output_buffer->TrimEnd(output_bus_->frames() - num_frames);
  if (discard_helper_->ProcessBuffers(buffer->time_info(),
                                      output_buffer.get())) {
    base::BindPostTaskToCurrentDefault(output_cb_)
        .Run(std::move(output_buffer));
  }

  std::move(decode_cb_bound).Run(OkStatus());
}

void AudioToolboxAudioDecoder::Reset(base::OnceClosure reset_cb) {
  // This could fail, but ResetCB has no error reporting mechanism, so just let
  // a subsequent decode call fail.
  const auto result = AudioConverterReset(decoder_.get());
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "AudioConverterReset() failed";
  discard_helper_->Reset(discard_helper_->decoder_delay());
  base::BindPostTaskToCurrentDefault(std::move(reset_cb)).Run();
}

bool AudioToolboxAudioDecoder::NeedsBitstreamConversion() const {
  // AAC shouldn't have an ADTS header for xHE-AAC since the AOT for xHE-AAC
  // doesn't fit within the ADTS header limitations.
  return false;
}

bool AudioToolboxAudioDecoder::CreateDecoder(const AudioDecoderConfig& config) {
  AudioStreamBasicDescription input_format = {};
  std::vector<uint8_t> magic_cookie;

  switch (config.codec()) {
    case AudioCodec::kAAC: {
      // Input is xHE-AAC / USAC.
      CHECK_EQ(config.profile(), AudioCodecProfile::kXHE_AAC);
      input_format.mFormatID = kAudioFormatMPEGD_USAC;
      magic_cookie = mp4::ESDescriptor::CreateEsds(config.aac_extra_data());

      // Have macOS fill in the rest of the input_format for us.
      UInt32 format_size = sizeof(input_format);
      auto status = AudioFormatGetProperty(
          kAudioFormatProperty_FormatInfo, magic_cookie.size(),
          magic_cookie.data(), &format_size, &input_format);
      if (status != noErr) {
        OSSTATUS_MEDIA_LOG(ERROR, status, media_log_)
            << "AudioFormatGetProperty() failed";
        return false;
      }
      break;
    }
#if BUILDFLAG(ENABLE_PLATFORM_AC3_EAC3_AUDIO)
    case AudioCodec::kAC3:
    case AudioCodec::kEAC3:
      // Input is AC3/E-AC3.
      input_format.mFormatID = config.codec() == AudioCodec::kAC3
                                   ? kAudioFormatAC3
                                   : kAudioFormatEnhancedAC3;

      // AC3/E-AC3 doesn't have an extra_data, so fill input format manually.
      input_format.mBytesPerPacket = 0;
      input_format.mSampleRate = config.samples_per_second();
      input_format.mChannelsPerFrame = config.channels();
      // This is a fixed value of 6 * 256 for AC3, and can be {1,2,3,6} * 256
      // for E-AC3. For now, set this value to 6 * 256 works for both codec
      // since we get frame count from AudioConverterFillComplexBuffer and trim
      // audio buffer each time during decoding.
      input_format.mFramesPerPacket = 6 * 256;
      break;
#endif
    default:
      NOTREACHED_IN_MIGRATION() << "Unsupported codec: " << config.codec();
      return false;
  }

  // Output is float planar.
  AudioStreamBasicDescription output_format = {};
  output_format.mFormatID = kAudioFormatLinearPCM;
  output_format.mFormatFlags =
      kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsNonInterleaved;
  output_format.mFramesPerPacket = 1;
  output_format.mBitsPerChannel = 32;

  // We don't want any channel or sample rate conversion.
  sample_rate_ = output_format.mSampleRate = input_format.mSampleRate;
  channel_count_ = output_format.mChannelsPerFrame =
      input_format.mChannelsPerFrame;

  // Note: This is important to get right or AudioConverterNew will balk. For
  // interleaved data, this value should be multiplied by the channel count.
  output_format.mBytesPerPacket = output_format.mBytesPerFrame =
      output_format.mBitsPerChannel / 8;

  // Create the decoder.
  auto result = AudioConverterNew(&input_format, &output_format,
                                  decoder_.InitializeInto());
  if (result != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
        << "AudioConverterNew() failed";
    return false;
  }

  if (channel_count_ > kMaxConcurrentChannels) {
    channel_layout_ = CHANNEL_LAYOUT_DISCRETE;
  } else {
    // Get the decoder's output channel layout.
    UInt32 size;
    result = AudioConverterGetPropertyInfo(
        decoder_.get(), kAudioConverterOutputChannelLayout, &size, NULL);
    if (result != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
          << "AudioConverterGetPropertyInfo() failed";
      return false;
    }

    ScopedAudioChannelLayout output_layout(size);
    result = AudioConverterGetProperty(decoder_.get(),
                                       kAudioConverterOutputChannelLayout,
                                       &size, output_layout.layout());
    if (result != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
          << "AudioConverterGetProperty() failed";
      return false;
    }

    // First, try to find a channel layout that matches the layout from decoder.
    // NOTE: We should retrieve layout from decoder, instead of using
    // channel layout from audio decoder config. Test result shows that if audio
    // converter thinks the audio is a 7.1_WIDE one, and we set output layout
    // to 7.1, this always lead to a loss of left and right channels.
    if (!AudioChannelLayoutToChannelLayout(*output_layout.layout(),
                                           &channel_layout_)) {
      // If we couldn't find a matched layout, use the guess result and hope
      // for the best.
      channel_layout_ = GuessChannelLayout(channel_count_);
    }
  }

  if (channel_count_ != static_cast<UInt32>(config.channels()) ||
      channel_layout_ != config.channel_layout()) {
    MEDIA_LOG(INFO, media_log_)
        << "Audio config updated: channels: " << channel_count_
        << ", channel layout: " << ChannelLayoutToString(channel_layout_);
  }

  // Next, convert back this layout to an audio channel layout with the same
  // channel order description. This let decoder output correct orders.
  auto ordered_layout =
      ChannelLayoutToAudioChannelLayout(channel_layout_, channel_count_);
  result = AudioConverterSetProperty(
      decoder_.get(), kAudioConverterOutputChannelLayout,
      ordered_layout->layout_size(), ordered_layout->layout());
  if (result != noErr) {
    OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
        << "AudioConverterSetProperty() failed";
    return false;
  }

  if (config.codec() == AudioCodec::kAAC) {
    // Instill the magic!
    result = AudioConverterSetProperty(
        decoder_.get(), kAudioConverterDecompressionMagicCookie,
        magic_cookie.size(), magic_cookie.data());
    if (result != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
          << "AudioConverterSetProperty() failed";
      return false;
    }

    // macOS doesn't provide a default target loudness. Use the value
    // recommended by Fraunhofer. AC3/E-AC3 doesn't support set this,
    // so limit this to xHE-AAC only.
    const Float32 kDefaultLoudness = -16.0;
    result = AudioConverterSetProperty(
        decoder_.get(), kAudioCodecPropertyProgramTargetLevel,
        sizeof(kDefaultLoudness), &kDefaultLoudness);
    if (result != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
          << "AudioConverterSetProperty() failed to set loudness.";
      return false;
    }

    // Likewise set the effect type recommended by Fraunhofer. There doesn't
    // appear to be a key name available for this yet.
    // Values: 0=none, night=1, noisy=2, limited=3
    const UInt32 kDefaultEffectType = 3;
    result = AudioConverterSetProperty(decoder_.get(), 0x64726370 /* "drcp" */,
                                       sizeof(kDefaultEffectType),
                                       &kDefaultEffectType);
    if (result != noErr) {
      OSSTATUS_MEDIA_LOG(ERROR, result, media_log_)
          << "AudioConverterSetProperty() failed to set DRC effect type.";
      return false;
    }
  }

  discard_helper_ = std::make_unique<AudioDiscardHelper>(
      sample_rate_, config.codec_delay(), false);
  discard_helper_->Reset(config.codec_delay());

  // Create staging structures we'll give to macOS for writing data into.
  output_bus_ = AudioBus::Create(input_format.mChannelsPerFrame,
                                 input_format.mFramesPerPacket);

  // AudioBufferList is a strange variable length structure that by default only
  // includes one buffer slot, so we need to construct our own multichannel one.
  //
  // NOTE: While we can allocate the AudioBufferList once, we need to fill it in
  // each time since the call to AudioConverterFillComplexBuffer() may modify
  // the values within the structure (particularly upon underflow).
  output_buffer_list_.reset(reinterpret_cast<AudioBufferList*>(
      calloc(1, sizeof(AudioBufferList) +
                    output_bus_->channels() * sizeof(AudioBuffer))));
  return true;
}

}  // namespace media
