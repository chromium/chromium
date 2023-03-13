// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_activate.h"

#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"
#include "media/cdm/win/test/media_foundation_clear_key_content_enabler.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyActivate::MediaFoundationClearKeyActivate() = default;

MediaFoundationClearKeyActivate::~MediaFoundationClearKeyActivate() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyActivate::RuntimeClassInitialize() {
  DVLOG_FUNC(1);
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyActivate::ActivateObject(
    _In_ REFIID riid,
    _COM_Outptr_ void** ppv) {
  DVLOG_FUNC(1);

  *ppv = nullptr;

  ComPtr<IMFContentEnabler> content_enabler;
  RETURN_IF_FAILED(MakeAndInitialize<MediaFoundationClearKeyContentEnabler>(
      &content_enabler));

  RETURN_IF_FAILED(content_enabler.CopyTo(riid, ppv));
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyActivate::ShutdownObject() {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::DetachObject() {
  DVLOG_FUNC(3);

  // API not used.
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

// IMFAttributes inherited by IMFActivate
STDMETHODIMP MediaFoundationClearKeyActivate::GetItem(
    __RPC__in REFGUID guidKey,
    __RPC__inout_opt PROPVARIANT* pValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetItemType(
    __RPC__in REFGUID guidKey,
    __RPC__out MF_ATTRIBUTE_TYPE* pType) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::CompareItem(
    __RPC__in REFGUID guidKey,
    __RPC__in REFPROPVARIANT Value,
    __RPC__out BOOL* pbResult) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::Compare(
    __RPC__in_opt IMFAttributes* pTheirs,
    MF_ATTRIBUTES_MATCH_TYPE MatchType,
    __RPC__out BOOL* pbResult) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetUINT32(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* punValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetUINT64(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT64* punValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetDouble(
    __RPC__in REFGUID guidKey,
    __RPC__out double* pfValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetGUID(
    __RPC__in REFGUID guidKey,
    __RPC__out GUID* pguidValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetStringLength(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetString(
    __RPC__in REFGUID guidKey,
    __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue,
    UINT32 cchBufSize,
    __RPC__inout_opt UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetAllocatedString(
    __RPC__in REFGUID guidKey,
    __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR* ppwszValue,
    __RPC__out UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetBlobSize(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* pcbBlobSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetBlob(
    __RPC__in REFGUID guidKey,
    __RPC__out_ecount_full(cbBufSize) UINT8* pBuf,
    UINT32 cbBufSize,
    __RPC__inout_opt UINT32* pcbBlobSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetAllocatedBlob(
    __RPC__in REFGUID guidKey,
    __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8** ppBuf,
    __RPC__out UINT32* pcbSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetUnknown(
    __RPC__in REFGUID guidKey,
    __RPC__in REFIID riid,
    __RPC__deref_out_opt LPVOID* ppv) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetItem(__RPC__in REFGUID guidKey,
                                                      __RPC__in REFPROPVARIANT
                                                          Value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::DeleteItem(
    __RPC__in REFGUID guidKey) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::DeleteAllItems() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetUINT32(__RPC__in REFGUID
                                                            guidKey,
                                                        UINT32 unValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetUINT64(__RPC__in REFGUID
                                                            guidKey,
                                                        UINT64 unValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetDouble(__RPC__in REFGUID
                                                            guidKey,
                                                        double fValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetGUID(__RPC__in REFGUID guidKey,
                                                      __RPC__in REFGUID
                                                          guidValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetString(
    __RPC__in REFGUID guidKey,
    __RPC__in_string LPCWSTR wszValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetBlob(
    __RPC__in REFGUID guidKey,
    __RPC__in_ecount_full(cbBufSize) const UINT8* pBuf,
    UINT32 cbBufSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::SetUnknown(
    __RPC__in REFGUID guidKey,
    __RPC__in_opt IUnknown* pUnknown) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::LockStore() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::UnlockStore() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetCount(
    __RPC__out UINT32* pcItems) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::GetItemByIndex(
    UINT32 unIndex,
    __RPC__out GUID* pguidKey,
    __RPC__inout_opt PROPVARIANT* pValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyActivate::CopyAllItems(
    __RPC__in_opt IMFAttributes* pDest) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

}  // namespace media
