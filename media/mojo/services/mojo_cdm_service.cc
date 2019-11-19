// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "media/base/cdm_config.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_key_information.h"
#include "media/base/key_systems.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/services/mojo_cdm_service_context.h"
#include "url/origin.h"

namespace media {

using SimpleMojoCdmPromise = MojoCdmPromise<void(mojom::CdmPromiseResultPtr)>;
using KeyStatusMojoCdmPromise =
    MojoCdmPromise<void(mojom::CdmPromiseResultPtr,
                        CdmKeyInformation::KeyStatus),
                   CdmKeyInformation::KeyStatus>;
using NewSessionMojoCdmPromise =
    MojoCdmPromise<void(mojom::CdmPromiseResultPtr, const std::string&),
                   std::string>;

MojoCdmService::MojoCdmService(CdmFactory* cdm_factory,
                               MojoCdmServiceContext* context)
    : cdm_factory_(cdm_factory),
      context_(context),
      cdm_id_(CdmContext::kInvalidCdmId) {
  DVLOG(1) << __func__;
  DCHECK(cdm_factory_);
  // |context_| can be null.
}

MojoCdmService::~MojoCdmService() {
  DVLOG(1) << __func__;

  if (!context_ || cdm_id_ == CdmContext::kInvalidCdmId)
    return;

  context_->UnregisterCdm(cdm_id_);
}

void MojoCdmService::SetClient(
    mojo::PendingAssociatedRemote<mojom::ContentDecryptionModuleClient>
        client) {
  client_.Bind(std::move(client));
}

void MojoCdmService::Initialize(const std::string& key_system,
                                const url::Origin& security_origin,
                                const CdmConfig& cdm_config,
                                InitializeCallback callback) {
  DVLOG(1) << __func__ << ": " << key_system;

  CHECK(!has_initialize_been_called_) << "Initialize should only happen once";
  has_initialize_been_called_ = true;

  auto weak_this = weak_factory_.GetWeakPtr();
  cdm_factory_->Create(
      key_system, security_origin, cdm_config,
      base::Bind(&MojoCdmService::OnSessionMessage, weak_this),
      base::Bind(&MojoCdmService::OnSessionClosed, weak_this),
      base::Bind(&MojoCdmService::OnSessionKeysChange, weak_this),
      base::Bind(&MojoCdmService::OnSessionExpirationUpdate, weak_this),
      base::Bind(&MojoCdmService::OnCdmCreated, weak_this,
                 base::Passed(&callback)));
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
  cdm_->UpdateSession(session_id, response,
                      std::unique_ptr<SimpleCdmPromise>(
                          new SimpleMojoCdmPromise(std::move(callback))));
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
    InitializeCallback callback,
    const scoped_refptr<::media::ContentDecryptionModule>& cdm,
    const std::string& error_message) {
  DVLOG(2) << __func__ << ": error_message=" << error_message;
  mojom::CdmPromiseResultPtr cdm_promise_result(mojom::CdmPromiseResult::New());

  // TODO(xhwang): This should not happen when KeySystemInfo is properly
  // populated. See http://crbug.com/469366
  if (!cdm) {
    cdm_promise_result->success = false;
    cdm_promise_result->exception = CdmPromise::Exception::NOT_SUPPORTED_ERROR;
    cdm_promise_result->system_code = 0;
    cdm_promise_result->error_message = error_message;
    std::move(callback).Run(std::move(cdm_promise_result), 0, nullptr);
    return;
  }

  CHECK(!cdm_) << "CDM should only be created once.";
  cdm_ = cdm;

  if (context_) {
    cdm_id_ = context_->RegisterCdm(this);
    DVLOG(1) << __func__ << ": CDM successfully registered with ID " << cdm_id_;
  }

  // If |cdm| has a decryptor, create the MojoDecryptorService
  // and pass the connection back to the client.
  mojom::DecryptorPtr decryptor_ptr;
  CdmContext* const cdm_context = cdm_->GetCdmContext();
  if (cdm_context && cdm_context->GetDecryptor()) {
    DVLOG(2) << __func__ << ": CDM supports Decryptor.";
    // Both |cdm_| and |decryptor_| are owned by |this|, so we don't need to
    // pass in a CdmContextRef.
    decryptor_.reset(
        new MojoDecryptorService(cdm_context->GetDecryptor(), nullptr));
    decryptor_binding_ = std::make_unique<mojo::Binding<mojom::Decryptor>>(
        decryptor_.get(), MakeRequest(&decryptor_ptr));
    // base::Unretained is safe because |decryptor_binding_| is owned by |this|.
    // If |this| is destructed, |decryptor_binding_| will be destructed as well
    // and the error handler should never be called.
    decryptor_binding_->set_connection_error_handler(base::BindOnce(
        &MojoCdmService::OnDecryptorConnectionError, base::Unretained(this)));
  }

  // If the |context_| is not null, we should support connecting the |cdm| with
  // the media player in the same process, which also has access to the
  // |context_|. Hence pass back the |cdm_id_| obtained from the |context_|.
  // Otherwise, if the |cdm| has a valid CDM ID by itself, this CDM can proxy
  // all or parts of its functionalities to another remote CDM or CdmProxy. In
  // this case, just we pass the remote CDM ID back.
  int cdm_id = context_ ? cdm_id_ : cdm_context->GetCdmId();

  cdm_promise_result->success = true;
  std::move(callback).Run(std::move(cdm_promise_result), cdm_id,
                          std::move(decryptor_ptr));
}

void MojoCdmService::OnSessionMessage(const std::string& session_id,
                                      ::media::CdmMessageType message_type,
                                      const std::vector<uint8_t>& message) {
  DVLOG(2) << __func__;
  client_->OnSessionMessage(session_id, message_type, message);
}

void MojoCdmService::OnSessionKeysChange(const std::string& session_id,
                                         bool has_additional_usable_key,
                                         CdmKeysInfo keys_info) {
  DVLOG(2) << __func__
           << " has_additional_usable_key = " << has_additional_usable_key;

  client_->OnSessionKeysChange(session_id, has_additional_usable_key,
                               std::move(keys_info));
}

void MojoCdmService::OnSessionExpirationUpdate(const std::string& session_id,
                                               base::Time new_expiry_time_sec) {
  DVLOG(2) << __func__ << " expiry = " << new_expiry_time_sec;
  client_->OnSessionExpirationUpdate(session_id,
                                     new_expiry_time_sec.ToDoubleT());
}

void MojoCdmService::OnSessionClosed(const std::string& session_id) {
  DVLOG(2) << __func__;
  client_->OnSessionClosed(session_id);
}

void MojoCdmService::OnDecryptorConnectionError() {
  DVLOG(2) << __func__;

  // MojoDecryptorService has lost connectivity to it's client, so it can be
  // freed.
  decryptor_.reset();
}

}  // namespace media
