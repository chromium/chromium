// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A toy server, which listens on a specified address for QUIC traffic and
// handles incoming responses.
//
// Note that this server is intended to verify correctness of the client and is
// in no way expected to be performant.

#ifndef NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SERVER_H_
#define NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SERVER_H_

#include <memory>

#include "base/macros.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/third_party/quic/core/crypto/quic_crypto_server_config.h"
#include "net/third_party/quic/core/quic_config.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packet_writer.h"
#include "net/third_party/quic/core/quic_version_manager.h"
#include "net/third_party/quic/platform/api/quic_socket_address.h"
#include "net/third_party/quic/tools/quic_simple_server_backend.h"
#include "net/tools/epoll_server/epoll_server.h"

namespace quic {

namespace test {
class QuicServerPeer;
}  // namespace test

class QuicDispatcher;
class QuicPacketReader;

class QuicServer : public net::EpollCallbackInterface {
 public:
  QuicServer(std::unique_ptr<ProofSource> proof_source,
             QuicSimpleServerBackend* quic_simple_server_backend);
  QuicServer(std::unique_ptr<ProofSource> proof_source,
             const QuicConfig& config,
             const QuicCryptoServerConfig::ConfigOptions& server_config_options,
             const ParsedQuicVersionVector& supported_versions,
             QuicSimpleServerBackend* quic_simple_server_backend);
  QuicServer(const QuicServer&) = delete;
  QuicServer& operator=(const QuicServer&) = delete;

  ~QuicServer() override;

  QuicString Name() const override { return "QuicServer"; }

  // Start listening on the specified address.
  bool CreateUDPSocketAndListen(const QuicSocketAddress& address);

  // Wait up to 50ms, and handle any events which occur.
  void WaitForEvents();

  void Start();
  void Run();

  // Server deletion is imminent.  Start cleaning up the epoll server.
  virtual void Shutdown();

  // From net::EpollCallbackInterface
  void OnRegistration(net::EpollServer* eps, int fd, int event_mask) override {}
  void OnModification(int fd, int event_mask) override {}
  void OnEvent(int fd, net::EpollEvent* event) override;
  void OnUnregistration(int fd, bool replaced) override {}

  void OnShutdown(net::EpollServer* eps, int fd) override {}

  void SetChloMultiplier(size_t multiplier) {
    crypto_config_.set_chlo_multiplier(multiplier);
  }

  void SetPreSharedKey(QuicStringPiece key) {
    crypto_config_.set_pre_shared_key(key);
  }

  bool overflow_supported() { return overflow_supported_; }

  QuicPacketCount packets_dropped() { return packets_dropped_; }

  int port() { return port_; }

 protected:
  virtual QuicPacketWriter* CreateWriter(int fd);

  virtual QuicDispatcher* CreateQuicDispatcher();

  const QuicConfig& config() const { return config_; }
  const QuicCryptoServerConfig& crypto_config() const { return crypto_config_; }
  net::EpollServer* epoll_server() { return &epoll_server_; }

  QuicDispatcher* dispatcher() { return dispatcher_.get(); }

  QuicVersionManager* version_manager() { return &version_manager_; }

  QuicSimpleServerBackend* server_backend() {
    return quic_simple_server_backend_;
  }

  void set_silent_close(bool value) { silent_close_ = value; }

 private:
  friend class quic::test::QuicServerPeer;

  // Initialize the internal state of the server.
  void Initialize();

  // Accepts data from the framer and demuxes clients to sessions.
  std::unique_ptr<QuicDispatcher> dispatcher_;
  // Frames incoming packets and hands them to the dispatcher.
  net::EpollServer epoll_server_;

  // The port the server is listening on.
  int port_;

  // Listening connection.  Also used for outbound client communication.
  int fd_;

  // If overflow_supported_ is true this will be the number of packets dropped
  // during the lifetime of the server.  This may overflow if enough packets
  // are dropped.
  QuicPacketCount packets_dropped_;

  // True if the kernel supports SO_RXQ_OVFL, the number of packets dropped
  // because the socket would otherwise overflow.
  bool overflow_supported_;

  // If true, do not call Shutdown on the dispatcher.  Connections will close
  // without sending a final connection close.
  bool silent_close_;

  // config_ contains non-crypto parameters that are negotiated in the crypto
  // handshake.
  QuicConfig config_;
  // crypto_config_ contains crypto parameters for the handshake.
  QuicCryptoServerConfig crypto_config_;
  // crypto_config_options_ contains crypto parameters for the handshake.
  QuicCryptoServerConfig::ConfigOptions crypto_config_options_;

  // Used to generate current supported versions.
  QuicVersionManager version_manager_;

  // Point to a QuicPacketReader object on the heap. The reader allocates more
  // space than allowed on the stack.
  std::unique_ptr<QuicPacketReader> packet_reader_;

  QuicSimpleServerBackend* quic_simple_server_backend_;  // unowned.

  base::WeakPtrFactory<QuicServer> weak_factory_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_TOOLS_QUIC_SERVER_H_
