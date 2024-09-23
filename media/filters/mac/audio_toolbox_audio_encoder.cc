// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/filters/mac/audio_toolbox_audio_encoder.h"

#include "base/apple/osstatus_logging.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/converting_audio_fifo.h"
#include "media/base/encoder_status.h"
#include "media/base/media_util.h"
#include "media/base/timestamp_constants.h"
#include "media/formats/mp4/es_descriptor.h"

namespace media {

namespace {

struct InputData {
  raw_ptr<const AudioBus> bus = nullptr;
  bool flushing = false;
};

constexpr int kAacFramesPerBuffer = 1024;

// Callback used to provide input data to the AudioConverter.
OSStatus ProvideInputCallback(AudioConverterRef decoder,
                              UInt32* num_packets,
                              AudioBufferList* buffer_list,
                              AudioStreamPacketDescription** packets,
                              void* user_data) {
  auto* input_data = reinterpret_cast<InputData*>(user_data);
  if (input_data->flushing) {
    *num_packets = 0;
    return noErr;
  }

  CHECK(input_data->bus);
  DCHECK_EQ(input_data->bus->frames(), kAacFramesPerBuffer);

  const AudioBus* bus = input_data->bus;
  buffer_list->mNumberBuffers = bus->channels();
  for (int i = 0; i < bus->channels(); ++i) {
    buffer_list->mBuffers[i].mNumberChannels = 1;
    buffer_list->mBuffers[i].mDataByteSize = bus->frames() * sizeof(float);

    // A non-const version of channel(i) exists, but the compiler doesn't select
    // it for some reason.
    buffer_list->mBuffers[i].mData = const_cast<float*>(bus->channel(i));
  }

  // nFramesPerPacket is 1 for the input stream.
  *num_packets = bus->frames();

  // This callback should never be called more than once. Otherwise, we will
  // run into the CHECK above.
  input_data->bus = nullptr;
  return noErr;
}

void GenerateOutputFormat(const AudioEncoder::Options& options,
                          AudioStreamBasicDescription& output_format) {
  DCHECK(options.codec == AudioCodec::kAAC);

  // Output is AAC-LC. Documentation:
  // https://developer.apple.com/documentation/coreaudiotypes/coreaudiotype_constants/mpeg-4_audio_object_type_constants
  // TODO(crbug.com/40834751): Implement support for other AAC profiles.
  output_format.mFormatID = kAudioFormatMPEG4AAC;
  output_format.mFormatFlags = kMPEG4Object_AAC_LC;
}

bool GenerateCodecDescription(AudioCodec codec,
                              AudioConverterRef encoder,
                              std::vector<uint8_t>& codec_desc) {
  DCHECK(codec == AudioCodec::kAAC);

  // AAC should always have a codec description available.
  UInt32 magic_cookie_size = 0;
  auto result = AudioConverterGetPropertyInfo(
      encoder, kAudioConverterCompressionMagicCookie, &magic_cookie_size,
      nullptr);
  if (result != noErr || !magic_cookie_size) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to get magic cookie info";
    return false;
  }

  std::vector<uint8_t> magic_cookie(magic_cookie_size, 0);
  result =
      AudioConverterGetProperty(encoder, kAudioConverterCompressionMagicCookie,
                                &magic_cookie_size, magic_cookie.data());
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to get magic cookie";
    return false;
  }

  // The magic cookie is an ISO-BMFF ESDS box. Use our mp4 tools to extract just
  // the plain AAC extradata that we need.
  mp4::ESDescriptor esds;
  if (!esds.Parse(magic_cookie)) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to parse magic cookie";
    return false;
  }

  if (!mp4::ESDescriptor::IsAAC(esds.object_type())) {
    OSSTATUS_DLOG(ERROR, result) << "Expected AAC audio object type";
    return false;
  }

  codec_desc = esds.decoder_specific_info();
  return true;
}

}  // namespace

AudioToolboxAudioEncoder::AudioToolboxAudioEncoder() = default;

AudioToolboxAudioEncoder::~AudioToolboxAudioEncoder() {
  if (!encoder_)
    return;

  const auto result = AudioConverterDispose(encoder_);
  OSSTATUS_DLOG_IF(WARNING, result != noErr, result)
      << "AudioConverterDispose() failed";
}

