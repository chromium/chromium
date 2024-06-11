// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/lock.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_systems.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {

using SimpleMojoCdmPromise = MojoCdmPromise<void(mojom::CdmPromiseResultPtr)>;
using KeyStatusMojoCdmPromise =
    MojoCdmPromise<void(mojom::CdmPromiseResultPtr,
                        CdmKeyInformation::KeyStatus),
                   CdmKeyInformation::KeyStatus>;
using NewSessionMojoCdmPromise =
    MojoCdmPromise<void(mojom::CdmPromiseResultPtr, const std::string&),
                   std::string>;

MojoCdmService::MojoCdmService(MojoCdmServiceContext* context)
    : context_(context) {
  DVLOG(1) << __func__;
  DCHECK(context_);
}

MojoCdmService::~MojoCdmService() {
  DVLOG(1) << __func__;

  if (!cdm_id_)
    return;

  context_->UnregisterCdm(cdm_id_.value());
}

void MojoCdmService::Initialize(CdmFactory* cdm_factory,
                                const CdmConfig& cdm_config,
                                InitializeCB init_cb) {
  auto weak_this = weak_factory_.GetWeakPtr();
  cdm_factory->Create(
      cdm_config,
      base::BindRepeating(&MojoCdmService::OnSessionMessage, weak_this),
      base::BindRepeating(&MojoCdmService::OnSessionClosed, weak_this),
      base::BindRepeating(&MojoCdmService::OnSessionKeysChange, weak_this),
      base::BindRepeating(&MojoCdmService::OnSessionExpirationUpdate,
                          weak_this),
      base::BindOnce(&MojoCdmService::OnCdmCreated, weak_this,
                     mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                         std::move(init_cb), nullptr,
                         CreateCdmStatus::kDisconnectionError)));
}

void MojoCdmService::SetClient(
    mojo::PendingAssociatedRemote<mojom::ContentDecryptionModuleClient>
        client) {
  client_.Bind(std::move(client));
}

