/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/encryptedmedia/media_key_session.h"

#include <cmath>
#include <limits>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "encrypted_media_utils.h"
#include "media/base/content_decryption_module.h"
#include "media/base/eme_constants.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_decryption_module.h"
#include "third_party/blink/public/platform/web_content_decryption_module_exception.h"
#include "third_party/blink/public/platform/web_content_decryption_module_session.h"
#include "third_party/blink/public/platform/web_encrypted_media_key_information.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable_creation_key.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_key_message_event.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/content_decryption_module_result.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/network/mime/content_type.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"

#define MEDIA_KEY_SESSION_LOG_LEVEL 3

namespace {

// Minimum and maximum length for session ids.
enum { MinSessionIdLength = 1, MaxSessionIdLength = 512 };

}  // namespace

namespace blink {

// Checks that |sessionId| looks correct and returns whether all checks pass.
static bool IsValidSessionId(const String& session_id) {
  if ((session_id.length() < MinSessionIdLength) ||
      (session_id.length() > MaxSessionIdLength))
    return false;

  if (!session_id.ContainsOnlyASCIIOrEmpty())
    return false;

  // Check that |sanitized_session_id| only contains printable characters for
  // easier logging. Note that checking alphanumeric is too strict because there
  // are key systems using Base64 session IDs (which may include spaces). See
  // https://crbug.com/902828.
  for (unsigned i = 0; i < session_id.length(); ++i) {
    if (!IsASCIIPrintable(session_id[i]))
      return false;
  }

  return true;
}

static bool IsPersistentSessionType(WebEncryptedMediaSessionType session_type) {
  // This implements section 5.1.1 Is persistent session type? from
  // https://w3c.github.io/encrypted-media/#is-persistent-session-type
  switch (session_type) {
    case WebEncryptedMediaSessionType::kTemporary:
      return false;
    case WebEncryptedMediaSessionType::kPersistentLicense:
      return true;
    case blink::WebEncryptedMediaSessionType::kUnknown:
      break;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

V8MediaKeySessionClosedReason::Enum ConvertSessionClosedReason(
    media::CdmSessionClosedReason reason) {
  switch (reason) {
    case media::CdmSessionClosedReason::kInternalError:
      return V8MediaKeySessionClosedReason::Enum::kInternalError;
    case media::CdmSessionClosedReason::kClose:
      return V8MediaKeySessionClosedReason::Enum::kClosedByApplication;
    case media::CdmSessionClosedReason::kReleaseAcknowledged:
      return V8MediaKeySessionClosedReason::Enum::kReleaseAcknowledged;
    case media::CdmSessionClosedReason::kHardwareContextReset:
      return V8MediaKeySessionClosedReason::Enum::kHardwareContextReset;
    case media::CdmSessionClosedReason::kResourceEvicted:
      return V8MediaKeySessionClosedReason::Enum::kResourceEvicted;
  }
}

static ScriptPromise<IDLUndefined> CreateRejectedPromiseNotCallable(
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The session is not callable.");
  return EmptyPromise();
}

static void ThrowAlreadyClosed(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The session is already closed.");
}

static void ThrowAlreadyInitialized(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "The session is already initialized.");
}

// A class holding a pending action.
class MediaKeySession::PendingAction final
    : public GarbageCollected<MediaKeySession::PendingAction> {
 public:
  using Type = EmeApiType;

  Type GetType() const { return type_; }

  ContentDecryptionModuleResult* Result() const { return result_.Get(); }

  DOMArrayBuffer* Data() const {
    DCHECK(type_ == Type::kGenerateRequest || type_ == Type::kUpdate);
    return data_.Get();
  }

  media::EmeInitDataType InitDataType() const {
    DCHECK_EQ(Type::kGenerateRequest, type_);
    return init_data_type_;
  }

  const String& SessionId() const {
    DCHECK_EQ(Type::kLoad, type_);
    return string_data_;
  }

  static PendingAction* CreatePendingGenerateRequest(
      ContentDecryptionModuleResult* result,
      media::EmeInitDataType init_data_type,
      DOMArrayBuffer* init_data) {
    DCHECK(result);
    DCHECK(init_data);
    return MakeGarbageCollected<PendingAction>(
        Type::kGenerateRequest, result, init_data_type, init_data, String());
  }

  static PendingAction* CreatePendingLoadRequest(
      ContentDecryptionModuleResult* result,
      const String& session_id) {
    DCHECK(result);
    return MakeGarbageCollected<PendingAction>(Type::kLoad, result,
                                               media::EmeInitDataType::UNKNOWN,
                                               nullptr, session_id);
  }

  static PendingAction* CreatePendingUpdate(
      ContentDecryptionModuleResult* result,
      DOMArrayBuffer* data) {
    DCHECK(result);
    DCHECK(data);
    return MakeGarbageCollected<PendingAction>(
        Type::kUpdate, result, media::EmeInitDataType::UNKNOWN, data, String());
  }

  static PendingAction* CreatePendingClose(
      ContentDecryptionModuleResult* result) {
    DCHECK(result);
    return MakeGarbageCollected<PendingAction>(Type::kClose, result,
                                               media::EmeInitDataType::UNKNOWN,
                                               nullptr, String());
  }

  static PendingAction* CreatePendingRemove(
      ContentDecryptionModuleResult* result) {
    DCHECK(result);
    return MakeGarbageCollected<PendingAction>(Type::kRemove, result,
                                               media::EmeInitDataType::UNKNOWN,
                                               nullptr, String());
  }

  PendingAction(Type type,
                ContentDecryptionModuleResult* result,
                media::EmeInitDataType init_data_type,
                DOMArrayBuffer* data,
                const String& string_data)
      : type_(type),
        result_(result),
        init_data_type_(init_data_type),
        data_(data),
        string_data_(string_data) {}
  ~PendingAction() = default;

  void Trace(Visitor* visitor) const {
    visitor->Trace(result_);
    visitor->Trace(data_);
  }

 private:
  const Type type_;
  const Member<ContentDecryptionModuleResult> result_;
  const media::EmeInitDataType init_data_type_;
  const Member<DOMArrayBuffer> data_;
  const String string_data_;
};

// This class wraps the promise resolver used when initializing a new session
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithSession() will resolve the promise with void, while
// completeWithError() will reject the promise with an exception. complete()
// is not expected to be called, and will reject the promise.
class NewSessionResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  NewSessionResultPromise(ScriptPromiseResolver<IDLUndefined>* resolver,
                          const MediaKeysConfig& config,
                          MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(resolver,
                                             config,
                                             EmeApiType::kGenerateRequest),
        session_(session) {}

  ~NewSessionResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void CompleteWithSession(
      WebContentDecryptionModuleResult::SessionStatus status) override {
    DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
        << "NewSessionResultPromise::" << __func__;

    if (!IsValidToFulfillPromise())
      return;

    DCHECK_EQ(status, WebContentDecryptionModuleResult::kNewSession);
    session_->FinishGenerateRequest();
    Resolve<IDLUndefined>();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(session_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  Member<MediaKeySession> session_;
};

// This class wraps the promise resolver used when loading a session
// and is passed to Chromium to fullfill the promise. This implementation of
// completeWithSession() will resolve the promise with true/false, while
// completeWithError() will reject the promise with an exception. complete()
// is not expected to be called, and will reject the promise.
class LoadSessionResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  LoadSessionResultPromise(ScriptPromiseResolver<IDLBoolean>* resolver,
                           const MediaKeysConfig& config,
                           MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(resolver,
                                             config,
                                             EmeApiType::kLoad),
        session_(session) {}

  ~LoadSessionResultPromise() override = default;

  // ContentDecryptionModuleResult implementation.
  void CompleteWithSession(
      WebContentDecryptionModuleResult::SessionStatus status) override {
    DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
        << "LoadSessionResultPromise::" << __func__;

    if (!IsValidToFulfillPromise())
      return;

    if (status == WebContentDecryptionModuleResult::kSessionNotFound) {
      Resolve<IDLBoolean>(false);
      return;
    }

    DCHECK_EQ(status, WebContentDecryptionModuleResult::kNewSession);
    session_->FinishLoad();
    Resolve<IDLBoolean>(true);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(session_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  Member<MediaKeySession> session_;
};

// This class wraps the promise resolver used by close. The implementation of
// complete() will resolve the promise with void and call
// OnClosePromiseResolved() on |session_|. All other complete() methods are
// not expected to be called (and will reject the promise).
class CloseSessionResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  CloseSessionResultPromise(ScriptPromiseResolver<IDLUndefined>* resolver,
                            const MediaKeysConfig& config,
                            MediaKeySession* session)
      : ContentDecryptionModuleResultPromise(resolver,
                                             config,
                                             EmeApiType::kClose),
        session_(session) {}

  ~CloseSessionResultPromise() override = default;

  // ContentDecryptionModuleResultPromise implementation.
  void Complete() override {
    DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
        << "CloseSessionResultPromise::" << __func__;

    if (!IsValidToFulfillPromise())
      return;

    session_->OnClosePromiseResolved();
    Resolve<IDLUndefined>();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(session_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  // Keep track of the MediaKeySession that created this promise so that it
  // remains reachable as long as this promise is reachable.
  Member<MediaKeySession> session_;
};

// This class wraps the promise resolver used by update/remove. The
// implementation of complete() will resolve the promise with void. All other
// complete() methods are not expected to be called (and will reject the
// promise).
class SimpleResultPromise : public ContentDecryptionModuleResultPromise {
 public:
  SimpleResultPromise(ScriptPromiseResolver<IDLUndefined>* resolver,
                      const MediaKeysConfig& config,
                      MediaKeySession* session,
                      EmeApiType type)
      : ContentDecryptionModuleResultPromise(resolver, config, type),
        session_(session) {}

  ~SimpleResultPromise() override = default;

  // ContentDecryptionModuleResultPromise implementation.
  void Complete() override {
    DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << "SimpleResultPromise::" << __func__;

    if (!IsValidToFulfillPromise())
      return;

    Resolve<IDLUndefined>();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(session_);
    ContentDecryptionModuleResultPromise::Trace(visitor);
  }

 private:
  // Keep track of the MediaKeySession that created this promise so that it
  // remains reachable as long as this promise is reachable.
  Member<MediaKeySession> session_;
};

MediaKeySession::MediaKeySession(ScriptState* script_state,
                                 MediaKeys* media_keys,
                                 WebEncryptedMediaSessionType session_type,
                                 const MediaKeysConfig& config)
    : ActiveScriptWrappable<MediaKeySession>({}),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      async_event_queue_(
          MakeGarbageCollected<EventQueue>(GetExecutionContext(),
                                           TaskType::kMediaElementEvent)),
      media_keys_(media_keys),
      session_type_(session_type),
      config_(config),
      expiration_(std::numeric_limits<double>::quiet_NaN()),
      key_statuses_map_(MakeGarbageCollected<MediaKeyStatusMap>()),
      closed_promise_(MakeGarbageCollected<ClosedPromise>(
          ExecutionContext::From(script_state))),
      action_timer_(ExecutionContext::From(script_state)
                        ->GetTaskRunner(TaskType::kMiscPlatformAPI),
                    this,
                    &MediaKeySession::ActionTimerFired) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";
  InstanceCounters::IncrementCounter(InstanceCounters::kMediaKeySessionCounter);

  // Create the matching Chromium object. It will not be usable until
  // initializeNewSession() is called in response to the user calling
  // generateRequest().
  WebContentDecryptionModule* cdm = media_keys->ContentDecryptionModule();
  session_ = cdm->CreateSession(session_type);
  session_->SetClientInterface(this);

  // From https://w3c.github.io/encrypted-media/#createSession:
  // MediaKeys::createSession(), step 3.
  // 3.1 Let the sessionId attribute be the empty string.
  DCHECK(session_id_.empty());

  // 3.2 Let the expiration attribute be NaN.
  DCHECK(std::isnan(expiration_));

  // 3.3 Let the closed attribute be a new promise.

  // 3.4 Let the keyStatuses attribute be empty.
  DCHECK_EQ(0u, key_statuses_map_->size());

  // 3.5 Let the session type be sessionType.
  DCHECK(session_type_ != WebEncryptedMediaSessionType::kUnknown);

  // 3.6 Let uninitialized be true.
  DCHECK(is_uninitialized_);

  // 3.7 Let callable be false.
  DCHECK(!is_callable_);

  // 3.8 Let the use distinctive identifier value be this object's
  // use distinctive identifier.
  // FIXME: Implement this (http://crbug.com/448922).

  // 3.9 Let the cdm implementation value be this object's cdm implementation.
  // 3.10 Let the cdm instance value be this object's cdm instance.
}

MediaKeySession::~MediaKeySession() {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";
  InstanceCounters::DecrementCounter(InstanceCounters::kMediaKeySessionCounter);
}

void MediaKeySession::Dispose() {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // Drop references to objects from content/ that aren't managed by blink.
  session_.reset();
}

String MediaKeySession::sessionId() const {
  return session_id_;
}

ScriptPromise<V8MediaKeySessionClosedReason> MediaKeySession::closed(
    ScriptState* script_state) {
  return closed_promise_->Promise(script_state->World());
}

MediaKeyStatusMap* MediaKeySession::keyStatuses() {
  return key_statuses_map_.Get();
}

ScriptPromise<IDLUndefined> MediaKeySession::generateRequest(
    ScriptState* script_state,
    const String& init_data_type_string,
    const DOMArrayPiece& init_data,
    ExceptionState& exception_state) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ") " << init_data_type_string;

  // From https://w3c.github.io/encrypted-media/#generateRequest:
  // Generates a request based on the initData. When this method is invoked,
  // the user agent must run the following steps:

  // 1. If this object's closing or closed value is true, return a promise
  //    rejected with an InvalidStateError.
  if (is_closing_ || is_closed_) {
    ThrowAlreadyClosed(exception_state);
    return EmptyPromise();
  }

  // 2. If this object's uninitialized value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!is_uninitialized_) {
    ThrowAlreadyInitialized(exception_state);
    return EmptyPromise();
  }

  // 3. Let this object's uninitialized be false.
  is_uninitialized_ = false;

  // 4. If initDataType is the empty string, return a promise rejected
  //    with a newly created TypeError.
  if (init_data_type_string.empty()) {
    exception_state.ThrowTypeError("The initDataType parameter is empty.");
    return EmptyPromise();
  }

  // 5. If initData is an empty array, return a promise rejected with a
  //    newly created TypeError.
  if (!init_data.ByteLength()) {
    exception_state.ThrowTypeError("The initData parameter is empty.");
    return EmptyPromise();
  }

  // 6. If the Key System implementation represented by this object's cdm
  //    implementation value does not support initDataType as an
  //    Initialization Data Type, return a promise rejected with a new
  //    DOMException whose name is NotSupportedError. String comparison
  //    is case-sensitive.
  //    (blink side doesn't know what the CDM supports, so the proper check
  //     will be done on the Chromium side. However, we can verify that
  //     |initDataType| is one of the registered values.)
  media::EmeInitDataType init_data_type =
      EncryptedMediaUtils::ConvertToInitDataType(init_data_type_string);
  if (init_data_type == media::EmeInitDataType::UNKNOWN) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "The initialization data type '" +
                                          init_data_type_string +
                                          "' is not supported.");
    return EmptyPromise();
  }

  // 7. Let init data be a copy of the contents of the initData parameter.
  DOMArrayBuffer* init_data_buffer =
      DOMArrayBuffer::Create(init_data.ByteSpan());

  // 8. Let session type be this object's session type.
  //    (Done in constructor.)

  // 9. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  NewSessionResultPromise* result =
      MakeGarbageCollected<NewSessionResultPromise>(resolver, config_, this);

  // 10. Run the following steps asynchronously (done in generateRequestTask())
  pending_actions_.push_back(PendingAction::CreatePendingGenerateRequest(
      result, init_data_type, init_data_buffer));
  DCHECK(!action_timer_.IsActive());
  action_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 11. Return promise.
  return promise;
}

void MediaKeySession::GenerateRequestTask(ContentDecryptionModuleResult* result,
                                          media::EmeInitDataType init_data_type,
                                          DOMArrayBuffer* init_data_buffer) {
  // NOTE: Continue step 10 of MediaKeySession::generateRequest().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // initializeNewSession() in Chromium will execute steps 10.1 to 10.9.
  session_->InitializeNewSession(
      init_data_type, static_cast<unsigned char*>(init_data_buffer->Data()),
      init_data_buffer->ByteLength(), result->Result());

  // Remaining steps (10.10) executed in finishGenerateRequest(),
  // called when |result| is resolved.
}

void MediaKeySession::FinishGenerateRequest() {
  // NOTE: Continue step 10.10 of MediaKeySession::generateRequest().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 10.10.1 If any of the preceding steps failed, reject promise with a
  //         new DOMException whose name is the appropriate error name.
  //         (Done by CDM calling result.completeWithError() as appropriate.)
  // 10.10.2 Set the sessionId attribute to session id.
  session_id_ = session_->SessionId();
  DCHECK(!session_id_.empty());

  // 10.10.3 Let this object's callable be true.
  is_callable_ = true;

  // 10.10.4 Run the Queue a "message" Event algorithm on the session,
  //         providing message type and message.
  //         (Done by the CDM.)
  // 10.10.5 Resolve promise.
  //         (Done by NewSessionResultPromise.)
}

ScriptPromise<IDLBoolean> MediaKeySession::load(
    ScriptState* script_state,
    const String& session_id,
    ExceptionState& exception_state) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ") " << session_id;

  // From https://w3c.github.io/encrypted-media/#load:
  // Loads the data stored for the specified session into this object. When
  // this method is invoked, the user agent must run the following steps:

  // 1. If this object's closing or closed value is true, return a promise
  //    rejected with an InvalidStateError.
  if (is_closing_ || is_closed_) {
    ThrowAlreadyClosed(exception_state);
    return EmptyPromise();
  }

  // 2. If this object's uninitialized value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!is_uninitialized_) {
    ThrowAlreadyInitialized(exception_state);
    return EmptyPromise();
  }

