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

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/frame/lifecycle.mojom-shared.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_string_stringsequence.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/websockets/close_event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

DOMWebSocket::EventQueue::EventQueue(EventTarget* target)
    : state_(kActive), target_(target) {}

void DOMWebSocket::EventQueue::Dispatch(Event* event) {
  switch (state_) {
    case kActive:
      DCHECK(events_.empty());
      target_->DispatchEvent(*event);
      break;
    case kPaused:
    case kUnpausePosted:
      events_.push_back(event);
      break;
    case kStopped:
      DCHECK(events_.empty());
      // Do nothing.
      break;
  }
}

bool DOMWebSocket::EventQueue::IsEmpty() const {
  return events_.empty();
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
      ->PostTask(FROM_HERE, WTF::BindOnce(&EventQueue::UnpauseTask,
                                          WrapWeakPersistent(this)));
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
  while (!events.empty()) {
    if (state_ == kStopped || state_ == kPaused || state_ == kUnpausePosted)
      break;
    DCHECK_EQ(state_, kActive);
    target_->DispatchEvent(*events.TakeFirst());
    // |this| can be stopped here.
  }
  if (state_ == kPaused || state_ == kUnpausePosted) {
    while (!events_.empty())
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

void DOMWebSocket::EventQueue::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(events_);
}

static void SetInvalidStateErrorForSendMethod(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "Still in CONNECTING state.");
}

constexpr WebSocketCommon::State DOMWebSocket::kConnecting;
constexpr WebSocketCommon::State DOMWebSocket::kOpen;
constexpr WebSocketCommon::State DOMWebSocket::kClosing;
constexpr WebSocketCommon::State DOMWebSocket::kClosed;

DOMWebSocket::DOMWebSocket(ExecutionContext* context)
    : ActiveScriptWrappable<DOMWebSocket>({}),
      ExecutionContextLifecycleStateObserver(context),
      buffered_amount_(0),
      consumed_buffered_amount_(0),
      buffered_amount_after_close_(0),
      subprotocol_(""),
      extensions_(""),
      event_queue_(MakeGarbageCollected<EventQueue>(this)),
      buffered_amount_update_task_pending_(false) {
  DVLOG(1) << "DOMWebSocket " << this << " created";
}

DOMWebSocket::~DOMWebSocket() {
  DVLOG(1) << "DOMWebSocket " << this << " destroyed";
  DCHECK(!channel_);
}

void DOMWebSocket::LogError(const String& message) {
  if (GetExecutionContext()) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::ConsoleMessageSource::kJavaScript,
            mojom::ConsoleMessageLevel::kError, message));
  }
}

DOMWebSocket* DOMWebSocket::Create(ExecutionContext* context,
                                   const String& url,
                                   ExceptionState& exception_state) {
  return Create(
      context, url,
      MakeGarbageCollected<V8UnionStringOrStringSequence>(Vector<String>()),
      exception_state);
}

DOMWebSocket* DOMWebSocket::Create(
    ExecutionContext* context,
    const String& url,
    const V8UnionStringOrStringSequence* protocols,
    ExceptionState& exception_state) {
  if (url.IsNull()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "Failed to create a WebSocket: the provided URL is invalid.");
    return nullptr;
  }

  DOMWebSocket* websocket = MakeGarbageCollected<DOMWebSocket>(context);
  websocket->UpdateStateIfNeeded();

  DCHECK(protocols);
  switch (protocols->GetContentType()) {
    case V8UnionStringOrStringSequence::ContentType::kString: {
      Vector<String> protocols_vector;
      protocols_vector.push_back(protocols->GetAsString());
      websocket->Connect(url, protocols_vector, exception_state);
      break;
    }
    case V8UnionStringOrStringSequence::ContentType::kStringSequence:
      websocket->Connect(url, protocols->GetAsStringSequence(),
                         exception_state);
      break;
  }

  if (exception_state.HadException())
    return nullptr;

  return websocket;
}

