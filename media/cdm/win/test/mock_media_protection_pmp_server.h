// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MOCK_MEDIA_PROTECTION_PMP_SERVER_H_
#define MEDIA_CDM_WIN_TEST_MOCK_MEDIA_PROTECTION_PMP_SERVER_H_

#include <mfidl.h>
#include <windows.media.protection.h>
#include <wrl/implements.h>

namespace media {

// Mock PMP server can be used under the situation where creating a real
// in-process PMP is not allowed or desired. PMP (Protected Media Path):
// https://learn.microsoft.com/en-us/windows/win32/medfound/protected-media-path
class MockMediaProtectionPMPServer final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ABI::Windows::Media::Protection::IMediaProtectionPMPServer,
          IMFGetService,
          Microsoft::WRL::FtmBase> {
 public:
  MockMediaProtectionPMPServer();
  ~MockMediaProtectionPMPServer() override;
  MockMediaProtectionPMPServer(const MockMediaProtectionPMPServer&) = delete;
  MockMediaProtectionPMPServer& operator=(const MockMediaProtectionPMPServer&) =
      delete;

  HRESULT RuntimeClassInitialize(
      ABI::Windows::Foundation::Collections::IPropertySet* property_pmp);

  // IInspectable
  STDMETHODIMP GetIids(_COM_Outptr_ ULONG* iidCount,
                       _COM_Outptr_ IID** iids) override;
  STDMETHODIMP GetRuntimeClassName(_COM_Outptr_ HSTRING* className) override;
  STDMETHODIMP GetTrustLevel(_COM_Outptr_ TrustLevel* trustLevel) override;

  // IMediaProtectionPMPServer
  STDMETHODIMP get_Properties(
      _COM_Outptr_ ABI::Windows::Foundation::Collections::IPropertySet**
          ppProperties) override;

  // IMFGetService
  STDMETHODIMP GetService(__RPC__in REFGUID guid_service,
                          __RPC__in REFIID riid,
                          __RPC__deref_out_opt LPVOID* ppv_object) override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IPropertySet>
      property_pmp_;
  Microsoft::WRL::ComPtr<IMFPMPServer> pmp_server_;
  Microsoft::WRL::ComPtr<IMFPMPHost> pmp_host_;
  Microsoft::WRL::ComPtr<IMFMediaSession> media_session_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MOCK_MEDIA_PROTECTION_PMP_SERVER_H_
