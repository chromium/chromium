// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_session.h"

#include <mfapi.h>
#include <mferror.h>
#include <wrl/client.h>

#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

using Microsoft::WRL::ComPtr;

MediaFoundationClearKeySession::MediaFoundationClearKeySession() = default;

MediaFoundationClearKeySession::~MediaFoundationClearKeySession() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeySession::RuntimeClassInitialize(
    _In_ MF_MEDIAKEYSESSION_TYPE session_type,
    _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_type_ = session_type;
  callbacks_ = callbacks;

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeySession::Update(
    _In_reads_bytes_(response_size) const BYTE* response,
    _In_ DWORD response_size) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_closed_) {
    return MF_INVALID_STATE_ERR;
  }

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::Close() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  is_closed_ = true;

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::GetSessionId(
    _COM_Outptr_ LPWSTR* id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::GetKeyStatuses(
    _Outptr_result_buffer_(*key_statuses_size) MFMediaKeyStatus** key_statuses,
    _Out_ UINT* key_statuses_count) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::Load(_In_ LPCWSTR session_id,
                                                  _Out_ BOOL* loaded) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP MediaFoundationClearKeySession::GenerateRequest(
    _In_ LPCWSTR init_data_type,
    _In_reads_bytes_(init_data_size) const BYTE* init_data,
    _In_ DWORD init_data_size) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_closed_) {
    return MF_INVALID_STATE_ERR;
  }

  DVLOG_FUNC(2) << "init_data_size=" << init_data_size;

  if (!init_data || init_data_size == 0) {
    return MF_TYPE_ERR;
  }

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::GetExpiration(
    _Out_ double* expiration) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeySession::Remove() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

}  // namespace media
