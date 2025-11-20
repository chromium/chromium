// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

#include <algorithm>
#include <string_view>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_writer.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/ipc/common/dxgi_helpers.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/win/mf_helpers.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/media_buildflags.h"
#include "third_party/libyuv/include/libyuv.h"

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
#include "media/formats/mp4/ac4.h"
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)

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
    NOTREACHED() << "Unexpected encryption scheme";
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
    : media_buffer_(media_buffer) {
  uint8_t* buffer;
  DWORD max_length;

  HRESULT hr = media_buffer_->Lock(&buffer, &max_length, nullptr);
  CHECK(SUCCEEDED(hr));

  // SAFETY: `IMFMediaBuffer::Lock` docs states that `max_length` is the maximum
  // amount of data that can be written to the buffer.
  data_ = UNSAFE_BUFFERS(base::raw_span<uint8_t>(buffer, max_length));
}

MediaBufferScopedPointer::~MediaBufferScopedPointer() {
  data_ = {};
  HRESULT hr = media_buffer_->Unlock();
  CHECK(SUCCEEDED(hr));
}

HRESULT CopyCoTaskMemWideString(LPCWSTR in_string, LPWSTR* out_string) {
  if (!in_string || !out_string) {
    return E_INVALIDARG;
  }

  std::wstring_view input_str_view = in_string;
  // `wstring_view::size()` doesn't count the null-terminator.
  const size_t size_in_chars = input_str_view.size() + 1;
  const size_t size = size_in_chars * sizeof(wchar_t);
  LPWSTR copy = reinterpret_cast<LPWSTR>(CoTaskMemAlloc(size));
  if (!copy) {
    return E_OUTOFMEMORY;
  }

  // SAFETY: We've just allocated sufficient memory with `CoTaskMemAlloc`
  // to fit `size_in_chars` characters.
  auto out_span = UNSAFE_BUFFERS(base::span(copy, size_in_chars));

  // Put a null-terminator at the end of the string.
  *out_span.rbegin() = 0;
  // Copy the real characters.
  out_span.copy_prefix_from(base::span(input_str_view));

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
  auto writer = base::SpanWriter(base::span(byte_array));
  CHECK(writer.WriteU32BigEndian(guid.Data1));
  CHECK(writer.WriteU16BigEndian(guid.Data2));
  CHECK(writer.WriteU16BigEndian(guid.Data3));
  // Data4 is already a byte array so no need to byte swap.
  writer.remaining_span().copy_from(base::span(guid.Data4));
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

  const auto& extra_data = decoder_config.extra_data();

  size_t wave_format_size = sizeof(HEAACWAVEINFO) + extra_data.size();
  std::vector<uint8_t> wave_format_buffer(wave_format_size);
  // TODO(crbug.com/40285824): Spanify this usage. This is somewhat iffy from
  // alignment point of view.
  HEAACWAVEINFO* aac_wave_format =
      UNSAFE_TODO(reinterpret_cast<HEAACWAVEINFO*>(wave_format_buffer.data()));

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
    base::span(wave_format_buffer)
        .subspan(sizeof(HEAACWAVEINFO))
        .copy_from(extra_data);
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

// MFVideoFormat_ABGR32 has been defined since Win10 RS5, but its definition
// has never made it to a public header.  The only component
// that supports this type is the video processor; if it is
// needed for other components it should be converted to AGRB32
// or a YUV type.
#ifndef MFVideoFormat_ABGR32
DEFINE_MEDIATYPE_GUID(MFVideoFormat_ABGR32, 32 /*D3DFMT_A8B8G8R8*/)
#endif

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
    case VideoPixelFormat::PIXEL_FORMAT_ABGR:
      if (base::win::GetVersion() < base::win::Version::WIN10_RS5) {
        // For a long time, there was no MFVideoFormat specific to BGR
        // in Media Foundation.  BGR formats were only handled as
        // textures, and both the DirectX Video Processor and pixel
        // shaders handled these fine because they operate off of the
        // texture format and not the MF media type. Use of the
        // MFVideoFormat_ABGR32 type only becomes important if the
        // video processor should output a BGR texture.
        return MFVideoFormat_ARGB32;
      } else {
        return MFVideoFormat_ABGR32;
      }
    case VideoPixelFormat::PIXEL_FORMAT_XBGR:
      if (base::win::GetVersion() < base::win::Version::WIN10_RS5) {
        return MFVideoFormat_RGB32;
      } else {
        return MFVideoFormat_ABGR32;
      }
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
  auto buffer_span = base::span(*buffer);
  RETURN_IF_FAILED(MFCreateMemoryBuffer(buffer_span.size(), &mf_buffer));

  BYTE* mf_buffer_data = nullptr;
  DWORD max_length = 0;
  RETURN_IF_FAILED(mf_buffer->Lock(&mf_buffer_data, &max_length, 0));
  // SAFETY: `IMFMediaBuffer::Lock` returns buffer size via `max_length`
  auto mf_buffer_span = UNSAFE_BUFFERS(base::span(mf_buffer_data, max_length));
  mf_buffer_span.copy_prefix_from(buffer_span);
  RETURN_IF_FAILED(mf_buffer->SetCurrentLength(buffer_span.size()));
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
  // SAFETY: `IMFSample::GetBlobSize` returns size via `iv_length`.
  auto iv_span = UNSAFE_BUFFERS(base::span(iv.get(), iv_length));
  std::ranges::fill(iv_span, 0);
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
    // SAFETY: `IMFSample::GetAllocatedBlob` returns the size in bytes via
    // `subsample_mappings_size`, we get the number of entries by dividing
    // it by the entry size.
    auto mappings_span = UNSAFE_BUFFERS(base::span(
        subsample_mappings.get(),
        subsample_mappings_size / sizeof(MediaFoundationSubsampleEntry)));

    for (MediaFoundationSubsampleEntry& entry : mappings_span) {
      DVLOG(3) << __func__ << ": subsample_mapping"
               << ".clear_bytes=" << entry.clear_bytes
               << ", cipher_bytes=" << entry.cipher_bytes;
      subsamples.emplace_back(entry.clear_bytes, entry.cipher_bytes);
    }
  }

  // Key ID
  const auto key_id_string = GetStringFromGUID(key_id);
  const auto iv_string = std::string(iv_span.begin(), iv_span.end());
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

