// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/windows/mf_audio_encoder.h"

#include <codecapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <stddef.h>
#include <string.h>
#include <wmcodecdsp.h>

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/win/com_init_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/win_util.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/channel_layout.h"
#include "media/base/encoder_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/win/mf_helpers.h"
#include "media/base/win/mf_initializer.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace media {
namespace {

// The following values are from:
// https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder
//
// Number of PCM samples per AAC frame
constexpr int kSamplesPerFrame = 1024;
constexpr int kBitsPerSample = 16;
constexpr int kBytesPerSample = 2;

// The AAC encoder has a default `AVG_BYTES_PER_SECOND` of 12000, so the
// default bitrate is 8x.
constexpr int kDefaultBitrate = 96000;
constexpr std::array<int, 4> kSupportedBitrates = {96000, 128000, 160000,
                                                   192000};
constexpr std::array<int, 2> kSupportedSampleRates = {44100, 48000};

// If the MFT does not specify a required block alignment, it is recommended
// that at least a 16-byte memory alignment is used.
// https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_output_stream_info
constexpr int kMinimumRecommendedBlockAlignment = 16;

// Since there is only one input and one output stream, both will have an ID
// of 0.
// https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getstreamids#remarks
constexpr DWORD kStreamId = 0;

// The encoder keeps two full frames of data buffered at all times, which means
// it must have three full frames of data buffered before it will produce an
// output frame. It only needs two frames to successfully flush.
constexpr int kMinSamplesForOutput = kSamplesPerFrame * 3;
constexpr int kMinSamplesForFlush = kSamplesPerFrame * 2;

EncoderStatus::Codes ValidateInputOptions(const AudioEncoder::Options& options,
                                          ChannelLayout* channel_layout,
                                          int* bitrate) {
  if (options.codec != AudioCodec::kAAC)
    return EncoderStatus::Codes::kEncoderUnsupportedCodec;

  if (!base::Contains(kSupportedSampleRates, options.sample_rate)) {
    return EncoderStatus::Codes::kEncoderUnsupportedConfig;
  }

  switch (options.channels) {
    case 1:
      *channel_layout = CHANNEL_LAYOUT_MONO;
      break;
    case 2:
      *channel_layout = CHANNEL_LAYOUT_STEREO;
      break;
    case 6:
      *channel_layout = CHANNEL_LAYOUT_5_1;
      break;
    default:
      return EncoderStatus::Codes::kEncoderUnsupportedConfig;
  }

  *bitrate = options.bitrate.value_or(kDefaultBitrate);
  if (!base::Contains(kSupportedBitrates, *bitrate)) {
    return EncoderStatus::Codes::kEncoderUnsupportedConfig;
  }

  return EncoderStatus::Codes::kOk;
}

HRESULT CreateMFEncoder(const IID& iid, void** out_encoder) {
  if (!InitializeMediaFoundation())
    return MF_E_PLATFORM_NOT_INITIALIZED;

  MFT_REGISTER_TYPE_INFO input_type = {MFMediaType_Audio, MFAudioFormat_PCM};
  MFT_REGISTER_TYPE_INFO output_type = {MFMediaType_Audio, MFAudioFormat_AAC};
  UINT32 flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER;

  base::win::ScopedCoMem<IMFActivate*> activates;
  UINT32 num_activates;
  RETURN_IF_FAILED(MFTEnumEx(MFT_CATEGORY_AUDIO_ENCODER, flags, &input_type,
                             &output_type, &activates, &num_activates));

  if (num_activates < 1)
    return ERROR_NOT_FOUND;

  HRESULT hr = activates[0]->ActivateObject(iid, out_encoder);

  // According to Windows App Development doc,
  // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mftenumex
  // the caller must release the pointers before CoTaskMemFree function inside
  // base::win::ScopedCoMem.
  for (UINT32 i = 0; i < num_activates; i++)
    activates[i]->Release();

  return hr;
}

HRESULT CreateInputMediaType(const int sample_rate,
                             const int channels,
                             ComMFMediaType* input_media_type) {
  // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder#input-types
  ComMFMediaType media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, kBitsPerSample));
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sample_rate));
  RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels));

  *input_media_type = std::move(media_type);
  return S_OK;
}