void DOMWebSocket::Connect(const String& url,
                           const Vector<String>& protocols,
                           ExceptionState& exception_state) {
  UseCounter::Count(GetExecutionContext(), WebFeature::kWebSocket);

  DVLOG(1) << "WebSocket " << this << " connect() url=" << url;

  channel_ = CreateChannel(GetExecutionContext(), this);
  auto result = common_.Connect(GetExecutionContext(), url, protocols, channel_,
                                exception_state);

  switch (result) {
    case WebSocketCommon::ConnectResult::kSuccess:
      DCHECK(!exception_state.HadException());
      origin_string_ = SecurityOrigin::Create(common_.Url())->ToString();
      return;

    case WebSocketCommon::ConnectResult::kException:
      DCHECK(exception_state.HadException());
      channel_ = nullptr;
      return;

    case WebSocketCommon::ConnectResult::kAsyncError:
      DCHECK(!exception_state.HadException());
      // Delay the event dispatch until after the current task by suspending and
      // resuming the queue. If we don't do this, the event is fired
      // synchronously with the constructor, meaning that it's impossible to
      // listen for.
      event_queue_->Pause();
      event_queue_->Dispatch(Event::Create(event_type_names::kError));
      event_queue_->Unpause();
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
      ->PostTask(FROM_HERE,
                 WTF::BindOnce(&DOMWebSocket::BufferedAmountUpdateTask,
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
  DVLOG(1) << "WebSocket " << this << " reflectBufferedAmountConsumption() "
           << buffered_amount_ << " => "
           << (buffered_amount_ - consumed_buffered_amount_);

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
  DVLOG(1) << "WebSocket " << this << " send() Sending String " << message;
  if (common_.GetState() == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  // No exception is raised if the connection was once established but has
  // subsequently been closed.
  std::string encoded_message = message.Utf8();
  if (common_.GetState() == kClosing || common_.GetState() == kClosed) {
    UpdateBufferedAmountAfterClose(encoded_message.length());
    return;
  }

  DCHECK(channel_);
  buffered_amount_ += encoded_message.length();
  channel_->Send(encoded_message, base::OnceClosure());
  NotifyWebSocketActivity();
}

void DOMWebSocket::send(DOMArrayBuffer* binary_data,
                        ExceptionState& exception_state) {
  DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBuffer "
           << binary_data;
  DCHECK(binary_data);
  if (common_.GetState() == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (common_.GetState() == kClosing || common_.GetState() == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->ByteLength());
    return;
  }
  DCHECK(channel_);
  buffered_amount_ += binary_data->ByteLength();
  channel_->Send(*binary_data, 0, binary_data->ByteLength(),
                 base::OnceClosure());
  NotifyWebSocketActivity();
}

void DOMWebSocket::send(NotShared<DOMArrayBufferView> array_buffer_view,
                        ExceptionState& exception_state) {
  DVLOG(1) << "WebSocket " << this << " send() Sending ArrayBufferView "
           << array_buffer_view.Get();
  DCHECK(array_buffer_view);
  if (common_.GetState() == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (common_.GetState() == kClosing || common_.GetState() == kClosed) {
    UpdateBufferedAmountAfterClose(array_buffer_view->byteLength());
    return;
  }
  DCHECK(channel_);
  buffered_amount_ += array_buffer_view->byteLength();
  channel_->Send(*array_buffer_view->buffer(), array_buffer_view->byteOffset(),
                 array_buffer_view->byteLength(), base::OnceClosure());
  NotifyWebSocketActivity();
}

void DOMWebSocket::send(Blob* binary_data, ExceptionState& exception_state) {
  DVLOG(1) << "WebSocket " << this << " send() Sending Blob "
           << binary_data->Uuid();
  DCHECK(binary_data);
  if (common_.GetState() == kConnecting) {
    SetInvalidStateErrorForSendMethod(exception_state);
    return;
  }
  if (common_.GetState() == kClosing || common_.GetState() == kClosed) {
    UpdateBufferedAmountAfterClose(binary_data->size());
    return;
  }
  uint64_t size = binary_data->size();
  buffered_amount_ += size;
  DCHECK(channel_);

  // When the runtime type of |binary_data| is File,
  // binary_data->GetBlobDataHandle()->size() returns -1. However, in order to
  // maintain the value of |buffered_amount_| correctly, the WebSocket code
  // needs to fix the size of the File at this point. For this reason,
  // construct a new BlobDataHandle here with the size that this method
  // observed.
  channel_->Send(BlobDataHandle::Create(binary_data->Uuid(),
                                        binary_data->type(), size,
                                        binary_data->AsMojoBlob()));
  NotifyWebSocketActivity();
}

void DOMWebSocket::close(uint16_t code,
                         const String& reason,
                         ExceptionState& exception_state) {
  CloseInternal(code, reason, exception_state);
}

void DOMWebSocket::close(ExceptionState& exception_state) {
  CloseInternal(std::nullopt, String(), exception_state);
}

void DOMWebSocket::close(uint16_t code, ExceptionState& exception_state) {
  CloseInternal(code, String(), exception_state);
}

void DOMWebSocket::CloseInternal(std::optional<uint16_t> code,
                                 const String& reason,
                                 ExceptionState& exception_state) {
  common_.CloseInternal(code, reason, channel_, exception_state);
}

const KURL& DOMWebSocket::url() const {
  return common_.Url();
}

WebSocketCommon::State DOMWebSocket::readyState() const {
  return common_.GetState();
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

V8BinaryType DOMWebSocket::binaryType() const {
  return V8BinaryType(binary_type_);
}

void DOMWebSocket::setBinaryType(const V8BinaryType& binary_type) {
  binary_type_ = binary_type.AsEnum();
}

const AtomicString& DOMWebSocket::InterfaceName() const {
  return event_target_names::kWebSocket;
}

ExecutionContext* DOMWebSocket::GetExecutionContext() const {
  return ExecutionContextLifecycleStateObserver::GetExecutionContext();
}

void DOMWebSocket::ContextDestroyed() {
  DVLOG(1) << "WebSocket " << this << " contextDestroyed()";
  event_queue_->ContextDestroyed();
  if (channel_) {
    ReleaseChannel();
  }
  if (common_.GetState() != kClosed) {
    common_.SetState(kClosed);
  }
}

bool DOMWebSocket::HasPendingActivity() const {
  return channel_ || !event_queue_->IsEmpty();
}

void DOMWebSocket::ContextLifecycleStateChanged(
    mojom::FrameLifecycleState state) {
  if (state == mojom::FrameLifecycleState::kRunning) {
    event_queue_->Unpause();

    // If |consumed_buffered_amount_| was updated while the object was paused
    // then the changes to |buffered_amount_| will not yet have been applied.
    // Post another task to update it.
    PostBufferedAmountUpdateTask();
  } else {
    event_queue_->Pause();
  }
}

void DOMWebSocket::DidConnect(const String& subprotocol,
                              const String& extensions) {
  DVLOG(1) << "WebSocket " << this << " DidConnect()";
  if (common_.GetState() != kConnecting)
    return;
  common_.SetState(kOpen);
  subprotocol_ = subprotocol;
  extensions_ = extensions;
  event_queue_->Dispatch(Event::Create(event_type_names::kOpen));
  NotifyWebSocketActivity();
}

void DOMWebSocket::DidReceiveTextMessage(const String& msg) {
  DVLOG(1) << "WebSocket " << this << " DidReceiveTextMessage() Text message "
           << msg;
  ReflectBufferedAmountConsumption();
  DCHECK_NE(common_.GetState(), kConnecting);
  if (common_.GetState() != kOpen)
    return;

  DCHECK(!origin_string_.IsNull());
  event_queue_->Dispatch(MessageEvent::Create(msg, origin_string_));
  NotifyWebSocketActivity();
}

void DOMWebSocket::DidReceiveBinaryMessage(
    const Vector<base::span<const char>>& data) {
  size_t size = 0;
  for (const auto& span : data) {
    size += span.size();
  }
  DVLOG(1) << "WebSocket " << this << " DidReceiveBinaryMessage() " << size
           << " byte binary message";
  ReflectBufferedAmountConsumption();
  DCHECK(!origin_string_.IsNull());

  DCHECK_NE(common_.GetState(), kConnecting);
  if (common_.GetState() != kOpen)
    return;

  switch (binary_type_) {
    case V8BinaryType::Enum::kBlob: {
      auto blob_data = std::make_unique<BlobData>();
      for (const auto& span : data) {
        blob_data->AppendBytes(base::as_bytes(span));
      }
      auto* blob = MakeGarbageCollected<Blob>(
          BlobDataHandle::Create(std::move(blob_data), size));
      event_queue_->Dispatch(MessageEvent::Create(blob, origin_string_));
      break;
    }

    case V8BinaryType::Enum::kArraybuffer:
      DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(data);
      event_queue_->Dispatch(
          MessageEvent::Create(array_buffer, origin_string_));
      break;
  }
  NotifyWebSocketActivity();
}

void DOMWebSocket::DidError() {
  DVLOG(1) << "WebSocket " << this << " DidError()";
  ReflectBufferedAmountConsumption();
  common_.SetState(kClosed);
  event_queue_->Dispatch(Event::Create(event_type_names::kError));
}

void DOMWebSocket::DidConsumeBufferedAmount(uint64_t consumed) {
  DCHECK_GE(buffered_amount_, consumed + consumed_buffered_amount_);
  DVLOG(1) << "WebSocket " << this << " DidConsumeBufferedAmount(" << consumed
           << ")";
  if (common_.GetState() == kClosed)
    return;
  consumed_buffered_amount_ += consumed;
  PostBufferedAmountUpdateTask();
}

void DOMWebSocket::DidStartClosingHandshake() {
  DVLOG(1) << "WebSocket " << this << " DidStartClosingHandshake()";
  ReflectBufferedAmountConsumption();
  common_.SetState(kClosing);
}

void DOMWebSocket::DidClose(
    ClosingHandshakeCompletionStatus closing_handshake_completion,
    uint16_t code,
    const String& reason) {
  DVLOG(1) << "WebSocket " << this << " DidClose()";
  ReflectBufferedAmountConsumption();
  if (!channel_)
    return;
  bool all_data_has_been_consumed =
      buffered_amount_ == consumed_buffered_amount_;
  bool was_clean = common_.GetState() == kClosing &&
                   all_data_has_been_consumed &&
                   closing_handshake_completion == kClosingHandshakeComplete &&
                   code != WebSocketChannel::kCloseEventCodeAbnormalClosure;
  common_.SetState(kClosed);

  ReleaseChannel();

  event_queue_->Dispatch(
      MakeGarbageCollected<CloseEvent>(was_clean, code, reason));
}

void DOMWebSocket::NotifyWebSocketActivity() {
  ExecutionContext* context = GetExecutionContext();
  if (context) {
    context->NotifyWebSocketActivity();
  }
}

void DOMWebSocket::Trace(Visitor* visitor) const {
  visitor->Trace(channel_);
  visitor->Trace(event_queue_);
  WebSocketChannelClient::Trace(visitor);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
}

}  // namespace blink
