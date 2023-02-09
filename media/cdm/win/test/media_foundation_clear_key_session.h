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
#include "base/threading/thread_checker.h"

namespace media {

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

  using HasUsableKeyChangedCB =
      base::RepeatingCallback<void(const std::wstring& session_id,
                                   bool has_usable_key)>;

  HRESULT RuntimeClassInitialize(
      _In_ MF_MEDIAKEYSESSION_TYPE session_type,
      _In_ IMFContentDecryptionModuleSessionCallbacks* callbacks);

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

 private:
  MF_MEDIAKEYSESSION_TYPE session_type_ = MF_MEDIAKEYSESSION_TYPE_TEMPORARY;
  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSessionCallbacks> callbacks_;

  bool is_closed_ = false;

  // Thread checker to enforce that this object is used on a specific thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_TEST_MEDIA_FOUNDATION_CLEAR_KEY_SESSION_H_
