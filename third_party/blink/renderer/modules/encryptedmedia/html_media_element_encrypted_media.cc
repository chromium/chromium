// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/html_media_element_encrypted_media.h"

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "media/base/eme_constants.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/encryptedmedia/content_decryption_module_result_promise.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_encrypted_event.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/content_decryption_module_result.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

#define EME_LOG_LEVEL 3

namespace blink {

// This class allows MediaKeys to be set asynchronously.
class SetMediaKeysHandler : public GarbageCollected<SetMediaKeysHandler> {
 public:
  static ScriptPromise<IDLUndefined> Create(ScriptState*,
                                            HTMLMediaElement&,
                                            MediaKeys*,
                                            const ExceptionContext&);

  SetMediaKeysHandler(ScriptState*,
                      HTMLMediaElement&,
                      MediaKeys*,
                      const ExceptionContext&);

  SetMediaKeysHandler(const SetMediaKeysHandler&) = delete;
  SetMediaKeysHandler& operator=(const SetMediaKeysHandler&) = delete;

  ~SetMediaKeysHandler();

  void Trace(Visitor*) const;

 private:
  void TimerFired(TimerBase*);

  void ClearExistingMediaKeys();
  void SetNewMediaKeys();

  void Finish();
  void Fail(WebContentDecryptionModuleException, const String& error_message);

  void ClearFailed(WebContentDecryptionModuleException,
                   const String& error_message);
  void SetFailed(WebContentDecryptionModuleException,
                 const String& error_message);