HRESULT CreateOutputMediaType(const int sample_rate,
                              const int channels,
                              const int bitrate,
                              media::AudioEncoder::AacOutputFormat format,
                              ComMFMediaType* output_media_type) {
  // https://docs.microsoft.com/en-us/windows/win32/medfound/aac-encoder#output-types
  ComMFMediaType media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_AAC));
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, kBitsPerSample));
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, sample_rate));
  RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels));

  // MF_MT_AUDIO_AVG_BYTES_PER_SECOND is missing documentation for the 5.1
  // channel case. It requires it to be multiplied by the number of channels.
  int adjusted_bitrate = channels > 2 ? bitrate * channels : bitrate;
  RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                                         adjusted_bitrate / 8));

  // Set payload format.
  // https://learn.microsoft.com/en-us/windows/win32/medfound/mf-mt-aac-payload-type
  // 0 - The stream contains raw_data_block elements only. (default)
  // 1 - Audio Data Transport Stream (ADTS).
  //     The stream contains an adts_sequence, as defined by MPEG-2.
  if (format == media::AudioEncoder::AacOutputFormat::ADTS) {
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 1));
  }

  *output_media_type = std::move(media_type);
  return S_OK;
}

HRESULT GetInputBufferRequirements(const ComMFTransform& mf_encoder,
                                   const ComMFMediaType& input_media_type,
                                   const int channels,
                                   int* input_buffer_alignment,
                                   size_t* min_input_buffer_size) {
  MFT_INPUT_STREAM_INFO input_stream_info = {};
  RETURN_IF_FAILED(
      mf_encoder->GetInputStreamInfo(kStreamId, &input_stream_info));

  // `cbAlignment` contains the memory alignment required for input buffers. If
  // the MFT does not require a specific alignment, the value is zero.
  // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_input_stream_info
  if (input_stream_info.cbAlignment != 0) {
    *input_buffer_alignment = input_stream_info.cbAlignment;
  } else {
    // For PCM audio formats, the block alignment is equal to the number of
    // audio channels multiplied by the number of bytes per audio sample.
    // https://docs.microsoft.com/en-us/windows/win32/medfound/mf-mt-audio-block-alignment-attribute
    *input_buffer_alignment =
        std::max(channels * kBytesPerSample, kMinimumRecommendedBlockAlignment);
  }

  // `cbSize` contains the minimum size of each input buffer, in bytes. If the
  // size is variable or the MFT does not require a specific size, the value is
  // zero.
  if (input_stream_info.cbSize != 0) {
    *min_input_buffer_size = input_stream_info.cbSize;
  } else {
    // For uncompressed audio, the value should be the audio frame size, which
    // you can get from the MF_MT_AUDIO_BLOCK_ALIGNMENT attribute in the media
    // type.
    // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_input_stream_info
    UINT32 input_block_attribute;
    RETURN_IF_FAILED(input_media_type->GetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT,
                                                 &input_block_attribute));

    *min_input_buffer_size = input_block_attribute;
  }

  // The buffer must be large enough to hold at least one sample.
  CHECK_GE(*min_input_buffer_size,
           static_cast<size_t>(channels * kBytesPerSample));
  return S_OK;
}

HRESULT GetOutputBufferRequirements(const ComMFTransform& mf_encoder,
                                    const ComMFMediaType& output_media_type,
                                    const int channels,
                                    int* output_buffer_alignment) {
  MFT_OUTPUT_STREAM_INFO output_stream_info = {};
  RETURN_IF_FAILED(
      mf_encoder->GetOutputStreamInfo(kStreamId, &output_stream_info));

  // `cbAlignment` contains the memory alignment required for output buffers. If
  // the MFT does not require a specific alignment, the value is zero.
  // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ns-mftransform-mft_output_stream_info
  if (output_stream_info.cbAlignment != 0) {
    *output_buffer_alignment = output_stream_info.cbAlignment;
  } else {
    // For PCM audio formats, the block alignment is equal to the number of
    // audio channels multiplied by the number of bytes per audio sample.
    // https://docs.microsoft.com/en-us/windows/win32/medfound/mf-mt-audio-block-alignment-attribute
    *output_buffer_alignment =
        std::max(channels * kBytesPerSample, kMinimumRecommendedBlockAlignment);
  }

  // We aren't interested in the output buffer size because we query the encoder
  // for it before getting the output each time.

  return S_OK;
}

