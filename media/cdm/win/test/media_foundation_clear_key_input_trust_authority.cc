// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_input_trust_authority.h"

#include <mfapi.h>
#include <mferror.h>
#include <nserror.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <memory>

#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_activate.h"
#include "media/cdm/win/test/media_foundation_clear_key_decryptor.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyInputTrustAuthority::
    MediaFoundationClearKeyInputTrustAuthority() = default;

MediaFoundationClearKeyInputTrustAuthority::
    ~MediaFoundationClearKeyInputTrustAuthority() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyInputTrustAuthority::RuntimeClassInitialize(
    _In_ UINT32 stream_id) {
  DVLOG_FUNC(1) << "stream_id=" << stream_id;

  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMFInputTrustAuthority
STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::GetDecrypter(
    REFIID riid,
    void** ppv) {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(GetShutdownStatus());

  ComPtr<IMFTransform> mf_decryptor;
  HRESULT hr =
      MakeAndInitialize<MediaFoundationClearKeyDecryptor, IMFTransform>(
          &mf_decryptor);
  RETURN_IF_FAILED(hr);
  RETURN_IF_FAILED(mf_decryptor.CopyTo(riid, ppv));

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::RequestAccess(
    _In_ MFPOLICYMANAGER_ACTION action,
    _COM_Outptr_ IMFActivate** content_enabler_activate) {
  DVLOG_FUNC(1);
  RETURN_IF_FAILED(GetShutdownStatus());

  *content_enabler_activate = nullptr;

  // The ITA only allows the PLAY, EXTRACT and NO actions
  // NOTE: Topology created only on the basis of EXTRACT or NO action will NOT
  // decrypt content.
  if (PEACTION_EXTRACT == action || PEACTION_NO == action) {
    return S_OK;
  }

  if (PEACTION_PLAY != action) {
    return MF_E_ITA_UNSUPPORTED_ACTION;
  }

  ComPtr<IMFActivate> activate;
  RETURN_IF_FAILED(
      MakeAndInitialize<MediaFoundationClearKeyActivate>(&activate));

  *content_enabler_activate = activate.Detach();

  return NS_E_LICENSE_REQUIRED;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::GetPolicy(
    _In_ MFPOLICYMANAGER_ACTION action,
    _COM_Outptr_ IMFOutputPolicy** policy) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::BindAccess(
    _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* params) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::UpdateAccess(
    _In_ MFINPUTTRUSTAUTHORITY_ACCESS_PARAMS* params) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::Reset() {
  RETURN_IF_FAILED(GetShutdownStatus());
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMFShutdown
STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::GetShutdownStatus(
    _Out_ MFSHUTDOWN_STATUS* status) {
  base::AutoLock lock(lock_);
  if (is_shutdown_) {
    *status = MFSHUTDOWN_COMPLETED;
    return S_OK;
  }

  return MF_E_INVALIDREQUEST;
}

STDMETHODIMP MediaFoundationClearKeyInputTrustAuthority::Shutdown() {
  base::AutoLock lock(lock_);

  if (is_shutdown_) {
    return MF_E_SHUTDOWN;
  }

  is_shutdown_ = true;
  return S_OK;
}

}  // namespace media