  Member<ScriptPromiseResolver<IDLUndefined>> resolver_;
  // Keep media element alive until promise is fulfilled
  Member<HTMLMediaElement> element_;
  Member<MediaKeys> new_media_keys_;
  bool made_reservation_;
  // Timer uses weak reference, so keep ourselves alive explicitly
  // while timer is pending.
  SelfKeepAlive<SetMediaKeysHandler> keep_alive_;
  HeapTaskRunnerTimer<SetMediaKeysHandler> timer_;
};

typedef base::OnceCallback<void()> SuccessCallback;
typedef base::OnceCallback<void(WebContentDecryptionModuleException,
                                const String&)>
    FailureCallback;

// Represents the result used when setContentDecryptionModule() is called.
// Calls |success| if result is resolved, |failure| if result is rejected.
class SetContentDecryptionModuleResult final
    : public ContentDecryptionModuleResult {
 public:
  SetContentDecryptionModuleResult(SuccessCallback success,
                                   FailureCallback failure)
      : success_callback_(std::move(success)),
        failure_callback_(std::move(failure)) {}

  // ContentDecryptionModuleResult implementation.
  void Complete() override {
    DVLOG(EME_LOG_LEVEL) << __func__ << ": promise resolved.";
    std::move(success_callback_).Run();
  }

  void CompleteWithContentDecryptionModule(
      std::unique_ptr<WebContentDecryptionModule>) override {
    NOTREACHED_IN_MIGRATION();
    std::move(failure_callback_)
        .Run(kWebContentDecryptionModuleExceptionInvalidStateError,
             "Unexpected completion.");
  }

  void CompleteWithSession(
      WebContentDecryptionModuleResult::SessionStatus status) override {
    NOTREACHED_IN_MIGRATION();
    std::move(failure_callback_)
        .Run(kWebContentDecryptionModuleExceptionInvalidStateError,
             "Unexpected completion.");
  }

  void CompleteWithKeyStatus(
      WebEncryptedMediaKeyInformation::KeyStatus key_status) override {
    NOTREACHED_IN_MIGRATION();
    std::move(failure_callback_)
        .Run(kWebContentDecryptionModuleExceptionInvalidStateError,
             "Unexpected completion.");
  }

  void CompleteWithError(WebContentDecryptionModuleException code,
                         uint32_t system_code,
                         const WebString& message) override {
    // Non-zero |systemCode| is appended to the |message|. If the |message|
    // is empty, we'll report "Rejected with system code (systemCode)".
    StringBuilder result;
    result.Append(message);
    if (system_code != 0) {
      if (result.empty())
        result.Append("Rejected with system code");
      result.Append(" (");
      result.AppendNumber(system_code);
      result.Append(')');
    }

    DVLOG(EME_LOG_LEVEL) << __func__ << ": promise rejected with code " << code
                         << " and message: " << result.ToString();

    std::move(failure_callback_).Run(code, result.ToString());
  }

 private:
  SuccessCallback success_callback_;
  FailureCallback failure_callback_;
};

ScriptPromise<IDLUndefined> SetMediaKeysHandler::Create(
    ScriptState* script_state,
    HTMLMediaElement& element,
    MediaKeys* media_keys,
    const ExceptionContext& exception_context) {
  SetMediaKeysHandler* handler = MakeGarbageCollected<SetMediaKeysHandler>(
      script_state, element, media_keys, exception_context);
  return handler->resolver_->Promise();
}

SetMediaKeysHandler::SetMediaKeysHandler(
    ScriptState* script_state,
    HTMLMediaElement& element,
    MediaKeys* media_keys,
    const ExceptionContext& exception_context)
    : resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state,
          exception_context)),
      element_(element),
      new_media_keys_(media_keys),
      made_reservation_(false),
      keep_alive_(this),
      timer_(ExecutionContext::From(script_state)
                 ->GetTaskRunner(TaskType::kMiscPlatformAPI),
             this,
             &SetMediaKeysHandler::TimerFired) {
  DVLOG(EME_LOG_LEVEL) << __func__;

  // 5. Run the following steps in parallel.
  timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

SetMediaKeysHandler::~SetMediaKeysHandler() = default;

void SetMediaKeysHandler::TimerFired(TimerBase*) {
  keep_alive_.Clear();
  ClearExistingMediaKeys();
}

void SetMediaKeysHandler::ClearExistingMediaKeys() {
  DVLOG(EME_LOG_LEVEL) << __func__;
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(*element_);

  // 5.1 If mediaKeys is not null, the CDM instance represented by
  //     mediaKeys is already in use by another media element, and
  //     the user agent is unable to use it with this element, let
  //     this object's attaching media keys value be false and
  //     reject promise with a QuotaExceededError.
  if (new_media_keys_) {
    if (!new_media_keys_->ReserveForMediaElement(element_.Get())) {
      this_element.is_attaching_media_keys_ = false;
      Fail(kWebContentDecryptionModuleExceptionQuotaExceededError,
           "The MediaKeys object is already in use by another media element.");
      return;
    }
    // Note that |m_newMediaKeys| is now considered reserved for
    // |m_element|, so it needs to be accepted or cancelled.
    made_reservation_ = true;
  }

  // 5.2 If the mediaKeys attribute is not null, run the following steps:
  if (this_element.media_keys_) {
    WebMediaPlayer* media_player = element_->GetWebMediaPlayer();
    if (media_player) {
      // 5.2.1 If the user agent or CDM do not support removing the
      //       association, let this object's attaching media keys
      //       value be false and reject promise with a NotSupportedError.
      // 5.2.2 If the association cannot currently be removed,
      //       let this object's attaching media keys value be false
      //       and reject promise with an InvalidStateError.
      // 5.2.3 Stop using the CDM instance represented by the mediaKeys
      //       attribute to decrypt media data and remove the association
      //       with the media element.
      // (All 3 steps handled as needed in Chromium.)
      SuccessCallback success_callback = WTF::BindOnce(
          &SetMediaKeysHandler::SetNewMediaKeys, WrapPersistent(this));
      FailureCallback failure_callback = WTF::BindOnce(
          &SetMediaKeysHandler::ClearFailed, WrapPersistent(this));
      ContentDecryptionModuleResult* result =
          MakeGarbageCollected<SetContentDecryptionModuleResult>(
              std::move(success_callback), std::move(failure_callback));
      media_player->SetContentDecryptionModule(nullptr, result->Result());

      // Don't do anything more until |result| is resolved (or rejected).
      return;
    }
  }

  // MediaKeys not currently set or no player connected, so continue on.
  SetNewMediaKeys();
}

void SetMediaKeysHandler::SetNewMediaKeys() {
  DVLOG(EME_LOG_LEVEL) << __func__;

  // 5.3 If mediaKeys is not null, run the following steps:
  if (new_media_keys_) {
    // 5.3.1 Associate the CDM instance represented by mediaKeys with the
    //       media element for decrypting media data.
    // 5.3.2 If the preceding step failed, run the following steps:
    //       (done in setFailed()).
    // 5.3.3 Queue a task to run the Attempt to Resume Playback If Necessary
    //       algorithm on the media element.
    //       (Handled in Chromium).
    if (element_->GetWebMediaPlayer()) {
      SuccessCallback success_callback =
          WTF::BindOnce(&SetMediaKeysHandler::Finish, WrapPersistent(this));
      FailureCallback failure_callback =
          WTF::BindOnce(&SetMediaKeysHandler::SetFailed, WrapPersistent(this));
      ContentDecryptionModuleResult* result =
          MakeGarbageCollected<SetContentDecryptionModuleResult>(
              std::move(success_callback), std::move(failure_callback));
      element_->GetWebMediaPlayer()->SetContentDecryptionModule(
          new_media_keys_->ContentDecryptionModule(), result->Result());

      // Don't do anything more until |result| is resolved (or rejected).
      return;
    }
  }

  // MediaKeys doesn't need to be set on the player, so continue on.
  Finish();
}

void SetMediaKeysHandler::Finish() {
  DVLOG(EME_LOG_LEVEL) << __func__;
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(*element_);

  // 5.4 Set the mediaKeys attribute to mediaKeys.
  if (this_element.media_keys_)
    this_element.media_keys_->ClearMediaElement();
  this_element.media_keys_ = new_media_keys_;
  if (made_reservation_)
    new_media_keys_->AcceptReservation();

  // 5.5 Let this object's attaching media keys value be false.
  this_element.is_attaching_media_keys_ = false;

  // 5.6 Resolve promise with undefined.
  resolver_->Resolve();
}

void SetMediaKeysHandler::Fail(WebContentDecryptionModuleException code,
                               const String& error_message) {
  // Reset ownership of |m_newMediaKeys|.
  if (made_reservation_)
    new_media_keys_->CancelReservation();

  // Make sure attaching media keys value is false.
  DCHECK(!HTMLMediaElementEncryptedMedia::From(*element_)
              .is_attaching_media_keys_);

  // Reject promise with an appropriate error.
  WebCdmExceptionToPromiseRejection(resolver_, code, error_message);
}

void SetMediaKeysHandler::ClearFailed(WebContentDecryptionModuleException code,
                                      const String& error_message) {
  DVLOG(EME_LOG_LEVEL) << __func__ << "(" << code << ", " << error_message
                       << ")";
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(*element_);

  // 5.2.4 If the preceding step failed, let this object's attaching media
  //      keys value be false and reject promise with an appropriate
  //      error name.
  this_element.is_attaching_media_keys_ = false;
  Fail(code, error_message);
}

void SetMediaKeysHandler::SetFailed(WebContentDecryptionModuleException code,
                                    const String& error_message) {
  DVLOG(EME_LOG_LEVEL) << __func__ << "(" << code << ", " << error_message
                       << ")";
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(*element_);

  // 5.3.2 If the preceding step failed (in setContentDecryptionModule()
  //       called from setNewMediaKeys()), run the following steps:
  // 5.3.2.1 Set the mediaKeys attribute to null.
  this_element.media_keys_.Clear();

  // 5.3.2.2 Let this object's attaching media keys value be false.
  this_element.is_attaching_media_keys_ = false;

  // 5.3.2.3 Reject promise with a new DOMException whose name is the
  //         appropriate error name.
  Fail(code, error_message);
}

void SetMediaKeysHandler::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(element_);
  visitor->Trace(new_media_keys_);
  visitor->Trace(timer_);
}

