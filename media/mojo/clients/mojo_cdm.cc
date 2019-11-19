// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "url/origin.h"

namespace media {

namespace {

void RecordConnectionError(bool connection_error_happened) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.MojoCdm.ConnectionError",
                        connection_error_happened);
}

}  // namespace

// static
void MojoCdm::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    mojo::PendingRemote<mojom::ContentDecryptionModule> remote_cdm,
    mojom::InterfaceFactory* interface_factory,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  scoped_refptr<MojoCdm> mojo_cdm(new MojoCdm(
      std::move(remote_cdm), interface_factory, session_message_cb,
      session_closed_cb, session_keys_change_cb, session_expiration_update_cb));

  // |mojo_cdm| ownership is passed to the promise.
  std::unique_ptr<CdmInitializedPromise> promise(
      new CdmInitializedPromise(cdm_created_cb, mojo_cdm));

  mojo_cdm->InitializeCdm(key_system, security_origin, cdm_config,
                          std::move(promise));
}

MojoCdm::MojoCdm(mojo::PendingRemote<mojom::ContentDecryptionModule> remote_cdm,
                 mojom::InterfaceFactory* interface_factory,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb)
    : remote_cdm_(std::move(remote_cdm)),
      interface_factory_(interface_factory),
      cdm_id_(CdmContext::kInvalidCdmId),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DVLOG(1) << __func__;
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);

  remote_cdm_->SetClient(client_receiver_.BindNewEndpointAndPassRemote());
}

MojoCdm::~MojoCdm() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::AutoLock auto_lock(lock_);

  // Release |decryptor_| on the correct thread. If GetDecryptor() is never
  // called but |decryptor_ptr_info_| is not null, it is not bound to any thread
  // and is safe to be released on the current thread.
  if (decryptor_task_runner_ &&
      !decryptor_task_runner_->BelongsToCurrentThread() && decryptor_) {
    decryptor_task_runner_->DeleteSoon(FROM_HERE, decryptor_.release());
  }

  // Reject any outstanding promises and close all the existing sessions.
  cdm_promise_adapter_.Clear();
  cdm_session_tracker_.CloseRemainingSessions(session_closed_cb_);
}

// Using base::Unretained(this) below is safe because |this| owns |remote_cdm_|,
// and if |this| is destroyed, |remote_cdm_| will be destroyed as well. Then the
// error handler can't be invoked and callbacks won't be dispatched.

void MojoCdm::InitializeCdm(const std::string& key_system,
                            const url::Origin& security_origin,
                            const CdmConfig& cdm_config,
                            std::unique_ptr<CdmInitializedPromise> promise) {
  DVLOG(1) << __func__ << ": " << key_system;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If connection error has happened, fail immediately.
  if (!remote_cdm_.is_connected()) {
    LOG(ERROR) << "Remote CDM encountered error.";
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "Mojo CDM creation failed.");
    return;
  }

  // Report a false event here as a baseline.
  RecordConnectionError(false);

  // Otherwise, set an error handler to catch the connection error.
  remote_cdm_.set_disconnect_with_reason_handler(
      base::Bind(&MojoCdm::OnConnectionError, base::Unretained(this)));

  pending_init_promise_ = std::move(promise);

  remote_cdm_->Initialize(
      key_system, security_origin, cdm_config,
      base::Bind(&MojoCdm::OnCdmInitialized, base::Unretained(this)));
}

void MojoCdm::OnConnectionError(uint32_t custom_reason,
                                const std::string& description) {
  LOG(ERROR) << "Remote CDM connection error: custom_reason=" << custom_reason
             << ", description=\"" << description << "\"";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RecordConnectionError(true);

  remote_cdm_.reset();

  // Handle initial connection error.
  if (pending_init_promise_) {
    DCHECK(!cdm_session_tracker_.HasRemainingSessions());
    pending_init_promise_->reject(CdmPromise::Exception::INVALID_STATE_ERROR,
                                  CdmPromise::SystemCode::kConnectionError,
                                  "Mojo CDM creation failed.");
    // Dropping the promise could cause |this| to be destructed.
    pending_init_promise_.reset();
    return;
  }

  // As communication with the remote CDM is broken, reject any outstanding
  // promises and close all the existing sessions.
  cdm_promise_adapter_.Clear();
  cdm_session_tracker_.CloseRemainingSessions(session_closed_cb_);
}

void MojoCdm::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR,
                    CdmPromise::SystemCode::kConnectionError,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->SetServerCertificate(
      certificate, base::Bind(&MojoCdm::OnSimpleCdmPromiseResult,
                              base::Unretained(this), promise_id));
}

void MojoCdm::GetStatusForPolicy(HdcpVersion min_hdcp_version,
                                 std::unique_ptr<KeyStatusCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->GetStatusForPolicy(
      min_hdcp_version, base::Bind(&MojoCdm::OnKeyStatusCdmPromiseResult,
                                   base::Unretained(this), promise_id));
}

void MojoCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->CreateSessionAndGenerateRequest(
      session_type, init_data_type, init_data,
      base::Bind(&MojoCdm::OnNewSessionCdmPromiseResult, base::Unretained(this),
                 promise_id));
}

void MojoCdm::LoadSession(CdmSessionType session_type,
                          const std::string& session_id,
                          std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->LoadSession(session_type, session_id,
                           base::Bind(&MojoCdm::OnNewSessionCdmPromiseResult,
                                      base::Unretained(this), promise_id));
}

void MojoCdm::UpdateSession(const std::string& session_id,
                            const std::vector<uint8_t>& response,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->UpdateSession(session_id, response,
                             base::Bind(&MojoCdm::OnSimpleCdmPromiseResult,
                                        base::Unretained(this), promise_id));
}

