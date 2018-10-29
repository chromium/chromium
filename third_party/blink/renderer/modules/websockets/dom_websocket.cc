/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "third_party/blink/renderer/modules/websockets/dom_websocket.h"

#include "base/feature_list.h"
#include "base/location.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_insecure_request_policy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_string_sequence.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/websockets/close_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/mixed_content_autoupgrade_status.h"
#include "third_party/blink/renderer/platform/network/network_log.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/cstring.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

static const size_t kMaxByteSizeForHistogram = 100 * 1000 * 1000;
static const int32_t kBucketCountForMessageSizeHistogram = 50;
static const char kWebSocketSubprotocolSeparator[] = ", ";

namespace {
void LogMixedAutoupgradeStatus(blink::MixedContentAutoupgradeStatus status) {
  // For websockets we use the response received element to log successful
  // connections.
  UMA_HISTOGRAM_ENUMERATION("MixedAutoupgrade.Websocket.Status", status);
}
}  // namespace

namespace blink {

DOMWebSocket::EventQueue::EventQueue(EventTarget* target)
    : state_(kActive), target_(target) {}

DOMWebSocket::EventQueue::~EventQueue() {
  ContextDestroyed();
}

void DOMWebSocket::EventQueue::Dispatch(Event* event) {
  switch (state_) {
    case kActive:
      DCHECK(events_.IsEmpty());
      DCHECK(target_->GetExecutionContext());
      target_->DispatchEvent(*event);
      break;
    case kPaused:
    case kUnpausePosted:
      events_.push_back(event);
      break;
    case kStopped:
      DCHECK(events_.IsEmpty());
      // Do nothing.
      break;
  }
}

bool DOMWebSocket::EventQueue::IsEmpty() const {
  return events_.IsEmpty();
}

void DOMWebSocket::EventQueue::Pause() {
  if (state_ == kStopped || state_ == kPaused)
    return;

  state_ = kPaused;
}

void DOMWebSocket::EventQueue::Unpause() {
  if (state_ != kPaused || state_ == kUnpausePosted)
    return;

  state_ = kUnpausePosted;
  target_->GetExecutionContext()
      ->GetTaskRunner(TaskType::kWebSocket)
      ->PostTask(FROM_HERE,
                 WTF::Bind(&EventQueue::UnpauseTask, WrapWeakPersistent(this)));
}

void DOMWebSocket::EventQueue::ContextDestroyed() {
  if (state_ == kStopped)
    return;

  state_ = kStopped;
  events_.clear();
}

bool DOMWebSocket::EventQueue::IsPaused() {
  return state_ == kPaused || state_ == kUnpausePosted;
}

void DOMWebSocket::EventQueue::DispatchQueuedEvents() {
  if (state_ != kActive)
    return;

  HeapDeque<Member<Event>> events;
  events.Swap(events_);
  while (!events.IsEmpty()) {
    if (state_ == kStopped || state_ == kPaused || state_ == kUnpausePosted)
      break;
    DCHECK_EQ(state_, kActive);
    DCHECK(target_->GetExecutionContext());
    target_->DispatchEvent(*events.TakeFirst());
    // |this| can be stopped here.
  }
  if (state_ == kPaused || state_ == kUnpausePosted) {
    while (!events_.IsEmpty())
      events.push_back(events_.TakeFirst());
    events.Swap(events_);
  }
}

void DOMWebSocket::EventQueue::UnpauseTask() {
  if (state_ != kUnpausePosted)
    return;
  state_ = kActive;
  DispatchQueuedEvents();
}

void DOMWebSocket::EventQueue::Trace(blink::Visitor* visitor) {
  visitor->Trace(target_);
  visitor->Trace(events_);
}

const size_t kMaxReasonSizeInBytes = 123;

static inline bool IsValidSubprotocolCharacter(UChar character) {
  const UChar kMinimumProtocolCharacter = '!';  // U+0021.
  const UChar kMaximumProtocolCharacter = '~';  // U+007E.
  // Set to true if character does not matches "separators" ABNF defined in
  // RFC2616. SP and HT are excluded since the range check excludes them.
  bool is_not_separator =
      character != '"' && character != '(' && character != ')' &&
      character != ',' && character != '/' &&
      !(character >= ':' &&
        character <=
            '@')  // U+003A - U+0040 (':', ';', '<', '=', '>', '?', '@').
      && !(character >= '[' &&
           character <= ']')  // U+005B - U+005D ('[', '\\', ']').
      && character != '{' && character != '}';
  return character >= kMinimumProtocolCharacter &&
         character <= kMaximumProtocolCharacter && is_not_separator;
}

bool DOMWebSocket::IsValidSubprotocolString(const String& protocol) {
  if (protocol.IsEmpty())
    return false;
  for (wtf_size_t i = 0; i < protocol.length(); ++i) {
    if (!IsValidSubprotocolCharacter(protocol[i]))
      return false;
  }
  return true;
}

static String EncodeSubprotocolString(const String& protocol) {
  StringBuilder builder;
  for (wtf_size_t i = 0; i < protocol.length(); i++) {
    if (protocol[i] < 0x20 || protocol[i] > 0x7E)
      builder.Append(String::Format("\\u%04X", protocol[i]));
    else if (protocol[i] == 0x5c)
      builder.Append("\\\\");
    else
      builder.Append(protocol[i]);
  }
  return builder.ToString();
}

static String JoinStrings(const Vector<String>& strings,
                          const char* separator) {
  StringBuilder builder;
  for (wtf_size_t i = 0; i < strings.size(); ++i) {
    if (i)
      builder.Append(separator);
    builder.Append(strings[i]);
  }
  return builder.ToString();
}

static void SetInvalidStateErrorForSendMethod(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Still in CONNECTING state.");
}

DOMWebSocket::DOMWebSocket(ExecutionContext* context)
    : PausableObject(context),
      state_(kConnecting),
      buffered_amount_(0),
      consumed_buffered_amount_(0),
      buffered_amount_after_close_(0),
      binary_type_(kBinaryTypeBlob),
      subprotocol_(""),
      extensions_(""),
      event_queue_(EventQueue::Create(this)),
      buffered_amount_update_task_pending_(false),
      was_autoupgraded_to_wss_(false) {}

DOMWebSocket::~DOMWebSocket() {
  DCHECK(!channel_);
}

void DOMWebSocket::LogError(const String& message) {
  if (GetExecutionContext()) {
    GetExecutionContext()->AddConsoleMessage(
        ConsoleMessage::Create(kJSMessageSource, kErrorMessageLevel, message));
  }
}

DOMWebSocket* DOMWebSocket::Create(ExecutionContext* context,
                                   const String& url,
                                   ExceptionState& exception_state) {
  StringOrStringSequence protocols;
  return Create(context, url, protocols, exception_state);
}

DOMWebSocket* DOMWebSocket::Create(ExecutionContext* context,
                                   const String& url,
                                   const StringOrStringSequence& protocols,
                                   ExceptionState& exception_state) {
  if (url.IsNull()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Failed to create a WebSocket: the provided URL is invalid.");
    return nullptr;
  }

  DOMWebSocket* websocket = new DOMWebSocket(context);
  websocket->PauseIfNeeded();

  if (protocols.IsNull()) {
    Vector<String> protocols_vector;
    websocket->Connect(url, protocols_vector, exception_state);
  } else if (protocols.IsString()) {
    Vector<String> protocols_vector;
    protocols_vector.push_back(protocols.GetAsString());
    websocket->Connect(url, protocols_vector, exception_state);
  } else {
    DCHECK(protocols.IsStringSequence());
    websocket->Connect(url, protocols.GetAsStringSequence(), exception_state);
  }

  if (exception_state.HadException())
    return nullptr;

  return websocket;
}

void DOMWebSocket::Connect(const String& url,
                           const Vector<String>& protocols,
                           ExceptionState& exception_state) {
  UseCounter::Count(GetExecutionContext(), WebFeature::kWebSocket);

  NETWORK_DVLOG(1) << "WebSocket " << this << " connect() url=" << url;
  url_ = KURL(NullURL(), url);

  bool upgrade_insecure_requests_set =
      GetExecutionContext()->GetSecurityContext().GetInsecureRequestPolicy() &
      kUpgradeInsecureRequests;

  if ((upgrade_insecure_requests_set ||
       MixedContentChecker::ShouldAutoupgrade(
           GetExecutionContext()->Url(),
           WebMixedContentContextType::kBlockable)) &&
      url_.Protocol() == "ws" &&
      !SecurityOrigin::Create(url_)->IsPotentiallyTrustworthy()) {
    if (!upgrade_insecure_requests_set) {
      was_autoupgraded_to_wss_ = true;
      LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kStarted);
    }
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kUpgradeInsecureRequestsUpgradedRequest);
    url_.SetProtocol("wss");
    if (url_.Port() == 80)
      url_.SetPort(443);
  }

  if (!url_.IsValid()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                      "The URL '" + url + "' is invalid.");
    return;
  }
  if (!url_.ProtocolIs("ws") && !url_.ProtocolIs("wss")) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL's scheme must be either 'ws' or 'wss'. '" + url_.Protocol() +
            "' is not allowed.");
    return;
  }

  if (url_.HasFragmentIdentifier()) {
    state_ = kClosed;
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "The URL contains a fragment identifier ('" +
            url_.FragmentIdentifier() +
            "'). Fragment identifiers are not allowed in WebSocket URLs.");
    return;
  }

  if (!IsPortAllowedForScheme(url_)) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "The port " + String::Number(url_.Port()) + " is not allowed.");
    return;
  }

  if (!ContentSecurityPolicy::ShouldBypassMainWorld(GetExecutionContext()) &&
      !GetExecutionContext()->GetContentSecurityPolicy()->AllowConnectToSource(
          url_)) {
    state_ = kClosed;

    // Delay the event dispatch until after the current task by suspending and
    // resuming the queue. If we don't do this, the event is fired synchronously
    // with the constructor, meaning that it's impossible to listen for.
    event_queue_->Pause();
    event_queue_->Dispatch(Event::Create(EventTypeNames::error));
    event_queue_->Unpause();
    return;
  }

  // Fail if not all elements in |protocols| are valid.
  for (const String& protocol : protocols) {
    if (!IsValidSubprotocolString(protocol)) {
      state_ = kClosed;
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "The subprotocol '" +
                                            EncodeSubprotocolString(protocol) +
                                            "' is invalid.");
      return;
    }
  }

  // Fail if there're duplicated elements in |protocols|.
  HashSet<String> visited;
  for (const String& protocol : protocols) {
    if (!visited.insert(protocol).is_new_entry) {
      state_ = kClosed;
      exception_state.ThrowDOMException(DOMExceptionCode::kSyntaxError,
                                        "The subprotocol '" +
                                            EncodeSubprotocolString(protocol) +
                                            "' is duplicated.");
      return;
    }
  }

  String protocol_string;
  if (!protocols.IsEmpty())
    protocol_string = JoinStrings(protocols, kWebSocketSubprotocolSeparator);

  origin_string_ = SecurityOrigin::Create(url_)->ToString();
  channel_ = CreateChannel(GetExecutionContext(), this);

  if (!channel_->Connect(url_, protocol_string)) {
    state_ = kClosed;
    exception_state.ThrowSecurityError(
        "An insecure WebSocket connection may not be initiated from a page "
        "loaded over HTTPS.");
    ReleaseChannel();
    return;
  }
}

