// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_helpers.h"

#include <d3d11.h>
#include <ks.h>
#include <ksmedia.h>

#include "base/check_op.h"
#include "base/win/windows_version.h"

namespace media {

namespace {

// ID3D11DeviceChild and ID3D11Device implement SetPrivateData with
// the exact same parameters.
template <typename T>
HRESULT SetDebugNameInternal(T* d3d11_object, const char* debug_string) {
  return d3d11_object->SetPrivateData(WKPDID_D3DDebugObjectName,
                                      strlen(debug_string), debug_string);
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
    case KSAUDIO_SPEAKER_QUAD:
      return CHANNEL_LAYOUT_QUAD;
    case KSAUDIO_SPEAKER_SURROUND:
      return CHANNEL_LAYOUT_4_0;
    case KSAUDIO_SPEAKER_5POINT1:
      return CHANNEL_LAYOUT_5_1_BACK;
    case KSAUDIO_SPEAKER_5POINT1_SURROUND:
      return CHANNEL_LAYOUT_5_1;
    case KSAUDIO_SPEAKER_7POINT1:
      return CHANNEL_LAYOUT_7_1_WIDE;
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

}  // namespace media
