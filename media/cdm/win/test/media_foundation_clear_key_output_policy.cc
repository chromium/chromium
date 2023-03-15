// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/test/media_foundation_clear_key_output_policy.h"

#include <mfapi.h>
#include <mferror.h>
#include <wrl/implements.h>

#include "base/notreached.h"
#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

MediaFoundationClearKeyOutputPolicy::MediaFoundationClearKeyOutputPolicy() =
    default;

MediaFoundationClearKeyOutputPolicy::~MediaFoundationClearKeyOutputPolicy() {
  DVLOG_FUNC(1);
}

HRESULT MediaFoundationClearKeyOutputPolicy::RuntimeClassInitialize(
    MFPOLICYMANAGER_ACTION action) {
  DVLOG_FUNC(1) << "action=" << action;

  if (action != PEACTION_PLAY && action != PEACTION_EXTRACT &&
      action != PEACTION_NO) {
    return MF_E_UNEXPECTED;
  }

  return S_OK;
}

// IMFOutputPolicy
STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GenerateRequiredSchemas(
    _In_ DWORD attributes,
    _In_ GUID guid_output_subtype,
    _In_count_(protection_schemas_supported_count)
        GUID* guid_protection_schemas_supported,
    _In_ DWORD protection_schemas_supported_count,
    _COM_Outptr_ IMFCollection** required_protection_schemas) {
  // We don't require an OTA (output trust authority) to enforce for testing.
  // However, we still need to return an empty collection on success.
  ComPtr<IMFCollection> collection;
  RETURN_IF_FAILED(MFCreateCollection(&collection));

  *required_protection_schemas = collection.Detach();

  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetOriginatorID(
    _Out_ GUID* guid_originator_id) {
  DVLOG_FUNC(1);
  *guid_originator_id = GUID_NULL;
  return S_OK;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetMinimumGRLVersion(
    _Out_ DWORD* minimum_grl_version) {
  DVLOG_FUNC(1);
  *minimum_grl_version = 0;
  return S_OK;
}

// IMFAttributes inherited by IMFOutputPolicy
STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetItem(
    __RPC__in REFGUID guidKey,
    __RPC__inout_opt PROPVARIANT* pValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetItemType(
    __RPC__in REFGUID guidKey,
    __RPC__out MF_ATTRIBUTE_TYPE* pType) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::CompareItem(
    __RPC__in REFGUID guidKey,
    __RPC__in REFPROPVARIANT Value,
    __RPC__out BOOL* pbResult) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::Compare(
    __RPC__in_opt IMFAttributes* pTheirs,
    MF_ATTRIBUTES_MATCH_TYPE MatchType,
    __RPC__out BOOL* pbResult) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetUINT32(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* punValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetUINT64(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT64* punValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetDouble(
    __RPC__in REFGUID guidKey,
    __RPC__out double* pfValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetGUID(
    __RPC__in REFGUID guidKey,
    __RPC__out GUID* pguidValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetStringLength(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetString(
    __RPC__in REFGUID guidKey,
    __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue,
    UINT32 cchBufSize,
    __RPC__inout_opt UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetAllocatedString(
    __RPC__in REFGUID guidKey,
    __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR* ppwszValue,
    __RPC__out UINT32* pcchLength) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetBlobSize(
    __RPC__in REFGUID guidKey,
    __RPC__out UINT32* pcbBlobSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetBlob(
    __RPC__in REFGUID guidKey,
    __RPC__out_ecount_full(cbBufSize) UINT8* pBuf,
    UINT32 cbBufSize,
    __RPC__inout_opt UINT32* pcbBlobSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetAllocatedBlob(
    __RPC__in REFGUID guidKey,
    __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8** ppBuf,
    __RPC__out UINT32* pcbSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetUnknown(
    __RPC__in REFGUID guidKey,
    __RPC__in REFIID riid,
    __RPC__deref_out_opt LPVOID* ppv) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetItem(
    __RPC__in REFGUID guidKey,
    __RPC__in REFPROPVARIANT Value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::DeleteItem(
    __RPC__in REFGUID guidKey) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::DeleteAllItems() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetUINT32(__RPC__in REFGUID
                                                                guidKey,
                                                            UINT32 unValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetUINT64(__RPC__in REFGUID
                                                                guidKey,
                                                            UINT64 unValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetDouble(__RPC__in REFGUID
                                                                guidKey,
                                                            double fValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetGUID(
    __RPC__in REFGUID guidKey,
    __RPC__in REFGUID guidValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetString(
    __RPC__in REFGUID guidKey,
    __RPC__in_string LPCWSTR wszValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetBlob(
    __RPC__in REFGUID guidKey,
    __RPC__in_ecount_full(cbBufSize) const UINT8* pBuf,
    UINT32 cbBufSize) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::SetUnknown(
    __RPC__in REFGUID guidKey,
    __RPC__in_opt IUnknown* pUnknown) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::LockStore() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::UnlockStore() {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetCount(
    __RPC__out UINT32* pcItems) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::GetItemByIndex(
    UINT32 unIndex,
    __RPC__out GUID* pguidKey,
    __RPC__inout_opt PROPVARIANT* pValue) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

STDMETHODIMP MediaFoundationClearKeyOutputPolicy::CopyAllItems(
    __RPC__in_opt IMFAttributes* pDest) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

}  // namespace media
