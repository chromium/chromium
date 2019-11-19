// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webcontentdecryptionmodulesession_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise.h"
#include "media/base/content_decryption_module.h"
#include "media/base/key_system_names.h"
#include "media/base/key_systems.h"
#include "media/base/limits.h"
#include "media/blink/cdm_result_promise.h"
#include "media/blink/cdm_result_promise_helper.h"
#include "media/blink/cdm_session_adapter.h"
#include "media/cdm/cenc_utils.h"
#include "media/cdm/json_web_key.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/modules/media/webmediaplayer_util.h"

namespace media {

namespace {

const char kCloseSessionUMAName[] = "CloseSession";
const char kGenerateRequestUMAName[] = "GenerateRequest";
const char kLoadSessionUMAName[] = "LoadSession";
const char kRemoveSessionUMAName[] = "RemoveSession";
const char kUpdateSessionUMAName[] = "UpdateSession";
const char kKeyStatusSystemCodeUMAName[] = "KeyStatusSystemCode";

blink::WebContentDecryptionModuleSession::Client::MessageType
convertMessageType(CdmMessageType message_type) {
  switch (message_type) {
    case CdmMessageType::LICENSE_REQUEST:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          kLicenseRequest;
    case CdmMessageType::LICENSE_RENEWAL:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          kLicenseRenewal;
    case CdmMessageType::LICENSE_RELEASE:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          kLicenseRelease;
    case CdmMessageType::INDIVIDUALIZATION_REQUEST:
      return blink::WebContentDecryptionModuleSession::Client::MessageType::
          kIndividualizationRequest;
  }

  NOTREACHED();
  return blink::WebContentDecryptionModuleSession::Client::MessageType::
      kLicenseRequest;
}

CdmSessionType convertSessionType(
    blink::WebEncryptedMediaSessionType session_type) {
  switch (session_type) {
    case blink::WebEncryptedMediaSessionType::kTemporary:
      return CdmSessionType::kTemporary;
    case blink::WebEncryptedMediaSessionType::kPersistentLicense:
      return CdmSessionType::kPersistentLicense;
    case blink::WebEncryptedMediaSessionType::kPersistentUsageRecord:
      return CdmSessionType::kPersistentUsageRecord;
    case blink::WebEncryptedMediaSessionType::kUnknown:
      break;
  }

  NOTREACHED();
  return CdmSessionType::kTemporary;
}

bool SanitizeInitData(EmeInitDataType init_data_type,
                      const unsigned char* init_data,
                      size_t init_data_length,
                      std::vector<uint8_t>* sanitized_init_data,
                      std::string* error_message) {
  DCHECK_GT(init_data_length, 0u);
  if (init_data_length > limits::kMaxInitDataLength) {
    error_message->assign("Initialization data too long.");
    return false;
  }

  switch (init_data_type) {
    case EmeInitDataType::WEBM:
      // |init_data| for WebM is a single key.
      if (init_data_length > limits::kMaxKeyIdLength) {
        error_message->assign("Initialization data for WebM is too long.");
        return false;
      }
      sanitized_init_data->assign(init_data, init_data + init_data_length);
      return true;

    case EmeInitDataType::CENC:
      sanitized_init_data->assign(init_data, init_data + init_data_length);
      if (!ValidatePsshInput(*sanitized_init_data)) {
        error_message->assign("Initialization data for CENC is incorrect.");
        return false;
      }
      return true;

    case EmeInitDataType::KEYIDS: {
      // Extract the keys and then rebuild the message. This ensures that any
      // extra data in the provided JSON is dropped.
      std::string init_data_string(init_data, init_data + init_data_length);
      KeyIdList key_ids;
      if (!ExtractKeyIdsFromKeyIdsInitData(init_data_string, &key_ids,
                                           error_message))
        return false;

      for (const auto& key_id : key_ids) {
        if (key_id.size() < limits::kMinKeyIdLength ||
            key_id.size() > limits::kMaxKeyIdLength) {
          error_message->assign("Incorrect key size.");
          return false;
        }
      }

      CreateKeyIdsInitData(key_ids, sanitized_init_data);
      return true;
    }

    case EmeInitDataType::UNKNOWN:
      break;
  }

  NOTREACHED();
  error_message->assign("Initialization data type is not supported.");
  return false;
}

bool SanitizeSessionId(const blink::WebString& session_id,
                       std::string* sanitized_session_id) {
  // The user agent should thoroughly validate the sessionId value before
  // passing it to the CDM. At a minimum, this should include checking that
  // the length and value (e.g. alphanumeric) are reasonable.
  if (!session_id.ContainsOnlyASCII())
    return false;

  sanitized_session_id->assign(session_id.Ascii());
  if (sanitized_session_id->length() > limits::kMaxSessionIdLength)
    return false;

  // Check that |sanitized_session_id| only contains non-space printable
  // characters for easier logging. Note that checking alphanumeric is too
  // strict because there are key systems using Base64 session IDs. See
  // https://crbug.com/902828.
  for (const char c : *sanitized_session_id) {
    if (!base::IsAsciiPrintable(c) || c == ' ')
      return false;
  }

  return true;
}

bool SanitizeResponse(const std::string& key_system,
                      const uint8_t* response,
                      size_t response_length,
                      std::vector<uint8_t>* sanitized_response) {
  // The user agent should thoroughly validate the response before passing it
  // to the CDM. This may include verifying values are within reasonable limits,
  // stripping irrelevant data or fields, pre-parsing it, sanitizing it,
  // and/or generating a fully sanitized version. The user agent should check
  // that the length and values of fields are reasonable. Unknown fields should
  // be rejected or removed.
  if (response_length > limits::kMaxSessionResponseLength)
    return false;

  if (IsClearKey(key_system) || IsExternalClearKey(key_system)) {
    std::string key_string(response, response + response_length);
    KeyIdAndKeyPairs keys;
    CdmSessionType session_type = CdmSessionType::kTemporary;
    if (!ExtractKeysFromJWKSet(key_string, &keys, &session_type))
      return false;

    // Must contain at least one key.
    if (keys.empty())
      return false;

    for (const auto key_pair : keys) {
      if (key_pair.first.size() < limits::kMinKeyIdLength ||
          key_pair.first.size() > limits::kMaxKeyIdLength) {
        return false;
      }
    }

    std::string sanitized_data = GenerateJWKSet(keys, session_type);
    sanitized_response->assign(sanitized_data.begin(), sanitized_data.end());
    return true;
  }

  // TODO(jrummell): Verify responses for Widevine.
  sanitized_response->assign(response, response + response_length);
  return true;
}

// If we need to close the session while destroying this object, we need a
// dummy promise that won't call back into this object (or try to pass
// something back to blink).
class IgnoreResponsePromise : public SimpleCdmPromise {
 public:
  IgnoreResponsePromise() = default;
  ~IgnoreResponsePromise() override = default;