void AudioToolboxAudioEncoder::Initialize(const Options& options,
                                          OutputCB output_cb,
                                          EncoderStatusCB done_cb) {
  if (output_cb_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  if (options.codec != AudioCodec::kAAC) {
    DLOG(WARNING) << "Only AAC encoding is supported by this encoder.";
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderUnsupportedCodec);
    return;
  }

  AudioStreamBasicDescription output_format = {};
  sample_rate_ = output_format.mSampleRate = options.sample_rate;
  channel_count_ = output_format.mChannelsPerFrame = options.channels;
  options_ = options;
  GenerateOutputFormat(options, output_format);

  if (!CreateEncoder(options, output_format)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  DCHECK(encoder_);

  if (!GenerateCodecDescription(options.codec, encoder_, codec_desc_)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  const AudioParameters fifo_params(AudioParameters::AUDIO_PCM_LINEAR,
                                    ChannelLayoutConfig::Guess(channel_count_),
                                    sample_rate_, kAacFramesPerBuffer);

  // `fifo_` will rebuffer frames to have kAacFramesPerBuffer, and remix to the
  // right number of channels if needed. `fifo_` should not resample any data.
  fifo_ = std::make_unique<ConvertingAudioFifo>(fifo_params, fifo_params);

  timestamp_helper_ = std::make_unique<AudioTimestampHelper>(sample_rate_);
  output_cb_ = output_cb;
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void AudioToolboxAudioEncoder::Encode(std::unique_ptr<AudioBus> input_bus,
                                      base::TimeTicks capture_time,
                                      EncoderStatusCB done_cb) {
  if (!encoder_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  DCHECK(timestamp_helper_);

  if (!timestamp_helper_->base_timestamp()) {
    timestamp_helper_->SetBaseTimestamp(capture_time - base::TimeTicks());
  }

  current_done_cb_ = std::move(done_cb);

  // This might synchronously call DoEncode().
  fifo_->Push(std::move(input_bus));
  DrainFifoOutput();

  if (current_done_cb_) {
    // If |current_donc_cb_| is null, DoEncode() has already reported an error.
    std::move(current_done_cb_).Run(EncoderStatus::Codes::kOk);
  }
}

void AudioToolboxAudioEncoder::Flush(EncoderStatusCB flush_cb) {
  DVLOG(1) << __func__;

  if (!encoder_) {
    std::move(flush_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (!timestamp_helper_->base_timestamp()) {
    // We never fed any data into the encoder. Skip the flush.
    std::move(flush_cb).Run(EncoderStatus::Codes::kOk);
    return;
  }

  current_done_cb_ = std::move(flush_cb);

  // Feed remaining data to the encoder. This might call DoEncode().
  fifo_->Flush();
  DrainFifoOutput();

  // Send an EOS to the encoder.
  DoEncode(nullptr);

  const auto result = AudioConverterReset(encoder_);

  auto status_code = EncoderStatus::Codes::kOk;
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioConverterReset() failed";
    status_code = EncoderStatus::Codes::kEncoderFailedFlush;
  }

  timestamp_helper_->Reset();

  if (current_done_cb_) {
    // If |current_done_cb_| is null, DoEncode() has already reported an error.
    std::move(current_done_cb_).Run(status_code);
  }
}

bool AudioToolboxAudioEncoder::CreateEncoder(
    const Options& options,
    const AudioStreamBasicDescription& output_format) {
  // Input is always float planar.
  AudioStreamBasicDescription input_format = {};
  input_format.mFormatID = kAudioFormatLinearPCM;
  input_format.mFormatFlags =
      kLinearPCMFormatFlagIsFloat | kLinearPCMFormatFlagIsNonInterleaved;
  input_format.mFramesPerPacket = 1;
  input_format.mBitsPerChannel = 32;
  input_format.mSampleRate = options.sample_rate;
  input_format.mChannelsPerFrame = options.channels;

  // Note: This is important to get right or AudioConverterNew will balk. For
  // interleaved data, this value should be multiplied by the channel count.
  input_format.mBytesPerPacket = input_format.mBytesPerFrame =
      input_format.mBitsPerChannel / 8;

  // Create the encoder.
  auto result = AudioConverterNew(&input_format, &output_format, &encoder_);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "AudioConverterNew() failed";
    return false;
  }

  // NOTE: We don't setup the AudioConverter channel layout here, though we may
  // need to in the future to support obscure multichannel layouts.

  if (options.bitrate && options.bitrate > 0) {
    UInt32 rate = options.bitrate.value();
    result = AudioConverterSetProperty(encoder_, kAudioConverterEncodeBitRate,
                                       sizeof(rate), &rate);
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "Failed to set encoder bitrate";
      return false;
    }
  }

  if (options.bitrate_mode) {
    const bool use_vbr =
        *options.bitrate_mode == AudioEncoder::BitrateMode::kVariable;

    UInt32 bitrate_mode = use_vbr ? kAudioCodecBitRateControlMode_Variable
                                  : kAudioCodecBitRateControlMode_Constant;

    result = AudioConverterSetProperty(encoder_,
                                       kAudioCodecPropertyBitRateControlMode,
                                       sizeof(bitrate_mode), &bitrate_mode);
    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result) << "Failed to set encoder bitrate mode";
      return false;
    }
  }

  // AudioConverter requires we provided a suitably sized output for the encoded
  // buffer, but won't tell us the size before we request it... so we need to
  // ask it what the maximum possible size is to allocate our output buffers.
  UInt32 prop_size = sizeof(UInt32);
  result = AudioConverterGetProperty(
      encoder_, kAudioConverterPropertyMaximumOutputPacketSize, &prop_size,
      &max_packet_size_);
  if (result != noErr) {
    OSSTATUS_DLOG(ERROR, result) << "Failed to retrieve maximum packet size";
    return false;
  }

  return true;
}

void AudioToolboxAudioEncoder::DrainFifoOutput() {
  while (fifo_->HasOutput()) {
    DoEncode(fifo_->PeekOutput());
    fifo_->PopOutput();
  }
}

void AudioToolboxAudioEncoder::DoEncode(const AudioBus* input_bus) {
  bool is_flushing = !input_bus;

  InputData input_data;
  input_data.bus = input_bus;
  input_data.flushing = is_flushing;

  do {
    temp_output_buf_.resize(max_packet_size_);

    AudioBufferList output_buffer_list = {};
    output_buffer_list.mNumberBuffers = 1;
    output_buffer_list.mBuffers[0].mNumberChannels = channel_count_;
    output_buffer_list.mBuffers[0].mData = temp_output_buf_.data();
    output_buffer_list.mBuffers[0].mDataByteSize = max_packet_size_;

    // Encodes |num_packets| into |packet_buffer| by calling the
    // ProvideInputCallback to fill an AudioBufferList that points into
    // |input_bus|. See media::AudioConverter for a similar mechanism.
    UInt32 num_packets = 1;
    AudioStreamPacketDescription packet_description = {};
    auto result = AudioConverterFillComplexBuffer(
        encoder_, ProvideInputCallback, &input_data, &num_packets,
        &output_buffer_list, &packet_description);

    // We expect "1 in, 1 out" when feeding packets into the encoder, except
    // when flushing.
    if (result == noErr && !num_packets) {
      DCHECK(is_flushing);
      return;
    }

    if (result != noErr) {
      OSSTATUS_DLOG(ERROR, result)
          << "AudioConverterFillComplexBuffer() failed";
      std::move(current_done_cb_)
          .Run(EncoderStatus::Codes::kEncoderFailedEncode);
      return;
    }

    DCHECK_LE(packet_description.mDataByteSize, max_packet_size_);
    temp_output_buf_.resize(packet_description.mDataByteSize);

    // All AAC-LC packets are 1024 frames in size. Note: If other AAC profiles
    // are added later, this value must be updated.
    auto num_frames = kAacFramesPerBuffer * num_packets;
    DVLOG(1) << __func__ << ": Output: num_frames=" << num_frames;

    bool adts_conversion_ok = true;
    auto format = options_.aac.value_or(AacOptions()).format;
    std::optional<CodecDescription> desc;
    if (timestamp_helper_->frame_count() == 0) {
      if (format == AudioEncoder::AacOutputFormat::AAC) {
        desc = codec_desc_;
      } else {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
        NullMediaLog log;
        adts_conversion_ok = aac_config_parser_.Parse(codec_desc_, &log);
#else
        adts_conversion_ok = false;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
      }
    }

    base::HeapArray<uint8_t> packet_buffer;

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (format == AudioEncoder::AacOutputFormat::ADTS) {
      int adts_header_size = 0;
      packet_buffer = aac_config_parser_.CreateAdtsFromEsds(temp_output_buf_,
                                                            &adts_header_size);
      adts_conversion_ok = !packet_buffer.empty();
    }
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

    if (!adts_conversion_ok) {
      OSSTATUS_DLOG(ERROR, result) << "Conversion to ADTS failed";
      std::move(current_done_cb_)
          .Run(EncoderStatus::Codes::kEncoderFailedEncode);
      return;
    }

    if (packet_buffer.empty()) {
      packet_buffer = base::HeapArray<uint8_t>::CopiedFrom(temp_output_buf_);
    }

    EncodedAudioBuffer encoded_buffer(
        AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                        ChannelLayoutConfig::Guess(channel_count_),
                        sample_rate_, num_frames),
        std::move(packet_buffer),
        base::TimeTicks() + timestamp_helper_->GetTimestamp(),
        timestamp_helper_->GetFrameDuration(num_frames));

    timestamp_helper_->AddFrames(num_frames);
    output_cb_.Run(std::move(encoded_buffer), desc);
  } while (is_flushing);  // Only encode once when we aren't flushing.
}

}  // namespace media