  // 3. Let this object's uninitialized value be false.
  is_uninitialized_ = false;

  // 4. If sessionId is the empty string, return a promise rejected with
  //    a newly created TypeError.
  if (session_id.empty()) {
    exception_state.ThrowTypeError("The sessionId parameter is empty.");
    return EmptyPromise();
  }

  // 5. If the result of running the "Is persistent session type?" algorithm
  //    on this object's session type is false, return a promise rejected
  //    with a newly created TypeError.
  if (!IsPersistentSessionType(session_type_)) {
    exception_state.ThrowTypeError("The session type is not persistent.");
    return EmptyPromise();
  }

  // Log the usage of loadSession().
  EncryptedMediaUtils::ReportUsage(EmeApiType::kLoad, GetExecutionContext(),
                                   config_.key_system,
                                   config_.use_hardware_secure_codecs,
                                   /*is_persistent_session=*/true);

  // 6. Let origin be the origin of this object's Document.
  //    (Available as getExecutionContext()->getSecurityOrigin() anytime.)

  // 7. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  LoadSessionResultPromise* result =
      MakeGarbageCollected<LoadSessionResultPromise>(resolver, config_, this);

  // 8. Run the following steps asynchronously (done in loadTask())
  pending_actions_.push_back(
      PendingAction::CreatePendingLoadRequest(result, session_id));
  DCHECK(!action_timer_.IsActive());
  action_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 9. Return promise.
  return promise;
}