void DOMWebSocket::UpdateBufferedAmountAfterClose(uint64_t payload_size) {
  buffered_amount_after_close_ += payload_size;

  LogError("WebSocket is already in CLOSING or CLOSED state.");
}

void DOMWebSocket::PostBufferedAmountUpdateTask() {
  if (buffered_amount_update_task_pending_)
    return;
  buffered_amount_update_task_pending_ = true;
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kWebSocket)
      ->PostTask(FROM_HERE, WTF::Bind(&DOMWebSocket::BufferedAmountUpdateTask,
                                      WrapWeakPersistent(this)));
}

void DOMWebSocket::BufferedAmountUpdateTask() {
  buffered_amount_update_task_pending_ = false;
  ReflectBufferedAmountConsumption();
}

void DOMWebSocket::ReflectBufferedAmountConsumption() {
  if (event_queue_->IsPaused())
    return;
  DCHECK_GE(buffered_amount_, consumed_buffered_amount_);
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " reflectBufferedAmountConsumption() " << buffered_amount_
                   << " => " << (buffered_amount_ - consumed_buffered_amount_);

  buffered_amount_ -= consumed_buffered_amount_;
  consumed_buffered_amount_ = 0;
}

void DOMWebSocket::ReleaseChannel() {
  DCHECK(channel_);
  channel_->Disconnect();
  channel_ = nullptr;
}

