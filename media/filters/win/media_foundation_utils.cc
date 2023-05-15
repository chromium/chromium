// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ensures MFAudioFormat_Xxx symbols are defined in mfapi.h which is included
// by media_foundation_audio_stream.h.
#include <initguid.h>  // NOLINT(build/include_order)

#include "media/filters/win/media_foundation_utils.h"

#include <mfapi.h>
#include <mferror.h>  // NOLINT(build/include_order)
#include <mfidl.h>
#include <mmreg.h>  // NOLINT(build/include_order)
#include <wrl.h>    // NOLINT(build/include_order)

#include "base/win/scoped_co_mem.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/win/mf_helpers.h"
#include "media/media_buildflags.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// This is supported by Media Foundation.
DEFINE_MEDIATYPE_GUID(MFVideoFormat_THEORA, FCC('theo'))

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
#if BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
    case AudioCodec::kDTS:
    case AudioCodec::kDTSE:
      return MFAudioFormat_DTS_RAW;
    case AudioCodec::kDTSXP2:
      return MFAudioFormat_DTS_UHD;
#endif  // BUILDFLAG(ENABLE_PLATFORM_DTS_AUDIO)
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

bool AreLowIVBytesZero(const std::string& iv) {
  if (iv.length() != 16) {
    return false;
  }

  for (size_t i = 8; i < iv.length(); i++) {
    if (iv[i] != '\0') {
      return false;
    }
  }
  return true;
}

// Add encryption related attributes to |mf_sample| and update |last_key_id|.
HRESULT AddEncryptAttributes(const DecryptConfig& decrypt_config,
                             IMFSample* mf_sample,
                             GUID* last_key_id) {
  DVLOG(3) << __func__;

  MFSampleEncryptionProtectionScheme mf_protection_scheme;
  if (decrypt_config.encryption_scheme() == EncryptionScheme::kCenc) {
    mf_protection_scheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR;
  } else if (decrypt_config.encryption_scheme() == EncryptionScheme::kCbcs) {
    mf_protection_scheme = MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CBC;
  } else {
    NOTREACHED() << "Unexpected encryption scheme";
    return MF_E_UNEXPECTED;
  }
  RETURN_IF_FAILED(mf_sample->SetUINT32(
      MFSampleExtension_Encryption_ProtectionScheme, mf_protection_scheme));

  // KID
  // https://matroska.org/technical/specs/index.html#ContentEncKeyID
  // For WebM case, key ID size is not specified.
  if (decrypt_config.key_id().length() != sizeof(GUID)) {
    DLOG(ERROR) << __func__ << ": Unsupported key ID size";
    return MF_E_UNEXPECTED;
  }
  GUID key_id = GetGUIDFromString(decrypt_config.key_id());
  RETURN_IF_FAILED(mf_sample->SetGUID(MFSampleExtension_Content_KeyID, key_id));
  *last_key_id = key_id;

  // IV
  size_t iv_length = decrypt_config.iv().length();
  DCHECK(iv_length == 16);
  // For cases where a 16-byte IV is specified, but the low 8-bytes are all
  // 0, ensure that a 8-byte IV is set (this allows HWDRM to work on
  // hardware / drivers which don't support CTR decryption with 16-byte IVs)
  if (AreLowIVBytesZero(decrypt_config.iv())) {
    iv_length = 8;
  }
  RETURN_IF_FAILED(mf_sample->SetBlob(
      MFSampleExtension_Encryption_SampleID,
      reinterpret_cast<const uint8_t*>(decrypt_config.iv().c_str()),
      iv_length));

  // Handle subsample entries.
  const auto& subsample_entries = decrypt_config.subsamples();
  if (subsample_entries.empty()) {
    return S_OK;
  }

  std::vector<MediaFoundationSubsampleEntry> mf_subsample_entries(
      subsample_entries.size());
  for (size_t i = 0; i < subsample_entries.size(); i++) {
    mf_subsample_entries[i] =
        MediaFoundationSubsampleEntry(subsample_entries[i]);
  }
  const uint32_t mf_sample_entries_size =
      sizeof(MediaFoundationSubsampleEntry) * mf_subsample_entries.size();
  RETURN_IF_FAILED(mf_sample->SetBlob(
      MFSampleExtension_Encryption_SubSample_Mapping,
      reinterpret_cast<const uint8_t*>(mf_subsample_entries.data()),
      mf_sample_entries_size));

  return S_OK;
}

}  // namespace

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