HRESULT CreateMFSampleFromAudioBus(const AudioBus& audio_bus,
                                   const int buffer_alignment,
                                   const LONGLONG duration,
                                   const LONGLONG timestamp,
                                   ComMFSample* output_sample) {
  DCHECK_GE(buffer_alignment, kMinimumRecommendedBlockAlignment);
  DCHECK_GT(duration, 0);

  // Create `dest_buffer` which we will fill with unencoded data, wrap in an
  // `IMFSample`, and return to the caller.
  ComMFMediaBuffer dest_buffer;
  size_t source_data_size =
      audio_bus.channels() * audio_bus.frames() * kBytesPerSample;

  // `buffer_alignment - 1` converts the int to correct constant value.
  // https://docs.microsoft.com/en-us/windows/win32/api/mfapi/nf-mfapi-mfcreatealignedmemorybuffer
  RETURN_IF_FAILED(MFCreateAlignedMemoryBuffer(
      source_data_size, buffer_alignment - 1, &dest_buffer));

  // Copy data from `audio_bus` into `dest_buffer`.
  BYTE* dest_buffer_ptr = nullptr;
  DWORD max_buffer_size = 0;
  DWORD current_buffer_size = 0;
  RETURN_IF_FAILED(dest_buffer->Lock(&dest_buffer_ptr, &max_buffer_size,
                                     &current_buffer_size));

  // Brand new buffer should be empty, and able to hold the entire input.
  DCHECK_EQ(current_buffer_size, 0ul);
  CHECK_GE(max_buffer_size, static_cast<DWORD>(source_data_size));

  // Convert data from `audio_bus` to interleaved signed int16_t data, as this
  // is the format required by the encoder.
  audio_bus.ToInterleaved<SignedInt16SampleTypeTraits>(
      audio_bus.frames(), reinterpret_cast<int16_t*>(dest_buffer_ptr));
  RETURN_IF_FAILED(dest_buffer->Unlock());
  RETURN_IF_FAILED(dest_buffer->SetCurrentLength(source_data_size));

  // Create the sample which holds `dest_buffer` and will be delivered to the
  // caller.
  ComMFSample sample;
  RETURN_IF_FAILED(MFCreateSample(&sample));
  RETURN_IF_FAILED(sample->AddBuffer(dest_buffer.Get()));
  RETURN_IF_FAILED(sample->SetSampleDuration(duration));
  RETURN_IF_FAILED(sample->SetSampleTime(timestamp));

  *output_sample = std::move(sample);
  return S_OK;
}

HRESULT GetSampleBuffer(const DWORD required_size,
                        const int buffer_alignment,
                        ComMFSample& sample,
                        ComMFMediaBuffer& buffer) {
  if (!sample)
    RETURN_IF_FAILED(MFCreateSample(&sample));

  DWORD buffer_count;
  RETURN_IF_FAILED(sample->GetBufferCount(&buffer_count));

  bool need_buffer_allocation = buffer_count == 0;
  if (!need_buffer_allocation) {
    RETURN_IF_FAILED(sample->GetBufferByIndex(0, &buffer));

    DWORD buffer_capacity;
    RETURN_IF_FAILED(buffer->GetMaxLength(&buffer_capacity));

    if (buffer_capacity < required_size)
      need_buffer_allocation = true;
  }

  if (need_buffer_allocation) {
    RETURN_IF_FAILED(
        MFCreateAlignedMemoryBuffer(required_size, buffer_alignment, &buffer));
    RETURN_IF_FAILED(sample->AddBuffer(buffer.Get()));
  }

  return S_OK;
}

}  // namespace

MFAudioEncoder::InputData::InputData(ComMFSample&& sample,
                                     const int sample_count,
                                     EncoderStatusCB&& done_cb)
    : sample(std::move(sample)),
      sample_count(sample_count),
      done_cb(std::move(done_cb)) {}
MFAudioEncoder::InputData::InputData(InputData&&) = default;
MFAudioEncoder::InputData::~InputData() = default;

