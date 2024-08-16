// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/api/packet_socket_factory.h"

namespace remoting::protocol {

class SessionOptionsProvider;

class ChromiumPacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  explicit ChromiumPacketSocketFactory(
      base::WeakPtr<SessionOptionsProvider> session_options_provider);

  ChromiumPacketSocketFactory(const ChromiumPacketSocketFactory&) = delete;
  ChromiumPacketSocketFactory& operator=(const ChromiumPacketSocketFactory&) =
      delete;

  ~ChromiumPacketSocketFactory() override;

  // rtc::PacketSocketFactory implementation.
  rtc::AsyncPacketSocket* CreateUdpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  rtc::AsyncListenSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::PacketSocketTcpOptions& opts) override;
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver()
      override;

 private:
  base::WeakPtr<SessionOptionsProvider> session_options_provider_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