// static
const char HTMLMediaElementEncryptedMedia::kSupplementName[] =
    "HTMLMediaElementEncryptedMedia";

HTMLMediaElementEncryptedMedia::HTMLMediaElementEncryptedMedia(
    HTMLMediaElement& element)
    : Supplement(element),
      is_waiting_for_key_(false),
      is_attaching_media_keys_(false) {}

HTMLMediaElementEncryptedMedia::~HTMLMediaElementEncryptedMedia() {
  DVLOG(EME_LOG_LEVEL) << __func__;
}

HTMLMediaElementEncryptedMedia& HTMLMediaElementEncryptedMedia::From(
    HTMLMediaElement& element) {
  HTMLMediaElementEncryptedMedia* supplement =
      Supplement<HTMLMediaElement>::From<HTMLMediaElementEncryptedMedia>(
          element);
  if (!supplement) {
    supplement = MakeGarbageCollected<HTMLMediaElementEncryptedMedia>(element);
    ProvideTo(element, supplement);
  }
  return *supplement;
}

MediaKeys* HTMLMediaElementEncryptedMedia::mediaKeys(
    HTMLMediaElement& element) {
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(element);
  return this_element.media_keys_.Get();
}

ScriptPromise<IDLUndefined> HTMLMediaElementEncryptedMedia::setMediaKeys(
    ScriptState* script_state,
    HTMLMediaElement& element,
    MediaKeys* media_keys,
    ExceptionState& exception_state) {
  HTMLMediaElementEncryptedMedia& this_element =
      HTMLMediaElementEncryptedMedia::From(element);
  DVLOG(EME_LOG_LEVEL) << __func__ << ": current("
                       << this_element.media_keys_.Get() << "), new("
                       << media_keys << ")";

  // From http://w3c.github.io/encrypted-media/#setMediaKeys

  // 1. If this object's attaching media keys value is true, return a
  //    promise rejected with an InvalidStateError.
  if (this_element.is_attaching_media_keys_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Another request is in progress.");
    return EmptyPromise();
  }

  // 2. If mediaKeys and the mediaKeys attribute are the same object,
  //    return a resolved promise.
  if (this_element.media_keys_ == media_keys)
    return ToResolvedUndefinedPromise(script_state);

  // 3. Let this object's attaching media keys value be true.
  this_element.is_attaching_media_keys_ = true;

  // 4. Let promise be a new promise. Remaining steps done in handler.
  return SetMediaKeysHandler::Create(script_state, element, media_keys,
                                     exception_state.GetContext());
}