void MediaKeySession::LoadTask(ContentDecryptionModuleResult* result,
                               const String& session_id) {
  // NOTE: Continue step 8 of MediaKeySession::load().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 8.1 Let sanitized session ID be a validated and/or sanitized
  //     version of sessionId. The user agent should thoroughly
  //     validate the sessionId value before passing it to the CDM.
  //     At a minimum, this should include checking that the length
  //     and value (e.g. alphanumeric) are reasonable.
  // 8.2 If the preceding step failed, or if sanitized session ID
  //     is empty, reject promise with a newly created TypeError.
  if (!IsValidSessionId(session_id)) {
    result->CompleteWithError(kWebContentDecryptionModuleExceptionTypeError, 0,
                              "Invalid sessionId");
    return;
  }

  // 8.3 If there is an unclosed session in the object's Document
  //     whose sessionId attribute is sanitized session ID, reject
  //     promise with a new DOMException whose name is
  //     QuotaExceededError. In other words, do not create a session
  //     if a non-closed session, regardless of type, already exists
  //     for this sanitized session ID in this browsing context.
  //     (Done in the CDM.)

  // 8.4 Let expiration time be NaN.
  //     (Done in the constructor.)
  DCHECK(std::isnan(expiration_));

  // load() in Chromium will execute steps 8.5 through 8.8.
  session_->Load(session_id, result->Result());

  // Remaining step (8.9) executed in finishLoad(), called when |result|
  // is resolved.
}

