// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_content_enabler.h"

#include <mferror.h>

#include "base/notreached.h"
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
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::Cancel() {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableData(
    _Outptr_result_bytebuffer_(*data_size) BYTE** data,
    _Out_ DWORD* data_size) {
  DVLOG_FUNC(3);

  // Does not support this method of content enabling with EME.
  return MF_E_NOT_AVAILABLE;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableURL(
    _Out_writes_bytes_(*url_size) LPWSTR* url,
    _Out_ DWORD* url_size,
    _Inout_ MF_URL_TRUST_STATUS* trust_status) {
  DVLOG_FUNC(3);

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
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyContentEnabler::GetEnableType(
    _Out_ GUID* type) {
  DVLOG_FUNC(1);

  if (!type) {
    return E_INVALIDARG;
  }

  *type = MEDIA_FOUNDATION_CLEARKEY_GUID_CONTENT_ENABLER_TYPE;
  return S_OK;
}

}  // namespace media
