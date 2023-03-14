// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_

#include <mfidl.h>
#include <wrl/implements.h>

#include "base/memory/scoped_refptr.h"
#include "media/cdm/aes_decryptor.h"

namespace media {

class MediaFoundationClearKeyDecryptor final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFTransform,
          Microsoft::WRL::CloakedIid<IMFShutdown>,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeyDecryptor();
  ~MediaFoundationClearKeyDecryptor() override;

  HRESULT RuntimeClassInitialize(
      _In_ scoped_refptr<AesDecryptor> aes_decryptor);

  // IMFTransform
  STDMETHODIMP GetStreamLimits(_Out_ DWORD* input_minimum,
                               _Out_ DWORD* input_maximum,
                               _Out_ DWORD* output_minimum,
                               _Out_ DWORD* output_maximum) override;
  STDMETHODIMP GetStreamCount(_Out_ DWORD* input_streams,
                              _Out_ DWORD* output_streams) override;
  STDMETHODIMP GetStreamIDs(_In_ DWORD input_ids_size,
                            _Out_ DWORD* input_ids,
                            _In_ DWORD output_ids_size,
                            _Out_ DWORD* output_ids) override;
  STDMETHODIMP GetInputStreamInfo(
      _In_ DWORD input_stream_id,
      _Out_ MFT_INPUT_STREAM_INFO* stream_info) override;
  STDMETHODIMP GetOutputStreamInfo(
      _In_ DWORD output_stream_id,
      _Out_ MFT_OUTPUT_STREAM_INFO* stream_info) override;
  STDMETHODIMP GetAttributes(_COM_Outptr_ IMFAttributes** attributes) override;
  STDMETHODIMP GetInputStreamAttributes(
      _In_ DWORD input_stream_id,
      _COM_Outptr_ IMFAttributes** attributes) override;
  STDMETHODIMP GetOutputStreamAttributes(
      _In_ DWORD output_stream_id,
      _COM_Outptr_ IMFAttributes** attributes) override;
  STDMETHODIMP DeleteInputStream(_In_ DWORD stream_id) override;
  STDMETHODIMP AddInputStreams(_In_ DWORD streams_count,
                               _In_count_(streams_count)
                                   DWORD* stream_ids) override;
  STDMETHODIMP GetInputAvailableType(_In_ DWORD input_stream_index,
                                     _In_ DWORD type_index,
                                     _COM_Outptr_ IMFMediaType** type) override;
  STDMETHODIMP GetOutputAvailableType(
      _In_ DWORD output_stream_index,
      _In_ DWORD type_index,
      _COM_Outptr_ IMFMediaType** type) override;
  STDMETHODIMP SetInputType(_In_ DWORD input_stream_index,
                            _In_ IMFMediaType* type,
                            _In_ DWORD flags) override;
  STDMETHODIMP SetOutputType(_In_ DWORD output_stream_index,
                             _In_ IMFMediaType* type,
                             _In_ DWORD flags) override;
  STDMETHODIMP GetInputCurrentType(_In_ DWORD input_stream_index,
                                   _COM_Outptr_ IMFMediaType** ptype) override;
  STDMETHODIMP GetOutputCurrentType(_In_ DWORD output_stream_index,
                                    _COM_Outptr_ IMFMediaType** ptype) override;
  STDMETHODIMP GetInputStatus(_In_ DWORD input_stream_index,
                              _Out_ DWORD* flags) override;
  STDMETHODIMP GetOutputStatus(_Out_ DWORD* flags) override;
  STDMETHODIMP SetOutputBounds(_In_ LONGLONG lower_bound,
                               _In_ LONGLONG upper_bound) override;
  STDMETHODIMP ProcessEvent(_In_ DWORD input_stream_id,
                            _In_ IMFMediaEvent* event) override;
  STDMETHODIMP ProcessMessage(_In_ MFT_MESSAGE_TYPE message,
                              _In_ ULONG_PTR param) override;
  STDMETHODIMP ProcessInput(_In_ DWORD input_stream_index,
                            _In_ IMFSample* sample,
                            _In_ DWORD flags) override;
  STDMETHODIMP ProcessOutput(_In_ DWORD flags,
                             _In_ DWORD output_samples_count,
                             _Inout_count_(output_samples_count)
                                 MFT_OUTPUT_DATA_BUFFER* output_samples,
                             _Out_ DWORD* status) override;

  // IMFShutdown
  STDMETHODIMP Shutdown() override;
  STDMETHODIMP GetShutdownStatus(_Out_ MFSHUTDOWN_STATUS* status) override;

 private:
  scoped_refptr<AesDecryptor> aes_decryptor_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_
