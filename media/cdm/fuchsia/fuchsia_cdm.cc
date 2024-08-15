// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/fuchsia/fuchsia_cdm.h"

#include <optional>
#include <string_view>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/logging.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_factory.h"
#include "media/base/cdm_promise.h"

#define REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm)         \
  if (!cdm) {                                                      \
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0, \
                    "CDM channel is disconnected.");               \
    return;                                                        \
  }

namespace media {

namespace {

std::string GetInitDataTypeName(EmeInitDataType type) {
  switch (type) {
    case EmeInitDataType::WEBM:
      return "webm";
    case EmeInitDataType::CENC:
      return "cenc";
    case EmeInitDataType::KEYIDS:
      return "keyids";
    case EmeInitDataType::UNKNOWN:
      return "unknown";
  }
}

fuchsia::media::drm::LicenseInitData CreateLicenseInitData(
    EmeInitDataType type,
    const std::vector<uint8_t>& data) {
  fuchsia::media::drm::LicenseInitData init_data;
  init_data.type = GetInitDataTypeName(type);
  init_data.data = data;
  return init_data;
}

fuchsia::media::drm::LicenseServerMessage CreateLicenseServerMessage(
    const std::vector<uint8_t>& response) {
  fuchsia::media::drm::LicenseServerMessage message;
  message.message = base::MemBufferFromString(
      std::string_view(reinterpret_cast<const char*>(response.data()),
                       response.size()),
      "cr-drm-license-server-message");
  return message;
}

fuchsia::media::drm::LicenseSessionType ToFuchsiaLicenseSessionType(
    CdmSessionType session_type) {
  switch (session_type) {
    case CdmSessionType::kTemporary:
      return fuchsia::media::drm::LicenseSessionType::TEMPORARY;
    case CdmSessionType::kPersistentLicense:
      return fuchsia::media::drm::LicenseSessionType::PERSISTENT_LICENSE;
  }
}

CdmMessageType ToCdmMessageType(fuchsia::media::drm::LicenseMessageType type) {
  switch (type) {
    case fuchsia::media::drm::LicenseMessageType::REQUEST:
      return CdmMessageType::LICENSE_REQUEST;
    case fuchsia::media::drm::LicenseMessageType::RENEWAL:
      return CdmMessageType::LICENSE_RENEWAL;
    case fuchsia::media::drm::LicenseMessageType::RELEASE:
      return CdmMessageType::LICENSE_RELEASE;
  }
}

CdmKeyInformation::KeyStatus ToCdmKeyStatus(
    fuchsia::media::drm::KeyStatus status) {
  switch (status) {
    case fuchsia::media::drm::KeyStatus::USABLE:
      return CdmKeyInformation::USABLE;
    case fuchsia::media::drm::KeyStatus::EXPIRED:
      return CdmKeyInformation::EXPIRED;
    case fuchsia::media::drm::KeyStatus::RELEASED:
      return CdmKeyInformation::RELEASED;
    case fuchsia::media::drm::KeyStatus::OUTPUT_RESTRICTED:
      return CdmKeyInformation::OUTPUT_RESTRICTED;
    case fuchsia::media::drm::KeyStatus::OUTPUT_DOWNSCALED:
      return CdmKeyInformation::OUTPUT_DOWNSCALED;
    case fuchsia::media::drm::KeyStatus::STATUS_PENDING:
      return CdmKeyInformation::KEY_STATUS_PENDING;
    case fuchsia::media::drm::KeyStatus::INTERNAL_ERROR:
      return CdmKeyInformation::INTERNAL_ERROR;
  }
}

CdmPromise::Exception ToCdmPromiseException(fuchsia::media::drm::Error error) {
  switch (error) {
    case fuchsia::media::drm::Error::TYPE:
      return CdmPromise::Exception::TYPE_ERROR;
    case fuchsia::media::drm::Error::NOT_SUPPORTED:
      return CdmPromise::Exception::NOT_SUPPORTED_ERROR;
    case fuchsia::media::drm::Error::INVALID_STATE:
      return CdmPromise::Exception::INVALID_STATE_ERROR;
    case fuchsia::media::drm::Error::QUOTA_EXCEEDED:
      return CdmPromise::Exception::QUOTA_EXCEEDED_ERROR;

    case fuchsia::media::drm::Error::NOT_PROVISIONED:
      // FuchsiaCdmManager is supposed to provision CDM.
      NOTREACHED();

    case fuchsia::media::drm::Error::INTERNAL:
      DLOG(ERROR) << "CDM failed due to an internal error.";
      return CdmPromise::Exception::INVALID_STATE_ERROR;
  }
}

}  // namespace

class FuchsiaCdm::CdmSession {
 public:
  using ResultCB =
      base::OnceCallback<void(std::optional<CdmPromise::Exception>)>;
  using SessionReadyCB = base::OnceCallback<void(bool success)>;