void DOMWebSocket::send(const String& message,
                        ExceptionState& exception_state) {
  CString encoded_message = message.Utf8();

  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending String "
                   << message;
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  // No exception is raised if the connection was once established but has
  // subsequently been closed.
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(encoded_message.length());
    return;
  }

  RecordSendTypeHistogram(kWebSocketSendTypeString);

  DCHECK(channel_);
  buffered_amount_ += encoded_message.length();
  channel_->Send(encoded_message);
}

void DOMWebSocket::send(DOMArrayBuffer* binary_data,
                        ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBuffer "
                   << binary_data;
  DCHECK(binary_data);
  DCHECK(binary_data->Buffer());
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->ByteLength());
    return;
  }
  RecordSendTypeHistogram(kWebSocketSendTypeArrayBuffer);
  RecordSendMessageSizeHistogram(kWebSocketSendTypeArrayBuffer,
                                 binary_data->ByteLength());
  DCHECK(channel_);
  buffered_amount_ += binary_data->ByteLength();
  channel_->Send(*binary_data, 0, binary_data->ByteLength());
}

void DOMWebSocket::send(NotShared<DOMArrayBufferView> array_buffer_view,
                        ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBufferView "
                   << array_buffer_view.View();
  DCHECK(array_buffer_view);
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(array_buffer_view.View()->byteLength());
    return;
  }
  RecordSendTypeHistogram(kWebSocketSendTypeArrayBufferView);
  RecordSendMessageSizeHistogram(kWebSocketSendTypeArrayBufferView,
                                 array_buffer_view.View()->byteLength());
  DCHECK(channel_);
  buffered_amount_ += array_buffer_view.View()->byteLength();
  channel_->Send(*array_buffer_view.View()->buffer(),
                 array_buffer_view.View()->byteOffset(),
                 array_buffer_view.View()->byteLength());
}