MFAudioEncoder::PendingData::PendingData(std::unique_ptr<AudioBus>&& audio_bus,
                                         const base::TimeTicks capture_time,
                                         EncoderStatusCB&& done_cb)
    : audio_bus(std::move(audio_bus)),
      capture_time(capture_time),
      done_cb(std::move(done_cb)) {}
MFAudioEncoder::PendingData::PendingData(PendingData&&) = default;
MFAudioEncoder::PendingData::~PendingData() = default;

MFAudioEncoder::MFAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(std::move(task_runner)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

MFAudioEncoder::~MFAudioEncoder() = default;

// `MFAudioEncoder` generally follows the steps outlined in this document:
// https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model
// `Initialize` performs the first few steps: Create the MFT, Set Media Types,
//  Get Buffer Requirements, and the first portion of Process Data.
void MFAudioEncoder::Initialize(const Options& options,
                                OutputCB output_cb,
                                EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb);
  DCHECK(output_cb);
  base::win::AssertComInitialized();

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));
  if (initialized_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializeTwice);
    return;
  }

  options_ = options;
  int bitrate;
  ChannelLayout channel_layout;
  EncoderStatus::Codes code =
      ValidateInputOptions(options_, &channel_layout, &bitrate);
  if (code != EncoderStatus::Codes::kOk) {
    std::move(done_cb).Run(code);
    return;
  }

  // Create the MF encoder.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#create-the-mft
  HRESULT hr = CreateMFEncoder(IID_PPV_ARGS(&mf_encoder_));
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderUnsupportedCodec);
    return;
  }

  if (options_.bitrate_mode.has_value() &&
      options_.bitrate_mode.value() == AudioEncoder::BitrateMode::kVariable &&
      options.codec == AudioCodec::kAAC) {
    ComCodecAPI codec_api;
    hr = mf_encoder_.As(&codec_api);

    if (SUCCEEDED(hr) &&
        codec_api->IsSupported(&CODECAPI_AVEncAACEnableVBR) == S_OK) {
      VARIANT var;
      var.vt = VT_UI4;
      var.ulVal = TRUE;
      hr = codec_api->SetValue(&CODECAPI_AVEncAACEnableVBR, &var);
      if (FAILED(hr)) {
        DVLOG(2) << "Configuring AAC encoder to VBR mode rejected. Fallback to "
                    "CBR mode.";
      }
    }
  }

  // We skip getting the stream counts and IDs because encoders only have one
  // input and output stream, and the ID of each is always 0.
  // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getstreamids#remarks

  // Set the input and output media types.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#set-media-types
  ComMFMediaType input_media_type;
  hr = CreateInputMediaType(options_.sample_rate, options_.channels,
                            &input_media_type);
  if (FAILED(hr) || !input_media_type) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  hr =
      mf_encoder_->SetInputType(kStreamId, input_media_type.Get(), /*flags=*/0);
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  auto format = options_.aac.value_or(AacOptions()).format;
  ComMFMediaType output_media_type;
  hr = CreateOutputMediaType(options_.sample_rate, options_.channels, bitrate,
                             format, &output_media_type);
  if (FAILED(hr) || !output_media_type) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  hr = mf_encoder_->SetOutputType(kStreamId, output_media_type.Get(),
                                  /*flags=*/0);
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  // Get buffer requirements.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#get-buffer-requirements
  hr = GetInputBufferRequirements(mf_encoder_, input_media_type,
                                  options_.channels, &input_buffer_alignment_,
                                  &min_input_buffer_size_);
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  hr =
      GetOutputBufferRequirements(mf_encoder_, output_media_type,
                                  options_.channels, &output_buffer_alignment_);

  /*
    https://learn.microsoft.com/en-us/windows/win32/medfound/aac-encoder

    After the output type is set, the AAC encoder updates the type by adding
    the MF_MT_USER_DATA attribute. This attribute contains the portion of
    the HEAACWAVEINFO structure that appears after the WAVEFORMATEX structure
    (that is, after the wfx member).
    This is followed by the AudioSpecificConfig() data,
    as defined by ISO/IEC 14496-3.
  */
  UINT32 desc_size = 0;
  if (output_media_type->GetBlobSize(MF_MT_USER_DATA, &desc_size) == S_OK &&
      desc_size > 0 && format == media::AudioEncoder::AacOutputFormat::AAC) {
    codec_desc_.resize(desc_size);
    size_t aac_config_offset =
        sizeof(HEAACWAVEINFO) - offsetof(HEAACWAVEINFO, wPayloadType);
    hr = output_media_type->GetBlob(MF_MT_USER_DATA, codec_desc_.data(),
                                    desc_size, nullptr);
    if (FAILED(hr) || aac_config_offset > codec_desc_.size()) {
      std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
      return;
    }
    codec_desc_.erase(codec_desc_.begin(),
                      codec_desc_.begin() + aac_config_offset);
  }

  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderInitializationError);
    return;
  }

  channel_count_ = options_.channels;
  audio_params_ = AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  {channel_layout, channel_count_},
                                  options_.sample_rate, kSamplesPerFrame);
  input_timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(options_.sample_rate);
  output_timestamp_tracker_ =
      std::make_unique<AudioTimestampHelper>(options_.sample_rate);
  output_cb_ = BindCallbackToCurrentLoopIfNeeded(std::move(output_cb));
  initialized_ = true;
  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void MFAudioEncoder::Encode(std::unique_ptr<AudioBus> audio_bus,
                            base::TimeTicks capture_time,
                            EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb);

  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));

  if (!initialized_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (state_ == EncoderState::kError) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  CHECK_EQ(audio_bus->channels(), channel_count_);

  // The `mf_encoder_` will not accept new input while flushing or draining. So,
  // we store new inputs in `pending_inputs_` until the flush is complete, and
  // we'll queue them up in `OnFlushComplete()`.
  if (state_ == EncoderState::kFlushing || state_ == EncoderState::kDraining) {
    pending_inputs_.emplace_back(std::move(audio_bus), capture_time,
                                 std::move(done_cb));
    return;
  }

  EnqueueInput(std::move(audio_bus), capture_time, std::move(done_cb));
}