  CdmSession(const FuchsiaCdm::SessionCallbacks* callbacks,
             base::RepeatingClosure on_new_key)
      : session_callbacks_(callbacks), on_new_key_(on_new_key) {
    // License session events, e.g. license request message, key status change.
    // Fuchsia CDM service guarantees callback of functions (e.g.
    // GenerateLicenseRequest) are called before event callbacks. So it's safe
    // to rely on this to resolve the EME promises and send session events to
    // JS. EME requires promises are resolved before session message.
    session_.events().OnLicenseMessageGenerated =
        fit::bind_member(this, &CdmSession::OnLicenseMessageGenerated);
    session_.events().OnKeyStatesChanged =
        fit::bind_member(this, &CdmSession::OnKeyStatesChanged);

    session_.set_error_handler(
        fit::bind_member(this, &CdmSession::OnSessionError));
  }

  CdmSession(const CdmSession&) = delete;
  CdmSession& operator=(const CdmSession&) = delete;

  ~CdmSession() {
    if (!session_id_.empty()) {
      session_callbacks_->closed_cb.Run(session_id_,
                                        CdmSessionClosedReason::kInternalError);
    }
  }

  fidl::InterfaceRequest<fuchsia::media::drm::LicenseSession> NewRequest() {
    return session_.NewRequest();
  }

  void GenerateLicenseRequest(EmeInitDataType init_data_type,
                              const std::vector<uint8_t>& init_data,
                              ResultCB generate_license_request_cb) {
    DCHECK(!result_cb_);
    result_cb_ = std::move(generate_license_request_cb);
    session_->GenerateLicenseRequest(
        CreateLicenseInitData(init_data_type, init_data),
        [this](fuchsia::media::drm::LicenseSession_GenerateLicenseRequest_Result
                   result) { ProcessResult(result); });
  }

  void GenerateLicenseRelease(ResultCB generate_license_release_cb) {
    DCHECK(!result_cb_);
    result_cb_ = std::move(generate_license_release_cb);
    pending_release_ = true;
    session_->GenerateLicenseRelease(
        [this](fuchsia::media::drm::LicenseSession_GenerateLicenseRelease_Result
                   result) { ProcessResult(result); });
  }

  void ProcessLicenseResponse(const std::vector<uint8_t>& response,
                              ResultCB process_license_response_cb) {
    DCHECK(!result_cb_);
    result_cb_ = std::move(process_license_response_cb);
    session_->ProcessLicenseResponse(
        CreateLicenseServerMessage(response),
        [this](fuchsia::media::drm::LicenseSession_ProcessLicenseResponse_Result
                   result) { ProcessResult(result); });
  }

  void set_session_id(const std::string& session_id) {
    session_id_ = session_id;
  }
  const std::string& session_id() const { return session_id_; }

  void set_session_ready_cb(SessionReadyCB session_ready_cb) {
    session_ready_cb_ = std::move(session_ready_cb);
    session_.events().OnReady =
        fit::bind_member(this, &CdmSession::OnSessionReady);
  }

  bool pending_release() const { return pending_release_; }

 private:
  void OnSessionReady() {
    DCHECK(session_ready_cb_);
    std::move(session_ready_cb_).Run(true);
  }

  void OnLicenseMessageGenerated(fuchsia::media::drm::LicenseMessage message) {
    DCHECK(!session_id_.empty());
    std::optional<std::string> session_msg =
        base::StringFromMemBuffer(message.message);

    if (!session_msg) {
      LOG(ERROR) << "Failed to generate message for session " << session_id_;
      return;
    }

    session_callbacks_->message_cb.Run(
        session_id_, ToCdmMessageType(message.type),
        std::vector<uint8_t>(session_msg->begin(), session_msg->end()));
  }