void DOMWebSocket::send(Blob* binary_data, ExceptionState& exception_state) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " send() Sending Blob "
                   << binary_data->Uuid();
  DCHECK(binary_data);
  if (state_ == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (state_ == kClosing || state_ == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->size());
    return;
  }
  unsigned long long size = binary_data->size();
  RecordSendTypeHistogram(kWebSocketSendTypeBlob);
  RecordSendMessageSizeHistogram(
      kWebSocketSendTypeBlob,
      clampTo<size_t>(size, 0, kMaxByteSizeForHistogram));
  buffered_amount_ += size;
  DCHECK(channel_);

  // When the runtime type of |binary_data| is File,
  // binary_data->GetBlobDataHandle()->size() returns -1. However, in order to
  // maintain the value of |buffered_amount_| correctly, the WebSocket code
  // needs to fix the size of the File at this point. For this reason,
  // construct a new BlobDataHandle here with the size that this method
  // observed.
  channel_->Send(
      BlobDataHandle::Create(binary_data->Uuid(), binary_data->type(), size));
}

void DOMWebSocket::close(unsigned short code,
                         const String& reason,
                         ExceptionState& exception_state) {
  CloseInternal(code, reason, exception_state);
}

void DOMWebSocket::close(ExceptionState& exception_state) {
  CloseInternal(WebSocketChannel::kCloseEventCodeNotSpecified, String(),
                exception_state);
}

void DOMWebSocket::close(unsigned short code, ExceptionState& exception_state) {
  CloseInternal(code, String(), exception_state);
}

void DOMWebSocket::CloseInternal(int code,
                                 const String& reason,
                                 ExceptionState& exception_state) {
  String cleansed_reason = reason;
  if (code == WebSocketChannel::kCloseEventCodeNotSpecified) {
    NETWORK_DVLOG(1) << "WebSocket " << this
                     << " close() without code and reason";
  } else {
    NETWORK_DVLOG(1) << "WebSocket " << this << " close() code=" << code
                     << " reason=" << reason;
    if (!(code == WebSocketChannel::kCloseEventCodeNormalClosure ||
          (WebSocketChannel::kCloseEventCodeMinimumUserDefined <= code &&
           code <= WebSocketChannel::kCloseEventCodeMaximumUserDefined))) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kInvalidAccessError,
          "The code must be either 1000, or between 3000 and 4999. " +
              String::Number(code) + " is neither.");
      return;
    }
    // Bindings specify USVString, so unpaired surrogates are already replaced
    // with U+FFFD.
    CString utf8 = reason.Utf8();
    if (utf8.length() > kMaxReasonSizeInBytes) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kSyntaxError,
          "The message must not be greater than " +
              String::Number(kMaxReasonSizeInBytes) + " bytes.");
      return;
    }
    if (!reason.IsEmpty() && !reason.Is8Bit()) {
      DCHECK_GT(utf8.length(), 0u);
      // reason might contain unpaired surrogates. Reconstruct it from
      // utf8.
      cleansed_reason = String::FromUTF8(utf8.data(), utf8.length());
    }
  }

  if (state_ == kClosing || state_ == kClosed)
    return;
  if (state_ == kConnecting) {
    state_ = kClosing;
    channel_->Fail("WebSocket is closed before the connection is established.",
                   kWarningMessageLevel,
                   SourceLocation::Create(String(), 0, 0, nullptr));
    return;
  }
  state_ = kClosing;
  if (channel_)
    channel_->Close(code, cleansed_reason);
}

