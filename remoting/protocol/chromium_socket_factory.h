// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
#define REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_

#include <stdint.h>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "third_party/webrtc/api/packet_socket_factory.h"

namespace remoting {
namespace protocol {

class ChromiumPacketSocketFactory : public rtc::PacketSocketFactory {
 public:
  explicit ChromiumPacketSocketFactory();
  ~ChromiumPacketSocketFactory() override;

  rtc::AsyncPacketSocket* CreateUdpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port) override;
  rtc::AsyncPacketSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override;
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::ProxyInfo& proxy_info,
      const std::string& user_agent,
      const rtc::PacketSocketTcpOptions& opts) override;
  rtc::AsyncResolverInterface* CreateAsyncResolver() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromiumPacketSocketFactory);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CHROMIUM_SOCKET_FACTORY_H_
