// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_decryptor.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

MediaFoundationClearKeyDecryptor::MediaFoundationClearKeyDecryptor() = default;

MediaFoundationClearKeyDecryptor::~MediaFoundationClearKeyDecryptor() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyDecryptor::RuntimeClassInitialize(
    _In_ scoped_refptr<AesDecryptor> aes_decryptor) {
  DVLOG_FUNC(1);

  aes_decryptor_ = std::move(aes_decryptor);

  return S_OK;
}

// IMFTransform
STDMETHODIMP MediaFoundationClearKeyDecryptor::GetStreamLimits(
    _Out_ DWORD* input_minimum,
    _Out_ DWORD* input_maximum,
    _Out_ DWORD* output_minimum,
    _Out_ DWORD* output_maximum) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP
MediaFoundationClearKeyDecryptor::GetStreamCount(_Out_ DWORD* input_streams,
                                                 _Out_ DWORD* output_streams) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetStreamIDs(
    _In_ DWORD input_ids_size,
    _Out_ DWORD* input_ids,
    _In_ DWORD output_ids_size,
    _Out_ DWORD* output_ids) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::
    MediaFoundationClearKeyDecryptor::GetInputStreamInfo(
        _In_ DWORD input_stream_id,
        _Out_ MFT_INPUT_STREAM_INFO* stream_info) {
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStreamInfo(
    _In_ DWORD output_stream_id,
    _Out_ MFT_OUTPUT_STREAM_INFO* stream_info) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetAttributes(
    _COM_Outptr_ IMFAttributes** attributes) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputStreamAttributes(
    _In_ DWORD input_stream_id,
    _COM_Outptr_ IMFAttributes** attributes) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStreamAttributes(
    _In_ DWORD output_stream_id,
    _COM_Outptr_ IMFAttributes** attributes) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::DeleteInputStream(
    _In_ DWORD stream_id) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::AddInputStreams(
    _In_ DWORD streams_count,
    _In_count_(streams_count) DWORD* stream_ids) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputAvailableType(
    _In_ DWORD input_stream_index,
    _In_ DWORD type_index,
    _COM_Outptr_ IMFMediaType** type) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputAvailableType(
    _In_ DWORD output_stream_index,
    _In_ DWORD type_index,
    _COM_Outptr_ IMFMediaType** type) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetInputType(
    _In_ DWORD input_stream_index,
    _In_ IMFMediaType* type,
    _In_ DWORD flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetOutputType(
    _In_ DWORD output_stream_index,
    _In_ IMFMediaType* type,
    _In_ DWORD flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputCurrentType(
    _In_ DWORD input_stream_index,
    _COM_Outptr_ IMFMediaType** type) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputCurrentType(
    _In_ DWORD output_stream_index,
    _COM_Outptr_ IMFMediaType** type) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputStatus(
    _In_ DWORD input_stream_index,
    _Out_ DWORD* flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStatus(
    _Out_ DWORD* flags) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetOutputBounds(
    _In_ LONGLONG lower_bound,
    _In_ LONGLONG upper_bound) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessEvent(
    _In_ DWORD input_stream_id,
    _In_ IMFMediaEvent* event) {
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessMessage(
    _In_ MFT_MESSAGE_TYPE message,
    _In_ ULONG_PTR param) {
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessInput(
    _In_ DWORD input_stream_index,
    _In_ IMFSample* sample,
    _In_ DWORD flags) {
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessOutput(
    _In_ DWORD flags,
    _In_ DWORD output_samples_count,
    _Inout_count_(output_samples_count) MFT_OUTPUT_DATA_BUFFER* output_samples,
    _Out_ DWORD* status) {
  return S_OK;
}

// IMFShutdown
STDMETHODIMP MediaFoundationClearKeyDecryptor::Shutdown() {
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetShutdownStatus(
    _Out_ MFSHUTDOWN_STATUS* status) {
  return S_OK;
}

}  // namespace media
