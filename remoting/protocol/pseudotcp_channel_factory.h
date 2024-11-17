// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_
#define REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting::protocol {

class DatagramChannelFactory;
class P2PDatagramSocket;

// StreamChannelFactory that creates PseudoTCP-based stream channels that run on
// top of datagram channels created using specified |datagram_channel_factory|.
class PseudoTcpChannelFactory : public StreamChannelFactory {
 public:
  // |datagram_channel_factory| must outlive this object.
  explicit PseudoTcpChannelFactory(
      DatagramChannelFactory* datagram_channel_factory);

  PseudoTcpChannelFactory(const PseudoTcpChannelFactory&) = delete;
  PseudoTcpChannelFactory& operator=(const PseudoTcpChannelFactory&) = delete;

  ~PseudoTcpChannelFactory() override;

  // StreamChannelFactory interface.
  void CreateChannel(const std::string& name,
                     ChannelCreatedCallback callback) override;
  void CancelChannelCreation(const std::string& name) override;

 private:
  typedef std::map<std::string, raw_ptr<P2PStreamSocket, CtnExperimental>>
      PendingSocketsMap;

  void OnDatagramChannelCreated(const std::string& name,
                                ChannelCreatedCallback callback,
                                std::unique_ptr<P2PDatagramSocket> socket);
  void OnPseudoTcpConnected(const std::string& name,
                            ChannelCreatedCallback callback,
                            int result);

  raw_ptr<DatagramChannelFactory> datagram_channel_factory_;

  PendingSocketsMap pending_sockets_;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_PSEUDOTCP_CHANNEL_FACTORY_H_