void MediaKeySession::FinishLoad() {
  // NOTE: Continue step 8.9 of MediaKeySession::load().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // 8.9.1 If any of the preceding steps failed, reject promise with a new
  //       DOMException whose name is the appropriate error name.
  //       (Done by CDM calling result.completeWithError() as appropriate.)

  // 8.9.2 Set the sessionId attribute to sanitized session ID.
  session_id_ = session_->SessionId();
  DCHECK(!session_id_.empty());

  // 8.9.3 Let this object's callable be true.
  is_callable_ = true;

  // 8.9.4 If the loaded session contains information about any keys (there
  //       are known keys), run the update key statuses algorithm on the
  //       session, providing each key's key ID along with the appropriate
  //       MediaKeyStatus. Should additional processing be necessary to
  //       determine with certainty the status of a key, use the non-"usable"
  //       MediaKeyStatus value that corresponds to the reason for the
  //       additional processing. Once the additional processing for one or
  //       more keys has completed, run the update key statuses algorithm
  //       again if any of the statuses has changed.
  //       (Done by the CDM.)

  // 8.9.5 Run the Update Expiration algorithm on the session,
  //       providing expiration time.
  //       (Done by the CDM.)

  // 8.9.6 If message is not null, run the queue a "message" event algorithm
  //       on the session, providing message type and message.
  //       (Done by the CDM.)

  // 8.9.7 Resolve promise with true.
  //       (Done by LoadSessionResultPromise.)
}

