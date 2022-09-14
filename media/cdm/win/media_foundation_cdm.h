// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_
#define MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_

#include <mfcontentdecryptionmodule.h>
#include <wrl.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_context.h"
#include "media/base/content_decryption_module.h"
#include "media/base/media_export.h"
#include "media/cdm/cdm_document_service.h"

namespace media {

// Key to the client token. The same value is also used in MediaFoundation CDMs.
// Do NOT change this value!
DEFINE_PROPERTYKEY(EME_CONTENTDECRYPTIONMODULE_CLIENT_TOKEN,
                   0xa4abc308,
                   0xd249,
                   0x4150,
                   0x90,
                   0x37,
                   0xc9,
                   0x97,
                   0xf8,
                   0xcf,
                   0x8d,
                   0x0f,
                   PID_FIRST_USABLE);

class MediaFoundationCdmSession;

// A CDM implementation based on Media Foundation IMFContentDecryptionModule on
// Windows.
class MEDIA_EXPORT MediaFoundationCdm final : public ContentDecryptionModule,
                                              public CdmContext {
 public:
  // Checks whether MediaFoundationCdm is available based on OS version. Further
  // checks need to be made to determine the usability and the capabilities.
  static bool IsAvailable();

  // Callback to create an IMFContentDecryptionModule. If failed,
  // IMFContentDecryptionModule must be null.
  using CreateMFCdmCB = base::RepeatingCallback<
      void(HRESULT&, Microsoft::WRL::ComPtr<IMFContentDecryptionModule>&)>;

  // Callback for `IsTypeSupportedCB` below.
  using IsTypeSupportedResultCB = base::OnceCallback<void(bool is_supported)>;

  // Callback to IMFMediaFoundataionCdmFactory's IsTypeSupported.
  using IsTypeSupportedCB =
      base::RepeatingCallback<void(const std::string& content_type,
                                   IsTypeSupportedResultCB)>;

  // Callback to MediaFoundationCdmFactory::StoreClientToken
  using StoreClientTokenCB =
      base::RepeatingCallback<void(const std::vector<uint8_t>&)>;

  // Callback to notify the CDM of an event, with an optional HRESULT associated
  // with that event (e.g. errors).
  using CdmEventCB = base::RepeatingCallback<void(CdmEvent, HRESULT hresult)>;

  // Constructs `MediaFoundationCdm`. Note that `Initialize()` must be called
  // before calling any other methods.
  // TODO(xhwang): Use a helper to reduce the number of callbacks.
  MediaFoundationCdm(
      const std::string& uma_prefix,
      const CreateMFCdmCB& create_mf_cdm_cb,
      const IsTypeSupportedCB& is_type_supported_cb,
      const StoreClientTokenCB& store_client_token_cb,
      const CdmEventCB& cdm_event_cb,
      const SessionMessageCB& session_message_cb,
      const SessionClosedCB& session_closed_cb,
      const SessionKeysChangeCB& session_keys_change_cb,
      const SessionExpirationUpdateCB& session_expiration_update_cb);
  MediaFoundationCdm(const MediaFoundationCdm&) = delete;
  MediaFoundationCdm& operator=(const MediaFoundationCdm&) = delete;

  // Initializes `this` and returns whether the initialization succeeds. Must
  // be called before any other methods.
  HRESULT Initialize();

  // ContentDecryptionModule implementation.
  void SetServerCertificate(const std::vector<uint8_t>& certificate,
                            std::unique_ptr<SimpleCdmPromise> promise) override;
  void GetStatusForPolicy(
      HdcpVersion min_hdcp_version,
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

  // CdmContext implementation.
  bool RequiresMediaFoundationRenderer() override;
  scoped_refptr<MediaFoundationCdmProxy> GetMediaFoundationCdmProxy() override;

 private:
  ~MediaFoundationCdm() override;

  // Returns whether the |session_id| is accepted by the |this|.
  bool OnSessionId(int session_token,
                   std::unique_ptr<NewSessionCdmPromise> promise,
                   const std::string& session_id);

  MediaFoundationCdmSession* GetSession(const std::string& session_id);

  void CloseSessionInternal(const std::string& session_id,
                            CdmSessionClosedReason reason,
                            std::unique_ptr<SimpleCdmPromise> promise);

  // Called when hardware context reset happens.
  void OnHardwareContextReset();

  // Called when CdmEvent happens.
  void OnCdmEvent(CdmEvent event, HRESULT hresult);

  // Called when IsTypeSupported() result is available.
  void OnIsTypeSupportedResult(std::unique_ptr<KeyStatusCdmPromise> promise,
                               bool is_supported);

  void StoreClientTokenIfNeeded();

  // Prefix for UMA reported in `this` and the `sessions_`.
  const std::string uma_prefix_;

  // Callback to create `mf_cdm_`.
  CreateMFCdmCB create_mf_cdm_cb_;

  // Callback to MFCdmFactory's IsTypeSupported().
  IsTypeSupportedCB is_type_supported_cb_;

  // Callback to MFCdmFactory's StoreClientToken().
  StoreClientTokenCB store_client_token_cb_;

  // Callback to report fatal errors.
  CdmEventCB cdm_event_cb_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  Microsoft::WRL::ComPtr<IMFContentDecryptionModule> mf_cdm_;

  // Used to generate unique tokens for identifying pending sessions before
  // session ID is available.
  int next_session_token_ = 0;

  // Session token to session map for sessions waiting for session ID.
  std::map<int, std::unique_ptr<MediaFoundationCdmSession>> pending_sessions_;

  // Session ID to session map.
  std::map<std::string, std::unique_ptr<MediaFoundationCdmSession>> sessions_;

  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;

  // Copy of the last client token we stored.
  std::vector<uint8_t> cached_client_token_;

  // This must be the last member.
  base::WeakPtrFactory<MediaFoundationCdm> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_MEDIA_FOUNDATION_CDM_H_