  void OnKeyStatesChanged(
      std::vector<fuchsia::media::drm::KeyState> key_states) {
    bool has_additional_usable_key = false;
    CdmKeysInfo keys_info;
    for (const auto& key_state : key_states) {
      if (!key_state.has_key_id() || !key_state.has_status()) {
        continue;
      }
      CdmKeyInformation::KeyStatus status = ToCdmKeyStatus(key_state.status());
      has_additional_usable_key |= (status == CdmKeyInformation::USABLE);
      keys_info.emplace_back(
          new CdmKeyInformation(key_state.key_id(), status, 0));
    }

    session_callbacks_->keys_change_cb.Run(
        session_id_, has_additional_usable_key, std::move(keys_info));

    if (has_additional_usable_key) {
      on_new_key_.Run();
    }
  }

  void OnSessionError(zx_status_t status) {
    ZX_LOG(ERROR, status) << "Session error.";

    if (session_ready_cb_) {
      std::move(session_ready_cb_).Run(false);
    }

    if (result_cb_) {
      std::move(result_cb_).Run(CdmPromise::Exception::TYPE_ERROR);
    }
  }

  template <typename T>
  void ProcessResult(const T& result) {
    DCHECK(result_cb_);
    std::move(result_cb_)
        .Run(result.is_err()
                 ? std::make_optional(ToCdmPromiseException(result.err()))
                 : std::nullopt);
  }

  const SessionCallbacks* const session_callbacks_;
  base::RepeatingClosure on_new_key_;

  fuchsia::media::drm::LicenseSessionPtr session_;
  std::string session_id_;

  // Callback for OnReady.
  SessionReadyCB session_ready_cb_;

  // Callback for license operation.
  ResultCB result_cb_;

  // `GenerateLicenseRelease` has been called and the session is waiting for
  // license release response from server.
  bool pending_release_ = false;
};

FuchsiaCdm::SessionCallbacks::SessionCallbacks() = default;
FuchsiaCdm::SessionCallbacks::SessionCallbacks(SessionCallbacks&&) = default;
FuchsiaCdm::SessionCallbacks::~SessionCallbacks() = default;
FuchsiaCdm::SessionCallbacks& FuchsiaCdm::SessionCallbacks::operator=(
    SessionCallbacks&&) = default;

FuchsiaCdm::FuchsiaCdm(fuchsia::media::drm::ContentDecryptionModulePtr cdm,
                       ReadyCB ready_cb,
                       SessionCallbacks callbacks)
    : cdm_(std::move(cdm)),
      ready_cb_(std::move(ready_cb)),
      session_callbacks_(std::move(callbacks)),
      decryptor_(this) {
  DCHECK(cdm_);
  cdm_.events().OnProvisioned =
      fit::bind_member(this, &FuchsiaCdm::OnProvisioned);
  cdm_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "The fuchsia.media.drm.ContentDecryptionModule"
                          << " channel was terminated.";

    // Reject all the pending promises.
    promises_.Clear(CdmPromiseAdapter::ClearReason::kConnectionError);

    // If the channel closed prior to invoking the ready_cb_, we should invoke
    // it here with failure.
    if (ready_cb_) {
      std::move(ready_cb_).Run(false, CreateCdmStatus::kDisconnectionError);
    }
  });
}

FuchsiaCdm::~FuchsiaCdm() = default;

std::unique_ptr<SysmemBufferStream> FuchsiaCdm::CreateStreamDecryptor(
    bool secure_mode) {
  fuchsia::media::drm::DecryptorParams params;
  params.set_require_secure_mode(secure_mode);
  params.mutable_input_details()->set_format_details_version_ordinal(0);

  fuchsia::media::StreamProcessorPtr stream_processor;
  cdm_->CreateDecryptor(std::move(params), stream_processor.NewRequest());

  auto decryptor =
      std::make_unique<FuchsiaStreamDecryptor>(std::move(stream_processor));

  // Save callback to use to notify the decryptor about a new key.
  auto new_key_cb = decryptor->GetOnNewKeyClosure();
  {
    base::AutoLock auto_lock(new_key_callbacks_lock_);
    new_key_callbacks_.push_back(std::move(new_key_cb));
  }

  return decryptor;
}

void FuchsiaCdm::SetServerCertificate(
    const std::vector<uint8_t>& certificate,
    std::unique_ptr<SimpleCdmPromise> promise) {
  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  uint32_t promise_id = promises_.SavePromise(std::move(promise));
  cdm_->SetServerCertificate(
      certificate,
      [this, promise_id](
          fuchsia::media::drm::
              ContentDecryptionModule_SetServerCertificate_Result result) {
        if (result.is_err()) {
          promises_.RejectPromise(promise_id,
                                  ToCdmPromiseException(result.err()), 0,
                                  "Fail to set server cert.");
          return;
        }

        promises_.ResolvePromise(promise_id);
      });
}