ScriptPromise<IDLUndefined> MediaKeySession::update(
    ScriptState* script_state,
    const DOMArrayPiece& response,
    ExceptionState& exception_state) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#update:
  // Provides messages, including licenses, to the CDM. When this method is
  // invoked, the user agent must run the following steps:

  // 1. If this object's closing or closed value is true, return a promise
  //    rejected with an InvalidStateError.
  if (is_closing_ || is_closed_) {
    ThrowAlreadyClosed(exception_state);
    return EmptyPromise();
  }

  // 2. If this object's callable value is false, return a promise
  //    rejected with an InvalidStateError.
  if (!is_callable_)
    return CreateRejectedPromiseNotCallable(exception_state);

  // 3. If response is an empty array, return a promise rejected with a
  //    newly created TypeError.
  if (!response.ByteLength()) {
    exception_state.ThrowTypeError("The response parameter is empty.");
    return EmptyPromise();
  }

  // 4. Let response copy be a copy of the contents of the response parameter.
  DOMArrayBuffer* response_copy = DOMArrayBuffer::Create(response.ByteSpan());

  // Log the usage of update().
  EncryptedMediaUtils::ReportUsage(EmeApiType::kUpdate, GetExecutionContext(),
                                   config_.key_system,
                                   config_.use_hardware_secure_codecs,
                                   IsPersistentSessionType(session_type_));

  // 5. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  SimpleResultPromise* result = MakeGarbageCollected<SimpleResultPromise>(
      resolver, config_, this, EmeApiType::kUpdate);

  // 6. Run the following steps asynchronously (done in updateTask())
  pending_actions_.push_back(
      PendingAction::CreatePendingUpdate(result, response_copy));
  if (!action_timer_.IsActive())
    action_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 7. Return promise.
  return promise;
}

