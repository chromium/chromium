// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_decryptor.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "media/base/decoder_buffer.h"
#include "media/base/subsample_entry.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

namespace {

#define ENUM_TO_STRING(enum) \
  case enum:                 \
    return #enum

std::string MessageTypeToString(MFT_MESSAGE_TYPE event) {
  switch (event) {
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_DRAIN);
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_MARKER);
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_FLUSH);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_END_STREAMING);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_START_OF_STREAM);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_END_OF_STREAM);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_RELEASE_RESOURCES);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_REACQUIRE_RESOURCES);
    ENUM_TO_STRING(MFT_MESSAGE_SET_D3D_MANAGER);
    ENUM_TO_STRING(MFT_MESSAGE_NOTIFY_EVENT);
    ENUM_TO_STRING(MFT_MESSAGE_DROP_SAMPLES);
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_TICK);
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_SET_OUTPUT_STREAM_STATE);
    ENUM_TO_STRING(MFT_MESSAGE_COMMAND_FLUSH_OUTPUT_STREAM);
  }
}

#undef ENUM_TO_STRING

}  // namespace

using Microsoft::WRL::ComPtr;

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
  DVLOG_FUNC(3);
  CHECK(input_minimum);
  CHECK(input_maximum);
  CHECK(output_minimum);
  CHECK(output_maximum);

  *input_minimum = 1;
  *input_maximum = 1;
  *output_minimum = 1;
  *output_maximum = 1;

  return S_OK;
}