constexpr size_t kOneMicrosecondInMFSampleTimeUnits = 10;

HRESULT InitializeSampleFromTexture(const VideoFrame* frame,
                                    ID3D11Texture2D* input_texture,
                                    Microsoft::WRL::ComPtr<IMFSample> sample) {
  Microsoft::WRL::ComPtr<IMFMediaBuffer> mf_buffer;
  HRESULT hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D),
                                         input_texture, 0, FALSE, &mf_buffer);
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
  if (frame->ColorSpace().GetPrimaryID() !=
      gfx::ColorSpace::PrimaryID::INVALID) {
    (void)sample->SetUINT32(
        MF_MT_VIDEO_PRIMARIES,
        VideoPrimariesToMFVideoPrimaries(frame->ColorSpace().GetPrimaryID()));
  }
  return S_OK;
}

Microsoft::WRL::ComPtr<IMFSample> CreateSampleFromTexture(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    scoped_refptr<VideoFrame> frame,
    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
    bool need_perform_copy) {
  CHECK(device);
  CHECK(frame);
  CHECK(input_texture);

  HRESULT hr;
  if (need_perform_copy) {
    D3D11_TEXTURE2D_DESC desc;
    input_texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VIDEO_ENCODER;
    desc.ArraySize = 1;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copied_texture;
    hr = device->CreateTexture2D(&desc, nullptr, &copied_texture);
    if (FAILED(hr)) {
      LOG(ERROR) << "Failed to create d3d11 texture: "
                 << logging::SystemErrorCodeToString(hr);
      return nullptr;
    }
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    device->GetImmediateContext(&device_context);
    D3D11_BOX src_box = {static_cast<UINT>(frame->visible_rect().x()),
                         static_cast<UINT>(frame->visible_rect().y()),
                         0,
                         static_cast<UINT>(frame->visible_rect().right()),
                         static_cast<UINT>(frame->visible_rect().bottom()),
                         1};
    device_context->CopySubresourceRegion(copied_texture.Get(), 0, 0, 0, 0,
                                          input_texture.Get(), 0, &src_box);
    input_texture = copied_texture;
  }

  Microsoft::WRL::ComPtr<IMFSample> sample;
  hr = MFCreateSample(&sample);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to create MF Sample: "
               << logging::SystemErrorCodeToString(hr);
    return nullptr;
  }

  if (FAILED(InitializeSampleFromTexture(frame.get(), input_texture.Get(),
                                         sample))) {
    return nullptr;
  }
  return sample;
}

