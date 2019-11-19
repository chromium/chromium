// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// 6 MB allows a reasonable amount to buffer on the read and write side.
// TODO(https://crbug.com/874296): Consider exposing these configurations.
const uint32_t RTCQuicStream::kWriteBufferSize = 6 * 1024 * 1024;
const uint32_t RTCQuicStream::kReadBufferSize = 6 * 1024 * 1024;

class RTCQuicStream::PendingReadBufferedAmountPromise
    : public GarbageCollected<PendingReadBufferedAmountPromise> {
 public:
  PendingReadBufferedAmountPromise(ScriptPromiseResolver* promise_resolver,
                                   uint32_t readable_amount)
      : promise_resolver_(promise_resolver),
        readable_amount_(readable_amount) {}

  ScriptPromiseResolver* promise_resolver() const { return promise_resolver_; }
  uint32_t readable_amount() const { return readable_amount_; }

  void Trace(Visitor* visitor) { visitor->Trace(promise_resolver_); }

 private:
  Member<ScriptPromiseResolver> promise_resolver_;
  uint32_t readable_amount_;
};

class RTCQuicStream::PendingWriteBufferedAmountPromise
    : public GarbageCollected<PendingWriteBufferedAmountPromise> {
 public:
  PendingWriteBufferedAmountPromise(ScriptPromiseResolver* promise_resolver,
                                    uint32_t threshold)
      : promise_resolver_(promise_resolver), threshold_(threshold) {}

  ScriptPromiseResolver* promise_resolver() const { return promise_resolver_; }
  uint32_t threshold() const { return threshold_; }

  void Trace(Visitor* visitor) { visitor->Trace(promise_resolver_); }

 private:
  Member<ScriptPromiseResolver> promise_resolver_;
  uint32_t threshold_;
};

RTCQuicStream::RTCQuicStream(ExecutionContext* context,
                             RTCQuicTransport* transport,
                             QuicStreamProxy* stream_proxy)
    : ContextClient(context), transport_(transport), proxy_(stream_proxy) {
  DCHECK(transport_);
  DCHECK(proxy_);
}

RTCQuicStream::~RTCQuicStream() = default;

RTCQuicTransport* RTCQuicStream::transport() const {
  return transport_;
}

String RTCQuicStream::state() const {
  switch (state_) {
    case RTCQuicStreamState::kNew:
      return "new";
    case RTCQuicStreamState::kOpening:
      return "opening";
    case RTCQuicStreamState::kOpen:
      return "open";
    case RTCQuicStreamState::kClosing:
      return "closing";
    case RTCQuicStreamState::kClosed:
      return "closed";
  }
  return String();
}

uint32_t RTCQuicStream::readBufferedAmount() const {
  return receive_buffer_.size();
}

uint32_t RTCQuicStream::maxReadBufferedAmount() const {
  return kReadBufferSize;
}

uint32_t RTCQuicStream::writeBufferedAmount() const {
  return write_buffered_amount_;
}

uint32_t RTCQuicStream::maxWriteBufferedAmount() const {
  return kWriteBufferSize;
}

RTCQuicStreamReadResult* RTCQuicStream::readInto(
    NotShared<DOMUint8Array> data,
    ExceptionState& exception_state) {
  if (RaiseIfNotReadable(exception_state)) {
    return 0;
  }
  uint32_t read_amount = static_cast<uint32_t>(receive_buffer_.ReadInto(
      base::make_span(data.View()->Data(), data.View()->length())));
  if (!received_fin_ && read_amount > 0) {
    proxy_->MarkReceivedDataConsumed(read_amount);
  }
  if (receive_buffer_.empty() && received_fin_) {
    read_fin_ = true;
    if (wrote_fin_) {
      DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
      Close(CloseReason::kReadWriteFinished);
    } else {
      DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
      state_ = RTCQuicStreamState::kClosing;
    }
  }
  auto* result = RTCQuicStreamReadResult::Create();
  result->setAmount(read_amount);
  result->setFinished(read_fin_);
  return result;
}

