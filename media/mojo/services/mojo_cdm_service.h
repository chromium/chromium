// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <optional>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/cdm_factory.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "media/mojo/services/mojo_cdm_promise.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "media/mojo/services/mojo_decryptor_service.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class CdmFactory;

// A mojom::ContentDecryptionModule implementation backed by a
// media::ContentDecryptionModule.
class MEDIA_MOJO_EXPORT MojoCdmService final
    : public mojom::ContentDecryptionModule {
 public:
  // Callback for Initialize(). Non-null `cdm_context` indicates success. Null
  // `cdm_context` indicates failure and the `status` provides a
  // reason.
  using InitializeCB = base::OnceCallback<void(mojom::CdmContextPtr cdm_context,
                                               CreateCdmStatus status)>;

  explicit MojoCdmService(MojoCdmServiceContext* context);

  MojoCdmService(const MojoCdmService&) = delete;
  MojoCdmService& operator=(const MojoCdmService&) = delete;

  ~MojoCdmService() final;

  // Initialize the MojoCdmService, including creating the real CDM using the
  // `cdm_factory`, which must not be null. The MojoCdmService should NOT be
  // used before the `init_cb` is returned.
  void Initialize(CdmFactory* cdm_factory,
                  const CdmConfig& cdm_config,
                  InitializeCB init_cb);

  // mojom::ContentDecryptionModule implementation.
  void SetClient(
      mojo::PendingAssociatedRemote<mojom::ContentDecryptionModuleClient>
          client) final;
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

  // Gets the remote ID of the CDM this is holding.
  base::UnguessableToken cdm_id() const { return cdm_id_.value(); }

 private:
  // Callback for CdmFactory::Create().
  void OnCdmCreated(InitializeCB callback,
                    const scoped_refptr<::media::ContentDecryptionModule>& cdm,
                    CreateCdmStatus status);

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& session_id,
                        ::media::CdmMessageType message_type,
                        const std::vector<uint8_t>& message);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 base::Time new_expiry_time);
  void OnSessionClosed(const std::string& session_id,
                       CdmSessionClosedReason reason);

  // Callback for when |decryptor_| loses connectivity.
  void OnDecryptorConnectionError();

  const raw_ptr<MojoCdmServiceContext> context_;
  scoped_refptr<::media::ContentDecryptionModule> cdm_;

  // MojoDecryptorService is passed the Decryptor from |cdm_|, so
  // |decryptor_| must not outlive |cdm_|.
  std::unique_ptr<MojoDecryptorService> decryptor_;
  mojo::PendingRemote<mojom::Decryptor> decryptor_remote_;
  std::unique_ptr<mojo::Receiver<mojom::Decryptor>> decryptor_receiver_;

  // Set to a valid CDM ID if the |cdm_| is successfully created.
  std::optional<base::UnguessableToken> cdm_id_;

  mojo::AssociatedRemote<mojom::ContentDecryptionModuleClient> client_;

  base::WeakPtrFactory<MojoCdmService> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
