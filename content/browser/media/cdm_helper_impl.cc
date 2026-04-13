// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_helper_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "media/base/cdm_config.h"
#include "media/mojo/mojom/cdm_service.mojom.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/frame_interface_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
#include "content/browser/media/service_factory.h"
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

namespace content {

CdmHelperImpl::CdmHelperImpl() = default;
CdmHelperImpl::~CdmHelperImpl() = default;

CdmHelperImpl::CdmClient::CdmClient() = default;
CdmHelperImpl::CdmClient::~CdmClient() = default;

// TODO(b/485217840): use session_id to match the callback.
void CdmHelperImpl::CdmClient::OnSessionMessage(
    const std::string& session_id,
    media::CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  if (callback_) {
    std::string message_str(message.begin(), message.end());
    std::move(callback_).Run(message_str, SignChallengeResult::kSuccess);
  }
}

void CdmHelperImpl::CdmClient::OnSessionClosed(
    const std::string& session_id,
    media::CdmSessionClosedReason reason) {}

void CdmHelperImpl::CdmClient::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    std::vector<std::unique_ptr<media::CdmKeyInformation>> keys_info) {}

void CdmHelperImpl::CdmClient::OnSessionExpirationUpdate(
    const std::string& session_id,
    double new_expiry_time_sec) {}

void CdmHelperImpl::CdmClient::SetCallback(
    base::OnceCallback<void(const std::string&, SignChallengeResult)>
        callback) {
  callback_ = std::move(callback);
}

bool CdmHelperImpl::CdmClient::HasCallback() {
  return !callback_.is_null();
}

void CdmHelperImpl::Initialize(
    const std::string& server_ceritificate,
    const std::string& key_system,
    base::OnceCallback<void(InitializeResult)> init_callback) {
  CHECK(!cdm_.is_bound());

  std::unique_ptr<CdmInfo> cdm_info =
      CdmRegistryImpl::GetInstance()->GetCdmInfo(
          key_system, CdmInfo::Robustness::kSoftwareSecure);
  if (!cdm_info) {
    DVLOG(1) << "No valid CdmInfo for " << key_system;
    std::move(init_callback).Run(InitializeResult::kInitializeError);
    return;
  }

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  media::mojom::CdmService& cdm_service =
      GetCdmService(/*browser_context=*/nullptr, /*site=*/GURL(), *cdm_info);

  mojo::PendingRemote<media::mojom::FrameInterfaceFactory> interfaces;
  std::ignore = interfaces.InitWithNewPipeAndPassReceiver();
  cdm_service.CreateCdmFactory(cdm_factory_.BindNewPipeAndPassReceiver(),
                               std::move(interfaces));

  cdm_factory_->CreateCdm(
      media::CdmConfig{/*key_system=*/key_system,
                       /*allow_distinctive_identifier=*/false,
                       /*allow_persistent_state=*/true,
                       /*use_hw_secure_codecs=*/false},
      base::BindOnce(&CdmHelperImpl::OnCdmCreated, weak_factory_.GetWeakPtr(),
                     server_ceritificate, std::move(init_callback)));
#else   // BUILDFLAG(ENABLE_LIBRARY_CDMS)
  DVLOG(1) << "Cdm is not enabled.";
  std::move(init_callback).Run(InitializeResult::kInitializeError);
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
}

void CdmHelperImpl::OnCdmCreated(
    const std::string& server_ceritificate,
    base::OnceCallback<void(InitializeResult)> init_callback,
    mojo::PendingRemote<media::mojom::ContentDecryptionModule> cdm_remote,
    media::mojom::CdmContextPtr cdm_context,
    media::CreateCdmStatus status) {
  if (status != media::CreateCdmStatus::kSuccess) {
    DVLOG(1) << "CDM creation failed.";
    std::move(init_callback).Run(InitializeResult::kInitializeError);
    return;
  }

  if (cdm_.is_bound()) {
    DVLOG(1) << "CDM already exists.";
    std::move(init_callback).Run(InitializeResult::kInitializeError);
    return;
  }

  cdm_.Bind(std::move(cdm_remote));

  cdm_client_ = std::make_unique<CdmClient>();
  cdm_client_receiver_ = std::make_unique<
      mojo::AssociatedReceiver<media::mojom::ContentDecryptionModuleClient>>(
      cdm_client_.get());

  mojo::PendingAssociatedRemote<media::mojom::ContentDecryptionModuleClient>
      client;
  cdm_client_receiver_->Bind(client.InitWithNewEndpointAndPassReceiver());
  cdm_->SetClient(std::move(client));

  std::vector<uint8_t> decoded_server_certificate;
  if (!base::HexStringToBytes(server_ceritificate,
                              &decoded_server_certificate)) {
    DVLOG(1) << "Failed to parse cdm server certificate.";
    std::move(init_callback).Run(InitializeResult::kInitializeError);
    return;
  }

  cdm_->SetServerCertificate(
      decoded_server_certificate,
      base::BindOnce(&CdmHelperImpl::OnSetServerCertificate,
                     weak_factory_.GetWeakPtr(), std::move(init_callback)));
}

void CdmHelperImpl::OnSetServerCertificate(
    base::OnceCallback<void(InitializeResult)> init_callback,
    media::mojom::CdmPromiseResultPtr result) {
  DVLOG(1) << "SetServerCertificate success: " << result->success;
  std::move(init_callback)
      .Run(result->success ? InitializeResult::kSuccess
                           : InitializeResult::kInitializeError);
}

void CdmHelperImpl::SignChallenge(
    const std::string& challenge,
    base::OnceCallback<void(const std::string&, SignChallengeResult)>
        callback) {
  if (!cdm_.is_bound()) {
    DVLOG(1) << "CDM is not initialized.";
    std::move(callback).Run("", SignChallengeResult::kCdmError);
    return;
  }

  CHECK(cdm_client_);
  if (cdm_client_->HasCallback()) {
    DVLOG(1) << "SignChallenge is already in progress.";
    std::move(callback).Run("", SignChallengeResult::kCdmError);
    return;
  }

  std::vector<uint8_t> challenge_data;
  if (!base::HexStringToBytes(challenge, &challenge_data)) {
    DVLOG(1) << "Failed to parse challenge.";
    std::move(callback).Run("", SignChallengeResult::kCdmError);
    return;
  }

  cdm_client_->SetCallback(std::move(callback));
  cdm_->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, media::EmeInitDataType::CENC,
      challenge_data,
      base::BindOnce([](media::mojom::CdmPromiseResultPtr result,
                        const std::string& session_id) {
        DVLOG(1) << "CreateSessionAndGenerateRequest result: "
                 << result->success << ", session_id: " << session_id;
      }));
}

}  // namespace content
