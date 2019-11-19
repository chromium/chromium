/*
 * Copyright (C) 2009, 2012 Ericsson AB. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/eventsource/event_source.h"

#include <memory>
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value_factory.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/eventsource/event_source_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

const uint64_t EventSource::kDefaultReconnectDelay = 3000;

inline EventSource::EventSource(ExecutionContext* context,
                                const KURL& url,
                                const EventSourceInit* event_source_init)
    : ContextLifecycleObserver(context),
      url_(url),
      current_url_(url),
      with_credentials_(event_source_init->withCredentials()),
      state_(kConnecting),
      connect_timer_(context->GetTaskRunner(TaskType::kRemoteEvent),
                     this,
                     &EventSource::ConnectTimerFired),
      reconnect_delay_(kDefaultReconnectDelay) {}

EventSource* EventSource::Create(ExecutionContext* context,
                                 const String& url,
                                 const EventSourceInit* event_source_init,
                                 ExceptionState& exception_state) {
  UseCounter::Count(context, IsA<Document>(context)
                                 ? WebFeature::kEventSourceDocument
                                 : WebFeature::kEventSourceWorker);

  if (url.IsEmpty()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Cannot open an EventSource to an empty URL.");
    return nullptr;
  }

  KURL full_url = context->CompleteURL(url);
  if (!full_url.IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Cannot open an EventSource to '" + url + "'. The URL is invalid.");
    return nullptr;
  }

  EventSource* source =
      MakeGarbageCollected<EventSource>(context, full_url, event_source_init);

  source->ScheduleInitialConnect();
  return source;
}

EventSource::~EventSource() {
  DCHECK_EQ(kClosed, state_);
  DCHECK(!loader_);
}

void EventSource::ScheduleInitialConnect() {
  DCHECK_EQ(kConnecting, state_);
  DCHECK(!loader_);

  connect_timer_.StartOneShot(base::TimeDelta(), FROM_HERE);
}

void EventSource::Connect() {
  DCHECK_EQ(kConnecting, state_);
  DCHECK(!loader_);
  DCHECK(GetExecutionContext());

  ExecutionContext& execution_context = *this->GetExecutionContext();
  ResourceRequest request(current_url_);
  request.SetHttpMethod(http_names::kGET);
  request.SetHttpHeaderField(http_names::kAccept, "text/event-stream");
  request.SetHttpHeaderField(http_names::kCacheControl, "no-cache");
  request.SetRequestContext(mojom::RequestContextType::EVENT_SOURCE);
  request.SetMode(network::mojom::RequestMode::kCors);
  request.SetCredentialsMode(
      with_credentials_ ? network::mojom::CredentialsMode::kInclude
                        : network::mojom::CredentialsMode::kSameOrigin);
  request.SetCacheMode(blink::mojom::FetchCacheMode::kNoStore);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      execution_context.GetSecurityContext().AddressSpace());
  request.SetCorsPreflightPolicy(
      network::mojom::CorsPreflightPolicy::kPreventPreflight);
  if (parser_ && !parser_->LastEventId().IsEmpty()) {
    // HTTP headers are Latin-1 byte strings, but the Last-Event-ID header is
    // encoded as UTF-8.
    // TODO(davidben): This should be captured in the type of
    // setHTTPHeaderField's arguments.
    std::string last_event_id_utf8 = parser_->LastEventId().Utf8();
    request.SetHttpHeaderField(
        http_names::kLastEventID,
        AtomicString(reinterpret_cast<const LChar*>(last_event_id_utf8.c_str()),
                     last_event_id_utf8.length()));
  }

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  probe::WillSendEventSourceRequest(&execution_context);
  loader_ = MakeGarbageCollected<ThreadableLoader>(execution_context, this,
                                                   resource_loader_options);
  loader_->Start(request);
}

void EventSource::NetworkRequestEnded() {
  loader_ = nullptr;

  if (state_ != kClosed)
    ScheduleReconnect();
}

void EventSource::ScheduleReconnect() {
  state_ = kConnecting;
  connect_timer_.StartOneShot(
      base::TimeDelta::FromMilliseconds(reconnect_delay_), FROM_HERE);
  DispatchEvent(*Event::Create(event_type_names::kError));
}

void EventSource::ConnectTimerFired(TimerBase*) {
  Connect();
}

String EventSource::url() const {
  return url_.GetString();
}

bool EventSource::withCredentials() const {
  return with_credentials_;
}

EventSource::State EventSource::readyState() const {
  return state_;
}

void EventSource::close() {
  if (state_ == kClosed) {
    DCHECK(!loader_);
    return;
  }
  if (parser_)
    parser_->Stop();

  // Stop trying to reconnect if EventSource was explicitly closed or if
  // contextDestroyed() was called.
  if (connect_timer_.IsActive()) {
    connect_timer_.Stop();
  }

  state_ = kClosed;

  if (loader_) {
    loader_->Cancel();
    loader_ = nullptr;
  }

}

const AtomicString& EventSource::InterfaceName() const {
  return event_target_names::kEventSource;
}

ExecutionContext* EventSource::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

void EventSource::DidReceiveResponse(uint64_t identifier,
                                     const ResourceResponse& response) {
  DCHECK_EQ(kConnecting, state_);
  DCHECK(loader_);

  resource_identifier_ = identifier;
  current_url_ = response.CurrentRequestUrl();
  event_stream_origin_ =
      SecurityOrigin::Create(response.CurrentRequestUrl())->ToString();
  int status_code = response.HttpStatusCode();
  bool mime_type_is_valid = response.MimeType() == "text/event-stream";
  bool response_is_valid = status_code == 200 && mime_type_is_valid;
  if (response_is_valid) {
    const String& charset = response.TextEncodingName();
    // If we have a charset, the only allowed value is UTF-8 (case-insensitive).
    response_is_valid =
        charset.IsEmpty() || DeprecatedEqualIgnoringCase(charset, "UTF-8");
    if (!response_is_valid) {
      StringBuilder message;
      message.Append("EventSource's response has a charset (\"");
      message.Append(charset);
      message.Append("\") that is not UTF-8. Aborting the connection.");
      // FIXME: We are missing the source line.
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kError, message.ToString()));
    }
  } else {
    // To keep the signal-to-noise ratio low, we only log 200-response with an
    // invalid MIME type.
    if (status_code == 200 && !mime_type_is_valid) {
      StringBuilder message;
      message.Append("EventSource's response has a MIME type (\"");
      message.Append(response.MimeType());
      message.Append(
          "\") that is not \"text/event-stream\". Aborting the connection.");
      // FIXME: We are missing the source line.
      GetExecutionContext()->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kJavaScript,
          mojom::ConsoleMessageLevel::kError, message.ToString()));
    }
  }

  if (response_is_valid) {
    state_ = kOpen;
    AtomicString last_event_id;
    if (parser_) {
      // The new parser takes over the event ID.
      last_event_id = parser_->LastEventId();
    }
    parser_ = MakeGarbageCollected<EventSourceParser>(last_event_id, this);
    DispatchEvent(*Event::Create(event_type_names::kOpen));
  } else {
    loader_->Cancel();
  }
}

void EventSource::DidReceiveData(const char* data, unsigned length) {
  DCHECK_EQ(kOpen, state_);
  DCHECK(loader_);
  DCHECK(parser_);

  parser_->AddBytes(data, length);
}

void EventSource::DidFinishLoading(uint64_t) {
  DCHECK_EQ(kOpen, state_);
  DCHECK(loader_);

  NetworkRequestEnded();
}

void EventSource::DidFail(const ResourceError& error) {
  DCHECK(loader_);
  if (error.IsCancellation() && state_ == kClosed) {
    NetworkRequestEnded();
    return;
  }

  DCHECK_NE(kClosed, state_);

  if (error.IsAccessCheck()) {
    AbortConnectionAttempt();
    return;
  }

  if (error.IsCancellation()) {
    // When the loading is cancelled for an external reason (e.g.,
    // window.stop()), dispatch an error event and do not reconnect.
    AbortConnectionAttempt();
    return;
  }
  NetworkRequestEnded();
}

void EventSource::DidFailRedirectCheck() {
  DCHECK(loader_);

  AbortConnectionAttempt();
}

void EventSource::OnMessageEvent(const AtomicString& event_type,
                                 const String& data,
                                 const AtomicString& last_event_id) {
  MessageEvent* e = MessageEvent::Create();
  e->initMessageEvent(event_type, false, false, data, event_stream_origin_,
                      last_event_id, nullptr, nullptr);

  probe::WillDispatchEventSourceEvent(GetExecutionContext(),
                                      resource_identifier_, event_type,
                                      last_event_id, data);
  DispatchEvent(*e);
}

void EventSource::OnReconnectionTimeSet(uint64_t reconnection_time) {
  reconnect_delay_ = reconnection_time;
}

void EventSource::AbortConnectionAttempt() {
  DCHECK_NE(kClosed, state_);

  loader_ = nullptr;
  state_ = kClosed;
  NetworkRequestEnded();

  DispatchEvent(*Event::Create(event_type_names::kError));
}

void EventSource::ContextDestroyed(ExecutionContext*) {
  close();
}

bool EventSource::HasPendingActivity() const {
  return state_ != kClosed;
}

void EventSource::Trace(blink::Visitor* visitor) {
  visitor->Trace(parser_);
  visitor->Trace(loader_);
  EventTargetWithInlineData::Trace(visitor);
  ThreadableLoaderClient::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
  EventSourceParser::Client::Trace(visitor);
}

}  // namespace blink