void FuchsiaCdm::GetStatusForPolicy(
    media::HdcpVersion min_hdcp_version,
    std::unique_ptr<KeyStatusCdmPromise> promise) {
  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  // Fuchsia devices do not support external display, so the internal display
  // can support all HDCP levels.
  promise->resolve(CdmKeyInformation::KeyStatus::USABLE);
}

void FuchsiaCdm::CreateSessionAndGenerateRequest(
    CdmSessionType session_type,
    EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<NewSessionCdmPromise> promise) {
  if (init_data_type == EmeInitDataType::UNKNOWN) {
    promise->reject(CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                    "init data type is not supported.");
    return;
  }

  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  uint32_t promise_id = promises_.SavePromise(std::move(promise));

  auto session = std::make_unique<CdmSession>(
      &session_callbacks_,
      base::BindRepeating(&FuchsiaCdm::OnNewKey, base::Unretained(this)));
  CdmSession* session_ptr = session.get();

  cdm_->CreateLicenseSession(
      ToFuchsiaLicenseSessionType(session_type), session_ptr->NewRequest(),
      [this, promise_id,
       session = std::move(session)](std::string session_id) mutable {
        OnCreateSession(std::move(session), promise_id, session_id);
      });

  // It's safe to pass raw pointer |session_ptr| because |session| owns the
  // callback so it's guaranteed to outlive the callback.
  session_ptr->GenerateLicenseRequest(
      init_data_type, init_data,
      base::BindOnce(&FuchsiaCdm::OnGenerateLicenseRequestStatus,
                     base::Unretained(this), session_ptr, promise_id));
}

void FuchsiaCdm::OnProvisioned() {
  if (ready_cb_) {
    std::move(ready_cb_).Run(true, CreateCdmStatus::kSuccess);
  }
}

void FuchsiaCdm::OnCreateSession(std::unique_ptr<CdmSession> session,
                                 uint32_t promise_id,
                                 const std::string& session_id) {
  if (session_id.empty()) {
    promises_.RejectPromise(promise_id,
                            CdmPromise::Exception::NOT_SUPPORTED_ERROR, 0,
                            "fail to create license session.");
    return;
  }

  session->set_session_id(session_id);
  DCHECK(!session_map_.contains(session_id))
      << "Duplicated session id " << session_id;
  session_map_[session_id] = std::move(session);
}

void FuchsiaCdm::OnGenerateLicenseRequestStatus(
    CdmSession* session,
    uint32_t promise_id,
    std::optional<CdmPromise::Exception> exception) {
  DCHECK(session);
  std::string session_id = session->session_id();

  if (exception.has_value()) {
    promises_.RejectPromise(promise_id, exception.value(), 0,
                            "fail to generate license.");
    session_map_.erase(session_id);
    return;
  }

  DCHECK(!session_id.empty());
  promises_.ResolvePromise(promise_id, session_id);
}

void FuchsiaCdm::LoadSession(CdmSessionType session_type,
                             const std::string& session_id,
                             std::unique_ptr<NewSessionCdmPromise> promise) {
  DCHECK_NE(session_type, CdmSessionType::kTemporary);
  DCHECK(!session_id.empty());
  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  if (session_map_.contains(session_id)) {
    promise->reject(CdmPromise::Exception::QUOTA_EXCEEDED_ERROR, 0,
                    "session already exists.");
    return;
  }

  uint32_t promise_id = promises_.SavePromise(std::move(promise));

  auto session = std::make_unique<CdmSession>(
      &session_callbacks_,
      base::BindRepeating(&FuchsiaCdm::OnNewKey, base::Unretained(this)));
  CdmSession* session_ptr = session.get();

  session_ptr->set_session_id(session_id);
  session_ptr->set_session_ready_cb(
      base::BindOnce(&FuchsiaCdm::OnSessionLoaded, base::Unretained(this),
                     std::move(session), promise_id));

  cdm_->LoadLicenseSession(session_id, session_ptr->NewRequest());
}

