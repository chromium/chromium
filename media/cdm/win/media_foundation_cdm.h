// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <map>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"

namespace media {

class MediaFoundationCdmSession;

// A CDM implementation based on Media Foundation IMFContentDecryptionModule on
// Windows.
class MEDIA_EXPORT MediaFoundationCdm : public ContentDecryptionModule,
                                        public CdmContext {
 public:
  // Checks whether MediaFoundationCdm is available based on OS version. Further
  // checks need to be made to determine the usability and the capabilities.
  static bool IsAvailable();

  MediaFoundationCdm(
      Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);
  MediaFoundationCdm(const MediaFoundationCdm&) = delete;
  MediaFoundationCdm& operator=(const MediaFoundationCdm&) = delete;

  // ContentDecryptionModule implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) final;
  void GetStatusForPolicy(HdcpVersion min_hdcp_version,
                          std::unique_ptr<KeyStatusCdmPromise> promise) final;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<NewSessionCdmPromise> promise) final;
  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   std::unique_ptr<NewSessionCdmPromise> promise) final;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<SimpleCdmPromise> promise) final;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<SimpleCdmPromise> promise) final;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<SimpleCdmPromise> promise) final;
  CdmContext* GetCdmContext() final;

  // CdmContext implementation.
  bool RequiresMediaFoundationRenderer() final;
  bool GetMediaFoundationCdmProxy(
      GetMediaFoundationCdmProxyCB get_mf_cdm_proxy_cb) final;

 private:
  ~MediaFoundationCdm() final;

  // Returns whether the |session_id| is accepted by the |this|.
  bool OnSessionId(int session_token,
                   std::unique_ptr<NewSessionCdmPromise> promise,
                   const std::string& session_id);

  MediaFoundationCdmSession* GetSession(const std::string& session_id);

  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  // Used to generate unique tokens for identifying pending sessions before
  // session ID is available.
  int next_session_token_ = 0;

  // Session token to session map for sessions waiting for session ID.
  std::map<int, std::unique_ptr<MediaFoundationCdmSession>> pending_sessions_;

  // Session ID to session map.
  std::map<std::string, std::unique_ptr<MediaFoundationCdmSession>> sessions_;

  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_