// MFTIME defines units of 100 nanoseconds.
MFTIME TimeDeltaToMfTime(base::TimeDelta time) {
  return time.InNanoseconds() / 100;
}

base::TimeDelta MfTimeToTimeDelta(MFTIME mf_time) {
  return base::Nanoseconds(mf_time * 100);
}

GUID VideoCodecToMFSubtype(VideoCodec codec, VideoCodecProfile profile) {
  switch (codec) {
    case VideoCodec::kH264:
      return MFVideoFormat_H264;
    case VideoCodec::kVC1:
      return MFVideoFormat_WVC1;
    case VideoCodec::kMPEG2:
      return MFVideoFormat_MPEG2;
    case VideoCodec::kMPEG4:
      return MFVideoFormat_MP4V;
    case VideoCodec::kTheora:
      return MFVideoFormat_THEORA;
    case VideoCodec::kVP8:
      return MFVideoFormat_VP80;
    case VideoCodec::kVP9:
      return MFVideoFormat_VP90;
    case VideoCodec::kHEVC:
      return MFVideoFormat_HEVC;
    case VideoCodec::kDolbyVision:
      if (profile == VideoCodecProfile::DOLBYVISION_PROFILE0 ||
          profile == VideoCodecProfile::DOLBYVISION_PROFILE9) {
        return MFVideoFormat_H264;
      } else {
        return MFVideoFormat_HEVC;
      }
    case VideoCodec::kAV1:
      return MFVideoFormat_AV1;
    default:
      return GUID_NULL;
  }
}

HRESULT GenerateSampleFromDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& buffer,
    IMFSample** sample_out,
    GUID* last_key_id,
    TransformSampleCB transform_sample_cb) {
  DVLOG(3) << __func__;

  ComPtr<IMFSample> mf_sample;
  RETURN_IF_FAILED(MFCreateSample(&mf_sample));

  if (buffer->is_key_frame()) {
    RETURN_IF_FAILED(mf_sample->SetUINT32(MFSampleExtension_CleanPoint, 1));
  }

  DVLOG(3) << __func__ << "buffer->duration()=" << buffer->duration()
           << ", buffer->timestamp()=" << buffer->timestamp();
  MFTIME sample_duration = TimeDeltaToMfTime(buffer->duration());
  RETURN_IF_FAILED(mf_sample->SetSampleDuration(sample_duration));

  MFTIME sample_time = TimeDeltaToMfTime(buffer->timestamp());
  RETURN_IF_FAILED(mf_sample->SetSampleTime(sample_time));

  ComPtr<IMFMediaBuffer> mf_buffer;
  size_t data_size = buffer->data_size();
  RETURN_IF_FAILED(MFCreateMemoryBuffer(buffer->data_size(), &mf_buffer));

  BYTE* mf_buffer_data = nullptr;
  DWORD max_length = 0;
  RETURN_IF_FAILED(mf_buffer->Lock(&mf_buffer_data, &max_length, 0));
  memcpy(mf_buffer_data, buffer->data(), data_size);
  RETURN_IF_FAILED(mf_buffer->SetCurrentLength(data_size));
  RETURN_IF_FAILED(mf_buffer->Unlock());

  RETURN_IF_FAILED(mf_sample->AddBuffer(mf_buffer.Get()));

  if (buffer->decrypt_config()) {
    RETURN_IF_FAILED(AddEncryptAttributes(*(buffer->decrypt_config()),
                                          mf_sample.Get(), last_key_id));
  }

  if (transform_sample_cb) {
    RETURN_IF_FAILED(std::move(transform_sample_cb).Run(mf_sample));
  }

  *sample_out = mf_sample.Detach();

  return S_OK;
}

