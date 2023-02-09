// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_cdm.h"

#include <mfapi.h>
#include <mferror.h>
#include <wrl.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"
#include "media/cdm/win/test/media_foundation_clear_key_session.h"
#include "media/cdm/win/test/media_foundation_clear_key_trusted_input.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyCdm::MediaFoundationClearKeyCdm() {
  DETACH_FROM_THREAD(thread_checker_);
}

MediaFoundationClearKeyCdm::~MediaFoundationClearKeyCdm() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  Shutdown();
}

HRESULT MediaFoundationClearKeyCdm::RuntimeClassInitialize(
    _In_ IPropertyStore* properties) {
  DVLOG_FUNC(1);

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMFContentDecryptionModule
STDMETHODIMP MediaFoundationClearKeyCdm::SetContentEnabler(
    _In_ IMFContentEnabler* content_enabler,
    _In_ IMFAsyncResult* result) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  if (!content_enabler || !result) {
    return E_INVALIDARG;
  }

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetSuspendNotify(
    _COM_Outptr_ IMFCdmSuspendNotify** notify) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::SetPMPHostApp(IMFPMPHostApp* host) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::CreateSession(
    _In_ MF_MEDIAKEYSESSION_TYPE session_type,
    _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks,
    _COM_Outptr_ IMFContentDecryptionModuleSession** session) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  RETURN_IF_FAILED((MakeAndInitialize<MediaFoundationClearKeySession,
                                      IMFContentDecryptionModuleSession>(
      session, session_type, callbacks)));

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::SetServerCertificate(
    _In_reads_bytes_opt_(server_certificate_size)
        const BYTE* server_certificate,
    _In_ DWORD server_certificate_size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdm::CreateTrustedInput(
    _In_reads_bytes_(content_init_data_size) const BYTE* content_init_data,
    _In_ DWORD content_init_data_size,
    _COM_Outptr_ IMFTrustedInput** trusted_input) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  ComPtr<IMFTrustedInput> trusted_input_new;
  HRESULT hr =
      MakeAndInitialize<MediaFoundationClearKeyTrustedInput, IMFTrustedInput>(
          &trusted_input_new);
  RETURN_IF_FAILED(hr);

  *trusted_input = trusted_input_new.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetProtectionSystemIds(
    _Outptr_result_buffer_(*count) GUID** system_ids,
    _Out_ DWORD* count) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RETURN_IF_FAILED(GetShutdownStatus());

  *system_ids = nullptr;
  *count = 0;

  GUID* system_id = static_cast<GUID*>(CoTaskMemAlloc(sizeof(GUID)));
  if (!system_id) {
    return E_OUTOFMEMORY;
  }

  *system_id = MEDIA_FOUNDATION_CLEARKEY_GUID_CLEARKEY_PROTECTION_SYSTEM_ID;
  *system_ids = system_id;
  *count = 1;

  return S_OK;
}

// IMFGetService
STDMETHODIMP MediaFoundationClearKeyCdm::GetService(
    __RPC__in REFGUID guid_service,
    __RPC__in REFIID riid,
    __RPC__deref_out_opt LPVOID* object) {
  DVLOG_FUNC(1);

  // This method can be called from a different MF thread, so the
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_) is not checked here.

  RETURN_IF_FAILED(GetShutdownStatus());

  if (MF_CONTENTDECRYPTIONMODULE_SERVICE != guid_service) {
    return MF_E_UNSUPPORTED_SERVICE;
  }

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMFShutdown
STDMETHODIMP MediaFoundationClearKeyCdm::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::AutoLock lock(lock_);
  if (is_shutdown_) {
    return MF_E_SHUTDOWN;
  }

  is_shutdown_ = true;
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdm::GetShutdownStatus(
    MFSHUTDOWN_STATUS* status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Per IMFShutdown::GetShutdownStatus spec, MF_E_INVALIDREQUEST is returned if
  // Shutdown has not been called beforehand.
  base::AutoLock lock(lock_);
  if (!is_shutdown_) {
    return MF_E_INVALIDREQUEST;
  }

  return S_OK;
}

}  // namespace media
