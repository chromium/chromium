// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_content_enabler.h"

#include <mferror.h>

#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_guids.h"

namespace media {

MediaFoundationClearKeyContentEnabler::MediaFoundationClearKeyContentEnabler() =
    default;

MediaFoundationClearKeyContentEnabler::
    ~MediaFoundationClearKeyContentEnabler() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyContentEnabler::RuntimeClassInitialize() {
  DVLOG_FUNC(1);
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::AutomaticEnable() {
  // API not used
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::Cancel() {
  // API not used
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableData(
    _Outptr_result_bytebuffer_(*data_size) BYTE** data,
    _Out_ DWORD* data_size) {
  // Does not support this method of content enabling with EME.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableURL(
    _Out_writes_bytes_(*url_size) LPWSTR* url,
    _Out_ DWORD* url_size,
    _Inout_ MF_URL_TRUST_STATUS* trust_status) {
  // Does not support this method of content enabling with EME.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::IsAutomaticSupported(
    _Out_ BOOL* automatic) {
  if (!automatic) {
    return E_INVALIDARG;
  }

  *automatic = FALSE;
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::MonitorEnable() {
  // API not used
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableType(
    _Out_ GUID* type) {
  if (!type) {
    return E_INVALIDARG;
  }

  *type = MEDIA_FOUNDATION_CLEARKEY_GUID_CONTENT_ENABLER_TYPE;
  return S_OK;
}

}  // namespace media