void MojoCdmService::SetServerCertificate(
    const std::vector<uint8_t>& certificate_data,
    SetServerCertificateCallback callback) {
  DVLOG(2) << __func__;
  cdm_->SetServerCertificate(
      certificate_data,
      std::make_unique<SimpleMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::GetStatusForPolicy(HdcpVersion min_hdcp_version,
                                        GetStatusForPolicyCallback callback) {
  DVLOG(2) << __func__;
  cdm_->GetStatusForPolicy(
      min_hdcp_version,
      std::make_unique<KeyStatusMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    CreateSessionAndGenerateRequestCallback callback) {
  DVLOG(2) << __func__;
  cdm_->CreateSessionAndGenerateRequest(
      session_type, init_data_type, init_data,
      std::make_unique<NewSessionMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::LoadSession(CdmSessionType session_type,
                                 const std::string& session_id,
                                 LoadSessionCallback callback) {
  DVLOG(2) << __func__;
  cdm_->LoadSession(
      session_type, session_id,
      std::make_unique<NewSessionMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::UpdateSession(const std::string& session_id,
                                   const std::vector<uint8_t>& response,
                                   UpdateSessionCallback callback) {
  DVLOG(2) << __func__;
  cdm_->UpdateSession(
      session_id, response,
      std::make_unique<SimpleMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::CloseSession(const std::string& session_id,
                                  CloseSessionCallback callback) {
  DVLOG(2) << __func__;
  cdm_->CloseSession(
      session_id, std::make_unique<SimpleMojoCdmPromise>(std::move(callback)));
}

void MojoCdmService::RemoveSession(const std::string& session_id,
                                   RemoveSessionCallback callback) {
  DVLOG(2) << __func__;
  cdm_->RemoveSession(
      session_id, std::make_unique<SimpleMojoCdmPromise>(std::move(callback)));
}

scoped_refptr<ContentDecryptionModule> MojoCdmService::GetCdm() {
  return cdm_;
}

void MojoCdmService::OnCdmCreated(
    InitializeCB init_cb,
    const scoped_refptr<::media::ContentDecryptionModule>& cdm,
    CreateCdmStatus status) {
  // TODO(xhwang): This should not happen when KeySystemInfo is properly
  // populated. See http://crbug.com/469366
  if (!cdm) {
    DVLOG(1) << __func__ << ": CDM creation failed ("
             << static_cast<int>(status) << ")";
    std::move(init_cb).Run(nullptr, status);
    return;
  }

  cdm_ = cdm;
  cdm_id_ = context_->RegisterCdm(this);
  DVLOG(1) << __func__ << ": CDM successfully registered with ID "
           << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id_));

  auto mojo_cdm_context = mojom::CdmContext::New();
  mojo_cdm_context->cdm_id = cdm_id();

  CdmContext* const cdm_context = cdm_->GetCdmContext();
  if (cdm_context) {
    // If |cdm| has a decryptor, create the MojoDecryptorService
    // and pass the connection back to the client.
    if (cdm_context->GetDecryptor()) {
      DVLOG(2) << __func__ << ": CDM supports Decryptor.";
      mojo::PendingRemote<mojom::Decryptor> decryptor_remote;
      // Both |cdm_| and |decryptor_| are owned by |this|, so we don't need to
      // pass in a CdmContextRef.
      decryptor_ = std::make_unique<MojoDecryptorService>(
          cdm_context->GetDecryptor(), /*cdm_context_ref=*/nullptr);
      decryptor_receiver_ = std::make_unique<mojo::Receiver<mojom::Decryptor>>(
          decryptor_.get(), decryptor_remote.InitWithNewPipeAndPassReceiver());
      // base::Unretained is safe because |decryptor_receiver_| is owned by
      // |this|. If |this| is destructed, |decryptor_receiver_| will be
      // destructed as well and the error handler should never be called.
      // The disconnection can happen due to race conditions during render
      // process teardown or crash.
      decryptor_receiver_->set_disconnect_handler(base::BindOnce(
          &MojoCdmService::OnDecryptorConnectionError, base::Unretained(this)));
      mojo_cdm_context->decryptor = std::move(decryptor_remote);
    }

#if BUILDFLAG(IS_WIN)
    mojo_cdm_context->requires_media_foundation_renderer =
        cdm_context->RequiresMediaFoundationRenderer();
#endif  // BUILDFLAG(IS_WIN)
  }

  std::move(init_cb).Run(std::move(mojo_cdm_context),
                         CreateCdmStatus::kSuccess);
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      ::media::CdmMessageType message_type,
                                      const std::vector<uint8_t>& message) {
  DVLOG(2) << __func__;
  if (client_) {
    client_->OnSessionMessage(session_id, message_type, message);
  }
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         CdmKeysInfo keys_info) {
  DVLOG(2) << __func__
           << " has_additional_usable_key = " << has_additional_usable_key;
  if (client_) {
    client_->OnSessionKeysChange(session_id, has_additional_usable_key,
                                 std::move(keys_info));
  }
}

void MojoCdmService::OnSessionExpirationUpdate(const std::string& session_id,
                                               base::Time new_expiry_time_sec) {
  DVLOG(2) << __func__ << " expiry = " << new_expiry_time_sec;
  if (client_) {
    client_->OnSessionExpirationUpdate(
        session_id, new_expiry_time_sec.InSecondsFSinceUnixEpoch());
  }
}

void MojoCdmService::OnSessionClosed(const std::string& session_id,
                                     CdmSessionClosedReason reason) {
  DVLOG(2) << __func__;
  if (client_) {
    client_->OnSessionClosed(session_id, reason);
  }
}

void MojoCdmService::OnDecryptorConnectionError() {
  DVLOG(2) << __func__;

  // MojoDecryptorService has lost connectivity to it's client, so it can be
  // freed. This could happen due to render process teardown or crash. No need
  // for recovery.
  decryptor_.reset();
}

}  // namespace media
