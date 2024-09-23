// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_cdm_factory.h"

#include <mfapi.h>
#include <mferror.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/win/scoped_hstring.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "media/cdm/win/test/media_foundation_clear_key_cdm_access.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

ActivatableClass(MediaFoundationClearKeyCdmFactory);

MediaFoundationClearKeyCdmFactory::MediaFoundationClearKeyCdmFactory() {
  DVLOG_FUNC(1);
}

MediaFoundationClearKeyCdmFactory::~MediaFoundationClearKeyCdmFactory() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyCdmFactory::RuntimeClassInitialize() {
  DVLOG_FUNC(1);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return S_OK;
}

STDMETHODIMP_(BOOL)
MediaFoundationClearKeyCdmFactory::IsTypeSupported(_In_ LPCWSTR key_system,
                                                   _In_opt_ LPCWSTR
                                                       content_type) {
  DVLOG_FUNC(1) << "key_system=" << key_system;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return CompareStringOrdinal(kMediaFoundationClearKeyKeySystemWideString, -1,
                              key_system, -1, FALSE) == CSTR_EQUAL;
}

STDMETHODIMP
MediaFoundationClearKeyCdmFactory::CreateContentDecryptionModuleAccess(
    _In_ LPCWSTR key_system,
    _In_count_(num_configurations) IPropertyStore** configurations,
    _In_ DWORD num_configurations,
    _COM_Outptr_ IMFContentDecryptionModuleAccess** cdm_access) {
  DVLOG_FUNC(1) << "key_system=" << key_system;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (key_system == nullptr || key_system[0] == L'\0') {
    DVLOG_FUNC(1) << "Key system is null or empty.";
    return MF_TYPE_ERR;
  }

  if (num_configurations == 0) {
    DVLOG_FUNC(1) << "No available configuration.";
    return MF_TYPE_ERR;
  }

  if (!IsTypeSupported(key_system, nullptr)) {
    DVLOG_FUNC(1) << "Type is not supported.";
    return MF_NOT_SUPPORTED_ERR;
  }

  RETURN_IF_FAILED(
      (MakeAndInitialize<MediaFoundationClearKeyCdmAccess,
                         IMFContentDecryptionModuleAccess>(cdm_access)));

  return S_OK;
}

}  // namespace media
