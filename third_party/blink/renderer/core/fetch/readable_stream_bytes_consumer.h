// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_READABLE_STREAM_BYTES_CONSUMER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_READABLE_STREAM_BYTES_CONSUMER_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ScriptState;

// This class is a BytesConsumer pulling bytes from ReadableStream
// implemented with V8 Extras.
// The stream will be immediately locked by the consumer and will never be
// released.
class CORE_EXPORT ReadableStreamBytesConsumer final : public BytesConsumer {
  USING_PRE_FINALIZER(ReadableStreamBytesConsumer, Dispose);

 public:
  ReadableStreamBytesConsumer(ScriptState*, ScriptValue stream_reader);
  ~ReadableStreamBytesConsumer() override;

  Result BeginRead(const char** buffer, size_t* available) override;
  Result EndRead(size_t read_size) override;
  void SetClient(BytesConsumer::Client*) override;
  void ClearClient() override;

  void Cancel() override;
  PublicState GetPublicState() const override;
  Error GetError() const override;
  String DebugName() const override { return "ReadableStreamBytesConsumer"; }

  void Trace(blink::Visitor*) override;

 private:
  class OnFulfilled;
  class OnRejected;

  void Dispose();
  void OnRead(DOMUint8Array*);
  void OnReadDone();
  void OnRejected();
  void Notify();

  TraceWrapperV8Reference<v8::Value> reader_;
  Member<ScriptState> script_state_;
  Member<BytesConsumer::Client> client_;
  Member<DOMUint8Array> pending_buffer_;
  size_t pending_offset_ = 0;
  PublicState state_ = PublicState::kReadableOrWaiting;
  bool is_reading_ = false;
  DISALLOW_COPY_AND_ASSIGN(ReadableStreamBytesConsumer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_READABLE_STREAM_BYTES_CONSUMER_H_