void MFAudioEncoder::Flush(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_cb);
  done_cb = BindCallbackToCurrentLoopIfNeeded(std::move(done_cb));

  if (!initialized_) {
    std::move(done_cb).Run(
        EncoderStatus::Codes::kEncoderInitializeNeverCompleted);
    return;
  }

  if (input_queue_.empty() && samples_in_encoder_ == 0) {
    std::move(done_cb).Run(EncoderStatus::Codes::kOk);
    return;
  }

  if (state_ == EncoderState::kError || state_ == EncoderState::kFlushing ||
      state_ == EncoderState::kDraining || !can_flush_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedFlush);
    return;
  }

  DCHECK(state_ == EncoderState::kIdle || state_ == EncoderState::kProcessing)
      << "state_ == " << static_cast<int>(state_);

  have_queued_input_task_ = false;
  have_queued_output_task_ = false;
  state_ = EncoderState::kFlushing;
  TryProcessOutput(base::BindOnce(&MFAudioEncoder::OnFlushComplete,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(done_cb)));
}

void MFAudioEncoder::EnqueueInput(std::unique_ptr<AudioBus> audio_bus,
                                  base::TimeTicks capture_time,
                                  EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);
  DCHECK(audio_bus);
  DCHECK(done_cb);
  DCHECK_NE(state_, EncoderState::kError);

  // If we have no timestamp, this is either the first input, or the first input
  // after flushing. In either case, we need to notify the encoder that we are
  // about to send data.
  HRESULT hr;
  if (!input_timestamp_tracker_->base_timestamp()) {
    DCHECK(!output_timestamp_tracker_->base_timestamp());
    hr = mf_encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,
                                     /*message_param=*/0);
    if (FAILED(hr)) {
      std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
      return;
    }

    hr = mf_encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM,
                                     /*message_param=*/0);
    if (FAILED(hr)) {
      std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
      return;
    }

    input_timestamp_tracker_->SetBaseTimestamp(capture_time -
                                               base::TimeTicks());
    output_timestamp_tracker_->SetBaseTimestamp(capture_time -
                                                base::TimeTicks());
  }

  // The `min_input_buffer_size_` is usually the size of a single sample, but
  // it can be the size of two samples if `channel_count_` is 1.
  if (static_cast<size_t>(audio_bus->frames() * channel_count_ *
                          kBytesPerSample) < min_input_buffer_size_) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  // MF requires the duration and timestamp to be in 100 nanosecond units.
  LONGLONG duration =
      input_timestamp_tracker_->GetFrameDuration(audio_bus->frames())
          .InNanoseconds() /
      100LL;
  LONGLONG timestamp =
      input_timestamp_tracker_->GetTimestamp().InNanoseconds() / 100LL;
  input_timestamp_tracker_->AddFrames(audio_bus->frames());

  ComMFSample input_sample;
  hr = CreateMFSampleFromAudioBus(*audio_bus, input_buffer_alignment_, duration,
                                  timestamp, &input_sample);
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedEncode);
    return;
  }

  input_queue_.emplace_back(std::move(input_sample), audio_bus->frames(),
                            std::move(done_cb));
  if (state_ == EncoderState::kIdle)
    TryProcessInput(/*flush_cb=*/base::NullCallback());
}

