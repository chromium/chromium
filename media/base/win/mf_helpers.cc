// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/win/mf_helpers.h"

#include <d3d11.h>

#include "base/check_op.h"

namespace media {

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
  return d3d11_device_child->SetPrivateData(WKPDID_D3DDebugObjectName,
                                            strlen(debug_string), debug_string);
}

}  // namespace media
