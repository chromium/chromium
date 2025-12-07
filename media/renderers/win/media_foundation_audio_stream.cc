// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/renderers/win/media_foundation_audio_stream.h"

#include <mferror.h>
#include <mmreg.h>
#include <wrl.h>

#include "base/win/scoped_co_mem.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

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
#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
    case AudioCodec::kAC4:
      RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationAC4AudioStream>(
          &audio_stream, stream_id, parent_source, demuxer_stream,
          std::move(media_log)));
      break;
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
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

#if BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
HRESULT MediaFoundationAC4AudioStream::GetMediaType(
    IMFMediaType** media_type_out) {
  AudioDecoderConfig decoder_config = demuxer_stream_->audio_decoder_config();
  return GetAC4AudioType(decoder_config, media_type_out);
}
#endif  // BUILDFLAG(ENABLE_PLATFORM_AC4_AUDIO)
}  // namespace media
