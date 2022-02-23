// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/direct_sockets/udp_readable_stream_wrapper.h"

#include "base/callback_forward.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_udp_message.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// UDPReadableStreamWrapper::UnderlyingSource declaration

class UDPReadableStreamWrapper::UnderlyingSource final
    : public UnderlyingSourceBase {
 public:
  UnderlyingSource(ScriptState* script_state, UDPReadableStreamWrapper* stream)
      : UnderlyingSourceBase(script_state),
        udp_readable_stream_wrapper_(stream) {}

  // UnderlyingSourceBase overrides.
  ScriptPromise Start(ScriptState* script_state) override;
  ScriptPromise pull(ScriptState* script_state) override;
  // Clears the queue and forwards the request to
  // UDPReadableStreamWrapper::CloseInternal().
  // Called from the reader side.
  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override;

  // Clears the queue and forwards the request to
  // UDPReadableStreamWrapper::CloseInternal().
  // Called from the socket side.
  void Close();

  void Trace(Visitor* visitor) const override {
    visitor->Trace(udp_readable_stream_wrapper_);
    visitor->Trace(queue_);
    UnderlyingSourceBase::Trace(visitor);
  }

  // Implementation of UDPReadableStreamWrapper::AcceptDatagram.
  void AcceptDatagram(base::span<const uint8_t> data,
                      const net::IPEndPoint& src_addr);

 private:
  struct QueueEntry : GarbageCollected<QueueEntry> {
    QueueEntry(UDPMessage* message, base::TimeTicks received_at)
        : message(message), received_at(std::move(received_at)) {}

    void Trace(Visitor* visitor) const { visitor->Trace(message); }

    const Member<UDPMessage> message;
    const base::TimeTicks received_at;
  };

  // Performs local cleanup: clears the queue and sets closed_ to true.
  void Cleanup();

  // Pops stale datagrams from the front of the queue (that have been
  // received more than kQueueDatagramAge time ago).
  // ensure_free_slot guarantees the following condition after the call:
  // |queue_| <= kQueueSizeLimit - 1.
  void DiscardStaleDatagrams(const base::TimeTicks& now, bool ensure_free_slot);

  constexpr static const uint32_t kQueueSizeLimit = 10;
  constexpr static const base::TimeDelta kQueueDatagramAgeLimit =
      base::Seconds(60);

  const Member<UDPReadableStreamWrapper> udp_readable_stream_wrapper_;
  HeapDeque<Member<const QueueEntry>> queue_;
  bool pending_read_ = false;
};

// UDPReadableStreamWrapper::UnderlyingSource definition

ScriptPromise UDPReadableStreamWrapper::UnderlyingSource::Start(
    ScriptState* script_state) {
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UDPReadableStreamWrapper::UnderlyingSource::pull(
    ScriptState* script_state) {
  if (pending_read_) {
    DCHECK(queue_.empty());
    return ScriptPromise::CastUndefined(script_state);
  }

  DiscardStaleDatagrams(base::TimeTicks::Now(),
                        /*ensure_free_slot=*/false);

  if (!queue_.empty()) {
    const QueueEntry* entry = queue_.TakeFirst();
    Controller()->Enqueue(entry->message);
  } else {
    pending_read_ = true;
  }

  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise UDPReadableStreamWrapper::UnderlyingSource::Cancel(
    ScriptState* script_state,
    ScriptValue reason) {
  Cleanup();
  udp_readable_stream_wrapper_->CloseInternal();
  return ScriptPromise::CastUndefined(script_state);
}

void UDPReadableStreamWrapper::UnderlyingSource::Close() {
  Controller()->Close();
  Cleanup();
  udp_readable_stream_wrapper_->CloseInternal();
}

void UDPReadableStreamWrapper::UnderlyingSource::Cleanup() {
  queue_.clear();
}

void UDPReadableStreamWrapper::UnderlyingSource::AcceptDatagram(
    base::span<const uint8_t> data,
    const net::IPEndPoint& src_addr) {
  // Copies |data|.
  auto* buffer = DOMUint8Array::Create(data.data(), data.size_bytes());

  auto* message = UDPMessage::Create();

  message->setData(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
      NotShared<DOMUint8Array>(buffer)));
  message->setRemoteAddress(String{src_addr.ToStringWithoutPort()});
  message->setRemotePort(src_addr.port());

  if (pending_read_) {
    pending_read_ = false;
    Controller()->Enqueue(message);
    return;
  }

  const auto now = base::TimeTicks::Now();

  DiscardStaleDatagrams(now, /*ensure_free_slot=*/
                        true);
  queue_.push_back(
      MakeGarbageCollected<const QueueEntry>(message, std::move(now)));
}

void UDPReadableStreamWrapper::UnderlyingSource::DiscardStaleDatagrams(
    const base::TimeTicks& now,
    bool ensure_free_slot) {
  while (!queue_.empty() &&
         (queue_.size() > kQueueSizeLimit ||
          now - queue_.front()->received_at > kQueueDatagramAgeLimit)) {
    queue_.pop_front();
  }

  if (ensure_free_slot && !queue_.empty() && queue_.size() == kQueueSizeLimit) {
    queue_.pop_front();
  }
}

// UDPReadableStreamWrapper definition

UDPReadableStreamWrapper::UDPReadableStreamWrapper(
    ScriptState* script_state,
    const Member<UDPSocketMojoRemote> udp_socket,
    base::OnceClosure on_close)
    : script_state_(script_state),
      udp_socket_(udp_socket),
      on_close_(std::move(on_close)) {
  if (udp_socket_->get().is_bound()) {
    udp_socket_->get()->ReceiveMore(kNumAdditionalDatagrams);
  }
  ScriptState::Scope scope(script_state);
  source_ = MakeGarbageCollected<UDPReadableStreamWrapper::UnderlyingSource>(
      script_state_, this);
  readable_ = ReadableStream::CreateWithCountQueueingStrategy(script_state_,
                                                              source_, 1);
}

UDPReadableStreamWrapper::~UDPReadableStreamWrapper() = default;

void UDPReadableStreamWrapper::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(udp_socket_);
  visitor->Trace(source_);
  visitor->Trace(readable_);
}

void UDPReadableStreamWrapper::Close() {
  source_->Close();
}

void UDPReadableStreamWrapper::CloseInternal() {
  // Only called once.
  std::move(on_close_).Run();
}

void UDPReadableStreamWrapper::AcceptDatagram(base::span<const uint8_t> data,
                                              const net::IPEndPoint& src_addr) {
  source_->AcceptDatagram(data, src_addr);
  if (udp_socket_->get().is_bound()) {
    udp_socket_->get()->ReceiveMore(kNumAdditionalDatagrams);
  }
}

}  // namespace blink
