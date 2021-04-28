// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ReadableStream;
class WritableStream;

class MODULES_EXPORT DatagramDuplexStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Currently we delegate to the WebTransport object rather than store the
  // readable and writable separately.
  // TODO(ricea): Once the legacy getters are removed from WebTransport, store
  // the readable and writable in this object.
  explicit DatagramDuplexStream(WebTransport* web_transport)
      : web_transport_(web_transport) {}

  ReadableStream* readable() const {
    return web_transport_->datagramReadable();
  }

  WritableStream* writable() const {
    return web_transport_->datagramWritable();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(web_transport_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  const Member<WebTransport> web_transport_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_
