// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_packet_transport.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_transport.h"
#include "third_party/webrtc/rtc_base/rtccertificate.h"
#include "third_party/webrtc/rtc_base/scoped_ref_ptr.h"

namespace blink {

// A simple config object for creating a P2PQuicTransport. It's constructor
// guarantees that the required objects for creating a P2PQuicTransport are part
// of the P2PQuicTransportConfig.
struct P2PQuicTransportConfig final {
  // This object is only moveable.
  explicit P2PQuicTransportConfig(
      P2PQuicTransport::Delegate* const delegate_in,
      P2PQuicPacketTransport* const packet_transport_in,
      const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>>
          certificates_in)
      : packet_transport(packet_transport_in),
        certificates(certificates_in),
        delegate(delegate_in) {
    DCHECK_GT(certificates.size(), 0u);
    DCHECK(packet_transport);
    DCHECK(delegate);
  }
  P2PQuicTransportConfig(const P2PQuicTransportConfig&) = delete;
  P2PQuicTransportConfig& operator=(const P2PQuicTransportConfig&) = delete;
  P2PQuicTransportConfig(P2PQuicTransportConfig&&) = default;
  P2PQuicTransportConfig& operator=(P2PQuicTransportConfig&&) = delete;
  ~P2PQuicTransportConfig() = default;

  //  The standard case is an ICE transport. It's lifetime will be managed by
  //  the ICE transport objects and outlive the P2PQuicTransport.
  P2PQuicPacketTransport* const packet_transport;
  bool is_server = true;
  // The certificates are owned by the P2PQuicTransport. These come from
  // blink::RTCCertificates: https://www.w3.org/TR/webrtc/#dom-rtccertificate
  const std::vector<rtc::scoped_refptr<rtc::RTCCertificate>> certificates;
  // Mandatory for creating a P2PQuicTransport and must outlive
  // the P2PQuicTransport. In the standard case the |delegate_| will be
  // the object that owns the P2PQuicTransport.
  P2PQuicTransport::Delegate* const delegate;
  // When set to true the P2PQuicTransport will immediately be able
  // to listen and respond to a crypto handshake upon construction.
  // This will NOT start a handshake.
  bool can_respond_to_crypto_handshake = true;
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
  virtual std::unique_ptr<P2PQuicTransport> CreateQuicTransport(
      P2PQuicTransportConfig config) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_TRANSPORT_FACTORY_H_
