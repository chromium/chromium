// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/mac/audio_toolbox_audio_decoder.h"

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/ranges/algorithm.h"
#include "base/sys_byteorder.h"
#include "base/task/bind_post_task.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_discard_helper.h"
#include "media/base/limits.h"
#include "media/base/status.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp4/es_descriptor.h"

namespace media {

namespace {

bool CanUseAudioToolbox(AudioCodecProfile profile) {
  // We only use AudioToolbox for decoding xHE-AAC content and that's only
  // available on macOS 10.15 or higher.
  if (__builtin_available(macOS 10.15, *))
    return profile == AudioCodecProfile::kXHE_AAC;
  return false;
}

// Descriptors use a variable length size entry. We've fixed the size to
// 4 bytes to make inline construction simple. The lowest 7 bits encode
// the actual value, an MSB==1 indicates there's another byte to decode,
// and an MSB==0 indicates there are no more bytes to decode.
void EncodeDescriptorSize(size_t size, uint8_t* output) {
  DCHECK_LT(size, (1u << (4u * 7u)));
  for (int i = 3; i > 0; i--)
    output[3 - i] = (size >> (7 * i)) | 0x80;
  output[3] = size & 0x7F;
}

std::vector<uint8_t> GenerateEsdsMagicCookie(
    const std::vector<uint8_t>& aac_extra_data) {
  // See media/formats/mp4/es_descriptor.h
#pragma pack(push, 1)
  struct Descriptor {
    uint8_t tag;
    uint8_t size[4];  // Note: Size is variable length, with a 1 in the MSB
                      // signaling another byte remains. Clamping to 4 here
                      // just makes it easier to construct the ESDS in place.
  };
  struct DecoderConfigDescriptor : Descriptor {
    uint8_t aot;
    uint8_t flags;
    uint8_t unused[11];
    Descriptor extra_data;
  };
  struct ESDescriptor : Descriptor {
    uint16_t id;
    uint8_t flags;
    DecoderConfigDescriptor decoder_config;
  };
#pragma pack(pop)

  std::vector<uint8_t> esds_data(sizeof(ESDescriptor) + aac_extra_data.size());
  auto* esds = reinterpret_cast<ESDescriptor*>(esds_data.data());

  esds->tag = mp4::kESDescrTag;
  EncodeDescriptorSize(
      sizeof(ESDescriptor) - sizeof(Descriptor) + aac_extra_data.size(),
      esds->size);

  esds->decoder_config.tag = mp4::kDecoderConfigDescrTag;
  EncodeDescriptorSize(sizeof(DecoderConfigDescriptor) - sizeof(Descriptor) +
                           aac_extra_data.size(),
                       esds->decoder_config.size);
  esds->decoder_config.aot = mp4::kISO_14496_3;  // AAC.
  esds->decoder_config.flags = 0x15;             // AudioStream

  esds->decoder_config.extra_data.tag = mp4::kDecoderSpecificInfoTag;
  EncodeDescriptorSize(aac_extra_data.size(),
                       esds->decoder_config.extra_data.size);

  base::ranges::copy(aac_extra_data, esds_data.begin() + sizeof(ESDescriptor));

  DCHECK(mp4::ESDescriptor().Parse(esds_data));
  return esds_data;
}

struct InputData {
  DecoderBuffer* buffer = nullptr;
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
  buffer_list->mBuffers[0].mDataByteSize = input_data->buffer->data_size();

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
  NOTREACHED() << "Only compatible with ASSUME policy";
  return converter;
}

// static
void AudioToolboxAudioDecoder::ScopedAudioConverterRefTraits::Release(
    AudioConverterRef converter) {
  const auto result = AudioConverterDispose(converter);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "AudioConverterDispose() failed";
}

AudioToolboxAudioDecoder::AudioToolboxAudioDecoder() = default;

AudioToolboxAudioDecoder::~AudioToolboxAudioDecoder() = default;

AudioDecoderType AudioToolboxAudioDecoder::GetDecoderType() const {
  return AudioDecoderType::kAudioToolbox;
}

void AudioToolboxAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                          CdmContext* cdm_context,
                                          InitCB init_cb,
                                          const OutputCB& output_cb,
                                          const WaitingCB& waiting_cb) {
  if (!CanUseAudioToolbox(config.profile())) {
    DLOG(WARNING) << "Only xHE-AAC decoding is supported by this decoder.";
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
      .Run(CreateAACDecoder(config)
               ? DecoderStatus::Codes::kOk
               : DecoderStatus::Codes::kFailedToCreateDecoder);
}

void AudioToolboxAudioDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      DecodeCB decode_cb) {
  // Make sure we are notified if https://crbug.com/49709 returns. Issue also
  // occurs with some damaged files.
  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp) {
    DLOG(ERROR) << "Received a buffer without timestamps!";
    base::BindPostTaskToCurrentDefault(std::move(decode_cb))
        .Run(DecoderStatus::Codes::kMissingTimestamp);
    return;
  }

