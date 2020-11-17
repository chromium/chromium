// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_H_
#define MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/content_decryption_module.h"
#include "media/fuchsia/cdm/fuchsia_cdm_context.h"
#include "media/fuchsia/cdm/fuchsia_decryptor.h"

namespace media {

class FuchsiaCdm : public ContentDecryptionModule,
                   public CdmContext,
                   public FuchsiaCdmContext {
 public:
  struct SessionCallbacks {
    SessionCallbacks();
    SessionCallbacks(SessionCallbacks&&);
    ~SessionCallbacks();

    SessionCallbacks& operator=(SessionCallbacks&&);

    SessionMessageCB message_cb;
    SessionClosedCB closed_cb;
    SessionKeysChangeCB keys_change_cb;
    SessionExpirationUpdateCB expiration_update_cb;

    DISALLOW_COPY_AND_ASSIGN(SessionCallbacks);
  };
  using ReadyCB = base::OnceCallback<void(bool, const std::string&)>;

  FuchsiaCdm(fuchsia::media::drm::ContentDecryptionModulePtr cdm,
             ReadyCB ready_cb,
             SessionCallbacks callbacks);

  // ContentDecryptionModule implementation:
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

  // CdmContext implementation:
  std::unique_ptr<CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;
  Decryptor* GetDecryptor() override;
  FuchsiaCdmContext* GetFuchsiaCdmContext() override;

  // FuchsiaCdmContext implementation:
  std::unique_ptr<FuchsiaSecureStreamDecryptor> CreateVideoDecryptor(
      FuchsiaSecureStreamDecryptor::Client* client) override;
  std::unique_ptr<FuchsiaClearStreamDecryptor> CreateAudioDecryptor() override;

 private:
  class CdmSession;

  ~FuchsiaCdm() override;

  void OnProvisioned();
  void OnCreateSession(std::unique_ptr<CdmSession> session,
                       uint32_t promise_id,
                       const std::string& session_id);
  void OnGenerateLicenseRequestStatus(
      CdmSession* session,
      uint32_t promise_id,
      base::Optional<CdmPromise::Exception> exception);
  void OnProcessLicenseServerMessageStatus(
      const std::string& session_id,
      uint32_t promise_id,
      base::Optional<CdmPromise::Exception> exception);
  void OnSessionLoaded(std::unique_ptr<CdmSession> session,
                       uint32_t promise_id,
                       bool loaded);

  void OnGenerateLicenseReleaseStatus(
      const std::string& session_id,
      uint32_t promise_id,
      base::Optional<CdmPromise::Exception> exception);

  void OnNewKey();

  CdmPromiseAdapter promises_;
  base::flat_map<std::string, std::unique_ptr<CdmSession>> session_map_;

  fuchsia::media::drm::ContentDecryptionModulePtr cdm_;
  ReadyCB ready_cb_;
  SessionCallbacks session_callbacks_;

  FuchsiaDecryptor decryptor_;

  base::Lock new_key_cb_for_video_lock_;
  base::RepeatingClosure new_key_cb_for_video_
      GUARDED_BY(new_key_cb_for_video_lock_);

  CallbackRegistry<EventCB::RunType> event_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(FuchsiaCdm);
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CDM_FUCHSIA_CDM_H_
