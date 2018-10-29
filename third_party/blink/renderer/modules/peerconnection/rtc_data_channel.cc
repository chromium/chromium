/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/peerconnection/rtc_data_channel.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_rtc_peer_connection_handler.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_peer_connection.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

static void ThrowNotOpenException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                    "RTCDataChannel.readyState is not 'open'");
}

static void ThrowCouldNotSendDataException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNetworkError,
                                    "Could not send data");
}

static void ThrowNoBlobSupportException(ExceptionState& exception_state) {
  exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                    "Blob support not implemented yet");
}

RTCDataChannel* RTCDataChannel::Create(
    ExecutionContext* context,
    std::unique_ptr<WebRTCDataChannelHandler> handler) {
  DCHECK(handler);
  RTCDataChannel* channel = new RTCDataChannel(context, std::move(handler));
  channel->PauseIfNeeded();

  return channel;
}

RTCDataChannel* RTCDataChannel::Create(
    ExecutionContext* context,
    WebRTCPeerConnectionHandler* peer_connection_handler,
    const String& label,
    const WebRTCDataChannelInit& init,
    ExceptionState& exception_state) {
  std::unique_ptr<WebRTCDataChannelHandler> handler =
      base::WrapUnique(peer_connection_handler->CreateDataChannel(label, init));
  if (!handler) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "RTCDataChannel is not supported");
    return nullptr;
  }
  RTCDataChannel* channel = new RTCDataChannel(context, std::move(handler));
  channel->PauseIfNeeded();

  return channel;
}

RTCDataChannel::RTCDataChannel(
    ExecutionContext* context,
    std::unique_ptr<WebRTCDataChannelHandler> handler)
    : PausableObject(context),
      handler_(std::move(handler)),
      ready_state_(kReadyStateConnecting),
      binary_type_(kBinaryTypeArrayBuffer),
      scheduled_event_timer_(context->GetTaskRunner(TaskType::kNetworking),
                             this,
                             &RTCDataChannel::ScheduledEventTimerFired),
      buffered_amount_low_threshold_(0U),
      stopped_(false) {
  handler_->SetClient(this);
}

RTCDataChannel::~RTCDataChannel() = default;

void RTCDataChannel::Dispose() {
  if (stopped_)
    return;

  // Promptly clears a raw reference from content/ to an on-heap object
  // so that content/ doesn't access it in a lazy sweeping phase.
  handler_->SetClient(nullptr);
  handler_.reset();
}

RTCDataChannel::ReadyState RTCDataChannel::GetHandlerState() const {
  return handler_->GetState();
}

String RTCDataChannel::label() const {
  return handler_->Label();
}

bool RTCDataChannel::reliable() const {
  return handler_->IsReliable();
}

bool RTCDataChannel::ordered() const {
  return handler_->Ordered();
}

unsigned short RTCDataChannel::maxRetransmitTime() const {
  return handler_->MaxRetransmitTime();
}

unsigned short RTCDataChannel::maxRetransmits() const {
  return handler_->MaxRetransmits();
}

String RTCDataChannel::protocol() const {
  return handler_->Protocol();
}

bool RTCDataChannel::negotiated() const {
  return handler_->Negotiated();
}

unsigned short RTCDataChannel::id() const {
  return handler_->Id();
}

String RTCDataChannel::readyState() const {
  switch (ready_state_) {
    case kReadyStateConnecting:
      return "connecting";
    case kReadyStateOpen:
      return "open";
    case kReadyStateClosing:
      return "closing";
    case kReadyStateClosed:
      return "closed";
  }

  NOTREACHED();
  return String();
}

unsigned RTCDataChannel::bufferedAmount() const {
  return SafeCast<unsigned>(handler_->BufferedAmount());
}

unsigned RTCDataChannel::bufferedAmountLowThreshold() const {
  return buffered_amount_low_threshold_;
}

void RTCDataChannel::setBufferedAmountLowThreshold(unsigned threshold) {
  buffered_amount_low_threshold_ = threshold;
}

String RTCDataChannel::binaryType() const {
  switch (binary_type_) {
    case kBinaryTypeBlob:
      return "blob";
    case kBinaryTypeArrayBuffer:
      return "arraybuffer";
  }
  NOTREACHED();
  return String();
}

void RTCDataChannel::setBinaryType(const String& binary_type,
                                   ExceptionState& exception_state) {
  if (binary_type == "blob")
    ThrowNoBlobSupportException(exception_state);
  else if (binary_type == "arraybuffer")
    binary_type_ = kBinaryTypeArrayBuffer;
  else
    exception_state.ThrowDOMException(DOMExceptionCode::kTypeMismatchError,
                                      "Unknown binary type : " + binary_type);
}

void RTCDataChannel::send(const String& data, ExceptionState& exception_state) {
  if (ready_state_ != kReadyStateOpen) {
    ThrowNotOpenException(exception_state);
    return;
  }
  if (!handler_->SendStringData(data)) {
    // FIXME: This should not throw an exception but instead forcefully close
    // the data channel.
    ThrowCouldNotSendDataException(exception_state);
  }
}

