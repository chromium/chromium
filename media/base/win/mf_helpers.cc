// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/base/win/mf_helpers.h"

#include <initguid.h>

#include <d3d11.h>
#include <d3d11_4.h>
#include <ks.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mmreg.h>
#include <wrl.h>

#include "base/check_op.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/win/mf_helpers.h"
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#include "media/formats/mp4/ac4.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#include "gpu/ipc/common/dxgi_helpers.h"
#include "media/media_buildflags.h"
#include "third_party/libyuv/include/libyuv.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// This is supported by Media Foundation.
DEFINE_MEDIATYPE_GUID(MFVideoFormat_THEORA, FCC('theo'))

// ID3D11DeviceChild and ID3D11Device implement SetPrivateData with
// the exact same parameters.
template <typename T>
HRESULT SetDebugNameInternal(T* d3d11_object, const char* debug_string) {
  return d3d11_object->SetPrivateData(WKPDID_D3DDebugObjectName,
                                      strlen(debug_string), debug_string);
}

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
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    case AudioCodec::kAC4:
      return MFAudioFormat_Dolby_AC4;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
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

    if (decrypt_config.HasPattern()) {
      DVLOG(3) << __func__ << ": encryption_pattern="
               << decrypt_config.encryption_pattern().value();

      // Invalid if crypt == 0 and skip >= 0.
      CHECK(!(decrypt_config.encryption_pattern()->crypt_byte_block() == 0 &&
              decrypt_config.encryption_pattern()->skip_byte_block() > 0));

      // Crypt and skip byte blocks for the sample-based protection pattern need
      // be set if the protection scheme is `cbcs`. No need to set for 10:0,
      // 1:0, or 0:0 patterns since Media Foundation Media Engine treats them as
      // `cbc1` always but it won't reset IV between subsamples (which means IV
      // to be restored to the constant IV after each subsample). Trying to set
      // crypt to non-zero and skip to 0 will cause an error:
      // - If either of these attributes are not present or have a value of 0,
      // the sample is `cbc1`. See
      // https://learn.microsoft.com/en-us/windows/win32/medfound/mfsampleextension-encryption-cryptbyteblock
      // and
      // https://learn.microsoft.com/en-us/windows/win32/medfound/mfsampleextension-encryption-skipbyteblock
      // - If `cBlocksStripeEncrypted` is 0, `cBlocksStripeClear` must be also
      // 0. See
      // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d10umddi/ns-d3d10umddi-d3dwddm2_4ddi_video_decoder_buffer_desc
      if (decrypt_config.encryption_pattern()->skip_byte_block() > 0) {
        RETURN_IF_FAILED(mf_sample->SetUINT32(
            MFSampleExtension_Encryption_CryptByteBlock,
            decrypt_config.encryption_pattern()->crypt_byte_block()));
        RETURN_IF_FAILED(mf_sample->SetUINT32(
            MFSampleExtension_Encryption_SkipByteBlock,
            decrypt_config.encryption_pattern()->skip_byte_block()));
      }
    }
  } else {
    NOTREACHED_IN_MIGRATION() << "Unexpected encryption scheme";
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

Microsoft::WRL::ComPtr<IMFSample> CreateEmptySampleWithBuffer(
    uint32_t buffer_length,
    int align) {
  CHECK_GT(buffer_length, 0U);

  Microsoft::WRL::ComPtr<IMFSample> sample;
  HRESULT hr = MFCreateSample(&sample);
  RETURN_ON_HR_FAILURE(hr, "MFCreateSample failed",
                       Microsoft::WRL::ComPtr<IMFSample>());

  Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
  if (align == 0) {
    // Note that MFCreateMemoryBuffer is same as MFCreateAlignedMemoryBuffer
    // with the align argument being 0.
    hr = MFCreateMemoryBuffer(buffer_length, &buffer);
  } else {
    hr = MFCreateAlignedMemoryBuffer(buffer_length, align - 1, &buffer);
  }
  RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer for sample",
                       Microsoft::WRL::ComPtr<IMFSample>());

  hr = sample->AddBuffer(buffer.Get());
  RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample",
                       Microsoft::WRL::ComPtr<IMFSample>());

  buffer->SetCurrentLength(0);
  return sample;
}