void MediaKeySession::UpdateTask(ContentDecryptionModuleResult* result,
                                 DOMArrayBuffer* sanitized_response) {
  // NOTE: Continue step 6 of MediaKeySession::update().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // update() in Chromium will execute steps 6.1 through 6.8.
  session_->Update(static_cast<unsigned char*>(sanitized_response->Data()),
                   sanitized_response->ByteLength(), result->Result());

  // Last step (6.8.2 Resolve promise) will be done when |result| is resolved.
}

ScriptPromise<IDLUndefined> MediaKeySession::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#close:
  // Indicates that the application no longer needs the session and the CDM
  // should release any resources associated with the session and close it.
  // Persisted data should not be released or cleared.
  // When this method is invoked, the user agent must run the following steps:

  // 1. If this object's closing or closed value is true, return a resolved
  //    promise.
  if (is_closing_ || is_closed_)
    return ToResolvedUndefinedPromise(script_state);

  // 2. If this object's callable value is false, return a promise rejected
  //    with an InvalidStateError.
  if (!is_callable_)
    return CreateRejectedPromiseNotCallable(exception_state);

  // 3. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  CloseSessionResultPromise* result =
      MakeGarbageCollected<CloseSessionResultPromise>(resolver, config_, this);

  // 4. Set this object's closing or closed value to true.
  is_closing_ = true;

  // 5. Run the following steps in parallel (done in closeTask()).
  pending_actions_.push_back(PendingAction::CreatePendingClose(result));
  if (!action_timer_.IsActive())
    action_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 6. Return promise.
  return promise;
}

void MediaKeySession::CloseTask(ContentDecryptionModuleResult* result) {
  // NOTE: Continue step 4 of MediaKeySession::close().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // close() in Chromium will execute steps 5.1 through 5.3.
  session_->Close(result->Result());

  // Last step (5.3.2 Resolve promise) will be done when |result| is resolved.
}

void MediaKeySession::OnClosePromiseResolved() {
  // Stop the CDM from firing any more events for this session now that it is
  // closed. This was deferred in OnSessionClosed() as the EME spec resolves
  // the promise after firing the event.
  Dispose();
}

ScriptPromise<IDLUndefined> MediaKeySession::remove(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From https://w3c.github.io/encrypted-media/#remove:
  // Removes stored session data associated with this object. When this
  // method is invoked, the user agent must run the following steps:

  // 1. If this object's closing or closed value is true, return a promise
  //    rejected with an InvalidStateError.
  if (is_closing_ || is_closed_) {
    ThrowAlreadyClosed(exception_state);
    return EmptyPromise();
  }

  // 2. If this object's callable value is false, return a promise rejected
  //    with an InvalidStateError.
  if (!is_callable_)
    return CreateRejectedPromiseNotCallable(exception_state);

  // 3. Let promise be a new promise.
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();
  SimpleResultPromise* result = MakeGarbageCollected<SimpleResultPromise>(
      resolver, config_, this, EmeApiType::kRemove);

  // 4. Run the following steps asynchronously (done in removeTask()).
  pending_actions_.push_back(PendingAction::CreatePendingRemove(result));
  if (!action_timer_.IsActive())
    action_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);

  // 5. Return promise.
  return promise;
}

