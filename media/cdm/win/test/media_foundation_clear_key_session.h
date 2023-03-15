// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_SESSION_H_
#define MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_SESSION_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "media/cdm/aes_decryptor.h"

namespace media {

// Called when the session specified by `session_id` is created or removed.
using SessionIdCB = base::OnceCallback<void(const std::string&)>;
using SessionIdCreatedCB = base::OnceCallback<void(
    const std::string&,
    Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession>)>;

class MediaFoundationClearKeySession final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMFContentDecryptionModuleSession,
          Microsoft::WRL::FtmBase> {
 public:
  MediaFoundationClearKeySession();
  MediaFoundationClearKeySession(const MediaFoundationClearKeySession&) =
      delete;
  MediaFoundationClearKeySession& operator=(
      const MediaFoundationClearKeySession&) = delete;
  ~MediaFoundationClearKeySession() override;

  HRESULT RuntimeClassInitialize(
      _In_ MF_MEDIAKEYSESSION_TYPE session_type,
      _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks,
      _In_ scoped_refptr<AesDecryptor> aes_decryptor,
      _In_ SessionIdCreatedCB session_id_created_cb,
      _In_ SessionIdCB session_id_removed_cb);

  // IMFContentDecryptionModuleSession
  STDMETHODIMP Update(_In_reads_bytes_(response_size) const BYTE* response,
                      _In_ DWORD response_size) override;
  STDMETHODIMP Close() override;
  STDMETHODIMP GetSessionId(_COM_Outptr_ LPWSTR* id) override;
  STDMETHODIMP GetKeyStatuses(_Outptr_result_buffer_(*key_statuses_count)
                                  MFMediaKeyStatus** key_statuses,
                              _Out_ UINT* key_statuses_count) override;
  STDMETHODIMP Load(_In_ LPCWSTR session_id, _Out_ BOOL* loaded) override;
  STDMETHODIMP GenerateRequest(_In_ LPCWSTR init_data_type,
                               _In_reads_bytes_(init_data_size)
                                   const BYTE* init_data,
                               _In_ DWORD init_data_size) override;
  STDMETHODIMP GetExpiration(_Out_ double* expiration) override;
  STDMETHODIMP Remove() override;

  void OnSessionMessage(const std::string& session_id,
                        CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionClosed(const std::string& session_id,
                       CdmSessionClosedReason reason);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);

 private:
  void OnSessionCreated(const std::string& session_id);

  MF_MEDIAKEYSESSION_TYPE session_type_ = MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSessionCallbacks> callbacks_;
  scoped_refptr<AesDecryptor> aes_decryptor_;
  std::string session_id_;
  CdmKeysInfo keys_info_;
  SessionIdCreatedCB session_id_created_cb_;
  SessionIdCB session_id_removed_cb_;

  // Thread checker to enforce that this object is used on a specific thread.
  THREAD_CHECKER(thread_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationClearKeySession> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_SESSION_H_