MediaBufferScopedPointer::MediaBufferScopedPointer(IMFMediaBuffer* media_buffer)
    : media_buffer_(media_buffer),
      buffer_(nullptr),
      max_length_(0),
      current_length_(0) {
  HRESULT hr = media_buffer_->Lock(&buffer_, &max_length_, &current_length_);
  CHECK(SUCCEEDED(hr));
}

MediaBufferScopedPointer::~MediaBufferScopedPointer() {
  HRESULT hr = media_buffer_->Unlock();
  CHECK(SUCCEEDED(hr));
}

HRESULT CopyCoTaskMemWideString(LPCWSTR in_string, LPWSTR* out_string) {
  if (!in_string || !out_string) {
    return E_INVALIDARG;
  }

  size_t size = (wcslen(in_string) + 1) * sizeof(wchar_t);
  LPWSTR copy = reinterpret_cast<LPWSTR>(CoTaskMemAlloc(size));
  if (!copy)
    return E_OUTOFMEMORY;

  wcscpy(copy, in_string);
  *out_string = copy;
  return S_OK;
}

HRESULT SetDebugName(ID3D11DeviceChild* d3d11_device_child,
                     const char* debug_string) {
  return SetDebugNameInternal(d3d11_device_child, debug_string);
}

HRESULT SetDebugName(ID3D11Device* d3d11_device, const char* debug_string) {
  return SetDebugNameInternal(d3d11_device, debug_string);
}

ChannelLayout ChannelConfigToChannelLayout(ChannelConfig config) {
  switch (config) {
    case KSAUDIO_SPEAKER_MONO:
      return CHANNEL_LAYOUT_MONO;
    case KSAUDIO_SPEAKER_STEREO:
      return CHANNEL_LAYOUT_STEREO;
    case KSAUDIO_SPEAKER_2POINT1:
      return CHANNEL_LAYOUT_2POINT1;
    case KSAUDIO_SPEAKER_3POINT0:
      return CHANNEL_LAYOUT_SURROUND;
    case KSAUDIO_SPEAKER_3POINT1:
      return CHANNEL_LAYOUT_3_1;
    case KSAUDIO_SPEAKER_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT0:
      return CHANNEL_LAYOUT_5_0;
    case KSAUDIO_SPEAKER_5POINT1:
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT0:
      return CHANNEL_LAYOUT_7_0;
    case KSAUDIO_SPEAKER_7POINT1:
      return CHANNEL_LAYOUT_7_1_WIDE_BACK;
    case KSAUDIO_SPEAKER_7POINT1_SURROUND:
      return CHANNEL_LAYOUT_7_1;
    case KSAUDIO_SPEAKER_DIRECTOUT:
      // When specifying the wave format for a direct-out stream, an application
      // should set the dwChannelMask member of the WAVEFORMATEXTENSIBLE
      // structure to the value KSAUDIO_SPEAKER_DIRECTOUT, which is zero.
      // A channel mask of zero indicates that no speaker positions are defined.
      // As always, the number of channels in the stream is specified in the
      // Format.nChannels member.
      return CHANNEL_LAYOUT_DISCRETE;
    default:
      DVLOG(2) << "Unsupported channel configuration: " << config;
      return CHANNEL_LAYOUT_UNSUPPORTED;
  }
}

// GUID is little endian. The byte array in network order is big endian.
std::vector<uint8_t> ByteArrayFromGUID(REFGUID guid) {
  std::vector<uint8_t> byte_array(sizeof(GUID));
  GUID* reversed_guid = reinterpret_cast<GUID*>(byte_array.data());
  *reversed_guid = guid;
  reversed_guid->Data1 = _byteswap_ulong(guid.Data1);
  reversed_guid->Data2 = _byteswap_ushort(guid.Data2);
  reversed_guid->Data3 = _byteswap_ushort(guid.Data3);
  // Data4 is already a byte array so no need to byte swap.
  return byte_array;
}

// |guid_string| is a binary serialization of a GUID in network byte order
// format.
GUID GetGUIDFromString(const std::string& guid_string) {
  DCHECK_EQ(guid_string.length(), sizeof(GUID));

  GUID reversed_guid =
      *(reinterpret_cast<UNALIGNED const GUID*>(guid_string.c_str()));
  reversed_guid.Data1 = _byteswap_ulong(reversed_guid.Data1);
  reversed_guid.Data2 = _byteswap_ushort(reversed_guid.Data2);
  reversed_guid.Data3 = _byteswap_ushort(reversed_guid.Data3);
  // Data4 is already a byte array so no need to byte swap.
  return reversed_guid;
}

