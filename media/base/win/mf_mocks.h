// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_MOCKS_H_
#define MEDIA_BASE_WIN_MF_MOCKS_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "media/base/win/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockMFCdmFactory
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleFactory> {
 public:
  MockMFCdmFactory();
  ~MockMFCdmFactory() override;

  // IMFContentDecryptionModuleFactory methods
  MOCK_STDCALL_METHOD2(IsTypeSupported,
                       BOOL(LPCWSTR key_system, LPCWSTR content_type));
  MOCK_STDCALL_METHOD4(CreateContentDecryptionModuleAccess,
                       HRESULT(LPCWSTR key_system,
                               IPropertyStore** configurations,
                               DWORD num_configurations,
                               IMFContentDecryptionModuleAccess** cdm_access));
};

class MockMFCdmAccess
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleAccess> {
 public:
  MockMFCdmAccess();
  ~MockMFCdmAccess() override;

  // IMFContentDecryptionModuleAccess methods
  MOCK_STDCALL_METHOD2(CreateContentDecryptionModule,
                       HRESULT(IPropertyStore* properties,
                               IMFContentDecryptionModule** cdm));
  MOCK_STDCALL_METHOD1(GetConfiguration, HRESULT(IPropertyStore** config));
  MOCK_STDCALL_METHOD1(GetKeySystem, HRESULT(LPWSTR* key_system));
};

class MockMFCdm
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModule> {
 public:
  MockMFCdm();
  ~MockMFCdm() override;

  // IMFContentDecryptionModule methods
  MOCK_STDCALL_METHOD2(SetContentEnabler,
                       HRESULT(IMFContentEnabler* content_enabler,
                               IMFAsyncResult* result));
  MOCK_STDCALL_METHOD1(GetSuspendNotify, HRESULT(IMFCdmSuspendNotify** notify));
  MOCK_STDCALL_METHOD1(SetPMPHostApp, HRESULT(IMFPMPHostApp* pmp_host_app));
  MOCK_STDCALL_METHOD3(
      CreateSession,
      HRESULT(MF_MEDIAKEYSESSION_TYPE session_type,
              IMFContentDecryptionModuleSessionCallbacks* callbacks,
              IMFContentDecryptionModuleSession** session));
  MOCK_STDCALL_METHOD2(SetServerCertificate,
                       HRESULT(const BYTE* certificate,
                               DWORD certificate_size));
  MOCK_STDCALL_METHOD3(CreateTrustedInput,
                       HRESULT(const BYTE* content_init_data,
                               DWORD content_init_data_size,
                               IMFTrustedInput** trusted_input));
  MOCK_STDCALL_METHOD2(GetProtectionSystemIds,
                       HRESULT(GUID** system_ids, DWORD* count));
};

class MockMFCdmSession
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleSession> {
 public:
  MockMFCdmSession();
  ~MockMFCdmSession() override;

  // IMFContentDecryptionModuleSession methods
  MOCK_STDCALL_METHOD1(GetSessionId, HRESULT(LPWSTR* session_id));
  MOCK_STDCALL_METHOD1(GetExpiration, HRESULT(double* expiration));
  MOCK_STDCALL_METHOD2(GetKeyStatuses,
                       HRESULT(MFMediaKeyStatus** key_statuses,
                               UINT* num_key_statuses));
  MOCK_STDCALL_METHOD2(Load, HRESULT(LPCWSTR session_id, BOOL* loaded));
  MOCK_STDCALL_METHOD3(GenerateRequest,
                       HRESULT(LPCWSTR init_data_type,
                               const BYTE* init_data,
                               DWORD init_data_size));
  MOCK_STDCALL_METHOD2(Update,
                       HRESULT(const BYTE* response, DWORD response_size));
  MOCK_STDCALL_METHOD0(Close, HRESULT());
  MOCK_STDCALL_METHOD0(Remove, HRESULT());
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_MOCKS_H_