  // SimpleCdmPromise implementation.
  void resolve() final { MarkPromiseSettled(); }
  void reject(CdmPromise::Exception exception_code,
              uint32_t system_code,
              const std::string& error_message) final {
    MarkPromiseSettled();
  }
};

}  // namespace

WebContentDecryptionModuleSessionImpl::WebContentDecryptionModuleSessionImpl(
    const scoped_refptr<CdmSessionAdapter>& adapter)
    : adapter_(adapter),
      has_close_been_called_(false),
      is_closed_(false),
      is_persistent_session_(false) {}

WebContentDecryptionModuleSessionImpl::
    ~WebContentDecryptionModuleSessionImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!session_id_.empty()) {
    adapter_->UnregisterSession(session_id_);

    // From http://w3c.github.io/encrypted-media/#mediakeysession-interface
    // "If a MediaKeySession object is not closed when it becomes inaccessible
    // to the page, the CDM shall close the key session associated with the
    // object."
    //
    // This object is destroyed when the corresponding blink object is no
    // longer needed (which may be due to it becoming inaccessible to the
    // page), so if the session is not closed and CloseSession() has not yet
    // been called, call CloseSession() now. Since this object is being
    // destroyed, there is no need for the promise to do anything as this
    // session will be gone.
    if (!is_closed_ && !has_close_been_called_) {
      adapter_->CloseSession(session_id_,
                             std::make_unique<IgnoreResponsePromise>());
    }
  }
}

void WebContentDecryptionModuleSessionImpl::SetClientInterface(Client* client) {
  client_ = client;
}

blink::WebString WebContentDecryptionModuleSessionImpl::SessionId() const {
  return blink::WebString::FromUTF8(session_id_);
}