  InputData input_data;
  input_data.buffer = buffer.get();
  if (!buffer->end_of_stream())
    input_data.packet.mDataByteSize = buffer->data_size();

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
      decoder_, ProvideInputCallback, &input_data, &num_frames,
      output_buffer_list_.get(), nullptr);

  if (result == kNoMoreDataError && !num_frames) {
    base::BindPostTaskToCurrentDefault(std::move(decode_cb)).Run(OkStatus());
    return;
  }

  if (result != noErr && result != kNoMoreDataError) {
    OSSTATUS_DLOG(ERROR, result) << "AudioConverterFillComplexBuffer() failed";
    base::BindPostTaskToCurrentDefault(std::move(decode_cb))
        .Run(DecoderStatus::Codes::kPlatformDecodeFailure);
    return;
  }

  auto output_buffer = AudioBuffer::CopyFrom(sample_rate_, buffer->timestamp(),
                                             output_bus_.get(), pool_);

  if (num_frames != static_cast<UInt32>(output_bus_->frames()))
    output_buffer->TrimEnd(output_bus_->frames() - num_frames);
  if (discard_helper_->ProcessBuffers(buffer->time_info(),
                                      output_buffer.get())) {
    base::BindPostTaskToCurrentDefault(output_cb_)
        .Run(std::move(output_buffer));
  }

  base::BindPostTaskToCurrentDefault(std::move(decode_cb)).Run(OkStatus());
}

void AudioToolboxAudioDecoder::Reset(base::OnceClosure reset_cb) {
  // This could fail, but ResetCB has no error reporting mechanism, so just let
  // a subsequent decode call fail.
  const auto result = AudioConverterReset(decoder_);
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

bool AudioToolboxAudioDecoder::CreateAACDecoder(
    const AudioDecoderConfig& config) {
  // Input is xHE-AAC / USAC.
  AudioStreamBasicDescription input_format = {};
  input_format.mFormatID = kAudioFormatMPEGD_USAC;

  auto magic_cookie = GenerateEsdsMagicCookie(config.aac_extra_data());

  // Have macOS fill in the rest of the input_format for us.
  UInt32 format_size = sizeof(input_format);
  auto status = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo,
                                       magic_cookie.size(), magic_cookie.data(),
                                       &format_size, &input_format);
  if (status != noErr) {
    OSSTATUS_DLOG(ERROR, status) << "AudioFormatGetProperty() failed";
    return false;
  }

  // Output is float planar.
  AudioStreamBasicDescription output_format = {};
  output_format.mFormatID = kAudioFormatLinearPCM;
  output_format.mFormatFlags =
      kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsNonInterleaved;
  output_format.mFramesPerPacket = 1;
  output_format.mBitsPerChannel = 32;
  output_format.mFramesPerPacket = 1;

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
    OSSTATUS_DLOG(ERROR, result) << "AudioConverterNew() failed";
    return false;
  }

  if (channel_count_ > kMaxConcurrentChannels) {
    channel_layout_ = CHANNEL_LAYOUT_DISCRETE;
  } else if (channel_count_ == static_cast<uint32_t>(config.channels())) {
    channel_layout_ = config.channel_layout();
  } else {
    // This could be improved to use retrieve the output layout from |decoder_|,
    // but since we'll almost always have the layout the config just use it.
    channel_layout_ = GuessChannelLayout(channel_count_);
  }

  // Instill the magic!
  result = AudioConverterSetProperty(decoder_,
                                     kAudioConverterDecompressionMagicCookie,
                                     magic_cookie.size(), magic_cookie.data());
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioConverterSetProperty() failed";
    return false;
  }

  // macOS doesn't provide a default target loudness. Use the value recommended
  // by Fraunhofer.
  const Float32 kDefaultLoudness = -16.0;
  result =
      AudioConverterSetProperty(decoder_, kAudioCodecPropertyProgramTargetLevel,
                                sizeof(kDefaultLoudness), &kDefaultLoudness);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioConverterSetProperty() failed to set loudness.";
    return false;
  }

  // Likewise set the effect type recommended by Fraunhofer. There doesn't
  // appear to be a key name available for this yet.
  // Values: 0=none, night=1, noisy=2, limited=3
  const UInt32 kDefaultEffectType = 3;
  result = AudioConverterSetProperty(decoder_, 0x64726370 /* "drcp" */,
                                     sizeof(kDefaultEffectType),
                                     &kDefaultEffectType);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result)
        << "AudioConverterSetProperty() failed to set DRC effect type.";
    return false;
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
