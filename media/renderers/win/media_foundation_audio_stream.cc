// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensures MFAudioFormat_Xxx symbols are defined in mfapi.h which is included
// by media_foundation_audio_stream.h.
#include <initguid.h>  // NOLINT(build/include_order)

#include "media/renderers/win/media_foundation_audio_stream.h"

#include <mferror.h>  // NOLINT(build/include_order)
#include <mmreg.h>    // NOLINT(build/include_order)
#include <wrl.h>      // NOLINT(build/include_order)

#include "base/win/scoped_co_mem.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// Given an audio format tag |wave_format|, it returns an audio subtype GUID per
// https://docs.microsoft.com/en-us/windows/win32/medfound/audio-subtype-guids
// |wave_format| must be one of the WAVE_FORMAT_* constants defined in mmreg.h.
GUID MediaFoundationSubTypeFromWaveFormat(uint32_t wave_format) {
  GUID format_base = MFAudioFormat_Base;
  format_base.Data1 = wave_format;
  return format_base;
}

GUID AudioCodecToMediaFoundationSubtype(AudioCodec codec) {
  DVLOG(1) << __func__ << ": codec=" << codec;

  switch (codec) {
    case AudioCodec::kAAC:
      return MFAudioFormat_AAC;
    case AudioCodec::kMP3:
      return MFAudioFormat_MP3;
    case AudioCodec::kPCM:
      return MFAudioFormat_PCM;
    case AudioCodec::kVorbis:
      return MFAudioFormat_Vorbis;
    case AudioCodec::kFLAC:
      return MFAudioFormat_FLAC;
    case AudioCodec::kAMR_NB:
      return MFAudioFormat_AMR_NB;
    case AudioCodec::kAMR_WB:
      return MFAudioFormat_AMR_WB;
    case AudioCodec::kPCM_MULAW:
      return MediaFoundationSubTypeFromWaveFormat(WAVE_FORMAT_MULAW);
    case AudioCodec::kGSM_MS:
      return MediaFoundationSubTypeFromWaveFormat(WAVE_FORMAT_GSM610);
    case AudioCodec::kPCM_S16BE:
      return MFAudioFormat_PCM;
    case AudioCodec::kPCM_S24BE:
      return MFAudioFormat_PCM;
    case AudioCodec::kOpus:
      return MFAudioFormat_Opus;
    case AudioCodec::kEAC3:
      return MFAudioFormat_Dolby_DDPlus;
    case AudioCodec::kPCM_ALAW:
      return MediaFoundationSubTypeFromWaveFormat(WAVE_FORMAT_ALAW);
    case AudioCodec::kALAC:
      return MFAudioFormat_ALAC;
    case AudioCodec::kAC3:
      return MFAudioFormat_Dolby_AC3;
    default:
      return GUID_NULL;
  }
}

bool IsUncompressedAudio(AudioCodec codec) {
  switch (codec) {
    case AudioCodec::kPCM:
    case AudioCodec::kPCM_S16BE:
    case AudioCodec::kPCM_S24BE:
      return true;
    default:
      return false;
  }
}