void MFAudioEncoder::RunTryProcessInput(FlushCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  have_queued_input_task_ = false;
  TryProcessInput(std::move(flush_cb));
}

void MFAudioEncoder::TryProcessInput(FlushCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  if (state_ == EncoderState::kError)
    return;

  // This is an old call that was posted before we started flushing. Another
  // call with a `flush_cb` will be posted if needed, so we can return early
  // from this one.
  if ((state_ == EncoderState::kFlushing ||
       state_ == EncoderState::kDraining) &&
      !flush_cb) {
    return;
  }

  if (state_ == EncoderState::kDraining) {
    DCHECK(flush_cb);
    DCHECK(input_queue_.empty());

    if (samples_in_encoder_ <= 0)
      std::move(flush_cb).Run();
    else
      TryProcessOutput(std::move(flush_cb));

    return;
  }

  if (state_ == EncoderState::kIdle)
    state_ = EncoderState::kProcessing;

  DCHECK(state_ == EncoderState::kProcessing ||
         state_ == EncoderState::kFlushing)
      << "state_ == " << static_cast<int>(state_);

  bool not_accepting = false;
  HRESULT hr = S_OK;
  while (SUCCEEDED(hr) && !input_queue_.empty()) {
    InputData& input_data = input_queue_.front();
    hr = mf_encoder_->ProcessInput(kStreamId, input_data.sample.Get(),
                                   /*flags=*/0);
    if (hr == MF_E_NOTACCEPTING) {
      not_accepting = true;
      break;
    }
    if (FAILED(hr)) {
      OnError();
      return;
    }

    std::move(input_data.done_cb).Run(EncoderStatus::Codes::kOk);
    samples_in_encoder_ += input_data.sample_count;
    input_queue_.pop_front();
  }

  if (samples_in_encoder_ >= kMinSamplesForOutput)
    can_produce_output_ = true;

  if (samples_in_encoder_ >= kMinSamplesForFlush)
    can_flush_ = true;

  // We must call `TryProcessOutput` if `not_accepting` is true in order for
  // the `mf_encoder_` to move data from its input buffer to a staging buffer,
  // which will allow us to provide more input.
  if (not_accepting || can_produce_output_ ||
      state_ == EncoderState::kFlushing) {
    TryProcessOutput(std::move(flush_cb));
    return;
  }

  state_ = EncoderState::kIdle;
}

void MFAudioEncoder::RunTryProcessOutput(FlushCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  have_queued_output_task_ = false;
  TryProcessOutput(std::move(flush_cb));
}