std::string GetStringFromGUID(REFGUID guid) {
  GUID guid_tmp = guid;
  guid_tmp.Data1 = _byteswap_ulong(guid_tmp.Data1);
  guid_tmp.Data2 = _byteswap_ushort(guid_tmp.Data2);
  guid_tmp.Data3 = _byteswap_ushort(guid_tmp.Data3);

  return std::string(reinterpret_cast<char*>(&guid_tmp), sizeof(GUID));
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

  int bits_per_sample = decoder_config.bytes_per_channel() * 8;
  if (bits_per_sample > 0) {
    RETURN_IF_FAILED(
        media_type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, bits_per_sample));
  }

  if (uncompressed) {
    unsigned long block_alignment = decoder_config.bytes_per_frame();
    if (block_alignment > 0) {
      RETURN_IF_FAILED(
          media_type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, block_alignment));
    }
    unsigned long average_bps = samples_per_second * block_alignment;
    if (average_bps > 0) {
      RETURN_IF_FAILED(
          media_type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, average_bps));
    }
  }
  *media_type_out = media_type.Detach();
  return S_OK;
}

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
HRESULT GetAacAudioType(const AudioDecoderConfig& decoder_config,
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

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
// An attribute defined to indicate if the input audio is already
// previrtualized. Now it is used to indicate if the input stream is a Dolby AC4
// IMS stream. That information will be used by Dolby AC4 MFT to create correct
// output media types.
// GUID: {4EACAB51-FFE5-421A-A2A7-8B7409A1CAC4}
// Type: UINT32(BOOL)
DEFINE_GUID(MF_MT_SPATIAL_AUDIO_IS_PREVIRTUALIZED,
            0x4eacab51,
            0xffe5,
            0x421a,
            0xa2,
            0xa7,
            0x8b,
            0x74,
            0x09,
            0xa1,
            0xca,
            0xc4);

HRESULT GetAC4AudioType(const AudioDecoderConfig& decoder_config,
                        IMFMediaType** media_type_out) {
  RETURN_IF_FAILED(GetDefaultAudioType(decoder_config, media_type_out));
  if (decoder_config.extra_data().size() != sizeof(media::mp4::AC4StreamInfo)) {
    return MF_E_INVALIDMEDIATYPE;
  }
  auto* media_type = *media_type_out;
  auto stream_info = *reinterpret_cast<const media::mp4::AC4StreamInfo*>(
      decoder_config.extra_data().data());
  RETURN_IF_FAILED(
      media_type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, stream_info.channels));
  RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_SPATIAL_AUDIO_IS_PREVIRTUALIZED,
                                         stream_info.is_ims));
  RETURN_IF_FAILED(media_type->SetUINT32(MF_MT_SPATIAL_AUDIO_DATA_PRESENT,
                                         stream_info.is_ajoc));
  return S_OK;
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

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

GUID VideoPixelFormatToMFSubtype(VideoPixelFormat video_pixel_format) {
  switch (video_pixel_format) {
    case VideoPixelFormat::PIXEL_FORMAT_I420:
      return MFVideoFormat_I420;
    case VideoPixelFormat::PIXEL_FORMAT_YV12:
      return MFVideoFormat_YV12;
    case VideoPixelFormat::PIXEL_FORMAT_NV12:
      return MFVideoFormat_NV12;
    case VideoPixelFormat::PIXEL_FORMAT_NV21:
      return MFVideoFormat_NV21;
    case VideoPixelFormat::PIXEL_FORMAT_YUY2:
      return MFVideoFormat_YUY2;
    case VideoPixelFormat::PIXEL_FORMAT_ARGB:
      return MFVideoFormat_ARGB32;
    case VideoPixelFormat::PIXEL_FORMAT_XRGB:
      return MFVideoFormat_RGB32;
    case VideoPixelFormat::PIXEL_FORMAT_RGB24:
      return MFVideoFormat_RGB24;
    default:
      return GUID_NULL;
  }
}