void WebContentDecryptionModuleSessionImpl::InitializeNewSession(
    EmeInitDataType eme_init_data_type,
    const unsigned char* init_data,
    size_t init_data_length,
    blink::WebEncryptedMediaSessionType session_type,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(init_data);
  DCHECK(session_id_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  // From https://w3c.github.io/encrypted-media/#generateRequest.
  // 6. If the Key System implementation represented by this object's cdm
  //    implementation value does not support initDataType as an Initialization
  //    Data Type, return a promise rejected with a NotSupportedError.
  //    String comparison is case-sensitive.
  if (!IsSupportedKeySystemWithInitDataType(adapter_->GetKeySystem(),
                                            eme_init_data_type)) {
    std::string message =
        "The initialization data type is not supported by the key system.";
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        blink::WebString::FromUTF8(message));
    return;
  }

  // 10.1 If the init data is not valid for initDataType, reject promise with
  //      a newly created TypeError.
  // 10.2 Let sanitized init data be a validated and sanitized version of init
  //      data. The user agent must thoroughly validate the Initialization Data
  //      before passing it to the CDM. This includes verifying that the length
  //      and values of fields are reasonable, verifying that values are within
  //      reasonable limits, and stripping irrelevant, unsupported, or unknown
  //      data or fields. It is recommended that user agents pre-parse,
  //      sanitize, and/or generate a fully sanitized version of the
  //      Initialization Data. If the Initialization Data format specified by
  //      initDataType supports multiple entries, the user agent should remove
  //      entries that are not needed by the CDM. The user agent must not
  //      re-order entries within the Initialization Data.
  // 10.3 If the preceding step failed, reject promise with a newly created
  //      TypeError.
  std::vector<uint8_t> sanitized_init_data;
  std::string message;
  if (!SanitizeInitData(eme_init_data_type, init_data, init_data_length,
                        &sanitized_init_data, &message)) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionTypeError, 0,
        blink::WebString::FromUTF8(message));
    return;
  }

  // 10.4 If sanitized init data is empty, reject promise with a
  //      NotSupportedError.
  if (sanitized_init_data.empty()) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionNotSupportedError, 0,
        "No initialization data provided.");
    return;
  }

  // 10.5 Let session id be the empty string.
  //      (Done in constructor.)

  // 10.6 Let message be null.
  // 10.7 Let message type be null.
  //      (Done by CDM.)

  // 10.8 Let cdm be the CDM instance represented by this object's cdm
  //      instance value.
  // 10.9 Use the cdm to execute the following steps:
  CdmSessionType cdm_session_type = convertSessionType(session_type);
  is_persistent_session_ = cdm_session_type != CdmSessionType::kTemporary;
  adapter_->InitializeNewSession(
      eme_init_data_type, sanitized_init_data, cdm_session_type,
      std::unique_ptr<NewSessionCdmPromise>(new NewSessionCdmResultPromise(
          result, adapter_->GetKeySystemUMAPrefix(), kGenerateRequestUMAName,
          base::Bind(
              &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
              weak_ptr_factory_.GetWeakPtr()),
          {SessionInitStatus::NEW_SESSION})));
}

void WebContentDecryptionModuleSessionImpl::Load(
    const blink::WebString& session_id,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id.IsEmpty());
  DCHECK(session_id_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  // From https://w3c.github.io/encrypted-media/#load.
  // 8.1 Let sanitized session ID be a validated and/or sanitized version of
  //     sessionId. The user agent should thoroughly validate the sessionId
  //     value before passing it to the CDM. At a minimum, this should include
  //     checking that the length and value (e.g. alphanumeric) are reasonable.
  // 8.2 If the preceding step failed, or if sanitized session ID is empty,
  //     reject promise with a newly created TypeError.
  std::string sanitized_session_id;
  if (!SanitizeSessionId(session_id, &sanitized_session_id)) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionTypeError, 0,
        "Invalid session ID.");
    return;
  }

  // TODO(jrummell): Now that there are 2 types of persistent sessions, the
  // session type should be passed from blink. Type should also be passed in the
  // constructor (and removed from initializeNewSession()).
  is_persistent_session_ = true;
  adapter_->LoadSession(
      CdmSessionType::kPersistentLicense, sanitized_session_id,
      std::unique_ptr<NewSessionCdmPromise>(new NewSessionCdmResultPromise(
          result, adapter_->GetKeySystemUMAPrefix(), kLoadSessionUMAName,
          base::Bind(
              &WebContentDecryptionModuleSessionImpl::OnSessionInitialized,
              weak_ptr_factory_.GetWeakPtr()),
          {SessionInitStatus::NEW_SESSION,
           SessionInitStatus::SESSION_NOT_FOUND})));
}