// Create a MediaEncryptedEvent for WD EME.
static Event* CreateEncryptedEvent(media::EmeInitDataType init_data_type,
                                   const unsigned char* init_data,
                                   unsigned init_data_length) {
  MediaEncryptedEventInit* initializer = MediaEncryptedEventInit::Create();
  initializer->setInitDataType(
      EncryptedMediaUtils::ConvertFromInitDataType(init_data_type));
  initializer->setInitData(DOMArrayBuffer::Create(
      UNSAFE_TODO(base::span(init_data, init_data_length))));
  initializer->setBubbles(false);
  initializer->setCancelable(false);

  return MakeGarbageCollected<MediaEncryptedEvent>(event_type_names::kEncrypted,
                                                   initializer);
}

void HTMLMediaElementEncryptedMedia::Encrypted(
    media::EmeInitDataType init_data_type,
    const unsigned char* init_data,
    unsigned init_data_length) {
  DVLOG(EME_LOG_LEVEL) << __func__;

  Event* event;
  if (GetSupplementable()->IsMediaDataCorsSameOrigin()) {
    event = CreateEncryptedEvent(init_data_type, init_data, init_data_length);
  } else {
    // Current page is not allowed to see content from the media file,
    // so don't return the initData. However, they still get an event.
    event = CreateEncryptedEvent(media::EmeInitDataType::UNKNOWN, nullptr, 0);
    GetSupplementable()->GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kWarning,
            "Media element must be CORS-same-origin with "
            "the embedding page. If cross-origin, you "
            "should use the `crossorigin` attribute and "
            "make sure CORS headers on the media data "
            "response are CORS-same-origin."));
  }

  event->SetTarget(GetSupplementable());
  GetSupplementable()->ScheduleEvent(event);
}

void HTMLMediaElementEncryptedMedia::DidBlockPlaybackWaitingForKey() {
  DVLOG(EME_LOG_LEVEL) << __func__;

  // From https://w3c.github.io/encrypted-media/#queue-waitingforkey:
  // It should only be called when the HTMLMediaElement object is potentially
  // playing and its readyState is equal to HAVE_FUTURE_DATA or greater.
  // FIXME: Is this really required?

  // 1. Let the media element be the specified HTMLMediaElement object.
  // 2. If the media element's waiting for key value is false, queue a task
  //    to fire a simple event named waitingforkey at the media element.
  if (!is_waiting_for_key_) {
    Event* event = Event::Create(event_type_names::kWaitingforkey);
    event->SetTarget(GetSupplementable());
    GetSupplementable()->ScheduleEvent(event);
  }

  // 3. Set the media element's waiting for key value to true.
  is_waiting_for_key_ = true;

  // 4. Suspend playback.
  //    (Already done on the Chromium side by the decryptors.)
}

void HTMLMediaElementEncryptedMedia::DidResumePlaybackBlockedForKey() {
  DVLOG(EME_LOG_LEVEL) << __func__;

  // Logic is on the Chromium side to attempt to resume playback when a new
  // key is available. However, |m_isWaitingForKey| needs to be cleared so
  // that a later waitingForKey() call can generate the event.
  is_waiting_for_key_ = false;
}

WebContentDecryptionModule*
HTMLMediaElementEncryptedMedia::ContentDecryptionModule() {
  return media_keys_ ? media_keys_->ContentDecryptionModule() : nullptr;
}

void HTMLMediaElementEncryptedMedia::Trace(Visitor* visitor) const {
  visitor->Trace(media_keys_);
  Supplement<HTMLMediaElement>::Trace(visitor);
}

}  // namespace blink
