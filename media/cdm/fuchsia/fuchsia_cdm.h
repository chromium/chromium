// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_H_
#define MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_H_

#include <fuchsia/media/drm/cpp/fidl.h>

#include <optional>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/fuchsia/fuchsia_cdm_context.h"
#include "media/cdm/fuchsia/fuchsia_decryptor.h"

namespace media {

class FuchsiaCdm : public ContentDecryptionModule,
                   public CdmContext,
                   public FuchsiaCdmContext {
 public:
  struct SessionCallbacks {
    SessionCallbacks();
    SessionCallbacks(SessionCallbacks&&);

    SessionCallbacks(const SessionCallbacks&) = delete;
    SessionCallbacks& operator=(const SessionCallbacks&) = delete;

    ~SessionCallbacks();

    SessionCallbacks& operator=(SessionCallbacks&&);

    SessionMessageCB message_cb;
    SessionClosedCB closed_cb;
    SessionKeysChangeCB keys_change_cb;
    SessionExpirationUpdateCB expiration_update_cb;
  };
  using ReadyCB = base::OnceCallback<void(bool, CreateCdmStatus)>;

  FuchsiaCdm(fuchsia::media::drm::ContentDecryptionModulePtr cdm,
             ReadyCB ready_cb,
             SessionCallbacks callbacks);

  FuchsiaCdm(const FuchsiaCdm&) = delete;
  FuchsiaCdm& operator=(const FuchsiaCdm&) = delete;

  // ContentDecryptionModule implementation:
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void GetStatusForPolicy(
      media::HdcpVersion min_hdcp_version,
      std::unique_ptr<KeyStatusCdmPromise> promise) override;
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
  std::unique_ptr<SysmemBufferStream> CreateStreamDecryptor(
      bool secure_mode) override;

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
      std::optional<CdmPromise::Exception> exception);
  void OnProcessLicenseServerMessageStatus(
      const std::string& session_id,
      uint32_t promise_id,
      std::optional<CdmPromise::Exception> exception);
  void OnSessionLoaded(std::unique_ptr<CdmSession> session,
                       uint32_t promise_id,
                       bool loaded);

  void OnGenerateLicenseReleaseStatus(
      const std::string& session_id,
      uint32_t promise_id,
      std::optional<CdmPromise::Exception> exception);

  void OnNewKey();

  fuchsia::media::drm::ContentDecryptionModulePtr cdm_;
  ReadyCB ready_cb_;
  SessionCallbacks session_callbacks_;

  FuchsiaDecryptor decryptor_;

  base::Lock new_key_callbacks_lock_;
  std::vector<base::RepeatingClosure> new_key_callbacks_
      GUARDED_BY(new_key_callbacks_lock_);

  CdmPromiseAdapter promises_;

  // CdmSession instances reference `session_callbacks_`, so they must be
  // deleted before the callbacks.
  base::flat_map<std::string, std::unique_ptr<CdmSession>> session_map_;

  CallbackRegistry<EventCB::RunType> event_callbacks_;
};

}  // namespace media

#endif  // MEDIA_CDM_FUCHSIA_FUCHSIA_CDM_H_
