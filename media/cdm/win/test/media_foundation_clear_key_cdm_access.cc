// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_cdm_access.h"

#include <mfapi.h>
#include <mferror.h>

#include "base/notreached.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/win/test/media_foundation_clear_key_cdm.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyCdmAccess::MediaFoundationClearKeyCdmAccess() {
  DVLOG_FUNC(1);
}

MediaFoundationClearKeyCdmAccess::~MediaFoundationClearKeyCdmAccess() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyCdmAccess::RuntimeClassInitialize() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdmAccess::CreateContentDecryptionModule(
    _In_opt_ IPropertyStore* properties,
    _COM_Outptr_ IMFContentDecryptionModule** cdm) {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (properties == nullptr) {
    DVLOG_FUNC(1) << "properties cannot be null.";
    return MF_E_UNEXPECTED;
  }

  *cdm = nullptr;

  ComPtr<IMFContentDecryptionModule> mf_cdm;
  RETURN_IF_FAILED((
      MakeAndInitialize<MediaFoundationClearKeyCdm, IMFContentDecryptionModule>(
          &mf_cdm, properties)));

  *cdm = mf_cdm.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyCdmAccess::GetConfiguration(
    _COM_Outptr_ IPropertyStore** output) {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyCdmAccess::GetKeySystem(
    _COM_Outptr_ LPWSTR* key_system) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Only a single key system is supported.
  return CopyCoTaskMemWideString(kMediaFoundationClearKeyKeySystemWideString,
                                 key_system);
}

}  // namespace media