void MediaKeySession::RemoveTask(ContentDecryptionModuleResult* result) {
  // NOTE: Continue step 4 of MediaKeySession::remove().
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // remove() in Chromium will execute steps 4.1 through 4.5.
  session_->Remove(result->Result());

  // Last step (4.5.6 Resolve promise) will be done when |result| is resolved.
}

void MediaKeySession::ActionTimerFired(TimerBase*) {
  DCHECK(pending_actions_.size());

  // Resolving promises now run synchronously and may result in additional
  // actions getting added to the queue. As a result, swap the queue to
  // a local copy to avoid problems if this happens.
  HeapDeque<Member<PendingAction>> pending_actions;
  pending_actions.Swap(pending_actions_);

  while (!pending_actions.empty()) {
    PendingAction* action = pending_actions.TakeFirst();
    switch (action->GetType()) {
      case PendingAction::Type::kGenerateRequest:
        GenerateRequestTask(action->Result(), action->InitDataType(),
                            action->Data());
        break;

      case PendingAction::Type::kLoad:
        LoadTask(action->Result(), action->SessionId());
        break;

      case PendingAction::Type::kUpdate:
        UpdateTask(action->Result(), action->Data());
        break;

      case PendingAction::Type::kClose:
        CloseTask(action->Result());
        break;

      case PendingAction::Type::kRemove:
        RemoveTask(action->Result());
        break;

      default:
        NOTREACHED_IN_MIGRATION();
    }
  }
}

// Queue a task to fire a simple event named keymessage at the new object.
void MediaKeySession::OnSessionMessage(media::CdmMessageType message_type,
                                       const unsigned char* message,
                                       size_t message_length) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // Verify that 'message' not fired before session initialization is complete.
  DCHECK(is_callable_);

  // From https://w3c.github.io/encrypted-media/#queue-message:
  // The following steps are run:
  // 1. Let the session be the specified MediaKeySession object.
  // 2. Queue a task to fire a simple event named message at the session.
  //    The event is of type MediaKeyMessageEvent and has:
  //    -> messageType = the specified message type
  //    -> message = the specified message

  MediaKeyMessageEventInit* init = MediaKeyMessageEventInit::Create();
  switch (message_type) {
    case media::CdmMessageType::LICENSE_REQUEST:
      init->setMessageType("license-request");
      break;
    case media::CdmMessageType::LICENSE_RENEWAL:
      init->setMessageType("license-renewal");
      break;
    case media::CdmMessageType::LICENSE_RELEASE:
      init->setMessageType("license-release");
      break;
    case media::CdmMessageType::INDIVIDUALIZATION_REQUEST:
      init->setMessageType("individualization-request");
      break;
  }
  init->setMessage(
      DOMArrayBuffer::Create(UNSAFE_TODO(base::span(message, message_length))));

  MediaKeyMessageEvent* event =
      MediaKeyMessageEvent::Create(event_type_names::kMessage, init);
  event->SetTarget(this);
  async_event_queue_->EnqueueEvent(FROM_HERE, *event);
}

void MediaKeySession::OnSessionClosed(media::CdmSessionClosedReason reason) {
  // Note that this is the event from the CDM when this session is actually
  // closed. The CDM can close a session at any time. Normally it would happen
  // as the result of a close() call, but also happens when update() has been
  // called with a record of license destruction or if the CDM crashes or
  // otherwise becomes unavailable.
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL) << __func__ << "(" << this << ")";

  // From http://w3c.github.io/encrypted-media/#session-closed
  // 1. Let session be the associated MediaKeySession object.
  // 2. Let promise be the session's closed attribute.
  // 3. If promise is resolved, abort these steps.
  if (closed_promise_->GetState() == ClosedPromise::kResolved)
    return;

  // 4. Set the session's closing or closed value to true.
  is_closed_ = true;

  // 5. Run the Update Key Statuses algorithm on the session, providing
  //    an empty sequence.
  OnSessionKeysChange(WebVector<WebEncryptedMediaKeyInformation>(), false);

  // 6. Run the Update Expiration algorithm on the session, providing NaN.
  OnSessionExpirationUpdate(std::numeric_limits<double>::quiet_NaN());

  // 7. Resolve promise.
  closed_promise_->Resolve(
      V8MediaKeySessionClosedReason(ConvertSessionClosedReason(reason))
          .AsString());

  // Fail any pending events, except if it's a close request.
  action_timer_.Stop();
  while (!pending_actions_.empty()) {
    PendingAction* action = pending_actions_.TakeFirst();
    if (action->GetType() == PendingAction::Type::kClose) {
      action->Result()->Complete();
    } else {
      action->Result()->CompleteWithError(
          kWebContentDecryptionModuleExceptionInvalidStateError, 0,
          "Session has been closed");
    }
  }

  // Stop the CDM from firing any more events for this session. If this
  // session is closed due to close() being called, the close() promise will
  // be resolved after this which will call Dispose().
  // https://w3c.github.io/encrypted-media/#session-closed
  if (!is_closing_) {
    // CDM closed the session for some other reason, so release the CDM
    // immediately.
    Dispose();
  }
}