HRESULT CreateDecryptConfigFromSample(
    IMFSample* mf_sample,
    const GUID& key_id,
    std::unique_ptr<DecryptConfig>* decrypt_config) {
  DVLOG(3) << __func__;
  CHECK(mf_sample);

  EncryptionScheme encryption_scheme = EncryptionScheme::kUnencrypted;
  UINT32 mf_protection_scheme = 0;
  RETURN_IF_FAILED(mf_sample->GetUINT32(
      MFSampleExtension_Encryption_ProtectionScheme, &mf_protection_scheme));
  switch (mf_protection_scheme) {
    case MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CTR:
      encryption_scheme = EncryptionScheme::kCenc;
      break;
    case MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_AES_CBC:
      encryption_scheme = EncryptionScheme::kCbcs;
      break;
    case MFSampleEncryptionProtectionScheme::
        MF_SAMPLE_ENCRYPTION_PROTECTION_SCHEME_NONE:
      DLOG(ERROR) << __func__
                  << ": Unexpected encryption scheme: mf_protection_scheme="
                  << mf_protection_scheme;
      return MF_E_UNEXPECTED;
  }

  // IV
  UINT32 iv_length = 0;
  base::win::ScopedCoMem<BYTE> iv;
  RETURN_IF_FAILED(mf_sample->GetBlobSize(MFSampleExtension_Encryption_SampleID,
                                          &iv_length));
  if (iv_length == 8) {
    // A 8-byte IV is set (this allows hardware decryption to work on hardware
    // / drivers which don't support CTR decryption with 16-byte IVs). For
    // DecryptBuffer, a 16-byte IV should be specified, but ensure the low
    // 8-bytes are all 0.
    iv_length = 16;
  }
  iv.Reset(reinterpret_cast<BYTE*>(CoTaskMemAlloc(iv_length * sizeof(BYTE))));
  if (!iv) {
    return E_OUTOFMEMORY;
  }
  ZeroMemory(iv, iv_length * sizeof(BYTE));
  RETURN_IF_FAILED(mf_sample->GetBlob(MFSampleExtension_Encryption_SampleID, iv,
                                      iv_length, nullptr));

  // Subsample entries
  std::vector<media::SubsampleEntry> subsamples;
  base::win::ScopedCoMem<MediaFoundationSubsampleEntry> subsample_mappings;
  uint32_t subsample_mappings_size = 0;

  // If `MFSampleExtension_Encryption_SubSample_Mapping` attribute doesn't
  // exist, we should not fail the call. i.e., Encrypted audio content.
  if (SUCCEEDED(mf_sample->GetAllocatedBlob(
          MFSampleExtension_Encryption_SubSample_Mapping,
          reinterpret_cast<uint8_t**>(&subsample_mappings),
          &subsample_mappings_size))) {
    if (subsample_mappings_size >= sizeof(MediaFoundationSubsampleEntry)) {
      uint32_t subsample_count =
          subsample_mappings_size / sizeof(MediaFoundationSubsampleEntry);
      for (uint32_t i = 0; i < subsample_count; ++i) {
        DVLOG(3) << __func__ << "subsample_mappings[" << i
                 << "].clear_bytes=" << subsample_mappings[i].clear_bytes
                 << ", cipher_bytes=" << subsample_mappings[i].cipher_bytes;
        subsamples.emplace_back(subsample_mappings[i].clear_bytes,
                                subsample_mappings[i].cipher_bytes);
      }
    }
  }

  // Key ID
  const auto key_id_string = GetStringFromGUID(key_id);
  const auto iv_string = std::string(iv.get(), iv.get() + iv_length);
  DVLOG(3) << __func__ << "key_id_string=" << key_id_string
           << ", iv_string=" << iv_string
           << ", iv_string.size()=" << iv_string.size();

  if (encryption_scheme == EncryptionScheme::kCenc) {
    *decrypt_config =
        DecryptConfig::CreateCencConfig(key_id_string, iv_string, subsamples);
  } else {
    *decrypt_config = DecryptConfig::CreateCbcsConfig(
        key_id_string, iv_string, subsamples, EncryptionPattern());
  }

  return S_OK;
}

}  // namespace media
