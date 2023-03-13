// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_H_

#include <mfcontentdecryptionmodule.h>
#include <mferror.h>
#include <mfidl.h>
#include <windows.media.protection.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/win/test/media_foundation_clear_key_session.h"

namespace media {

class MediaFoundationClearKeyCdm final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModule,
          IMFGetService,
          IMFShutdown,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeyCdm();
  MediaFoundationClearKeyCdm(const MediaFoundationClearKeyCdm&) = delete;
  MediaFoundationClearKeyCdm& operator=(const MediaFoundationClearKeyCdm&) =
      delete;
  ~MediaFoundationClearKeyCdm() override;

  HRESULT RuntimeClassInitialize(_In_ IPropertyStore* properties);

  // IMFContentDecryptionModule
  STDMETHODIMP SetContentEnabler(_In_ IMFContentEnabler* content_enabler,
                                 _In_ IMFAsyncResult* result) override;
  STDMETHODIMP GetSuspendNotify(
      _COM_Outptr_ IMFCdmSuspendNotify** notify) override;
  STDMETHODIMP SetPMPHostApp(IMFPMPHostApp* pmp_host_app) override;
  STDMETHODIMP CreateSession(
      MF_MEDIAKEYSESSION_TYPE session_type,
      IMFContentDecryptionModuleSessionCallbacks* callbacks,
      IMFContentDecryptionModuleSession** session) override;
  STDMETHODIMP SetServerCertificate(_In_reads_bytes_opt_(certificate_size)
                                        const BYTE* certificate,
                                    _In_ DWORD certificate_size) override;
  STDMETHODIMP CreateTrustedInput(
      _In_reads_bytes_(content_init_data_size) const BYTE* content_init_data,
      _In_ DWORD content_init_data_size,
      _COM_Outptr_ IMFTrustedInput** trusted_input) override;
  STDMETHODIMP GetProtectionSystemIds(_Outptr_result_buffer_(*system_ids)
                                          GUID** system_ids,
                                      _Out_ DWORD* count) override;
  // IMFGetService
  STDMETHODIMP GetService(__RPC__in REFGUID guid_service,
                          __RPC__in REFIID riid,
                          __RPC__deref_out_opt LPVOID* ppv_object) override;

  // IMFShutdown
  STDMETHODIMP Shutdown() override;
  STDMETHODIMP GetShutdownStatus(MFSHUTDOWN_STATUS* status) override;

 private:
  scoped_refptr<AesDecryptor> GetAesDecryptor();
  void OnSessionMessage(const std::string& session_id,
                        CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionClosed(const std::string& session_id,
                       CdmSessionClosedReason reason);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionIdCreated(
      const std::string& session_id,
      Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> session);
  void OnSessionIdRemoved(const std::string& session_id);
  MediaFoundationClearKeySession* FindSession(const std::string& session_id);

  // To use the hardware protection layer, we need to create an in-process PMP
  // server.
  Microsoft::WRL::ComPtr<
      ABI::Windows::Media::Protection::IMediaProtectionPMPServer>
      media_protection_pmp_server_;
  scoped_refptr<AesDecryptor> aes_decryptor_;

  // Session ID to session map.
  std::map<std::string,
           Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession>>
      sessions_;

  HRESULT GetShutdownStatus() {
    base::AutoLock lock(lock_);
    return (is_shutdown_) ? MF_E_SHUTDOWN : S_OK;
  }

  // To protect access to `is_shutdown_` from multiple threads.
  // `SetContentEnabler`, `CreateTrustedInput` and `GetService` may run from MF
  // work queue threads. Note that this `lock_` does not lock the whole
  // function, so it is still possible that Shutdown() is called when above
  // calls are running.
  base::Lock lock_;

  // For IMFShutdown
  bool is_shutdown_ GUARDED_BY(lock_) = false;

  // Thread checker to enforce that this object is used on a specific thread.
  THREAD_CHECKER(thread_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationClearKeyCdm> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_CDM_H_
