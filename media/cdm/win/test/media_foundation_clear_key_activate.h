// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_ACTIVATE_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_ACTIVATE_H_

#include <mfobjects.h>
#include <wrl/implements.h>

namespace media {

class MediaFoundationClearKeyActivate
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFActivate,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeyActivate();
  MediaFoundationClearKeyActivate(const MediaFoundationClearKeyActivate&) =
      delete;
  MediaFoundationClearKeyActivate& operator=(
      const MediaFoundationClearKeyActivate&) = delete;
  ~MediaFoundationClearKeyActivate() override;

  HRESULT RuntimeClassInitialize();

  // IMFActivate
  STDMETHODIMP ActivateObject(_In_ REFIID riid,
                              _COM_Outptr_ void** ppv) override;
  STDMETHODIMP ShutdownObject() override;
  STDMETHODIMP DetachObject() override;

  // IMFAttributes inherited by IMFActivate
  STDMETHODIMP GetItem(__RPC__in REFGUID guidKey,
                       __RPC__inout_opt PROPVARIANT* pValue) override;
  STDMETHODIMP GetItemType(__RPC__in REFGUID guidKey,
                           __RPC__out MF_ATTRIBUTE_TYPE* pType) override;
  STDMETHODIMP CompareItem(__RPC__in REFGUID guidKey,
                           __RPC__in REFPROPVARIANT Value,
                           __RPC__out BOOL* pbResult) override;
  STDMETHODIMP Compare(__RPC__in_opt IMFAttributes* pTheirs,
                       MF_ATTRIBUTES_MATCH_TYPE MatchType,
                       __RPC__out BOOL* pbResult) override;
  STDMETHODIMP GetUINT32(__RPC__in REFGUID guidKey,
                         __RPC__out UINT32* punValue) override;
  STDMETHODIMP GetUINT64(__RPC__in REFGUID guidKey,
                         __RPC__out UINT64* punValue) override;
  STDMETHODIMP GetDouble(__RPC__in REFGUID guidKey,
                         __RPC__out double* pfValue) override;
  STDMETHODIMP GetGUID(__RPC__in REFGUID guidKey,
                       __RPC__out GUID* pguidValue) override;
  STDMETHODIMP GetStringLength(__RPC__in REFGUID guidKey,
                               __RPC__out UINT32* pcchLength) override;
  STDMETHODIMP GetString(__RPC__in REFGUID guidKey,
                         __RPC__out_ecount_full(cchBufSize) LPWSTR pwszValue,
                         UINT32 cchBufSize,
                         __RPC__inout_opt UINT32* pcchLength) override;
  STDMETHODIMP GetAllocatedString(
      __RPC__in REFGUID guidKey,
      __RPC__deref_out_ecount_full_opt((*pcchLength + 1)) LPWSTR* ppwszValue,
      __RPC__out UINT32* pcchLength) override;
  STDMETHODIMP GetBlobSize(__RPC__in REFGUID guidKey,
                           __RPC__out UINT32* pcbBlobSize) override;
  STDMETHODIMP GetBlob(__RPC__in REFGUID guidKey,
                       __RPC__out_ecount_full(cbBufSize) UINT8* pBuf,
                       UINT32 cbBufSize,
                       __RPC__inout_opt UINT32* pcbBlobSize) override;
  STDMETHODIMP GetAllocatedBlob(__RPC__in REFGUID guidKey,
                                __RPC__deref_out_ecount_full_opt(*pcbSize)
                                    UINT8** ppBuf,
                                __RPC__out UINT32* pcbSize) override;
  STDMETHODIMP GetUnknown(__RPC__in REFGUID guidKey,
                          __RPC__in REFIID riid,
                          __RPC__deref_out_opt LPVOID* ppv) override;
  STDMETHODIMP SetItem(__RPC__in REFGUID guidKey,
                       __RPC__in REFPROPVARIANT Value) override;
  STDMETHODIMP DeleteItem(__RPC__in REFGUID guidKey) override;
  STDMETHODIMP DeleteAllItems() override;
  STDMETHODIMP SetUINT32(__RPC__in REFGUID guidKey, UINT32 unValue) override;
  STDMETHODIMP SetUINT64(__RPC__in REFGUID guidKey, UINT64 unValue) override;
  STDMETHODIMP SetDouble(__RPC__in REFGUID guidKey, double fValue) override;
  STDMETHODIMP SetGUID(__RPC__in REFGUID guidKey,
                       __RPC__in REFGUID guidValue) override;
  STDMETHODIMP SetString(__RPC__in REFGUID guidKey,
                         __RPC__in_string LPCWSTR wszValue) override;
  STDMETHODIMP SetBlob(__RPC__in REFGUID guidKey,
                       __RPC__in_ecount_full(cbBufSize) const UINT8* pBuf,
                       UINT32 cbBufSize) override;
  STDMETHODIMP SetUnknown(__RPC__in REFGUID guidKey,
                          __RPC__in_opt IUnknown* pUnknown) override;
  STDMETHODIMP LockStore() override;
  STDMETHODIMP UnlockStore() override;
  STDMETHODIMP GetCount(__RPC__out UINT32* pcItems) override;
  STDMETHODIMP GetItemByIndex(UINT32 unIndex,
                              __RPC__out GUID* pguidKey,
                              __RPC__inout_opt PROPVARIANT* pValue) override;
  STDMETHODIMP CopyAllItems(__RPC__in_opt IMFAttributes* pDest) override;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_ACTIVATE_H_
