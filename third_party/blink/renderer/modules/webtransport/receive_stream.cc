// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webtransport/receive_stream.h"

#include <utility>

#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

namespace {

void ForgetStream(WebTransport* transport,
                  uint32_t stream_id,
                  std::optional<uint8_t> stop_sending_code) {
  if (stop_sending_code) {
    transport->StopSending(stream_id, *stop_sending_code);
  }
  transport->ForgetIncomingStream(stream_id);
}

}  // namespace

ReceiveStream::ReceiveStream(ScriptState* script_state,
                             WebTransport* web_transport,
                             uint32_t stream_id,
                             mojo::ScopedDataPipeConsumerHandle handle)
    : incoming_stream_(MakeGarbageCollected<IncomingStream>(
          script_state,
          WTF::BindOnce(ForgetStream,
                        WrapWeakPersistent(web_transport),
                        stream_id),
          std::move(handle))) {}

void ReceiveStream::Trace(Visitor* visitor) const {
  visitor->Trace(incoming_stream_);
  ReadableStream::Trace(visitor);
}

}  // namespace blink
