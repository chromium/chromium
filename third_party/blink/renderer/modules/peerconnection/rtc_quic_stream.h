// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_

#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_stream_proxy.h"
#include "third_party/blink/renderer/modules/peerconnection/byte_buffer_queue.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_read_result.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_stream_write_parameters.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_quic_transport.h"

namespace blink {

class ScriptPromise;

enum class RTCQuicStreamState { kNew, kOpening, kOpen, kClosing, kClosed };

// The RTCQuicStream does not need to be ActiveScriptWrappable since the
// RTCQuicTransport that it is associated with holds a strong reference to it
// as long as it is not closed.
class MODULES_EXPORT RTCQuicStream final : public EventTargetWithInlineData,
                                           public ContextClient,
                                           public QuicStreamProxy::Delegate {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(RTCQuicStream);

 public:
  // TODO(steveanton): These maybe should be adjustable.
  static const uint32_t kWriteBufferSize;
  static const uint32_t kReadBufferSize;

  enum class CloseReason {
    // Both read and write sides have been finished.
    kReadWriteFinished,
    // reset() was called.
    kLocalReset,
    // The remote stream sent a reset().
    kRemoteReset,
    // The RTCQuicTransport has closed.
    kQuicTransportClosed,
    // The ExecutionContext is being destroyed.
    kContextDestroyed,
  };

  RTCQuicStream(ExecutionContext* context,
                RTCQuicTransport* transport,
                QuicStreamProxy* stream_proxy);
  ~RTCQuicStream() override;

  // Called by the RTCQuicTransport when it is being closed.
  void OnQuicTransportClosed(RTCQuicTransport::CloseReason reason);

  // rtc_quic_stream.idl
  RTCQuicTransport* transport() const;
  String state() const;
  uint32_t readBufferedAmount() const;
  uint32_t maxReadBufferedAmount() const;
  uint32_t writeBufferedAmount() const;
  uint32_t maxWriteBufferedAmount() const;
  RTCQuicStreamReadResult* readInto(NotShared<DOMUint8Array> data,
                                    ExceptionState& exception_state);
  void write(const RTCQuicStreamWriteParameters* data,
             ExceptionState& exception_state);
  void reset();
  ScriptPromise waitForWriteBufferedAmountBelow(
      ScriptState* script_state,
      uint32_t threshold,
      ExceptionState& exception_state);
  ScriptPromise waitForReadable(ScriptState* script_state,
                                uint32_t amount,
                                ExceptionState& exception_state);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange, kStatechange)

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // For garbage collection.
  void Trace(Visitor* visitor) override;

 private:
  class PendingReadBufferedAmountPromise;
  class PendingWriteBufferedAmountPromise;

  // QuicStreamProxy::Delegate overrides.
  void OnRemoteReset() override;
  void OnDataReceived(Vector<uint8_t> data, bool fin) override;
  void OnWriteDataConsumed(uint32_t amount) override;

  bool RaiseIfNotReadable(ExceptionState&);
  bool RaiseIfNotWritable(ExceptionState&);

  // Permenantly closes the RTCQuicStream with the given reason.
  // The RTCQuicStream must not already be closed.
  // This will transition the state to closed.
  void Close(CloseReason reason);

  bool IsClosed() const { return state_ == RTCQuicStreamState::kClosed; }

  void RejectPendingWaitForReadablePromises();
  void RejectPendingWaitForWriteBufferedAmountBelowPromises();

  Member<RTCQuicTransport> transport_;
  RTCQuicStreamState state_ = RTCQuicStreamState::kOpen;

  // Data that has been received but not read.
  // OnDataReceived() appends to the read buffer.
  // readInto() will read out from the front of the buffer.
  ByteBufferQueue receive_buffer_;
  // True if the fin has been received from the network.
  bool received_fin_ = false;
  // True if the fin has been read out via readInto(). This signifies that the
  // RTCQuicStream is closed for reading.
  bool read_fin_ = false;
  // Pending waitForReadable promises.
  HeapVector<Member<PendingReadBufferedAmountPromise>>
      pending_read_buffered_amount_promises_;

  // Amount of bytes written but may not yet have been sent by the underlying
  // P2PQuicStream.
  // write() increases this number.
  // OnDataSent() decreases this number.
  uint32_t write_buffered_amount_ = 0;
  // True if finish() has been called.
  bool wrote_fin_ = false;
  // Pending waitForWriteBufferedAmountBelow Promises.
  HeapVector<Member<PendingWriteBufferedAmountPromise>>
      pending_write_buffered_amount_promises_;

  QuicStreamProxy* proxy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_QUIC_STREAM_H_