void MediaKeySession::OnSessionExpirationUpdate(
    double updated_expiry_time_in_ms) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ") " << updated_expiry_time_in_ms;

  // From https://w3c.github.io/encrypted-media/#update-expiration:
  // The following steps are run:
  // 1. Let the session be the associated MediaKeySession object.
  // 2. Let expiration time be NaN.
  double expiration_time = std::numeric_limits<double>::quiet_NaN();

  // 3. If the new expiration time is not NaN, let expiration time be the
  //    new expiration time in milliseconds since 01 January 1970 UTC.
  if (!std::isnan(updated_expiry_time_in_ms))
    expiration_time = updated_expiry_time_in_ms;

  // 4. Set the session's expiration attribute to expiration time.
  expiration_ = expiration_time;
}

void MediaKeySession::OnSessionKeysChange(
    const WebVector<WebEncryptedMediaKeyInformation>& keys,
    bool has_additional_usable_key) {
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ") with " << keys.size()
      << " keys and hasAdditionalUsableKey is "
      << (has_additional_usable_key ? "true" : "false");

  // From https://w3c.github.io/encrypted-media/#update-key-statuses:
  // The following steps are run:
  // 1. Let the session be the associated MediaKeySession object.
  // 2. Let the input statuses be the sequence of pairs key ID and
  //    associated MediaKeyStatus pairs.
  // 3. Let the statuses be session's keyStatuses attribute.

  // 4. Run the following steps to replace the contents of statuses:
  // 4.1 Empty statuses.
  key_statuses_map_->Clear();

  // 4.2 For each pair in input statuses.
  for (size_t i = 0; i < keys.size(); ++i) {
    // 4.2.1 Let pair be the pair.
    const auto& key = keys[i];
    // 4.2.2 Insert an entry for pair's key ID into statuses with the
    //       value of pair's MediaKeyStatus value.
    key_statuses_map_->AddEntry(
        key.Id(), EncryptedMediaUtils::ConvertKeyStatusToString(key.Status()));
  }

  // 5. Queue a task to fire a simple event named keystatuseschange
  //    at the session.
  Event* event = Event::Create(event_type_names::kKeystatuseschange);
  event->SetTarget(this);
  async_event_queue_->EnqueueEvent(FROM_HERE, *event);

  // 6. Queue a task to run the attempt to resume playback if necessary
  //    algorithm on each of the media element(s) whose mediaKeys attribute
  //    is the MediaKeys object that created the session. The user agent
  //    may choose to skip this step if it knows resuming will fail.
  // FIXME: Attempt to resume playback if |hasAdditionalUsableKey| is true.
  // http://crbug.com/413413
}

const AtomicString& MediaKeySession::InterfaceName() const {
  return event_target_names::kMediaKeySession;
}

ExecutionContext* MediaKeySession::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

bool MediaKeySession::HasPendingActivity() const {
  // Remain around if there are pending events or MediaKeys is still around
  // and we're not closed.
  DVLOG(MEDIA_KEY_SESSION_LOG_LEVEL)
      << __func__ << "(" << this << ")"
      << (!pending_actions_.empty() ? " !pending_actions_.IsEmpty()" : "")
      << (async_event_queue_->HasPendingEvents()
              ? " async_event_queue_->HasPendingEvents()"
              : "")
      << ((media_keys_ && !is_closed_) ? " media_keys_ && !is_closed_" : "");

  return !pending_actions_.empty() || async_event_queue_->HasPendingEvents() ||
         (media_keys_ && !is_closed_);
}

void MediaKeySession::ContextDestroyed() {
  // Stop the CDM from firing any more events for this session.
  session_.reset();
  is_closed_ = true;
  action_timer_.Stop();
  pending_actions_.clear();
}

void MediaKeySession::Trace(Visitor* visitor) const {
  visitor->Trace(async_event_queue_);
  visitor->Trace(pending_actions_);
  visitor->Trace(media_keys_);
  visitor->Trace(key_statuses_map_);
  visitor->Trace(closed_promise_);
  visitor->Trace(action_timer_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