const KURL& DOMWebSocket::url() const {
  return url_;
}

DOMWebSocket::State DOMWebSocket::readyState() const {
  return state_;
}

uint64_t DOMWebSocket::bufferedAmount() const {
  // TODO(ricea): Check for overflow once machines with exabytes of RAM become
  // commonplace.
  return buffered_amount_after_close_ + buffered_amount_;
}

String DOMWebSocket::protocol() const {
  return subprotocol_;
}

String DOMWebSocket::extensions() const {
  return extensions_;
}

String DOMWebSocket::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void DOMWebSocket::setBinaryType(const String& binary_type) {
  if (binary_type == "blob") {
    binary_type_ = kBinaryTypeBlob;
    return;
  }
  if (binary_type == "arraybuffer") {
    binary_type_ = kBinaryTypeArrayBuffer;
    return;
  }
  NOTREACHED();
}

const AtomicString& DOMWebSocket::InterfaceName() const {
  return EventTargetNames::DOMWebSocket;
}

ExecutionContext* DOMWebSocket::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

void DOMWebSocket::ContextDestroyed(ExecutionContext*) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " contextDestroyed()";
  event_queue_->ContextDestroyed();
  if (channel_) {
    channel_->Close(WebSocketChannel::kCloseEventCodeGoingAway, String());
    ReleaseChannel();
  }
  if (state_ != kClosed)
    state_ = kClosed;
}

bool DOMWebSocket::HasPendingActivity() const {
  return channel_ || !event_queue_->IsEmpty();
}

void DOMWebSocket::Pause() {
  event_queue_->Pause();
}

void DOMWebSocket::Unpause() {
  event_queue_->Unpause();

  // If |consumed_buffered_amount_| was updated while the object was paused then
  // the changes to |buffered_amount_| will not yet have been applied. Post
  // another task to update it.
  PostBufferedAmountUpdateTask();
}

void DOMWebSocket::DidConnect(const String& subprotocol,
                              const String& extensions) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidConnect()";
  if (was_autoupgraded_to_wss_)
    LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kResponseReceived);
  if (state_ != kConnecting)
    return;
  state_ = kOpen;
  subprotocol_ = subprotocol;
  extensions_ = extensions;
  event_queue_->Dispatch(Event::Create(EventTypeNames::open));
}

void DOMWebSocket::DidReceiveTextMessage(const String& msg) {
  NETWORK_DVLOG(1) << "WebSocket " << this
                   << " DidReceiveTextMessage() Text message " << msg;
  ReflectBufferedAmountConsumption();
  DCHECK_NE(state_, kConnecting);
  if (state_ != kOpen)
    return;
  RecordReceiveTypeHistogram(kWebSocketReceiveTypeString);

  DCHECK(!origin_string_.IsNull());
  event_queue_->Dispatch(MessageEvent::Create(msg, origin_string_));
}

void DOMWebSocket::DidReceiveBinaryMessage(
    std::unique_ptr<Vector<char>> binary_data) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidReceiveBinaryMessage() "
                   << binary_data->size() << " byte binary message";
  ReflectBufferedAmountConsumption();
  DCHECK(!origin_string_.IsNull());

  DCHECK_NE(state_, kConnecting);
  if (state_ != kOpen)
    return;

  switch (binary_type_) {
    case kBinaryTypeBlob: {
      size_t size = binary_data->size();
      scoped_refptr<RawData> raw_data = RawData::Create();
      binary_data->swap(*raw_data->MutableData());
      std::unique_ptr<BlobData> blob_data = BlobData::Create();
      blob_data->AppendData(std::move(raw_data));
      Blob* blob =
          Blob::Create(BlobDataHandle::Create(std::move(blob_data), size));
      RecordReceiveTypeHistogram(kWebSocketReceiveTypeBlob);
      RecordReceiveMessageSizeHistogram(kWebSocketReceiveTypeBlob, size);
      event_queue_->Dispatch(MessageEvent::Create(blob, origin_string_));
      break;
    }

    case kBinaryTypeArrayBuffer:
      DOMArrayBuffer* array_buffer =
          DOMArrayBuffer::Create(binary_data->data(), binary_data->size());
      RecordReceiveTypeHistogram(kWebSocketReceiveTypeArrayBuffer);
      RecordReceiveMessageSizeHistogram(kWebSocketReceiveTypeArrayBuffer,
                                        binary_data->size());
      event_queue_->Dispatch(
          MessageEvent::Create(array_buffer, origin_string_));
      break;
  }
}