MFVideoPrimaries VideoPrimariesToMFVideoPrimaries(
    gfx::ColorSpace::PrimaryID primaries) {
  switch (primaries) {
    case gfx::ColorSpace::PrimaryID::BT709:
      return MFVideoPrimaries_BT709;
    case gfx::ColorSpace::PrimaryID::BT470M:
      return MFVideoPrimaries_BT470_2_SysM;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      return MFVideoPrimaries_BT470_2_SysBG;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      return MFVideoPrimaries_SMPTE170M;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      return MFVideoPrimaries_SMPTE240M;
    case gfx::ColorSpace::PrimaryID::BT2020:
      return MFVideoPrimaries_BT2020;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      return MFVideoPrimaries_EBU3213;
    default:
      return MFVideoPrimaries_Unknown;
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

  DVLOG(3) << __func__ << ": buffer->duration()=" << buffer->duration()
           << ", buffer->timestamp()=" << buffer->timestamp();
  MFTIME sample_duration = TimeDeltaToMfTime(buffer->duration());
  RETURN_IF_FAILED(mf_sample->SetSampleDuration(sample_duration));

  MFTIME sample_time = TimeDeltaToMfTime(buffer->timestamp());
  RETURN_IF_FAILED(mf_sample->SetSampleTime(sample_time));

  ComPtr<IMFMediaBuffer> mf_buffer;
  size_t data_size = buffer->size();
  RETURN_IF_FAILED(MFCreateMemoryBuffer(buffer->size(), &mf_buffer));

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
        DVLOG(3) << __func__ << ": subsample_mappings[" << i
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
  DVLOG(3) << __func__ << ": key_id_string=" << key_id_string
           << ", iv_string=" << iv_string
           << ", iv_string.size()=" << iv_string.size();

  if (encryption_scheme == EncryptionScheme::kCenc) {
    *decrypt_config =
        DecryptConfig::CreateCencConfig(key_id_string, iv_string, subsamples);
  } else {
    EncryptionPattern encryption_pattern;

    // Try to get crypt and skip byte blocks for pattern encryption. Use the
    // values only if both `MFSampleExtension_Encryption_CryptByteBlock` and
    // `MFSampleExtension_Encryption_SkipByteBlock` are present. Otherwise,
    // assume both are zeros.
    UINT32 crypt_byte_block = 0;
    UINT32 skip_byte_block = 0;
    if (SUCCEEDED(mf_sample->GetUINT32(
            MFSampleExtension_Encryption_CryptByteBlock, &crypt_byte_block)) &&
        SUCCEEDED(mf_sample->GetUINT32(
            MFSampleExtension_Encryption_SkipByteBlock, &skip_byte_block))) {
      encryption_pattern = EncryptionPattern(crypt_byte_block, skip_byte_block);
    }

    DVLOG(3) << __func__ << ": encryption_pattern=" << encryption_pattern;
    *decrypt_config = DecryptConfig::CreateCbcsConfig(
        key_id_string, iv_string, subsamples, encryption_pattern);
  }

  return S_OK;
}

HRESULT GenerateSampleFromVideoFrame(
    const VideoFrame* frame,
    DXGIDeviceManager* dxgi_device_manager,
    bool use_dxgi_buffer,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture,
    DWORD buffer_alignment,
    IMFSample** sample_out) {
  constexpr size_t kOneMicrosecondInMFSampleTimeUnits = 10;

  HRESULT hr;
  Microsoft::WRL::ComPtr<IMFSample> sample;
  hr = MFCreateSample(&sample);
  RETURN_ON_HR_FAILURE(hr, "Failed to create sample", hr);

  if (frame->storage_type() ==
          VideoFrame::StorageType::STORAGE_GPU_MEMORY_BUFFER &&
      dxgi_device_manager != nullptr) {
    gfx::GpuMemoryBufferHandle buffer_handle =
        frame->GetGpuMemoryBufferHandle();
    if (buffer_handle.is_null()) {
      LOG(ERROR) << "Failed to get GMB for input frame";
      return MF_E_INVALID_STREAM_DATA;
    }

    CHECK_EQ(buffer_handle.type, gfx::GpuMemoryBufferType::DXGI_SHARED_HANDLE);

    auto d3d_device = dxgi_device_manager->GetDevice();
    if (!d3d_device) {
      LOG(ERROR) << "Failed to get device from MF DXGI device manager";
      return E_HANDLE;
    }

    Microsoft::WRL::ComPtr<ID3D11Device1> device1;
    hr = d3d_device.As(&device1);
    RETURN_ON_HR_FAILURE(hr, "Failed to query ID3D11Device1", hr);

    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
    hr = device1->OpenSharedResource1(buffer_handle.dxgi_handle.Get(),
                                      IID_PPV_ARGS(&input_texture));
    RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);

    if (use_dxgi_buffer) {
      Microsoft::WRL::ComPtr<IMFMediaBuffer> mf_buffer;
      hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                     input_texture.Get(), 0, FALSE, &mf_buffer);
      RETURN_ON_HR_FAILURE(hr, "Failed to create MF DXGI surface buffer", hr);

      DWORD buffer_length = 0;
      hr = mf_buffer->GetMaxLength(&buffer_length);
      RETURN_ON_HR_FAILURE(hr, "Failed to get max buffer length", hr);
      hr = mf_buffer->SetCurrentLength(buffer_length);
      RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);

      RETURN_ON_HR_FAILURE(hr, "Failed to create sample", hr);
      hr = sample->AddBuffer(mf_buffer.Get());
      RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
      hr = sample->SetSampleTime(frame->timestamp().InMicroseconds() *
                                 kOneMicrosecondInMFSampleTimeUnits);
      RETURN_ON_HR_FAILURE(hr, "Failed to set sample timestamp", hr);
    } else {
      Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
      size_t allocation_size =
          VideoFrame::AllocationSize(frame->format(), frame->coded_size());
      hr = MFCreateAlignedMemoryBuffer(
          allocation_size,
          buffer_alignment == 0 ? buffer_alignment : buffer_alignment - 1,
          &input_buffer);
      RETURN_ON_HR_FAILURE(
          hr, "Failed to create memory buffer for input sample", hr);

      MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
      bool copy_succeeded = gpu::CopyD3D11TexToMem(
          input_texture.Get(), scoped_buffer.get(), scoped_buffer.max_length(),
          d3d_device.Get(), staging_texture);
      if (!copy_succeeded) {
        LOG(ERROR) << "Failed to copy sample to memory.";
        return E_FAIL;
      }

      size_t copied_bytes = frame->visible_rect().width() *
                            frame->visible_rect().height() * 3 / 2;
      hr = input_buffer->SetCurrentLength(copied_bytes);
      RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);
      hr = sample->AddBuffer(input_buffer.Get());
      RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
    }
  } else if (frame->HasSharedImage()) {
    // TODO(crbug.com/40162806): Handle non-GMB textures. This needs access to
    // SharedImageManager.
    return E_UNEXPECTED;
  } else {
    size_t allocation_size = VideoFrame::AllocationSize(
        frame->format(), frame->visible_rect().size());
    Microsoft::WRL::ComPtr<IMFMediaBuffer> input_buffer;
    hr = MFCreateAlignedMemoryBuffer(
        allocation_size,
        buffer_alignment == 0 ? buffer_alignment : buffer_alignment - 1,
        &input_buffer);
    RETURN_ON_HR_FAILURE(hr, "Failed to create memory buffer", hr);
    MediaBufferScopedPointer scoped_buffer(input_buffer.Get());
    size_t buffer_offset = 0;
    for (size_t i = 0; i < VideoFrame::NumPlanes(frame->format()); i++) {
      // |width| in libyuv::CopyPlane() is in bytes, not pixels.
      gfx::Size plane_size = VideoFrame::PlaneSize(
          frame->format(), i, frame->visible_rect().size());
      libyuv::CopyPlane(frame->visible_data(i),
                        frame->layout().planes()[i].stride,
                        scoped_buffer.get() + buffer_offset,
                        frame->layout().planes()[i].stride, plane_size.width(),
                        plane_size.height());
      buffer_offset +=
          plane_size.height() *
          VideoFrame::RowBytes(i, frame->format(), plane_size.width());
    }
    hr = input_buffer->SetCurrentLength(allocation_size);
    RETURN_ON_HR_FAILURE(hr, "Failed to set current buffer length", hr);
    hr = sample->AddBuffer(input_buffer.Get());
    RETURN_ON_HR_FAILURE(hr, "Failed to add buffer to sample", hr);
  }

  hr = sample->SetSampleTime(frame->timestamp().InMicroseconds() *
                             kOneMicrosecondInMFSampleTimeUnits);
  RETURN_ON_HR_FAILURE(hr, "Failed to set sample timestamp", hr);

  if (frame->ColorSpace().GetPrimaryID() !=
      gfx::ColorSpace::PrimaryID::INVALID) {
    hr = sample->SetUINT32(
        MF_MT_VIDEO_PRIMARIES,
        VideoPrimariesToMFVideoPrimaries(frame->ColorSpace().GetPrimaryID()));
  }

  *sample_out = sample.Detach();

  return S_OK;
}

}  // namespace media