void WebContentDecryptionModuleSessionImpl::Update(
    const uint8_t* response,
    size_t response_length,
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(response);
  DCHECK(!session_id_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  // From https://w3c.github.io/encrypted-media/#update.
  // 6.1 Let sanitized response be a validated and/or sanitized version of
  //     response copy. The user agent should thoroughly validate the response
  //     before passing it to the CDM. This may include verifying values are
  //     within reasonable limits, stripping irrelevant data or fields,
  //     pre-parsing it, sanitizing it, and/or generating a fully sanitized
  //     version. The user agent should check that the length and values of
  //     fields are reasonable. Unknown fields should be rejected or removed.
  // 6.2 If the preceding step failed, or if sanitized response is empty,
  //     reject promise with a newly created TypeError.
  std::vector<uint8_t> sanitized_response;
  if (!SanitizeResponse(adapter_->GetKeySystem(), response, response_length,
                        &sanitized_response)) {
    result.CompleteWithError(
        blink::kWebContentDecryptionModuleExceptionTypeError, 0,
        "Invalid response.");
    return;
  }

  adapter_->UpdateSession(
      session_id_, sanitized_response,
      std::make_unique<CdmResultPromise<>>(
          result, adapter_->GetKeySystemUMAPrefix(), kUpdateSessionUMAName));
}

void WebContentDecryptionModuleSessionImpl::Close(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  // close() shouldn't be called if the session is already closed. Since the
  // operation is asynchronous, there is a window where close() was called
  // just before the closed event arrives. The CDM should handle the case where
  // close() is called after it has already closed the session. However, if
  // we can tell the session is now closed, simply resolve the promise.
  if (is_closed_) {
    result.Complete();
    return;
  }

  has_close_been_called_ = true;
  adapter_->CloseSession(
      session_id_,
      std::make_unique<CdmResultPromise<>>(
          result, adapter_->GetKeySystemUMAPrefix(), kCloseSessionUMAName));
}

void WebContentDecryptionModuleSessionImpl::Remove(
    blink::WebContentDecryptionModuleResult result) {
  DCHECK(!session_id_.empty());
  DCHECK(thread_checker_.CalledOnValidThread());

  adapter_->RemoveSession(
      session_id_,
      std::make_unique<CdmResultPromise<>>(
          result, adapter_->GetKeySystemUMAPrefix(), kRemoveSessionUMAName));
}

void WebContentDecryptionModuleSessionImpl::OnSessionMessage(
    CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  DCHECK(client_) << "Client not set before message event";
  DCHECK(thread_checker_.CalledOnValidThread());
  client_->Message(convertMessageType(message_type), message.data(),
                   message.size());
}

void WebContentDecryptionModuleSessionImpl::OnSessionKeysChange(
    bool has_additional_usable_key,
    CdmKeysInfo keys_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  blink::WebVector<blink::WebEncryptedMediaKeyInformation> keys(
      keys_info.size());
  for (size_t i = 0; i < keys_info.size(); ++i) {
    auto& key_info = keys_info[i];
    keys[i].SetId(blink::WebData(reinterpret_cast<char*>(&key_info->key_id[0]),
                                 key_info->key_id.size()));
    keys[i].SetStatus(ConvertCdmKeyStatus(key_info->status));
    keys[i].SetSystemCode(key_info->system_code);

    base::UmaHistogramSparse(
        adapter_->GetKeySystemUMAPrefix() + kKeyStatusSystemCodeUMAName,
        key_info->system_code);
  }

  // Now send the event to blink.
  client_->KeysStatusesChange(keys, has_additional_usable_key);
}

void WebContentDecryptionModuleSessionImpl::OnSessionExpirationUpdate(
    base::Time new_expiry_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // The check works around an issue in base::Time that converts null base::Time
  // to |1601-01-01 00:00:00 UTC| in ToJsTime(). See http://crbug.com/679079
  client_->ExpirationChanged(new_expiry_time.is_null()
                                 ? std::numeric_limits<double>::quiet_NaN()
                                 : new_expiry_time.ToJsTime());
}

void WebContentDecryptionModuleSessionImpl::OnSessionClosed() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Only send one closed event to blink.
  if (is_closed_)
    return;

  is_closed_ = true;
  client_->Close();
}

void WebContentDecryptionModuleSessionImpl::OnSessionInitialized(
    const std::string& session_id,
    SessionInitStatus* status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // CDM will return NULL if the session to be loaded can't be found.
  if (session_id.empty()) {
    *status = SessionInitStatus::SESSION_NOT_FOUND;
    return;
  }

  DCHECK(session_id_.empty()) << "Session ID may not be changed once set.";
  session_id_ = session_id;
  *status =
      adapter_->RegisterSession(session_id_, weak_ptr_factory_.GetWeakPtr())
          ? SessionInitStatus::NEW_SESSION
          : SessionInitStatus::SESSION_ALREADY_EXISTS;
}

}  // namespace media
