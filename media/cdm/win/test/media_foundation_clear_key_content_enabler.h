// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CONTENT_ENABLER_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CONTENT_ENABLER_H_

#include <mfidl.h>
#include <wrl/implements.h>

namespace media {

class MediaFoundationClearKeyContentEnabler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentEnabler,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeyContentEnabler();
  MediaFoundationClearKeyContentEnabler(
      const MediaFoundationClearKeyContentEnabler&) = delete;
  MediaFoundationClearKeyContentEnabler& operator=(
      const MediaFoundationClearKeyContentEnabler&) = delete;
  ~MediaFoundationClearKeyContentEnabler() override;

  HRESULT RuntimeClassInitialize();

  // IMFContentEnabler
  STDMETHODIMP AutomaticEnable() override;
  STDMETHODIMP Cancel() override;
  STDMETHODIMP GetEnableData(_Outptr_result_bytebuffer_(*data_size) BYTE** data,
                             _Out_ DWORD* data_size) override;
  STDMETHODIMP GetEnableType(_Out_ GUID* type) override;
  STDMETHODIMP GetEnableURL(_Out_writes_bytes_(*url_size) LPWSTR* url,
                            _Out_ DWORD* url_size,
                            _Inout_ MF_URL_TRUST_STATUS* trust_status) override;
  STDMETHODIMP IsAutomaticSupported(_Out_ BOOL* automatic) override;
  STDMETHODIMP MonitorEnable() override;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CONTENT_ENABLER_H_
