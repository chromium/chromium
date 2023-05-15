// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_

#include <mferror.h>
#include <mfidl.h>
#include <wrl/implements.h>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "media/base/decoder_buffer.h"
#include "media/cdm/aes_decryptor.h"

namespace media {

enum class StreamType { kUnknown, kVideo, kAudio };

// This transform (decryptor) decrypts the encrypted content or bypasses the
// clear content. An instance for audio or video gets created by
// `IMFInputTrustAuthority::GetDecrypter()`.
// - Once an instance is created and the streaming is about to start, a set of
// Set and Get interfaces (i.e., `SetInputType`, `GetOutputAvailableType`,
// `SetOutputType`, `GetStreamCount`, `GetStreamIDs` and etc) are getting called
// to set up the transform to be ready for processing the stream data.
// - `ProcessMessage()` receives `MFT_MESSAGE_NOTIFY_START_OF_STREAM` when the
// streaming begins.
// - The input samples are getting fed into `ProcessInput()` while
// `MediaFoundationStreamWrapper` produces the input stream data.
// - As a synchronous MFT decryptor, the input sample is simply stored for
// `ProcessOutput()` to process it later. Note that `ProcessOutput()` can be
// called first before `ProcessInput()`. In this case it should return
// `MF_E_TRANSFORM_NEED_MORE_INPUT` saying the transform cannot produce output
// data until it receives more input data.
// `ProcessMessage()` receives `MFT_MESSAGE_NOTIFY_END_OF_STREAM` message once
// the stream reaches the end of stream.
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
  HRESULT GetShutdownStatus() {
    base::AutoLock lock(lock_);
    return (is_shutdown_) ? MF_E_SHUTDOWN : S_OK;
  }

  // Generates a DecoderBuffer from a Media Foundation sample.
  HRESULT GenerateDecoderBufferFromSample(
      IMFSample* mf_sample,
      GUID* key_id,
      scoped_refptr<DecoderBuffer>* buffer_out);

  // Flushes all stored data as the transform receives a flush command or a
  // request to release all resources.
  void FlushAllStoredData();

  // For IMFShutdown
  bool is_shutdown_ GUARDED_BY(lock_) = false;

  // AES decryptor
  scoped_refptr<AesDecryptor> aes_decryptor_;

  // The media type for an input stream on the transform.
  Microsoft::WRL::ComPtr<IMFMediaType> input_media_type_ GUARDED_BY(lock_);

  // The media type for an output stream on the transform.
  Microsoft::WRL::ComPtr<IMFMediaType> output_media_type_ GUARDED_BY(lock_);

  // The available media type for an output stream on the transform.
  Microsoft::WRL::ComPtr<IMFMediaType> available_output_media_type_
      GUARDED_BY(lock_);

  // The input sample data to the input stream on the transform.
  Microsoft::WRL::ComPtr<IMFSample> sample_ GUARDED_BY(lock_);

  // The input stream type (either Audio or Video).
  StreamType stream_type_ GUARDED_BY(lock_) = StreamType::kUnknown;

  // To protect access to data from multiple threads. GetAttributes,
  // GetInputCurrentType, GetOutputCurrentType, GetStreamCount, GetStreamIDs,
  // GetOutputStreamInfo, GetInputAvailableType, GetOutputAvailableType,
  // SetInputType, SetOutputType, ProcessMessage ProcessInput and ProcessOutput
  // methods can run from MF work queue threads.
  base::Lock lock_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_DECRYPTOR_H_