void FuchsiaCdm::OnSessionLoaded(std::unique_ptr<CdmSession> session,
                                 uint32_t promise_id,
                                 bool loaded) {
  if (!loaded) {
    promises_.ResolvePromise(promise_id, std::string());
    return;
  }

  std::string session_id = session->session_id();
  DCHECK(!session_map_.contains(session_id))
      << "Duplicated session id " << session_id;

  session_map_.emplace(session_id, std::move(session));

  promises_.ResolvePromise(promise_id, session_id);
}

void FuchsiaCdm::UpdateSession(const std::string& session_id,
                               const std::vector<uint8_t>& response,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  auto it = session_map_.find(session_id);
  if (it == session_map_.end()) {
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "session doesn't exist.");
    return;
  }

  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  // Caller should NOT pass in an empty response.
  DCHECK(!response.empty());

  uint32_t promise_id = promises_.SavePromise(std::move(promise));

  CdmSession* session = it->second.get();
  DCHECK(session);

  session->ProcessLicenseResponse(
      response, base::BindOnce(&FuchsiaCdm::OnProcessLicenseServerMessageStatus,
                               base::Unretained(this), session_id, promise_id));
}

void FuchsiaCdm::OnProcessLicenseServerMessageStatus(
    const std::string& session_id,
    uint32_t promise_id,
    std::optional<CdmPromise::Exception> exception) {
  if (exception.has_value()) {
    promises_.RejectPromise(promise_id, exception.value(), 0,
                            "fail to process license.");
    return;
  }

  promises_.ResolvePromise(promise_id);

  auto it = session_map_.find(session_id);
  if (it == session_map_.end()) {
    return;
  }

  // Close the session if the session is waiting for license release ack.
  CdmSession* session = it->second.get();
  DCHECK(session);

  if (!session->pending_release()) {
    return;
  }

  session_map_.erase(it);
}

void FuchsiaCdm::CloseSession(const std::string& session_id,
                              std::unique_ptr<SimpleCdmPromise> promise) {
  // CdmSession will call SessionClosedCB in its destruct. This should be done
  // before the promise is resolved.
  session_map_.erase(session_id);

  promise->resolve();
}

void FuchsiaCdm::RemoveSession(const std::string& session_id,
                               std::unique_ptr<SimpleCdmPromise> promise) {
  auto it = session_map_.find(session_id);
  if (it == session_map_.end()) {
    promise->reject(CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                    "session doesn't exist.");
    return;
  }

  REJECT_PROMISE_AND_RETURN_IF_BAD_CDM(promise, cdm_);

  uint32_t promise_id = promises_.SavePromise(std::move(promise));

  CdmSession* session = it->second.get();
  DCHECK(session);

  // For a temporary session, the API will remove the keys and close the
  // session. For a persistent license session, the API will invalidate the keys
  // and generates a license release message.
  session->GenerateLicenseRelease(
      base::BindOnce(&FuchsiaCdm::OnGenerateLicenseReleaseStatus,
                     base::Unretained(this), session_id, promise_id));
}

void FuchsiaCdm::OnGenerateLicenseReleaseStatus(
    const std::string& session_id,
    uint32_t promise_id,
    std::optional<CdmPromise::Exception> exception) {
  if (exception.has_value()) {
    promises_.RejectPromise(promise_id, exception.value(), 0,
                            "Failed to release license.");
    session_map_.erase(session_id);
    return;
  }

  DCHECK(!session_id.empty());
  promises_.ResolvePromise(promise_id);
}

CdmContext* FuchsiaCdm::GetCdmContext() {
  return this;
}

std::unique_ptr<CallbackRegistration> FuchsiaCdm::RegisterEventCB(
    EventCB event_cb) {
  return event_callbacks_.Register(std::move(event_cb));
}

Decryptor* FuchsiaCdm::GetDecryptor() {
  return &decryptor_;
}

FuchsiaCdmContext* FuchsiaCdm::GetFuchsiaCdmContext() {
  return this;
}

void FuchsiaCdm::OnNewKey() {
  event_callbacks_.Notify(Event::kHasAdditionalUsableKey);
  {
    base::AutoLock auto_lock(new_key_callbacks_lock_);

    // Remove cancelled callbacks.
    new_key_callbacks_.erase(
        std::remove_if(
            new_key_callbacks_.begin(), new_key_callbacks_.end(),
            [](const base::RepeatingClosure& cb) { return cb.IsCancelled(); }),
        new_key_callbacks_.end());

    for (auto& cb : new_key_callbacks_) {
      cb.Run();
    }
  }
}

}  // namespace media
