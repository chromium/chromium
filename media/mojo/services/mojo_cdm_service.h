// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_promise.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace media {

class CdmFactory;

// A mojom::ContentDecryptionModule implementation backed by a
// media::ContentDecryptionModule.
class MEDIA_MOJO_EXPORT MojoCdmService : public mojom::ContentDecryptionModule {
 public:
  // Constructs a MojoCdmService and strongly binds it to the |request|.
  // - |cdm_factory| is used to create CDM instances. Must not be null.
  // - |context| is used to keep track of all CDM instances such that we can
  //   connect the CDM with a media player (e.g. decoder). Can be null if the
  //   CDM does not need to be connected with any media player in this process.
  MojoCdmService(CdmFactory* cdm_factory, MojoCdmServiceContext* context);

  ~MojoCdmService() final;

  // mojom::ContentDecryptionModule implementation.
  void SetClient(
      mojo::PendingAssociatedRemote<mojom::ContentDecryptionModuleClient>
          client) final;
  void Initialize(const std::string& key_system,
                  const url::Origin& security_origin,
                  const CdmConfig& cdm_config,
                  InitializeCallback callback) final;
  void SetServerCertificate(const std::vector<uint8_t>& certificate_data,
                            SetServerCertificateCallback callback) final;
  void GetStatusForPolicy(HdcpVersion min_hdcp_version,
                          GetStatusForPolicyCallback callback) final;
  void CreateSessionAndGenerateRequest(
      CdmSessionType session_type,
      EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      CreateSessionAndGenerateRequestCallback callback) final;
  void LoadSession(CdmSessionType session_type,
                   const std::string& session_id,
                   LoadSessionCallback callback) final;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     UpdateSessionCallback callback) final;
  void CloseSession(const std::string& session_id,
                    CloseSessionCallback callback) final;
  void RemoveSession(const std::string& session_id,
                     RemoveSessionCallback callback) final;

  // Get CDM to be used by the media pipeline.
  scoped_refptr<::media::ContentDecryptionModule> GetCdm();

 private:
  // Callback for CdmFactory::Create().
  void OnCdmCreated(InitializeCallback callback,
                    const scoped_refptr<::media::ContentDecryptionModule>& cdm,
                    const std::string& error_message);

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& session_id,
                        ::media::CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);
  void OnSessionClosed(const std::string& session_id);

  // Callback for when |decryptor_| loses connectivity.
  void OnDecryptorConnectionError();

  bool has_initialize_been_called_ = false;

  CdmFactory* cdm_factory_;
  MojoCdmServiceContext* const context_ = nullptr;
  scoped_refptr<::media::ContentDecryptionModule> cdm_;

  // MojoDecryptorService is passed the Decryptor from |cdm_|, so
  // |decryptor_| must not outlive |cdm_|.
  std::unique_ptr<MojoDecryptorService> decryptor_;
  std::unique_ptr<mojo::Binding<mojom::Decryptor>> decryptor_binding_;

  // Set to a valid CDM ID if the |cdm_| is successfully created.
  int cdm_id_;

  mojo::AssociatedRemote<mojom::ContentDecryptionModuleClient> client_;

  base::WeakPtrFactory<MojoCdmService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MojoCdmService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