HRESULT GenerateSampleFromVideoFrame(
    const VideoFrame* frame,
    DXGIDeviceManager* dxgi_device_manager,
    bool use_dxgi_buffer,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>* staging_texture,
    DWORD buffer_alignment,
    IMFSample** sample_out) {
  // A shared image sample cannot be created synchronously.  Use
  // GenerateSampleFromSharedImageVideoFrame. Note that this is not true for
  // mappable shared image since it has a GpuMemoryBufferHandle. So skipping the
  // CHECK when frame has a mappable buffer.
  if (!frame->HasMappableGpuBuffer()) {
    CHECK(!frame->HasSharedImage());
  }

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
    hr = device1->OpenSharedResource1(
        buffer_handle.dxgi_handle().buffer_handle(),
        IID_PPV_ARGS(&input_texture));
    RETURN_ON_HR_FAILURE(hr, "Failed to open shared GMB D3D texture", hr);

    if (use_dxgi_buffer) {
      hr = InitializeSampleFromTexture(frame, input_texture.Get(), sample);
      RETURN_ON_HR_FAILURE(hr, "Failed to initialize sample from texture", hr);
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
      bool copy_succeeded =
          gpu::CopyD3D11TexToMem(input_texture.Get(), scoped_buffer.as_span(),
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
                        scoped_buffer.as_span().subspan(buffer_offset).data(),
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

void GenerateResourceOnSyncTokenReleased(
    scoped_refptr<VideoFrame> frame,
    bool use_same_device,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    ResourceAvailableCB sample_available_cb) {
  TRACE_EVENT0("media", "GenerateResourceOnSyncTokenReleased");

#define RETURN_ON_FAILURE_WITH_CALLBACK(hr, message)                       \
  if (FAILED(hr)) {                                                        \
    LOG(ERROR) << message << ": " << logging::SystemErrorCodeToString(hr); \
    std::move(sample_available_cb)                                         \
        .Run(std::move(frame), nullptr, std::nullopt, std::nullopt, hr);   \
    return;                                                                \
  }

  Microsoft::WRL::ComPtr<ID3D11Device> shared_d3d11_device =
      command_buffer_helper->GetSharedImageStub()
          ->shared_context_state()
          ->GetD3D11Device();
  HRESULT hr = shared_d3d11_device ? S_OK : E_FAIL;
  RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Invalid shared d3d11 device");
  gpu::SharedImageManager* shared_image_manager =
      command_buffer_helper->GetSharedImageManager();
  std::unique_ptr<gpu::VideoImageRepresentation> image_representation =
      shared_image_manager->ProduceVideo(
          shared_d3d11_device, frame->shared_image()->mailbox(),
          command_buffer_helper->GetMemoryTypeTracker());
  auto scoped_read_access = image_representation->BeginScopedReadAccess();
  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture =
      scoped_read_access->GetD3D11Texture();

  // If same device, pass the generated IMFSample directly, otherwise, create a
  // shared handle for cross-device texture sharing.
  if (use_same_device) {
    // If this texture is NV12 and going to be fed directly to the encoder,
    // create a copy of it. Hardware encoders are not guaranteed to be done
    // with the texture when ProcessInput returns.
    ComPtr<IMFSample> sample =
        CreateSampleFromTexture(shared_d3d11_device, frame, input_texture,
                                frame->format() == PIXEL_FORMAT_NV12);
    RETURN_ON_FAILURE_WITH_CALLBACK(sample != nullptr ? S_OK : E_FAIL,
                                    "Failed to create MF sample");
    std::move(sample_available_cb)
        .Run(std::move(frame), std::move(sample), std::nullopt, std::nullopt,
             S_OK);
    return;
  }

  TRACE_EVENT0("media", "CreateSharedHandleOnSyncTokenReleased");
  bool input_texture_has_been_copied = false;
  Microsoft::WRL::ComPtr<IDXGIResource1> dxgi_resource;
  hr = input_texture.As(&dxgi_resource);
  RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to get DXGI resource");
  HANDLE shared_handle;
  hr = dxgi_resource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ,
                                         nullptr, &shared_handle);
  if (FAILED(hr)) {
    TRACE_EVENT0("media", "CopyTextureOnCreateSharedHandleFailed");
    D3D11_TEXTURE2D_DESC texture_desc;
    input_texture->GetDesc(&texture_desc);
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags =
        D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.ArraySize = 1;
    texture_desc.CPUAccessFlags = 0;
    texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                             D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> shared_texture;
    hr = shared_d3d11_device->CreateTexture2D(&texture_desc, nullptr,
                                              &shared_texture);
    RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to create shared texture");

    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
    hr = shared_texture.As(&keyed_mutex);
    RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to get mutex");
    CHECK(SUCCEEDED(keyed_mutex->AcquireSync(0, INFINITE)));
    gpu::DXGIScopedReleaseKeyedMutex scoped_keyed_mutex(keyed_mutex, 0);

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    shared_d3d11_device->GetImmediateContext(&device_context);
    D3D11_BOX src_box = {static_cast<UINT>(frame->visible_rect().x()),
                         static_cast<UINT>(frame->visible_rect().y()),
                         0,
                         static_cast<UINT>(frame->visible_rect().right()),
                         static_cast<UINT>(frame->visible_rect().bottom()),
                         1};
    device_context->CopySubresourceRegion(shared_texture.Get(), 0, 0, 0, 0,
                                          input_texture.Get(), 0, &src_box);
    Microsoft::WRL::ComPtr<IDXGIResource1> shared_dxgi_resource;
    hr = shared_texture.As(&shared_dxgi_resource);
    CHECK(SUCCEEDED(hr));
    hr = shared_dxgi_resource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ, nullptr, &shared_handle);
    RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to create shared handle");
    input_texture_has_been_copied = true;
  }
  base::win::ScopedHandle scoped_shared_handle(shared_handle);

  // If the producer and consumer are different d3d11 device, we need to
  // guarantee all previous operations on producer device are finished before
  // using the texture.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
  hr = shared_d3d11_device.As(&dxgi_device2);
  RETURN_ON_FAILURE_WITH_CALLBACK(hr, "Failed to query dxgi device2");
  hr = dxgi_device2->EnqueueSetEvent(event.handle());
  if (SUCCEEDED(hr)) {
    event.Wait();
  } else {
    LOG(WARNING) << "Failed to set event: "
                 << logging::SystemErrorCodeToString(hr);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> d3d11_context;
    shared_d3d11_device->GetImmediateContext(&d3d11_context);
    if (d3d11_context) {
      d3d11_context->Flush();
    }
  }

  std::move(sample_available_cb)
      .Run(std::move(frame), nullptr, std::move(scoped_shared_handle),
           input_texture_has_been_copied, S_OK);
#undef RETURN_ON_FAILURE_WITH_CALLBACK
}

void GenerateResourceFromSharedImageVideoFrame(
    scoped_refptr<VideoFrame> frame,
    bool use_same_device,
    scoped_refptr<CommandBufferHelper> command_buffer_helper,
    ResourceAvailableCB sample_available_cb) {
  gpu::SyncToken acquire_sync_token = frame->acquire_sync_token();
  command_buffer_helper->WaitForSyncToken(
      acquire_sync_token,
      base::BindOnce(&GenerateResourceOnSyncTokenReleased, std::move(frame),
                     use_same_device, std::move(command_buffer_helper),
                     std::move(sample_available_cb)));
}

}  // namespace media
