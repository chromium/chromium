// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"

namespace blink {

// A simple config object for creating a P2PQuicTransport. Its constructor
// guarantees that the required configuration for creating a P2PQuicTransport
// are part of the P2PQuicTransportConfig.
struct P2PQuicTransportConfig final {
  // This object is only moveable.
  explicit P2PQuicTransportConfig(
      quic::Perspective perspective,
      const Vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates_in,
      uint32_t stream_delegate_read_buffer_size_in,
      uint32_t stream_write_buffer_size_in)
      : perspective(perspective),
        certificates(certificates_in),
        stream_delegate_read_buffer_size(stream_delegate_read_buffer_size_in),
        stream_write_buffer_size(stream_write_buffer_size_in) {
    DCHECK_GT(stream_delegate_read_buffer_size, 0u);
    DCHECK_GT(stream_write_buffer_size, 0u);
  }

  // Client or server.
  quic::Perspective perspective;
  // The certificates are owned by the P2PQuicTransport. These come from
  // blink::RTCCertificates: https://www.w3.org/TR/webrtc/#dom-rtccertificate
  // This can be empty if pre shared keys are being used to establish a
  // connection.
  const Vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
  // The amount that the delegate can store in its read buffer. This is a
  // mandatory field that must be set to ensure that the
  // P2PQuicStream::Delegate will not give the delegate more data than it can
  // store.
  const uint32_t stream_delegate_read_buffer_size;
  // The amount that the P2PQuicStream will allow to buffer. This is a mandatory
  // field that must be set to ensure that the client of the P2PQuicStream does
  // not write more data than can be buffered.
  const uint32_t stream_write_buffer_size;
};

// For creating a P2PQuicTransport. This factory should be injected into
// whichever object plans to own and use a P2PQuicTransport. The
// P2PQuicTransportFactory needs to outlive the P2PQuicTransport it creates.
//
// This object should be run entirely on the webrtc worker thread.
class P2PQuicTransportFactory {
 public:
  virtual ~P2PQuicTransportFactory() = default;

  // Creates the P2PQuicTransport. This should be called on the same
  // thread that the P2PQuicTransport will be used on.
  // |delegate| receives callbacks from the P2PQuicTransport on the same thread.
  //     It must outlive the P2PQuicTransport.
  // |packet_transport| is used to send and receive UDP packets. It must outlive
  //     the P2PQuicTransport.
  virtual std::unique_ptr<P2PQuicTransport> CreateQuicTransport(
      P2PQuicTransport::Delegate* delegate,
      P2PQuicPacketTransport* packet_transport,
      const P2PQuicTransportConfig& config) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_