void MFAudioEncoder::TryProcessOutput(FlushCB flush_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  if (state_ == EncoderState::kError)
    return;

  if (state_ == EncoderState::kIdle)
    state_ = EncoderState::kProcessing;

  DCHECK(state_ == EncoderState::kProcessing ||
         state_ == EncoderState::kFlushing || state_ == EncoderState::kDraining)
      << "state_ == " << static_cast<int>(state_);

  // This is an old call that was posted before we started flushing. So, we can
  // return early, since `Flush()` will have called `TryProcessOutput()` and
  // provided a `flush_cb`.
  if ((state_ == EncoderState::kFlushing ||
       state_ == EncoderState::kDraining) &&
      !flush_cb) {
    return;
  }

  DWORD status;
  HRESULT hr = mf_encoder_->GetOutputStatus(&status);
  while (SUCCEEDED(hr) && (status == MFT_OUTPUT_STATUS_SAMPLE_READY ||
                           state_ == EncoderState::kDraining)) {
    EncodedAudioBuffer encoded_audio;
    hr = ProcessOutput(encoded_audio);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
      break;
    if (FAILED(hr)) {
      OnError();
      return;
    }

    std::optional<CodecDescription> desc;
    if (!codec_desc_.empty()) {
      desc = codec_desc_;
      codec_desc_.clear();
    }

    output_cb_.Run(std::move(encoded_audio), desc);
    samples_in_encoder_ -= kSamplesPerFrame;
    hr = mf_encoder_->GetOutputStatus(&status);
  }

  if (!input_queue_.empty()) {
    // Setting our state to idle before posting tasks allows the next call to
    // `EnqueueInput` to call `TryProcessInput`. This lets callers run this
    // encoder synchronously.
    if (state_ == EncoderState::kProcessing)
      state_ = EncoderState::kIdle;

    if (!have_queued_input_task_ || flush_cb) {
      have_queued_input_task_ = true;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MFAudioEncoder::RunTryProcessInput,
                         weak_ptr_factory_.GetWeakPtr(), std::move(flush_cb)));
    }

    return;
  }

  // If we've emptied the input queue, and we are flushing, then we can now
  // tell the encoder to drain. Once it starts draining, it will not accept
  // any further input until all the output has been processed. This is why we
  // waited until now to send this message.
  if (state_ == EncoderState::kFlushing) {
    state_ = EncoderState::kDraining;
    hr = mf_encoder_->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM,
                                     /*param=*/0);
    if (FAILED(hr)) {
      OnError();
      return;
    }

    hr = mf_encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, /*param=*/0);
    if (FAILED(hr)) {
      OnError();
      return;
    }
  }

  if (state_ == EncoderState::kDraining) {
    // When draining, the encoder will produce output even if it has less than
    // `kMinSamplesForOutput` buffered. It will 0 pad what data it has so that
    // it can produce the final frame.
    if (samples_in_encoder_ > 0)
      TryProcessOutput(std::move(flush_cb));
    else
      std::move(flush_cb).Run();

    return;
  }

  // If `mf_encoder_` has enough samples buffered to produce another output
  // frame, we should continue to check for output. Since it is not ready to be
  // processed right now, we post a task to yield the thread to other work that
  // is ready.
  if (samples_in_encoder_ >= kMinSamplesForOutput) {
    if (state_ == EncoderState::kProcessing)
      state_ = EncoderState::kIdle;

    if (!have_queued_output_task_ || flush_cb) {
      have_queued_output_task_ = true;
      task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MFAudioEncoder::RunTryProcessOutput,
                         weak_ptr_factory_.GetWeakPtr(), std::move(flush_cb)));
    }

    return;
  }

  state_ = EncoderState::kIdle;
}

