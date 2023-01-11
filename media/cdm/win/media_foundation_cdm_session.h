// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace media {

// A class wrapping IMFContentDecryptionModuleSession.
class MEDIA_EXPORT MediaFoundationCdmSession {
 public:
  MediaFoundationCdmSession(
      const std::string& uma_prefix,
      const SessionMessageCB& session_message_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);
  MediaFoundationCdmSession(const MediaFoundationCdmSession&) = delete;
  MediaFoundationCdmSession& operator=(const MediaFoundationCdmSession&) =
      delete;
  ~MediaFoundationCdmSession();

  // Initializes the session. All other methods should only be called after
  // Initialize() returns S_OK.
  HRESULT Initialize(IMFContentDecryptionModule* mf_cdm,
                     CdmSessionType session_type);

  // EME MediaKeySession methods. Returns S_OK on success, otherwise forwards
  // the HRESULT from IMFContentDecryptionModuleSession.

  // Callback to pass the session ID to the caller. The return value indicates
  // whether the session ID is accepted by the caller. If returns false, the
  // session ID is rejected by the caller (e.g. empty of duplicate session IDs),
  // and |this| could be destructed immediately by the caller.
  using SessionIdCB = base::OnceCallback<bool(const std::string&)>;

  // Creates session ID and generates requests. Returns an error HRESULT on
  // immediate failure, in which case no callbacks will be run. Otherwise,
  // returns S_OK, with the following two cases:
  // - If |session_id_| is successfully set, |session_id_cb| will be run with
  // |session_id_| followed by the session message via |session_message_cb_|.
  // - Otherwise, |session_id_cb| will be run with an empty session ID to
  // indicate error. No session message in this case.
  HRESULT GenerateRequest(EmeInitDataType init_data_type,
                          const std::vector<uint8_t>& init_data,
                          SessionIdCB session_id_cb);

  HRESULT Load(const std::string& session_id);
  HRESULT Update(const std::vector<uint8_t>& response);
  HRESULT Close();
  HRESULT Remove();

 private:
  // A wrapper function to report UMA for the HRESULT `hr` of the `api` call.
  // Returns the `hr` as is for chained calls.
  HRESULT WithUmaReported(HRESULT hr, const std::string& api);

  // Callbacks for forwarding session events.
  void OnSessionMessage(CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange();

  // Sets |session_id_| and returns whether the operation succeeded.
  // Note: |this| could already been destructed if false is returned.
  bool SetSessionId();

  HRESULT UpdateExpirationIfNeeded();

  const std::string uma_prefix_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  Microsoft::WRL::ComPtr<IMFContentDecryptionModuleSession> mf_cdm_session_;

  // Callback passed in GenerateRequest() to return the session ID.
  SessionIdCB session_id_cb_;

  std::string session_id_;

  base::Time expiration_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<MediaFoundationCdmSession> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_SESSION_H_
