// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_

#include <stdint.h>

#include <optional>

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webtransport/web_transport.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class ReadableStream;
class WritableStream;

inline constexpr uint32_t kDefaultIncomingMaxBufferedDatagrams = 1;

class MODULES_EXPORT DatagramDuplexStream : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Currently we delegate to the WebTransport object rather than store the
  // readable and writable separately.
  // TODO(ricea): Once the legacy getters are removed from WebTransport, store
  // the readable and writable in this object.
  explicit DatagramDuplexStream(
      WebTransport* web_transport,
      uint32_t initial_outgoing_max_buffered_datagrams)
      : web_transport_(web_transport),
        outgoing_max_buffered_datagrams_(
            initial_outgoing_max_buffered_datagrams) {}

  ReadableStream* readable() const {
    return web_transport_->datagramReadable();
  }

  WritableStream* writable() const {
    return web_transport_->datagramWritable();
  }

  uint32_t maxDatagramSize() const { return max_datagram_size_; }
  std::optional<double> incomingMaxAge() const { return incoming_max_age_; }
  void setIncomingMaxAge(std::optional<double> max_age);

  std::optional<double> outgoingMaxAge() const { return outgoing_max_age_; }
  void setOutgoingMaxAge(std::optional<double> max_age);

  // Spec-renamed attributes use Web IDL unsigned long (uint32_t).
  uint32_t incomingMaxBufferedDatagrams() const {
    return incoming_max_buffered_datagrams_;
  }
  void setIncomingMaxBufferedDatagrams(uint32_t value) {
    incoming_max_buffered_datagrams_ = value;
  }

  uint32_t outgoingMaxBufferedDatagrams() const {
    return outgoing_max_buffered_datagrams_;
  }
  void setOutgoingMaxBufferedDatagrams(uint32_t value) {
    outgoing_max_buffered_datagrams_ = value;
  }

  // Deprecated aliases preserve the old Web IDL long (int32_t) surface.
  // Negative setter values are ignored to match previous behavior.
  // Values larger than INT32_MAX are clamped for the deprecated getters because
  // the old IDL type cannot represent the full uint32_t range.
  int32_t incomingHighWaterMark() const {
    return base::saturated_cast<int32_t>(incoming_max_buffered_datagrams_);
  }
  void setIncomingHighWaterMark(int32_t value);

  int32_t outgoingHighWaterMark() const {
    return base::saturated_cast<int32_t>(outgoing_max_buffered_datagrams_);
  }
  void setOutgoingHighWaterMark(int32_t value);

  void Trace(Visitor* visitor) const override {
    visitor->Trace(web_transport_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  const Member<WebTransport> web_transport_;

  // TODO(yhirano): Update this variable when the session is established.
  // We need to choose an initial value without knowing the actual network
  // condition, so let's choose a conservative value. This will be update when
  // the path migration happens.
  uint32_t max_datagram_size_ = 1024;
  std::optional<double> incoming_max_age_;
  std::optional<double> outgoing_max_age_;
  uint32_t incoming_max_buffered_datagrams_ =
      kDefaultIncomingMaxBufferedDatagrams;
  uint32_t outgoing_max_buffered_datagrams_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBTRANSPORT_DATAGRAM_DUPLEX_STREAM_H_