void RTCDataChannel::send(DOMArrayBuffer* data,
                          ExceptionState& exception_state) {
  if (ready_state_ != kReadyStateOpen) {
    ThrowNotOpenException(exception_state);
    return;
  }

  size_t data_length = data->ByteLength();
  if (!data_length)
    return;

  if (!handler_->SendRawData(static_cast<const char*>((data->Data())),
                             data_length)) {
    // FIXME: This should not throw an exception but instead forcefully close
    // the data channel.
    ThrowCouldNotSendDataException(exception_state);
  }
}

void RTCDataChannel::send(NotShared<DOMArrayBufferView> data,
                          ExceptionState& exception_state) {
  if (!handler_->SendRawData(
          static_cast<const char*>(data.View()->BaseAddress()),
          data.View()->byteLength())) {
    // FIXME: This should not throw an exception but instead forcefully close
    // the data channel.
    ThrowCouldNotSendDataException(exception_state);
  }
}

void RTCDataChannel::send(Blob* data, ExceptionState& exception_state) {
  // FIXME: implement
  ThrowNoBlobSupportException(exception_state);
}

void RTCDataChannel::close() {
  if (handler_)
    handler_->Close();
}

void RTCDataChannel::DidChangeReadyState(
    WebRTCDataChannelHandlerClient::ReadyState new_state) {
  if (ready_state_ == kReadyStateClosed)
    return;

  ready_state_ = new_state;

  switch (ready_state_) {
    case kReadyStateOpen:
      ScheduleDispatchEvent(Event::Create(EventTypeNames::open));
      break;
    case kReadyStateClosed:
      ScheduleDispatchEvent(Event::Create(EventTypeNames::close));
      break;
    default:
      break;
  }
}

void RTCDataChannel::DidDecreaseBufferedAmount(unsigned previous_amount) {
  if (previous_amount > buffered_amount_low_threshold_ &&
      bufferedAmount() <= buffered_amount_low_threshold_) {
    ScheduleDispatchEvent(Event::Create(EventTypeNames::bufferedamountlow));
  }
}

void RTCDataChannel::DidReceiveStringData(const WebString& text) {
  ScheduleDispatchEvent(MessageEvent::Create(text));
}

void RTCDataChannel::DidReceiveRawData(const char* data, size_t data_length) {
  if (binary_type_ == kBinaryTypeBlob) {
    // FIXME: Implement.
    return;
  }
  if (binary_type_ == kBinaryTypeArrayBuffer) {
    DOMArrayBuffer* buffer =
        DOMArrayBuffer::Create(data, SafeCast<unsigned>(data_length));
    ScheduleDispatchEvent(MessageEvent::Create(buffer));
    return;
  }
  NOTREACHED();
}

void RTCDataChannel::DidDetectError() {
  ScheduleDispatchEvent(Event::Create(EventTypeNames::error));
}

const AtomicString& RTCDataChannel::InterfaceName() const {
  return EventTargetNames::RTCDataChannel;
}

ExecutionContext* RTCDataChannel::GetExecutionContext() const {
  return PausableObject::GetExecutionContext();
}

// PausableObject
void RTCDataChannel::Pause() {
  scheduled_event_timer_.Stop();
}

void RTCDataChannel::Unpause() {
  if (!scheduled_events_.IsEmpty() && !scheduled_event_timer_.IsActive())
    scheduled_event_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void RTCDataChannel::ContextDestroyed(ExecutionContext*) {
  if (stopped_)
    return;

  stopped_ = true;
  handler_->SetClient(nullptr);
  handler_.reset();
  ready_state_ = kReadyStateClosed;
}

// ActiveScriptWrappable
bool RTCDataChannel::HasPendingActivity() const {
  if (stopped_)
    return false;

  // A RTCDataChannel object must not be garbage collected if its
  // * readyState is connecting and at least one event listener is registered
  //   for open events, message events, error events, or close events.
  // * readyState is open and at least one event listener is registered for
  //   message events, error events, or close events.
  // * readyState is closing and at least one event listener is registered for
  //   error events, or close events.
  // * underlying data transport is established and data is queued to be
  //   transmitted.
  bool has_valid_listeners = false;
  switch (ready_state_) {
    case kReadyStateConnecting:
      has_valid_listeners |= HasEventListeners(EventTypeNames::open);
      FALLTHROUGH;
    case kReadyStateOpen:
      has_valid_listeners |= HasEventListeners(EventTypeNames::message);
      FALLTHROUGH;
    case kReadyStateClosing:
      has_valid_listeners |= HasEventListeners(EventTypeNames::error) ||
                             HasEventListeners(EventTypeNames::close);
      break;
    default:
      break;
  }

  if (has_valid_listeners)
    return true;

  return ready_state_ != kReadyStateClosed && bufferedAmount() > 0;
}

void RTCDataChannel::ScheduleDispatchEvent(Event* event) {
  scheduled_events_.push_back(event);

  if (!scheduled_event_timer_.IsActive())
    scheduled_event_timer_.StartOneShot(TimeDelta(), FROM_HERE);
}

void RTCDataChannel::ScheduledEventTimerFired(TimerBase*) {
  HeapVector<Member<Event>> events;
  events.swap(scheduled_events_);

  HeapVector<Member<Event>>::iterator it = events.begin();
  for (; it != events.end(); ++it)
    DispatchEvent(*it->Release());

  events.clear();
}

void RTCDataChannel::Trace(blink::Visitor* visitor) {
  visitor->Trace(scheduled_events_);
  EventTargetWithInlineData::Trace(visitor);
  PausableObject::Trace(visitor);
}

}  // namespace blink