void RTCQuicStream::write(const RTCQuicStreamWriteParameters* data,
                          ExceptionState& exception_state) {
  bool finish = data->finish();
  bool has_write_data = data->hasData() && data->data().View()->length() > 0;
  if (!has_write_data && !finish) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot write empty data, unless data.finish is set to true.");
    return;
  }
  if (RaiseIfNotWritable(exception_state)) {
    return;
  }
  Vector<uint8_t> data_vector;
  if (has_write_data) {
    DOMUint8Array* write_data = data->data().View();
    uint32_t remaining_write_buffer_size =
        kWriteBufferSize - writeBufferedAmount();
    if (write_data->length() > remaining_write_buffer_size) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kOperationError,
          "The write data size of " + String::Number(write_data->length()) +
              " bytes would exceed the remaining write buffer size of " +
              String::Number(remaining_write_buffer_size) + " bytes.");
      return;
    }
    data_vector.resize(write_data->length());
    memcpy(data_vector.data(), write_data->Data(), write_data->length());
    write_buffered_amount_ += write_data->length();
  }
  proxy_->WriteData(std::move(data_vector), finish);
  if (finish) {
    wrote_fin_ = true;
    if (!read_fin_) {
      DCHECK_EQ(state_, RTCQuicStreamState::kOpen);
      state_ = RTCQuicStreamState::kClosing;
      RejectPendingWaitForWriteBufferedAmountBelowPromises();
    } else {
      DCHECK_EQ(state_, RTCQuicStreamState::kClosing);
      Close(CloseReason::kReadWriteFinished);
    }
  }
}

void RTCQuicStream::reset() {
  if (IsClosed()) {
    return;
  }
  Close(CloseReason::kLocalReset);
}

ScriptPromise RTCQuicStream::waitForReadable(ScriptState* script_state,
                                             uint32_t amount,
                                             ExceptionState& exception_state) {
  if (RaiseIfNotReadable(exception_state)) {
    return ScriptPromise();
  }
  if (amount > kReadBufferSize) {
    exception_state.ThrowTypeError(
        "The amount " + String::Number(amount) +
        " is greater than the maximum read buffer size of " +
        String::Number(kReadBufferSize) + ".");
    return ScriptPromise();
  }
  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = promise_resolver->Promise();
  if (received_fin_ || receive_buffer_.size() >= amount) {
    promise_resolver->Resolve();
  } else {
    pending_read_buffered_amount_promises_.push_back(
        MakeGarbageCollected<PendingReadBufferedAmountPromise>(promise_resolver,
                                                               amount));
  }
  return promise;
}

ScriptPromise RTCQuicStream::waitForWriteBufferedAmountBelow(
    ScriptState* script_state,
    uint32_t threshold,
    ExceptionState& exception_state) {
  if (RaiseIfNotWritable(exception_state)) {
    return ScriptPromise();
  }
  auto* promise_resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = promise_resolver->Promise();
  if (write_buffered_amount_ <= threshold) {
    promise_resolver->Resolve();
  } else {
    pending_write_buffered_amount_promises_.push_back(
        MakeGarbageCollected<PendingWriteBufferedAmountPromise>(
            promise_resolver, threshold));
  }
  return promise;
}

bool RTCQuicStream::RaiseIfNotReadable(ExceptionState& exception_state) {
  if (read_fin_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The stream is not readable: The end of the stream has been read.");
    return true;
  }
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The stream is not readable: The stream is closed.");
    return true;
  }
  return false;
}

bool RTCQuicStream::RaiseIfNotWritable(ExceptionState& exception_state) {
  if (wrote_fin_) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The stream is not writable: finish() has been called.");
    return true;
  }
  if (IsClosed()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "The stream is not writable: The stream is closed.");
    return true;
  }
  return false;
}

void RTCQuicStream::RejectPendingWaitForReadablePromises() {
  // TODO(https://github.com/w3c/webrtc-quic/issues/81): The promise resolve
  // order is under specified.
  for (PendingReadBufferedAmountPromise* pending_promise :
       pending_read_buffered_amount_promises_) {
    ScriptState::Scope scope(
        pending_promise->promise_resolver()->GetScriptState());
    ExceptionState exception_state(
        pending_promise->promise_resolver()->GetScriptState()->GetIsolate(),
        ExceptionState::kExecutionContext, "RTCQuicStream", "waitForReadable");
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The RTCQuicStream is not readable.");
    pending_promise->promise_resolver()->Reject(exception_state);
  }
  pending_read_buffered_amount_promises_.clear();
}

void RTCQuicStream::RejectPendingWaitForWriteBufferedAmountBelowPromises() {
  // TODO(https://github.com/w3c/webrtc-quic/issues/81): The promise resolve
  // order is under specified.
  for (PendingWriteBufferedAmountPromise* pending_promise :
       pending_write_buffered_amount_promises_) {
    ScriptState::Scope scope(
        pending_promise->promise_resolver()->GetScriptState());
    ExceptionState exception_state(
        pending_promise->promise_resolver()->GetScriptState()->GetIsolate(),
        ExceptionState::kExecutionContext, "RTCQuicStream",
        "waitForWriteBufferedAmountBelow");
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The stream is no longer writable.");
    pending_promise->promise_resolver()->Reject(exception_state);
  }
  pending_write_buffered_amount_promises_.clear();
}

