// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CLEAR_KEY_PERSISTENT_SESSION_CDM_H_
#define MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CLEAR_KEY_PERSISTENT_SESSION_CDM_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/library_cdm/cdm_host_proxy.h"
#include "media/cdm/library_cdm/clear_key_cdm/cdm_file_adapter.h"

namespace media {

// This class is a wrapper on top of AesDecryptor that supports persistent
// sessions. LoadSession(), UpdateSession(), and RemoveSession() have
// special handling.
class ClearKeyPersistentSessionCdm : public ContentDecryptionModule {
 public:
  ClearKeyPersistentSessionCdm(
      CdmHostProxy* cdm_host_proxy,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);

  ClearKeyPersistentSessionCdm(const ClearKeyPersistentSessionCdm&) = delete;
  ClearKeyPersistentSessionCdm& operator=(const ClearKeyPersistentSessionCdm&) =
      delete;

  // ContentDecryptionModule implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) override;
  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) override;
  CdmContext* GetCdmContext() override;

 private:
  ~ClearKeyPersistentSessionCdm() override;

  // When LoadSession() is called, first open and read the session state.
  // Then call |cdm_| to create the session with the state provided.
  void OnFileOpenedForLoadSession(const std::string& session_id,
                                  std::unique_ptr<CdmFileAdapter> file,
                                  std::unique_ptr<NewSessionCdmPromise> promise,
                                  CdmFileAdapter::Status status);
  void OnFileReadForLoadSession(const std::string& session_id,
                                std::unique_ptr<CdmFileAdapter> file,
                                std::unique_ptr<NewSessionCdmPromise> promise,
                                bool success,
                                const std::vector<uint8_t>& data);

  // When UpdateSession() is called (on a persistent session), save the
  // current session state after it's been updated.
  void OnFileOpenedForUpdateSession(const std::string& session_id,
                                    bool key_added,
                                    std::unique_ptr<CdmFileAdapter> file,
                                    std::unique_ptr<SimpleCdmPromise> promise,
                                    CdmFileAdapter::Status status);
  void OnFileWrittenForUpdateSession(const std::string& session_id,
                                     bool key_added,
                                     std::unique_ptr<CdmFileAdapter> file,
                                     std::unique_ptr<SimpleCdmPromise> promise,
                                     bool success);

  // When RemoveSession() is called (on a persistent session), delete the
  // file (by writing 0 bytes) and then call |cdm_| to actually remove the
  // session from memory.
  void OnFileOpenedForRemoveSession(const std::string& session_id,
                                    std::unique_ptr<CdmFileAdapter> file,
                                    std::unique_ptr<SimpleCdmPromise> promise,
                                    CdmFileAdapter::Status status);
  void OnFileWrittenForRemoveSession(const std::string& session_id,
                                     std::unique_ptr<CdmFileAdapter> file,
                                     std::unique_ptr<SimpleCdmPromise> promise,
                                     bool success);

  // Add |session_id| to the list of open persistent sessions.
  void AddPersistentSession(const std::string& session_id);

  // When the session is closed, remove it from the list of open persistent
  // sessions if it was a persistent session.
  void OnSessionClosed(const std::string& session_id,
                       CdmSessionClosedReason reason);

  void OnSessionMessage(const std::string& session_id,
                        CdmMessageType message_type,
                        const std::vector<uint8_t>& message);

  scoped_refptr<AesDecryptor> cdm_;
  const raw_ptr<CdmHostProxy> cdm_host_proxy_ = nullptr;

  // Callbacks for firing session events. Other events aren't intercepted.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;

  // Keep track of current open persistent sessions.
  std::set<std::string> persistent_sessions_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ClearKeyPersistentSessionCdm> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CLEAR_KEY_PERSISTENT_SESSION_CDM_H_