// Given an AudioDecoderConfig, get its corresponding IMFMediaType format.
// Note:
// IMFMediaType is derived from IMFAttributes and hence all the of information
// in a media type is store as attributes.
// https://docs.microsoft.com/en-us/windows/win32/medfound/media-type-attributes
// has a list of media type attributes.
HRESULT GetDefaultAudioType(const AudioDecoderConfig decoder_config,
                            IMFMediaType** media_type_out) {
  DVLOG(1) << __func__;

  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));

  GUID mf_subtype = AudioCodecToMediaFoundationSubtype(decoder_config.codec());
  if (mf_subtype == GUID_NULL) {
    DLOG(ERROR) << "Unsupported codec type: " << decoder_config.codec();
    return MF_E_TOPO_CODEC_NOT_FOUND;
  }
  RETURN_IF_FAILED(media_type->SetGUID(MF_MT_SUBTYPE, mf_subtype));

  bool uncompressed = IsUncompressedAudio(decoder_config.codec());

  if (uncompressed) {
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, 1));
  } else {
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_COMPRESSED, 1));
  }

  int channels = decoder_config.channels();
  if (channels > 0) {
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels));
  }

  int samples_per_second = decoder_config.samples_per_second();
  if (samples_per_second > 0) {
    RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,
                                           samples_per_second));
  }

  int bits_per_sample = decoder_config.bytes_per_frame() * 8;
  if (bits_per_sample > 0) {
    RETURN_IF_FAILED(
        media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bits_per_sample));
  }

  if (uncompressed) {
    unsigned long block_alignment = channels * (bits_per_sample / 8);
    if (block_alignment > 0) {
      RETURN_IF_FAILED(
          media_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, block_alignment));
    }
    unsigned long average_bps = samples_per_second * (bits_per_sample / 8);
    if (average_bps > 0) {
      RETURN_IF_FAILED(
          media_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, average_bps));
    }
  }
  *media_type_out = media_type.Detach();
  return S_OK;
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
HRESULT GetAacAudioType(const AudioDecoderConfig decoder_config,
                        IMFMediaType** media_type_out) {
  DVLOG(1) << __func__;

  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(GetDefaultAudioType(decoder_config, &media_type));

  // On Windows `extra_data` is not populated for AAC in `decoder_config`. Use
  // `aac_extra_data` instead. See crbug.com/1245123.
  const auto& extra_data = decoder_config.aac_extra_data();

  size_t wave_format_size = sizeof(HEAACWAVEINFO) + extra_data.size();
  std::vector<uint8_t> wave_format_buffer(wave_format_size);
  HEAACWAVEINFO* aac_wave_format =
      reinterpret_cast<HEAACWAVEINFO*>(wave_format_buffer.data());

  aac_wave_format->wfx.wFormatTag = WAVE_FORMAT_MPEG_HEAAC;
  aac_wave_format->wfx.nChannels = decoder_config.channels();
  aac_wave_format->wfx.wBitsPerSample = decoder_config.bytes_per_channel() * 8;
  aac_wave_format->wfx.nSamplesPerSec = decoder_config.samples_per_second();
  aac_wave_format->wfx.nAvgBytesPerSec =
      decoder_config.samples_per_second() * decoder_config.bytes_per_frame();
  aac_wave_format->wfx.nBlockAlign = 1;

  size_t extra_size = wave_format_size - sizeof(WAVEFORMATEX);
  aac_wave_format->wfx.cbSize = static_cast<WORD>(extra_size);
  aac_wave_format->wPayloadType = 0;  // RAW AAC
  aac_wave_format->wAudioProfileLevelIndication =
      0xFE;                          // no audio profile specified
  aac_wave_format->wStructType = 0;  // audio specific config follows
  aac_wave_format->wReserved1 = 0;
  aac_wave_format->dwReserved2 = 0;

  if (!extra_data.empty()) {
    memcpy(reinterpret_cast<uint8_t*>(aac_wave_format) + sizeof(HEAACWAVEINFO),
           extra_data.data(), extra_data.size());
  }

  RETURN_IF_FAILED(MFInitMediaTypeFromWaveFormatEx(
      media_type.Get(), reinterpret_cast<const WAVEFORMATEX*>(aac_wave_format),
      wave_format_size));
  *media_type_out = media_type.Detach();
  return S_OK;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace

/*static*/
HRESULT MediaFoundationAudioStream::Create(
    int stream_id,
    IMFMediaSource* parent_source,
    DemuxerStream* demuxer_stream,
    std::unique_ptr<MediaLog> media_log,
    MediaFoundationStreamWrapper** stream_out) {
  DVLOG(1) << __func__ << ": stream_id=" << stream_id;

  ComPtr<IMFMediaStream> audio_stream;
  AudioCodec codec = demuxer_stream->audio_decoder_config().codec();
  switch (codec) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    case AudioCodec::kAAC:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationAACAudioStream>(
          &audio_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
    default:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationAudioStream>(
          &audio_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
  }
  *stream_out =
      static_cast<MediaFoundationStreamWrapper*>(audio_stream.Detach());
  return S_OK;
}

bool MediaFoundationAudioStream::IsEncrypted() const {
  AudioDecoderConfig audio_config = demuxer_stream_->audio_decoder_config();
  return audio_config.is_encrypted();
}

HRESULT MediaFoundationAudioStream::GetMediaType(
    IMFMediaType** media_type_out) {
  AudioDecoderConfig decoder_config = demuxer_stream_->audio_decoder_config();
  return GetDefaultAudioType(decoder_config, media_type_out);
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
HRESULT MediaFoundationAACAudioStream::GetMediaType(
    IMFMediaType** media_type_out) {
  AudioDecoderConfig decoder_config = demuxer_stream_->audio_decoder_config();
  enable_adts_header_removal_ = !decoder_config.should_discard_decoder_delay();
  return GetAacAudioType(decoder_config, media_type_out);
}

// This detects whether the given |sample| has an ADTS header in the clear. If
// yes, it removes the ADTS header (and modifies the subsample mappings
// accordingly).
HRESULT MediaFoundationAACAudioStream::TransformSample(
    Microsoft::WRL::ComPtr<IMFSample>& sample) {
  DVLOG_FUNC(3);

  if (!enable_adts_header_removal_)
    return S_OK;

  ComPtr<IMFMediaBuffer> mf_buffer;
  ComPtr<IMFMediaBuffer> new_mf_buffer;
  DWORD buffer_count = 0;
  RETURN_IF_FAILED(sample->GetBufferCount(&buffer_count));
  if (buffer_count != 1) {
    DLOG(ERROR) << __func__ << ": buffer_count=" << buffer_count;
    return MF_E_UNEXPECTED;
  }
  RETURN_IF_FAILED(sample->GetBufferByIndex(0, &mf_buffer));
  BYTE* mf_buffer_data = nullptr;
  DWORD max_length = 0;
  DWORD current_length = 0;
  const int kADTSHeaderSize = 7;
  bool might_contain_adts_header = false;
  RETURN_IF_FAILED(
      mf_buffer->Lock(&mf_buffer_data, &max_length, &current_length));
  if (current_length >= kADTSHeaderSize && mf_buffer_data[0] == 0xff &&
      mf_buffer_data[1] == 0xf1 && mf_buffer_data[6] == 0xfc) {
    might_contain_adts_header = true;
  }
  RETURN_IF_FAILED(mf_buffer->Unlock());
  if (!might_contain_adts_header)
    return S_OK;

  bool contains_clear_adts_header = true;
  // If the stream is encrypted, ensure that the sub-sample mappings are
  // adjusted to account for the ADTS header removal.
  base::win::ScopedCoMem<MediaFoundationSubsampleEntry> subsample_mappings;
  uint32_t subsample_mappings_size = 0;
  // MFSampleExtension_Encryption_SubSample_Mapping is documented in "mfapi.h".
  // If the attribute doesn't exist, we should not fail the call.
  if (SUCCEEDED(sample->GetAllocatedBlob(
          MFSampleExtension_Encryption_SubSample_Mapping,
          reinterpret_cast<uint8_t**>(&subsample_mappings),
          &subsample_mappings_size)) &&
      subsample_mappings_size >= sizeof(MediaFoundationSubsampleEntry)) {
    uint32_t subsample_count =
        subsample_mappings_size / sizeof(MediaFoundationSubsampleEntry);
    if (subsample_count == 1 &&
        subsample_mappings.get()[0].clear_bytes == kADTSHeaderSize) {
      // When MFSampleExtension_Encryption_SubSample_Mapping attribute is
      // removed, the whole sample is considered as encrypted.
      RETURN_IF_FAILED(
          sample->DeleteItem(MFSampleExtension_Encryption_SubSample_Mapping));
    } else if (subsample_mappings.get()[0].clear_bytes >= kADTSHeaderSize) {
      subsample_mappings.get()[0].clear_bytes -= kADTSHeaderSize;
      RETURN_IF_FAILED(sample->SetBlob(
          MFSampleExtension_Encryption_SubSample_Mapping,
          reinterpret_cast<const uint8_t*>(subsample_mappings.get()),
          subsample_mappings_size));
    } else {
      // There is no ADTS header inserted by Demuxer, we don't need to remove
      // it.
      contains_clear_adts_header = false;
    }
  }
  if (contains_clear_adts_header) {
    // Remove ADTS header from |sample|.
    RETURN_IF_FAILED(MFCreateMediaBufferWrapper(
        mf_buffer.Get(), kADTSHeaderSize, current_length - kADTSHeaderSize,
        &new_mf_buffer));
    RETURN_IF_FAILED(sample->RemoveAllBuffers());
    RETURN_IF_FAILED(sample->AddBuffer(new_mf_buffer.Get()));
  }
  return S_OK;
}
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

}  // namespace media