void RTCQuicStream::OnRemoteReset() {
  Close(CloseReason::kRemoteReset);
}

void RTCQuicStream::OnDataReceived(Vector<uint8_t> data, bool fin) {
  DCHECK(!received_fin_);
  DCHECK_LE(data.size(), kReadBufferSize - receive_buffer_.size());
  received_fin_ = fin;
  receive_buffer_.Append(std::move(data));
  // TODO(https://github.com/w3c/webrtc-quic/issues/81): The promise resolve
  // order is under specified.
  for (auto* it = pending_read_buffered_amount_promises_.begin();
       it != pending_read_buffered_amount_promises_.end();
       /* incremented manually */) {
    PendingReadBufferedAmountPromise* pending_promise = *it;
    if (received_fin_ ||
        receive_buffer_.size() >= pending_promise->readable_amount()) {
      pending_promise->promise_resolver()->Resolve();
      it = pending_read_buffered_amount_promises_.erase(it);
    } else {
      ++it;
    }
  }
}

void RTCQuicStream::OnWriteDataConsumed(uint32_t amount) {
  DCHECK_GE(write_buffered_amount_, amount);
  write_buffered_amount_ -= amount;
  // TODO(https://github.com/w3c/webrtc-quic/issues/81): The promise resolve
  // order is under specified.
  for (auto* it = pending_write_buffered_amount_promises_.begin();
       it != pending_write_buffered_amount_promises_.end();
       /* incremented manually */) {
    PendingWriteBufferedAmountPromise* pending_promise = *it;
    if (write_buffered_amount_ <= pending_promise->threshold()) {
      pending_promise->promise_resolver()->Resolve();
      it = pending_write_buffered_amount_promises_.erase(it);
    } else {
      ++it;
    }
  }
}

void RTCQuicStream::OnQuicTransportClosed(
    RTCQuicTransport::CloseReason reason) {
  switch (reason) {
    case RTCQuicTransport::CloseReason::kContextDestroyed:
      Close(CloseReason::kContextDestroyed);
      break;
    default:
      Close(CloseReason::kQuicTransportClosed);
      break;
  }
}

void RTCQuicStream::Close(CloseReason reason) {
  DCHECK_NE(state_, RTCQuicStreamState::kClosed);

  // Tear down the QuicStreamProxy.
  // If the Close is caused by a remote event or regular use of WriteData, the
  // QuicStreamProxy will have already been deleted.
  // If the Close is caused by the transport then the transport is responsible
  // for deleting the QuicStreamProxy.
  if (reason == CloseReason::kLocalReset) {
    // This deletes the QuicStreamProxy.
    proxy_->Reset();
  }
  proxy_ = nullptr;

  // Remove this stream from the RTCQuicTransport unless closing from a
  // transport-level event.
  switch (reason) {
    case CloseReason::kReadWriteFinished:
    case CloseReason::kLocalReset:
    case CloseReason::kRemoteReset:
      transport_->RemoveStream(this);
      break;
    case CloseReason::kQuicTransportClosed:
    case CloseReason::kContextDestroyed:
      // The RTCQuicTransport will handle clearing its list of streams.
      break;
  }

  // Clear observable state.
  receive_buffer_.Clear();
  write_buffered_amount_ = 0;

  // It's illegal to resolve or reject promises when the ExecutionContext is
  // being destroyed.
  if (reason != CloseReason::kContextDestroyed) {
    RejectPendingWaitForReadablePromises();
    RejectPendingWaitForWriteBufferedAmountBelowPromises();
  }

  // Change the state. Fire the statechange event only if the close is caused by
  // a remote stream event.
  state_ = RTCQuicStreamState::kClosed;
  if (reason == CloseReason::kRemoteReset) {
    DispatchEvent(*Event::Create(event_type_names::kStatechange));
  }
}

const AtomicString& RTCQuicStream::InterfaceName() const {
  return event_target_names::kRTCQuicStream;
}

ExecutionContext* RTCQuicStream::GetExecutionContext() const {
  return ContextClient::GetExecutionContext();
}

void RTCQuicStream::Trace(blink::Visitor* visitor) {
  visitor->Trace(transport_);
  visitor->Trace(pending_read_buffered_amount_promises_);
  visitor->Trace(pending_write_buffered_amount_promises_);
  EventTargetWithInlineData::Trace(visitor);
  ContextClient::Trace(visitor);
}

}  // namespace blink