STDMETHODIMP
MediaFoundationClearKeyDecryptor::GetStreamCount(_Out_ DWORD* input_streams,
                                                 _Out_ DWORD* output_streams) {
  DVLOG_FUNC(3);
  CHECK(input_streams);
  CHECK(output_streams);

  *input_streams = 1;
  *output_streams = 1;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetStreamIDs(
    _In_ DWORD input_ids_size,
    _Out_ DWORD* input_ids,
    _In_ DWORD output_ids_size,
    _Out_ DWORD* output_ids) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL since the transform has a fixed
  // number of streams and the streams are numbered consecutively from 0 to n-1.
  // The same thing applies to GetInputStreamAttributes(),
  // GetOutputStreamAttributes(), DeleteInputStream() and AddInputStreams()
  // methods. See
  // https://learn.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getstreamids.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::
    MediaFoundationClearKeyDecryptor::GetInputStreamInfo(
        _In_ DWORD input_stream_id,
        _Out_ MFT_INPUT_STREAM_INFO* stream_info) {
  DVLOG_FUNC(3);
  CHECK(stream_info);

  if (input_stream_id != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  *stream_info = {0};

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStreamInfo(
    _In_ DWORD output_stream_id,
    _Out_ MFT_OUTPUT_STREAM_INFO* stream_info) {
  DVLOG_FUNC(3);
  CHECK(stream_info);

  if (output_stream_id != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  stream_info->dwFlags = MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
  stream_info->cbSize = 0;
  stream_info->cbAlignment = 0;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetAttributes(
    _COM_Outptr_ IMFAttributes** attributes) {
  DVLOG_FUNC(3);

  // Even though there is an exception where Hardware-based MFTs must implement
  // this method, we don't need it for now. However, we can implement it as
  // needed when adding more browser tests or changing to an async
  // MFT(`MF_TRANSFORM_ASYNC` to be TRUE). See
  // https://learn.microsoft.com/en-us/windows/win32/api/mftransform/nf-mftransform-imftransform-getattributes

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputStreamAttributes(
    _In_ DWORD input_stream_id,
    _COM_Outptr_ IMFAttributes** attributes) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStreamAttributes(
    _In_ DWORD output_stream_id,
    _COM_Outptr_ IMFAttributes** attributes) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::DeleteInputStream(
    _In_ DWORD stream_id) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::AddInputStreams(
    _In_ DWORD streams_count,
    _In_count_(streams_count) DWORD* stream_ids) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputAvailableType(
    _In_ DWORD input_stream_index,
    _In_ DWORD type_index,
    _COM_Outptr_ IMFMediaType** type) {
  DVLOG_FUNC(3);
  base::AutoLock lock(lock_);

  if (input_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  if (!input_media_type_) {
    return E_NOTIMPL;
  }

  if (type_index != 0) {
    return MF_E_NO_MORE_TYPES;
  }

  // Since the transform stores a media type internally, the transform should
  // return a clone of the media type, not a pointer to the original type.
  // Otherwise, the caller might modify the type and alter the internal state of
  // the MFT.
  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(input_media_type_->CopyAllItems(media_type.Get()));
  *type = media_type.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputAvailableType(
    _In_ DWORD output_stream_index,
    _In_ DWORD type_index,
    _COM_Outptr_ IMFMediaType** type) {
  DVLOG_FUNC(1);
  base::AutoLock lock(lock_);

  if (output_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  if (!available_output_media_type_) {
    return MF_E_TRANSFORM_TYPE_NOT_SET;
  }

  if (type_index != 0) {
    return MF_E_NO_MORE_TYPES;
  }

  // Since the transform stores a media type internally, the transform should
  // return a clone of the media type, not a pointer to the original type.
  // Otherwise, the caller might modify the type and alter the internal state of
  // the MFT.
  ComPtr<IMFMediaType> media_type;
  RETURN_IF_FAILED(MFCreateMediaType(&media_type));
  RETURN_IF_FAILED(
      available_output_media_type_->CopyAllItems(media_type.Get()));
  *type = media_type.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetInputType(
    _In_ DWORD input_stream_index,
    _In_ IMFMediaType* type,
    _In_ DWORD flags) {
  DVLOG_FUNC(1) << "input_stream_index=" << input_stream_index
                << ", flags=" << flags;
  base::AutoLock lock(lock_);

  if (input_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  if (flags & MFT_SET_TYPE_TEST_ONLY) {
    DVLOG_FUNC(3) << "MFT_SET_TYPE_TEST_ONLY";
    return S_OK;
  }

  if (!type) {
    return E_INVALIDARG;
  }

  input_media_type_ = type;

  GUID guid_major_type = GUID_NULL;
  ComPtr<IMFMediaType> unwrapped_media_type;
  RETURN_IF_FAILED(input_media_type_->GetMajorType(&guid_major_type));

  // Handle both clear and protected content and do NOT send an error if we
  // get non-wrapped samples.
  if (MFMediaType_Protected == guid_major_type) {
    DVLOG_FUNC(3) << "MFMediaType_Protected";
    RETURN_IF_FAILED(
        MFUnwrapMediaType(input_media_type_.Get(), &unwrapped_media_type));
  } else {
    unwrapped_media_type = input_media_type_;
  }

  RETURN_IF_FAILED(unwrapped_media_type->GetMajorType(&guid_major_type));

  if (guid_major_type == MFMediaType_Audio) {
    DVLOG_FUNC(3) << "MFMediaType_Audio";
    if (stream_type_ == StreamType::kVideo) {
      // If we were already set up as video we can't change to audio.
      return MF_E_INVALIDMEDIATYPE;
    }

    stream_type_ = StreamType::kAudio;
  } else if (guid_major_type == MFMediaType_Video) {
    DVLOG_FUNC(3) << "MFMediaType_Video";
    GUID guid_sub_type = GUID_NULL;
    if (stream_type_ == StreamType::kAudio) {
      // If we were already set up as audio we can't change to video.
      return MF_E_INVALIDMEDIATYPE;
    }

    stream_type_ = StreamType::kVideo;
    RETURN_IF_FAILED(
        unwrapped_media_type->GetGUID(MF_MT_SUBTYPE, &guid_sub_type));
  }

  available_output_media_type_ = unwrapped_media_type;

  DVLOG_FUNC(3) << "stream_type_=" << static_cast<int>(stream_type_);
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetOutputType(
    _In_ DWORD output_stream_index,
    _In_ IMFMediaType* type,
    _In_ DWORD flags) {
  DVLOG_FUNC(1) << "output_stream_index=" << output_stream_index
                << ", flags=" << flags;
  base::AutoLock lock(lock_);

  if (output_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  if (!available_output_media_type_) {
    return MF_E_TRANSFORM_TYPE_NOT_SET;
  }

  if (!type) {
    // Clear the media type if `type` is null.
    output_media_type_ = nullptr;
    return S_OK;
  }

  DWORD is_equal_flags = 0;

  RETURN_IF_FAILED(
      type->IsEqual(available_output_media_type_.Get(), &is_equal_flags));

  if (!(is_equal_flags & MF_MEDIATYPE_EQUAL_MAJOR_TYPES)) {
    return MF_E_INVALIDMEDIATYPE;
  }
  if (!(is_equal_flags & MF_MEDIATYPE_EQUAL_FORMAT_TYPES)) {
    return MF_E_INVALIDMEDIATYPE;
  }

  if (!(flags & MFT_SET_TYPE_TEST_ONLY)) {
    DVLOG_FUNC(3) << "!MFT_SET_TYPE_TEST_ONLY";
    output_media_type_ = type;
  } else {
    DVLOG_FUNC(3) << "MFT_SET_TYPE_TEST_ONLY";
  }

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputCurrentType(
    _In_ DWORD input_stream_index,
    _COM_Outptr_ IMFMediaType** type) {
  DVLOG_FUNC(1) << "input_stream_index=" << input_stream_index;
  base::AutoLock lock(lock_);

  if (input_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  } else if (input_media_type_ != nullptr) {
    *type = input_media_type_.Get();
    (*type)->AddRef();
  } else {
    return MF_E_TRANSFORM_TYPE_NOT_SET;
  }

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputCurrentType(
    _In_ DWORD output_stream_index,
    _COM_Outptr_ IMFMediaType** type) {
  DVLOG_FUNC(1) << "output_stream_index=" << output_stream_index;
  base::AutoLock lock(lock_);

  if (output_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  } else if (output_media_type_ != nullptr) {
    *type = output_media_type_.Get();
    (*type)->AddRef();
  } else {
    return MF_E_TRANSFORM_TYPE_NOT_SET;
  }

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetInputStatus(
    _In_ DWORD input_stream_index,
    _Out_ DWORD* flags) {
  DVLOG_FUNC(3) << "input_stream_index=" << input_stream_index;

  // Optional API. It will be determined whether the transform can accept more
  // data in the ProcessInput().
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::GetOutputStatus(
    _Out_ DWORD* flags) {
  DVLOG_FUNC(3);

  // Optional API. It will be determined whether the transform has output data
  // in the ProcessOutput().
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::SetOutputBounds(
    _In_ LONGLONG lower_bound,
    _In_ LONGLONG upper_bound) {
  DVLOG_FUNC(3);

  // Optional API. Expected to return E_NOTIMPL.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessEvent(
    _In_ DWORD input_stream_id,
    _In_ IMFMediaEvent* event) {
  DVLOG_FUNC(3);

  // API not required for synchronous MFTs.
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessMessage(
    _In_ MFT_MESSAGE_TYPE message,
    _In_ ULONG_PTR param) {
  DVLOG_FUNC(3) << "message=" << MessageTypeToString(message);
  RETURN_IF_FAILED(GetShutdownStatus());

  switch (message) {
    case MFT_MESSAGE_COMMAND_FLUSH:
      // Flush all stored data. MFT should discard any media samples it is
      // holding.
      FlushAllStoredData();
      break;
    case MFT_MESSAGE_NOTIFY_RELEASE_RESOURCES:
      // When we are told to release resources we need to flush all output
      // samples.
      FlushAllStoredData();
      break;
    // Message types not required for synchronous MFTs to respond.
    case MFT_MESSAGE_COMMAND_DRAIN:
    case MFT_MESSAGE_COMMAND_MARKER:
    case MFT_MESSAGE_NOTIFY_BEGIN_STREAMING:
    case MFT_MESSAGE_NOTIFY_END_STREAMING:
    case MFT_MESSAGE_NOTIFY_START_OF_STREAM:
    case MFT_MESSAGE_NOTIFY_END_OF_STREAM:
    // Applies only if `MF_SA_D3D_AWARE` attribute is set to TRUE.
    case MFT_MESSAGE_SET_D3D_MANAGER:
    // Applies only if `MFT_POLICY_SET_AWARE` attribute is set to TRUE.
    case MFT_MESSAGE_NOTIFY_EVENT:
    // An MFT is allowed to ignore message types it doesn't care about.
    case MFT_MESSAGE_NOTIFY_REACQUIRE_RESOURCES:
    case MFT_MESSAGE_DROP_SAMPLES:
    case MFT_MESSAGE_COMMAND_TICK:
    case MFT_MESSAGE_COMMAND_SET_OUTPUT_STREAM_STATE:
    case MFT_MESSAGE_COMMAND_FLUSH_OUTPUT_STREAM:
      DVLOG_FUNC(3) << "fallthrough!";
      break;
  }

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessInput(
    _In_ DWORD input_stream_index,
    _In_ IMFSample* sample,
    _In_ DWORD flags) {
  DVLOG_FUNC(1) << "input_stream_index=" << input_stream_index
                << ", flags=" << flags;
  RETURN_IF_FAILED(GetShutdownStatus());

  base::AutoLock lock(lock_);

  if (input_stream_index != 0) {
    return MF_E_INVALIDSTREAMNUMBER;
  }

  if (!sample) {
    return E_INVALIDARG;
  }

  if (sample_) {
    DVLOG_FUNC(3) << "The previous input sample is not processed yet.";
    return MF_E_NOTACCEPTING;
  }

  // We only store the input sample here to process it in the `ProcessOutput()`.
  sample_ = sample;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::ProcessOutput(
    _In_ DWORD flags,
    _In_ DWORD output_samples_count,
    _Inout_count_(output_samples_count) MFT_OUTPUT_DATA_BUFFER* output_samples,
    _Out_ DWORD* status) {
  DVLOG_FUNC(1) << "flags=" << flags
                << ", output_samples_count=" << output_samples_count;
  RETURN_IF_FAILED(GetShutdownStatus());

  base::AutoLock lock(lock_);

  if (!output_samples || !status || output_samples_count != 1) {
    return E_INVALIDARG;
  }

  if (!input_media_type_ || !output_media_type_) {
    return MF_E_TRANSFORM_TYPE_NOT_SET;
  }

  // It should be 0 unless there is a stream change that we don't support.
  *status = 0;

  // Validate that the caller didn't provide a sample since we indicate the
  // callee is the allocator by setting MFT_OUTPUT_STREAM_PROVIDES_SAMPLES flag
  // in the GetOutputStreamInfo().
  if (output_samples[0].pSample != nullptr) {
    return E_INVALIDARG;
  }

  // Ensure synchronous MFTs never need any event.
  output_samples[0].pEvents = nullptr;

  DVLOG_FUNC(3) << "output_samples[0].dwStreamID="
                << output_samples[0].dwStreamID
                << ", dwStatus=" << output_samples[0].dwStatus;

  if (!sample_) {
    DVLOG_FUNC(3) << "MF_E_TRANSFORM_NEED_MORE_INPUT";
    // The MFT cannot produce output data until it receives more input data.
    // Note that ProcessOutput() may be called to see if there is still data
    // to process before ProcessInput().
    return MF_E_TRANSFORM_NEED_MORE_INPUT;
  }

  // If we were not able to get the content key id, this must be a clear
  // sample.
  GUID key_id_guid;
  if (FAILED(sample_->GetGUID(MFSampleExtension_Content_KeyID, &key_id_guid))) {
    DVLOG_FUNC(3) << "Clear sample detected!";

    output_samples[0].pSample = sample_.Detach();
    return S_OK;
  }

  // Convert the Media Foundation sample to a DecoderBuffer.
  scoped_refptr<DecoderBuffer> encrypted_buffer;
  RETURN_IF_FAILED(GenerateDecoderBufferFromSample(
      sample_.Detach(), &key_id_guid, &encrypted_buffer));
  DVLOG_FUNC(3) << "encrypted_buffer=" +
                       encrypted_buffer->AsHumanReadableString(true);

  // Decrypt the protected content.
  Decryptor::Status decryptor_status = Decryptor::kError;

  // TODO(crbug.com/40910495): We may remove the tracking code of stream type
  // if two decryptors get created for audio and video respectively.
  CHECK(stream_type_ != StreamType::kUnknown);
  Decryptor::StreamType stream_type = stream_type_ == StreamType::kVideo
                                          ? Decryptor::kVideo
                                          : Decryptor::kAudio;
  scoped_refptr<DecoderBuffer> decrypted_buffer;
  bool is_decrypt_completed = false;
  aes_decryptor_->Decrypt(
      stream_type, encrypted_buffer,
      base::BindOnce(
          [](Decryptor::Status* status_copy,
             scoped_refptr<DecoderBuffer>* buffer_copy,
             bool* is_decrypt_completed, Decryptor::Status status,
             scoped_refptr<DecoderBuffer> buffer) {
            *status_copy = status;
            *buffer_copy = std::move(buffer);
            *is_decrypt_completed = true;
          },
          &decryptor_status, &decrypted_buffer, &is_decrypt_completed));

  // Ensure the decryption is done synchronously.
  CHECK(is_decrypt_completed);

  // Convert the DecoderBuffer back to a Media Foundation sample.
  ComPtr<IMFSample> sample_decrypted = nullptr;
  GUID last_key_id = GUID_NULL;
  RETURN_IF_FAILED(
      GenerateSampleFromDecoderBuffer(decrypted_buffer.get(), &sample_decrypted,
                                      &last_key_id, base::NullCallback()));
  DVLOG_FUNC(3) << "decrypted_buffer=" +
                       decrypted_buffer->AsHumanReadableString(true);

  output_samples[0].pSample = sample_decrypted.Detach();

  return S_OK;
}

// IMFShutdown
STDMETHODIMP MediaFoundationClearKeyDecryptor::GetShutdownStatus(
    _Out_ MFSHUTDOWN_STATUS* status) {
  DVLOG_FUNC(1);

  base::AutoLock lock(lock_);
  if (is_shutdown_) {
    *status = MFSHUTDOWN_COMPLETED;
    return S_OK;
  }

  return MF_E_INVALIDREQUEST;
}

STDMETHODIMP MediaFoundationClearKeyDecryptor::Shutdown() {
  DVLOG_FUNC(1);
  base::AutoLock lock(lock_);

  if (is_shutdown_) {
    return MF_E_SHUTDOWN;
  }

  is_shutdown_ = true;
  return S_OK;
}

HRESULT MediaFoundationClearKeyDecryptor::GenerateDecoderBufferFromSample(
    IMFSample* mf_sample,
    GUID* key_id,
    scoped_refptr<DecoderBuffer>* buffer_out) {
  DVLOG_FUNC(3);
  CHECK(mf_sample);
  CHECK(key_id);

  ComPtr<IMFMediaBuffer> mf_buffer;
  ComPtr<IMFMediaBuffer> new_mf_buffer;
  DWORD buffer_count = 0;

  RETURN_IF_FAILED(mf_sample->GetBufferCount(&buffer_count));
  if (buffer_count != 1) {
    DLOG(ERROR) << __func__ << ": buffer_count=" << buffer_count;
    return MF_E_UNEXPECTED;
  }
  RETURN_IF_FAILED(mf_sample->GetBufferByIndex(0, &mf_buffer));

  BYTE* mf_buffer_data = nullptr;
  DWORD current_length = 0;
  RETURN_IF_FAILED(mf_buffer->Lock(&mf_buffer_data, nullptr, &current_length));
  auto decoder_buffer = DecoderBuffer::CopyFrom(
      // SAFETY: `IMFMediaBuffer::Lock` returns the length of `mf_buffer_data`
      // as `current_length`.
      // https://learn.microsoft.com/en-us/windows/win32/api/mfobjects/nf-mfobjects-imfmediabuffer-lock
      UNSAFE_BUFFERS(base::span(mf_buffer_data, current_length)));
  RETURN_IF_FAILED(mf_buffer->Unlock());

  UINT32 clean_point = 0;
  HRESULT hr = mf_sample->GetUINT32(MFSampleExtension_CleanPoint, &clean_point);
  if (hr == MF_E_ATTRIBUTENOTFOUND) {
    hr = S_OK;
  }
  RETURN_IF_FAILED(hr);
  decoder_buffer->set_is_key_frame(clean_point);

  // Only set the sample duration if the source sample has one.
  MFTIME sample_duration = 0;
  hr = mf_sample->GetSampleDuration(&sample_duration);
  if (hr == MF_E_NO_SAMPLE_DURATION) {
    hr = S_OK;
  } else {
    RETURN_IF_FAILED(hr);
    auto timedelta_sample_duration = MfTimeToTimeDelta(sample_duration);
    DVLOG_FUNC(3) << "timedelta_sample_duration=" << timedelta_sample_duration;
    decoder_buffer->set_duration(timedelta_sample_duration);
  }

  // Only set the sample time if the source sample has one.
  MFTIME sample_time = 0;
  hr = mf_sample->GetSampleTime(&sample_time);
  if (hr == MF_E_NO_SAMPLE_DURATION) {
    hr = S_OK;
  } else {
    RETURN_IF_FAILED(hr);
    auto timedelta_sample_time = MfTimeToTimeDelta(sample_time);
    DVLOG_FUNC(3) << "timedelta_sample_time=" << timedelta_sample_time;
    decoder_buffer->set_timestamp(timedelta_sample_time);
  }

  std::unique_ptr<DecryptConfig> decrypt_config;
  RETURN_IF_FAILED(
      CreateDecryptConfigFromSample(mf_sample, *key_id, &decrypt_config));
  decoder_buffer->set_decrypt_config(std::move(decrypt_config));
  *buffer_out = std::move(decoder_buffer);

  return S_OK;
}

void MediaFoundationClearKeyDecryptor::FlushAllStoredData() {
  base::AutoLock lock(lock_);
  DVLOG_FUNC(1) << ", sample_=" << sample_;

  if (sample_) {
    sample_.Reset();
  }
}

}  // namespace media