void MojoCdm::CloseSession(const std::string& session_id,
                           std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->CloseSession(session_id,
                            base::Bind(&MojoCdm::OnSimpleCdmPromiseResult,
                                       base::Unretained(this), promise_id));
}

void MojoCdm::RemoveSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!remote_cdm_) {
    promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "CDM connection lost.");
    return;
  }

  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  remote_cdm_->RemoveSession(session_id,
                             base::Bind(&MojoCdm::OnSimpleCdmPromiseResult,
                                        base::Unretained(this), promise_id));
}

CdmContext* MojoCdm::GetCdmContext() {
  DVLOG(2) << __func__;
  return this;
}

Decryptor* MojoCdm::GetDecryptor() {
  base::AutoLock auto_lock(lock_);

  if (!decryptor_task_runner_)
    decryptor_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(decryptor_task_runner_->BelongsToCurrentThread());

  if (decryptor_)
    return decryptor_.get();

  mojo::PendingRemote<mojom::Decryptor> decryptor_remote;

  // Can be called on a different thread.
  if (decryptor_ptr_info_.is_valid()) {
    DVLOG(1) << __func__ << ": Using Decryptor exposed by the CDM directly";
    decryptor_remote = std::move(decryptor_ptr_info_);
  } else if (interface_factory_ && cdm_id_ != CdmContext::kInvalidCdmId) {
#if BUILDFLAG(ENABLE_CDM_PROXY)
    // TODO(xhwang): Pass back info on whether Decryptor is supported by the
    // remote CDM.
    DVLOG(1) << __func__ << ": Using Decryptor associated with CDM ID "
             << cdm_id_ << ", typically hosted by CdmProxy in MediaService";
    interface_factory_->CreateDecryptor(
        cdm_id_, decryptor_remote.InitWithNewPipeAndPassReceiver());
#endif  // BUILDFLAG(ENABLE_CDM_PROXY)
  }

  if (decryptor_remote)
    decryptor_.reset(new MojoDecryptor(std::move(decryptor_remote)));

  return decryptor_.get();
}

int MojoCdm::GetCdmId() const {
  // Can be called on a different thread.
  base::AutoLock auto_lock(lock_);
  DVLOG(2) << __func__ << ": cdm_id = " << cdm_id_;
  return cdm_id_;
}

void MojoCdm::OnSessionMessage(const std::string& session_id,
                               MessageType message_type,
                               const std::vector<uint8_t>& message) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_message_cb_.Run(session_id, message_type, message);
}

void MojoCdm::OnSessionClosed(const std::string& session_id) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cdm_session_tracker_.RemoveSession(session_id);
  session_closed_cb_.Run(session_id);
}

void MojoCdm::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    std::vector<std::unique_ptr<CdmKeyInformation>> keys_info) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(jrummell): Handling resume playback should be done in the media
  // player, not in the Decryptors. http://crbug.com/413413.
  if (has_additional_usable_key) {
    base::AutoLock auto_lock(lock_);
    if (decryptor_) {
      DCHECK(decryptor_task_runner_);
      decryptor_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&MojoCdm::OnKeyAdded, weak_factory_.GetWeakPtr()));
    }
  }

  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void MojoCdm::OnSessionExpirationUpdate(const std::string& session_id,
                                        double new_expiry_time_sec) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_expiration_update_cb_.Run(
      session_id, base::Time::FromDoubleT(new_expiry_time_sec));
}

void MojoCdm::OnCdmInitialized(mojom::CdmPromiseResultPtr result,
                               int cdm_id,
                               mojom::DecryptorPtr decryptor) {
  DVLOG(2) << __func__ << " cdm_id: " << cdm_id;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(pending_init_promise_);

  if (!result->success) {
    pending_init_promise_->reject(result->exception, result->system_code,
                                  result->error_message);
    pending_init_promise_.reset();
    return;
  }

  {
    base::AutoLock auto_lock(lock_);
    DCHECK_NE(CdmContext::kInvalidCdmId, cdm_id);
    cdm_id_ = cdm_id;
    decryptor_ptr_info_ = decryptor.PassInterface();
  }

  pending_init_promise_->resolve();
  pending_init_promise_.reset();
}

void MojoCdm::OnKeyAdded() {
  base::AutoLock auto_lock(lock_);

  DCHECK(decryptor_task_runner_);
  DCHECK(decryptor_task_runner_->BelongsToCurrentThread());
  DCHECK(decryptor_);

  decryptor_->OnKeyAdded();
}

void MojoCdm::OnSimpleCdmPromiseResult(uint32_t promise_id,
                                       mojom::CdmPromiseResultPtr result) {
  if (result->success)
    cdm_promise_adapter_.ResolvePromise(promise_id);
  else {
    cdm_promise_adapter_.RejectPromise(promise_id, result->exception,
                                       result->system_code,
                                       result->error_message);
  }
}

void MojoCdm::OnKeyStatusCdmPromiseResult(
    uint32_t promise_id,
    mojom::CdmPromiseResultPtr result,
    CdmKeyInformation::KeyStatus key_status) {
  if (result->success) {
    cdm_promise_adapter_.ResolvePromise(promise_id, key_status);
  } else {
    cdm_promise_adapter_.RejectPromise(promise_id, result->exception,
                                       result->system_code,
                                       result->error_message);
  }
}

void MojoCdm::OnNewSessionCdmPromiseResult(uint32_t promise_id,
                                           mojom::CdmPromiseResultPtr result,
                                           const std::string& session_id) {
  if (result->success) {
    cdm_session_tracker_.AddSession(session_id);
    cdm_promise_adapter_.ResolvePromise(promise_id, session_id);
  } else {
    cdm_promise_adapter_.RejectPromise(promise_id, result->exception,
                                       result->system_code,
                                       result->error_message);
  }
}

}  // namespace media