HRESULT MFAudioEncoder::ProcessOutput(EncodedAudioBuffer& encoded_audio) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get buffer requirements.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#get-buffer-requirements
  MFT_OUTPUT_STREAM_INFO output_stream_info = {};
  RETURN_IF_FAILED(
      mf_encoder_->GetOutputStreamInfo(kStreamId, &output_stream_info));

  // On the first run, `output_sample_` will be empty, but `GetSampleBuffer`
  // allocates it, if necessary, and (re)allocates the buffer if it is needed or
  // if it is too small.
  ComMFMediaBuffer output_buffer;
  RETURN_IF_FAILED(GetSampleBuffer(output_stream_info.cbSize,
                                   output_buffer_alignment_, output_sample_,
                                   output_buffer));

  MFT_OUTPUT_DATA_BUFFER output_data_container = {};
  output_data_container.pSample = output_sample_.Get();

  // Retrieve the output.
  // https://docs.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model#process-data
  DWORD status;
  HRESULT hr = mf_encoder_->ProcessOutput(/*flags=*/0, /*buffer_count=*/1,
                                          &output_data_container, &status);
  // Avoid logging "need more input" as an error, since this is expected
  // relatively frequently.
  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
    return hr;
  RETURN_IF_FAILED(hr);

  // `status` is only set if `ProcessOutput` returns
  // `MF_E_TRANSFORM_STREAM_CHANGE`.
  // https://docs.microsoft.com/en-us/windows/win32/api/mftransform/ne-mftransform-_mft_process_output_status#remarks
  CHECK_EQ(status, 0u);

  if (output_data_container.pEvents) {
    DVLOG(2) << "Got events from ProcessOutput, but discarding.";
    output_data_container.pEvents->Release();
  }

  DWORD total_length;
  RETURN_IF_FAILED(output_sample_->GetTotalLength(&total_length));

  // Copy the data from `output_buffer` into `encoded_data`.
  BYTE* output_buffer_ptr = nullptr;
  RETURN_IF_FAILED(output_buffer->Lock(&output_buffer_ptr, 0, 0));

  auto encoded_data =
      base::HeapArray<uint8_t>::CopiedFrom({output_buffer_ptr, total_length});
  RETURN_IF_FAILED(output_buffer->Unlock());

  LONGLONG sample_duration = 0;
  RETURN_IF_FAILED(output_sample_->GetSampleDuration(&sample_duration));
  base::TimeDelta duration = base::Nanoseconds(sample_duration * 100);

  // We use `output_timestamp_tracker_` instead of the timestamp from
  // `output_sample` since it is more accurate. The timestamp on the sample will
  // drift over time since it is calculated by summing the durations, which are
  // underestimated due to truncation.
  base::TimeTicks timestamp =
      output_timestamp_tracker_->GetTimestamp() + base::TimeTicks();
  output_timestamp_tracker_->AddFrames(kSamplesPerFrame);

  encoded_audio = EncodedAudioBuffer(audio_params_, std::move(encoded_data),
                                     timestamp, duration);
  return S_OK;
}

void MFAudioEncoder::OnFlushComplete(EncoderStatusCB done_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);

  if (state_ == EncoderState::kError) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedFlush);
    return;
  }

  DCHECK_EQ(state_, EncoderState::kDraining);
  DCHECK(input_queue_.empty());
  DCHECK_LE(samples_in_encoder_, 0);

  // Tell the encoder that the drain is complete. Without this, it will not
  // accept new input samples.
  HRESULT hr =
      mf_encoder_->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, /*param=*/0);
  if (FAILED(hr)) {
    std::move(done_cb).Run(EncoderStatus::Codes::kEncoderFailedFlush);
    return;
  }

  samples_in_encoder_ = 0;
  can_produce_output_ = false;
  can_flush_ = false;
  input_timestamp_tracker_->Reset();
  output_timestamp_tracker_->Reset();
  state_ = EncoderState::kIdle;

  if (!pending_inputs_.empty()) {
    for (auto& pending_data : pending_inputs_) {
      EnqueueInput(std::move(pending_data.audio_bus), pending_data.capture_time,
                   std::move(pending_data.done_cb));
    }
    pending_inputs_.clear();
  }

  std::move(done_cb).Run(EncoderStatus::Codes::kOk);
}

void MFAudioEncoder::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(initialized_);
  state_ = EncoderState::kError;

  // Notify the caller that we won't be encoding any queued inputs.
  while (!input_queue_.empty()) {
    InputData& input_data = input_queue_.front();
    std::move(input_data.done_cb)
        .Run(EncoderStatus::Codes::kEncoderFailedEncode);
    input_queue_.pop_front();
  }

  while (!pending_inputs_.empty()) {
    PendingData& pending_data = pending_inputs_.front();
    std::move(pending_data.done_cb)
        .Run(EncoderStatus::Codes::kEncoderFailedEncode);
    pending_inputs_.pop_front();
  }
}

// static.
uint32_t MFAudioEncoder::ClampAccCodecBitrate(uint32_t bitrate) {
  // 0 audio bitrate could mean multiple things such as no audio, use
  // default, etc. So, the client should handle the case by itself.
  CHECK_GT(bitrate, 0u);

  auto it = std::lower_bound(std::begin(kSupportedBitrates),
                             std::end(kSupportedBitrates), bitrate);
  if (it != std::end(kSupportedBitrates)) {
    return *it;
  }

  return kSupportedBitrates[sizeof(kSupportedBitrates) /
                                sizeof(kSupportedBitrates[0]) -
                            1];
}

}  // namespace media
