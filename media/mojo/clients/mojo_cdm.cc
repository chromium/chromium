// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_cdm.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "media/base/callback_timeout_helpers.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/key_systems.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_decryptor.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/mojom/decryptor.mojom.h"
#include "services/service_manager/public/cpp/connect.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"

namespace media {

namespace {

void RecordConnectionError(bool connection_error_happened) {
  UMA_HISTOGRAM_BOOLEAN("Media.EME.MojoCdm.ConnectionError",
                        connection_error_happened);
}

void OnCallbackTimeout(const std::string& uma_name,
                       bool called_on_destruction) {
  DVLOG(1) << "Callback Timeout: " << uma_name
           << ", called_on_destruction=" << called_on_destruction;
  base::UmaHistogramEnumeration(
      uma_name, called_on_destruction
                    ? CallbackTimeoutStatus::kDestructedBeforeTimeout
                    : CallbackTimeoutStatus::kTimeout);
}

constexpr auto kMojoCdmTimeout = base::Seconds(20);

}  // namespace

MojoCdm::MojoCdm(mojo::Remote<mojom::ContentDecryptionModule> remote_cdm,
                 media::mojom::CdmContextPtr cdm_context,
                 const CdmConfig& cdm_config,
                 const SessionMessageCB& session_message_cb,
                 const SessionClosedCB& session_closed_cb,
                 const SessionKeysChangeCB& session_keys_change_cb,
                 const SessionExpirationUpdateCB& session_expiration_update_cb)
    : remote_cdm_(std::move(remote_cdm)),
      cdm_id_(cdm_context->cdm_id),
      decryptor_remote_(std::move(cdm_context->decryptor)),
#if BUILDFLAG(IS_WIN)
      requires_media_foundation_renderer_(
          cdm_context->requires_media_foundation_renderer),
#endif  // BUILDFLAG(IS_WIN)
      cdm_config_(cdm_config),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(cdm_id_);
  DVLOG(2) << __func__ << " cdm_id: "
           << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id_));
  DCHECK(session_message_cb_);
  DCHECK(session_closed_cb_);
  DCHECK(session_keys_change_cb_);
  DCHECK(session_expiration_update_cb_);

  remote_cdm_->SetClient(client_receiver_.BindNewEndpointAndPassRemote());

  // Report a false event here as a baseline.
  RecordConnectionError(false);

  // Otherwise, set an error handler to catch the connection error.
  remote_cdm_.set_disconnect_with_reason_handler(
      base::BindOnce(&MojoCdm::OnConnectionError, base::Unretained(this)));
}

MojoCdm::~MojoCdm() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  base::AutoLock auto_lock(lock_);

  // Release |decryptor_| on the correct thread. If GetDecryptor() is never
  // called but |decryptor_remote_| is not null, it is not bound to any
  // thread and is safe to be released on the current thread.
  if (decryptor_task_runner_ &&
      !decryptor_task_runner_->BelongsToCurrentThread() && decryptor_) {
    decryptor_task_runner_->DeleteSoon(FROM_HERE, decryptor_.release());
  }

  // Reject any outstanding promises and close all the existing sessions.
  cdm_promise_adapter_.Clear(CdmPromiseAdapter::ClearReason::kDestruction);
  cdm_session_tracker_.CloseRemainingSessions(
      session_closed_cb_, CdmSessionClosedReason::kInternalError);
}

// Using base::Unretained(this) below is safe because |this| owns |remote_cdm_|,
// and if |this| is destroyed, |remote_cdm_| will be destroyed as well. Then the
// error handler can't be invoked and callbacks won't be dispatched.

void MojoCdm::OnConnectionError(uint32_t custom_reason,
                                const std::string& description) {
  LOG(ERROR) << "Remote CDM connection error: custom_reason=" << custom_reason
             << ", description=\"" << description << "\"";
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RecordConnectionError(true);

  remote_cdm_.reset();

  // As communication with the remote CDM is broken, reject any outstanding
  // promises and close all the existing sessions.
  cdm_promise_adapter_.Clear(CdmPromiseAdapter::ClearReason::kConnectionError);
  cdm_session_tracker_.CloseRemainingSessions(
      session_closed_cb_, CdmSessionClosedReason::kInternalError);
}

void MojoCdm::SetServerCertificate(const std::vector<uint8_t>& certificate,
                                   std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->SetServerCertificate(
      certificate, base::BindOnce(&MojoCdm::OnSimpleCdmPromiseResult,
                                  base::Unretained(this), promise_id));
}

void MojoCdm::GetStatusForPolicy(HdcpVersion min_hdcp_version,
                                 std::unique_ptr<KeyStatusCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->GetStatusForPolicy(
      min_hdcp_version, base::BindOnce(&MojoCdm::OnKeyStatusCdmPromiseResult,
                                       base::Unretained(this), promise_id));
}

void MojoCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  std::string uma_name =
      "Media.EME." +
      GetKeySystemNameForUMA(cdm_config_.key_system,
                             cdm_config_.use_hw_secure_codecs) +
      ".GenerateRequest.MojoCdmTimeout";
  // Report "kCreate" as a baseline.
  base::UmaHistogramEnumeration(uma_name, CallbackTimeoutStatus::kCreate);

  remote_cdm_->CreateSessionAndGenerateRequest(
      session_type, init_data_type, init_data,
      WrapCallbackWithTimeoutHandler(
          base::BindOnce(&MojoCdm::OnNewSessionCdmPromiseResult,
                         base::Unretained(this), promise_id),
          kMojoCdmTimeout, base::BindOnce(&OnCallbackTimeout, uma_name)));
}

void MojoCdm::LoadSession(CdmSessionType session_type,
                          const std::string& session_id,
                          std::unique_ptr<NewSessionCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->LoadSession(
      session_type, session_id,
      base::BindOnce(&MojoCdm::OnNewSessionCdmPromiseResult,
                     base::Unretained(this), promise_id));
}

void MojoCdm::UpdateSession(const std::string& session_id,
                            const std::vector<uint8_t>& response,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->UpdateSession(
      session_id, response,
      base::BindOnce(&MojoCdm::OnSimpleCdmPromiseResult, base::Unretained(this),
                     promise_id));
}

void MojoCdm::CloseSession(const std::string& session_id,
                           std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->CloseSession(session_id,
                            base::BindOnce(&MojoCdm::OnSimpleCdmPromiseResult,
                                           base::Unretained(this), promise_id));
}

void MojoCdm::RemoveSession(const std::string& session_id,
                            std::unique_ptr<SimpleCdmPromise> promise) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  uint32_t promise_id =
      cdm_promise_adapter_.SavePromise(std::move(promise), __func__);

  if (!remote_cdm_) {
    RejectPromiseConnectionLost(promise_id);
    return;
  }

  remote_cdm_->RemoveSession(
      session_id, base::BindOnce(&MojoCdm::OnSimpleCdmPromiseResult,
                                 base::Unretained(this), promise_id));
}

CdmContext* MojoCdm::GetCdmContext() {
  DVLOG(2) << __func__;
  return this;
}

std::unique_ptr<CallbackRegistration> MojoCdm::RegisterEventCB(
    EventCB event_cb) {
  return event_callbacks_.Register(std::move(event_cb));
}

Decryptor* MojoCdm::GetDecryptor() {
  base::AutoLock auto_lock(lock_);

  if (!decryptor_task_runner_)
    decryptor_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  DCHECK(decryptor_task_runner_->BelongsToCurrentThread());

  if (decryptor_)
    return decryptor_.get();

  // Can be called on a different thread.
  if (decryptor_remote_.is_valid()) {
    DVLOG(1) << __func__ << ": Using Decryptor exposed by the CDM directly";
    decryptor_ = std::make_unique<MojoDecryptor>(std::move(decryptor_remote_));
  }

  return decryptor_.get();
}

std::optional<base::UnguessableToken> MojoCdm::GetCdmId() const {
  // Can be called on a different thread.
  base::AutoLock auto_lock(lock_);
  DVLOG(2) << __func__ << ": cdm_id="
           << CdmContext::CdmIdToString(base::OptionalToPtr(cdm_id_));
  return cdm_id_;
}

#if BUILDFLAG(IS_WIN)
bool MojoCdm::RequiresMediaFoundationRenderer() {
  base::AutoLock auto_lock(lock_);
  DVLOG(2) << __func__ << ": requires_media_foundation_renderer_="
           << requires_media_foundation_renderer_;
  return requires_media_foundation_renderer_;
}
#endif  // BUILDFLAG(IS_WIN)

void MojoCdm::OnSessionMessage(const std::string& session_id,
                               MessageType message_type,
                               const std::vector<uint8_t>& message) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_message_cb_.Run(session_id, message_type, message);
}

void MojoCdm::OnSessionClosed(const std::string& session_id,
                              CdmSessionClosedReason reason) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  cdm_session_tracker_.RemoveSession(session_id);
  session_closed_cb_.Run(session_id, reason);
}

void MojoCdm::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    std::vector<std::unique_ptr<CdmKeyInformation>> keys_info) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (has_additional_usable_key)
    event_callbacks_.Notify(Event::kHasAdditionalUsableKey);

  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void MojoCdm::OnSessionExpirationUpdate(const std::string& session_id,
                                        double new_expiry_time_sec) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  session_expiration_update_cb_.Run(
      session_id, base::Time::FromSecondsSinceUnixEpoch(new_expiry_time_sec));
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

void MojoCdm::RejectPromiseConnectionLost(uint32_t promise_id) {
  cdm_promise_adapter_.RejectPromise(
      promise_id, CdmPromise::Exception::INVALID_STATE_ERROR,
      CdmPromise::SystemCode::kConnectionError, "CDM connection lost.");
}

}  // namespace media
