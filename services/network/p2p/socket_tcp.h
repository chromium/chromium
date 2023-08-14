// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_P2P_SOCKET_TCP_H_
#define SERVICES_NETWORK_P2P_SOCKET_TCP_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/ip_endpoint.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/p2p/socket.h"
#include "services/network/public/cpp/p2p_socket_type.h"

namespace net {
class DrainableIOBuffer;
class GrowableIOBuffer;
class StreamSocket;
}  // namespace net

namespace network {
class ProxyResolvingClientSocketFactory;

class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketTcpBase : public P2PSocket {
 public:
  P2PSocketTcpBase(
      Delegate* delegate,
      mojo::PendingRemote<mojom::P2PSocketClient> client,
      mojo::PendingReceiver<mojom::P2PSocket> socket,
      P2PSocketType type,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);

  P2PSocketTcpBase(const P2PSocketTcpBase&) = delete;
  P2PSocketTcpBase& operator=(const P2PSocketTcpBase&) = delete;

  ~P2PSocketTcpBase() override;

  void InitAccepted(const net::IPEndPoint& remote_address,
                    std::unique_ptr<net::StreamSocket> socket);

  // P2PSocket overrides.
  void Init(
      const net::IPEndPoint& local_address,
      uint16_t min_port,
      uint16_t max_port,
      const P2PHostAndIPEndPoint& remote_address,
      const net::NetworkAnonymizationKey& network_anonymization_key) override;

  // mojom::P2PSocket implementation:
  void Send(base::span<const uint8_t> data,
            const P2PPacketInfo& packet_info) override;
  void SendBatch(std::vector<mojom::P2PSendPacketPtr> packet_batch) override;
  void SetOption(P2PSocketOption option, int32_t value) override;

 protected:
  struct SendBuffer {
    SendBuffer();
    SendBuffer(int32_t packet_id, scoped_refptr<net::DrainableIOBuffer> buffer);
    SendBuffer(const SendBuffer& rhs);
    ~SendBuffer();

    int32_t rtc_packet_id;
    scoped_refptr<net::DrainableIOBuffer> buffer;
  };

  // Derived classes will provide the implementation.
  virtual bool ProcessInput(base::span<const uint8_t> input,
                            size_t* bytes_consumed) = 0;
  virtual void DoSend(const net::IPEndPoint& to,
                      base::span<const uint8_t> data,
                      const rtc::PacketOptions& options) = 0;

  void WriteOrQueue(SendBuffer& send_buffer);
  [[nodiscard]] bool OnPacket(base::span<const uint8_t> data);

  bool SendPacket(base::span<const uint8_t> data,
                  const P2PPacketInfo& packet_info);

 private:
  friend class P2PSocketTcpTestBase;
  friend class P2PSocketTcpServerTest;

  void DoRead();
  void DoWrite();

  // Return |false| in case of an error. The socket is destroyed in that case,
  // so the caller should not use |this|.
  [[nodiscard]] bool HandleReadResult(int result);
  [[nodiscard]] bool HandleWriteResult(int result);

  // Callbacks for Connect(), Read() and Write().
  void OnConnected(int result);
  void OnRead(int result);
  void OnWritten(int result);

  // Helper method to send socket create message and start read.
  void OnOpen();
  bool DoSendSocketCreateMsg();

  P2PHostAndIPEndPoint remote_address_;

  std::unique_ptr<net::StreamSocket> socket_;
  scoped_refptr<net::GrowableIOBuffer> read_buffer_;
  base::queue<SendBuffer> write_queue_;
  SendBuffer write_buffer_;

  bool write_pending_ = false;

  bool connected_ = false;
  const P2PSocketType type_;
  const net::NetworkTrafficAnnotationTag traffic_annotation_;
  raw_ptr<ProxyResolvingClientSocketFactory> proxy_resolving_socket_factory_;
};

class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketTcp : public P2PSocketTcpBase {
 public:
  P2PSocketTcp(
      Delegate* delegate,
      mojo::PendingRemote<mojom::P2PSocketClient> client,
      mojo::PendingReceiver<mojom::P2PSocket> socket,
      P2PSocketType type,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);

  P2PSocketTcp(const P2PSocketTcp&) = delete;
  P2PSocketTcp& operator=(const P2PSocketTcp&) = delete;

  ~P2PSocketTcp() override;

 protected:
  bool ProcessInput(base::span<const uint8_t> input,
                    size_t* bytes_consumed) override;
  void DoSend(const net::IPEndPoint& to,
              base::span<const uint8_t> data,
              const rtc::PacketOptions& options) override;
};

// P2PSocketStunTcp class provides the framing of STUN messages when used
// with TURN. These messages will not have length at front of the packet and
// are padded to multiple of 4 bytes.
// Formatting of messages is defined in RFC5766.
class COMPONENT_EXPORT(NETWORK_SERVICE) P2PSocketStunTcp
    : public P2PSocketTcpBase {
 public:
  P2PSocketStunTcp(
      Delegate* delegate,
      mojo::PendingRemote<mojom::P2PSocketClient> client,
      mojo::PendingReceiver<mojom::P2PSocket> socket,
      P2PSocketType type,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      ProxyResolvingClientSocketFactory* proxy_resolving_socket_factory);

  P2PSocketStunTcp(const P2PSocketStunTcp&) = delete;
  P2PSocketStunTcp& operator=(const P2PSocketStunTcp&) = delete;

  ~P2PSocketStunTcp() override;

 protected:
  bool ProcessInput(base::span<const uint8_t> input,
                    size_t* bytes_consumed) override;
  void DoSend(const net::IPEndPoint& to,
              base::span<const uint8_t> data,
              const rtc::PacketOptions& options) override;

 private:
  int GetExpectedPacketSize(base::span<const uint8_t> data, int* pad_bytes);
};

}  // namespace network

#endif  // SERVICES_NETWORK_P2P_SOCKET_TCP_H_
