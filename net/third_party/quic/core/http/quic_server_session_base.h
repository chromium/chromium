// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A server specific QuicSession subclass.

#ifndef NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SERVER_SESSION_BASE_H_
#define NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SERVER_SESSION_BASE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "net/third_party/quic/core/crypto/quic_compressed_certs_cache.h"
#include "net/third_party/quic/core/http/quic_spdy_session.h"
#include "net/third_party/quic/core/quic_crypto_server_stream.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"

namespace quic {

class QuicConfig;
class QuicConnection;
class QuicCryptoServerConfig;

namespace test {
class QuicServerSessionBasePeer;
class QuicSimpleServerSessionPeer;
}  // namespace test

class QUIC_EXPORT_PRIVATE QuicServerSessionBase : public QuicSpdySession {
 public:
  // Does not take ownership of |connection|. |crypto_config| must outlive the
  // session. |helper| must outlive any created crypto streams.
  QuicServerSessionBase(const QuicConfig& config,
                        const ParsedQuicVersionVector& supported_versions,
                        QuicConnection* connection,
                        QuicSession::Visitor* visitor,
                        QuicCryptoServerStream::Helper* helper,
                        const QuicCryptoServerConfig* crypto_config,
                        QuicCompressedCertsCache* compressed_certs_cache);
  QuicServerSessionBase(const QuicServerSessionBase&) = delete;
  QuicServerSessionBase& operator=(const QuicServerSessionBase&) = delete;

  // Override the base class to cancel any ongoing asychronous crypto.
  void OnConnectionClosed(QuicErrorCode error,
                          const QuicString& error_details,
                          ConnectionCloseSource source) override;

  // Sends a server config update to the client, containing new bandwidth
  // estimate.
  void OnCongestionWindowChange(QuicTime now) override;

  ~QuicServerSessionBase() override;

  void Initialize() override;

  const QuicCryptoServerStreamBase* crypto_stream() const {
    return crypto_stream_.get();
  }

  // Override base class to process bandwidth related config received from
  // client.
  void OnConfigNegotiated() override;

  void set_serving_region(const QuicString& serving_region) {
    serving_region_ = serving_region;
  }

 protected:
  // QuicSession methods(override them with return type of QuicSpdyStream*):
  QuicCryptoServerStreamBase* GetMutableCryptoStream() override;

  const QuicCryptoServerStreamBase* GetCryptoStream() const override;

  // If an outgoing stream can be created, return true.
  // Return false when connection is closed or forward secure encryption hasn't
  // established yet or number of server initiated streams already reaches the
  // upper limit.
  bool ShouldCreateOutgoingStream() override;

  // If we should create an incoming stream, returns true. Otherwise
  // does error handling, including communicating the error to the client and
  // possibly closing the connection, and returns false.
  bool ShouldCreateIncomingStream(QuicStreamId id) override;

  virtual QuicCryptoServerStreamBase* CreateQuicCryptoServerStream(
      const QuicCryptoServerConfig* crypto_config,
      QuicCompressedCertsCache* compressed_certs_cache) = 0;

  const QuicCryptoServerConfig* crypto_config() { return crypto_config_; }

  QuicCryptoServerStream::Helper* stream_helper() { return helper_; }

 private:
  friend class test::QuicServerSessionBasePeer;
  friend class test::QuicSimpleServerSessionPeer;

  const QuicCryptoServerConfig* crypto_config_;

  // The cache which contains most recently compressed certs.
  // Owned by QuicDispatcher.
  QuicCompressedCertsCache* compressed_certs_cache_;

  std::unique_ptr<QuicCryptoServerStreamBase> crypto_stream_;

  // Pointer to the helper used to create crypto server streams. Must outlive
  // streams created via CreateQuicCryptoServerStream.
  QuicCryptoServerStream::Helper* helper_;

  // Whether bandwidth resumption is enabled for this connection.
  bool bandwidth_resumption_enabled_;

  // The most recent bandwidth estimate sent to the client.
  QuicBandwidth bandwidth_estimate_sent_to_client_;

  // Text describing server location. Sent to the client as part of the bandwith
  // estimate in the source-address token. Optional, can be left empty.
  QuicString serving_region_;

  // Time at which we send the last SCUP to the client.
  QuicTime last_scup_time_;

  // Number of packets sent to the peer, at the time we last sent a SCUP.
  int64_t last_scup_packet_number_;

  // Converts QuicBandwidth to an int32 bytes/second that can be
  // stored in CachedNetworkParameters.  TODO(jokulik): This function
  // should go away once we fix http://b//27897982
  int32_t BandwidthToCachedParameterBytesPerSecond(
      const QuicBandwidth& bandwidth);
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_HTTP_QUIC_SERVER_SESSION_BASE_H_