void DOMWebSocket::DidError() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidError()";
  if (state_ == kConnecting && was_autoupgraded_to_wss_)
    LogMixedAutoupgradeStatus(MixedContentAutoupgradeStatus::kFailed);
  ReflectBufferedAmountConsumption();
  state_ = kClosed;
  event_queue_->Dispatch(Event::Create(EventTypeNames::error));
}

void DOMWebSocket::DidConsumeBufferedAmount(uint64_t consumed) {
  DCHECK_GE(buffered_amount_, consumed + consumed_buffered_amount_);
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidConsumeBufferedAmount("
                   << consumed << ")";
  if (state_ == kClosed)
    return;
  consumed_buffered_amount_ += consumed;
  PostBufferedAmountUpdateTask();
}

void DOMWebSocket::DidStartClosingHandshake() {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidStartClosingHandshake()";
  ReflectBufferedAmountConsumption();
  state_ = kClosing;
}

void DOMWebSocket::DidClose(
    ClosingHandshakeCompletionStatus closing_handshake_completion,
    unsigned short code,
    const String& reason) {
  NETWORK_DVLOG(1) << "WebSocket " << this << " DidClose()";
  ReflectBufferedAmountConsumption();
  if (!channel_)
    return;
  bool all_data_has_been_consumed =
      buffered_amount_ == consumed_buffered_amount_;
  bool was_clean = state_ == kClosing && all_data_has_been_consumed &&
                   closing_handshake_completion == kClosingHandshakeComplete &&
                   code != WebSocketChannel::kCloseEventCodeAbnormalClosure;
  state_ = kClosed;

  ReleaseChannel();

  event_queue_->Dispatch(CloseEvent::Create(was_clean, code, reason));
}

void DOMWebSocket::RecordSendTypeHistogram(WebSocketSendType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, send_type_histogram,
      ("WebCore.WebSocket.SendType", kWebSocketSendTypeMax));
  send_type_histogram.Count(type);
}

void DOMWebSocket::RecordSendMessageSizeHistogram(WebSocketSendType type,
                                                  size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t size_to_count = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case kWebSocketSendTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.ArrayBuffer", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketSendTypeArrayBufferView: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_view_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.ArrayBufferView", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_view_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketSendTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blob_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Send.Blob", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      blob_message_size_histogram.Count(size_to_count);
      return;
    }

    default:
      NOTREACHED();
  }
}

void DOMWebSocket::RecordReceiveTypeHistogram(WebSocketReceiveType type) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, receive_type_histogram,
      ("WebCore.WebSocket.ReceiveType", kWebSocketReceiveTypeMax));
  receive_type_histogram.Count(type);
}

void DOMWebSocket::RecordReceiveMessageSizeHistogram(WebSocketReceiveType type,
                                                     size_t size) {
  // Truncate |size| to avoid overflowing int32_t.
  int32_t size_to_count = clampTo<int32_t>(size, 0, kMaxByteSizeForHistogram);
  switch (type) {
    case kWebSocketReceiveTypeArrayBuffer: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, array_buffer_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Receive.ArrayBuffer", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      array_buffer_message_size_histogram.Count(size_to_count);
      return;
    }

    case kWebSocketReceiveTypeBlob: {
      DEFINE_THREAD_SAFE_STATIC_LOCAL(
          CustomCountHistogram, blob_message_size_histogram,
          ("WebCore.WebSocket.MessageSize.Receive.Blob", 1,
           kMaxByteSizeForHistogram, kBucketCountForMessageSizeHistogram));
      blob_message_size_histogram.Count(size_to_count);
      return;
    }

    default:
      NOTREACHED();
  }
}

void DOMWebSocket::Trace(blink::Visitor* visitor) {
  visitor->Trace(channel_);
  visitor->Trace(event_queue_);
  WebSocketChannelClient::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

}  // namespace blink
