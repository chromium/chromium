// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_ICE_TRANSPORT_H_
#define REMOTING_PROTOCOL_ICE_TRANSPORT_H_

#include <list>
#include <map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/protocol/datagram_channel_factory.h"
#include "remoting/protocol/ice_transport_channel.h"
#include "remoting/protocol/jingle_messages.h"
#include "remoting/protocol/transport.h"

namespace remoting {
namespace protocol {

class ChannelMultiplexer;
class PseudoTcpChannelFactory;
class SecureChannelFactory;
class MessageChannelFactory;

class IceTransport : public Transport,
                     public IceTransportChannel::Delegate,
                     public DatagramChannelFactory {
 public:
  class EventHandler {
   public:
    // Called when transport route changes.
    virtual void OnIceTransportRouteChange(const std::string& channel_name,
                                           const TransportRoute& route) = 0;

    // Called when there is an error connecting the session.
    virtual void OnIceTransportError(ErrorCode error) = 0;
  };

  // |transport_context| must outlive the session.
  IceTransport(scoped_refptr<TransportContext> transport_context,
               EventHandler* event_handler);
  ~IceTransport() override;

  MessageChannelFactory* GetChannelFactory();
  MessageChannelFactory* GetMultiplexedChannelFactory();

  // Transport interface.
  void Start(Authenticator* authenticator,
             SendTransportInfoCallback send_transport_info_callback) override;
  bool ProcessTransportInfo(jingle_xmpp::XmlElement* transport_info) override;

 private:
  typedef std::map<std::string, IceTransportChannel*> ChannelsMap;

  // DatagramChannelFactory interface.
  void CreateChannel(const std::string& name,
                     const ChannelCreatedCallback& callback) override;
  void CancelChannelCreation(const std::string& name) override;

  // Passes transport info to a new |channel| in case it was received before the
  // channel was created.
  void AddPendingRemoteTransportInfo(IceTransportChannel* channel);

  // IceTransportChannel::Delegate interface.
  void OnChannelIceCredentials(IceTransportChannel* transport,
                               const std::string& ufrag,
                               const std::string& password) override;
  void OnChannelCandidate(IceTransportChannel* transport,
                          const cricket::Candidate& candidate) override;
  void OnChannelRouteChange(IceTransportChannel* transport,
                            const TransportRoute& route) override;
  void OnChannelFailed(IceTransportChannel* transport) override;
  void OnChannelDeleted(IceTransportChannel* transport) override;

  // Creates empty |pending_transport_info_message_| and schedules timer for
  // SentTransportInfo() to sent the message later.
  void EnsurePendingTransportInfoMessage();

  // Sends transport-info message with candidates from |pending_candidates_|.
  void SendTransportInfo();

  // Callback passed to StreamMessageChannelFactoryAdapter to handle read/write
  // errors on the data channels.
  void OnChannelError(int error);

  scoped_refptr<TransportContext> transport_context_;
  EventHandler* event_handler_;

  SendTransportInfoCallback send_transport_info_callback_;

  ChannelsMap channels_;
  std::unique_ptr<PseudoTcpChannelFactory> pseudotcp_channel_factory_;
  std::unique_ptr<SecureChannelFactory> secure_channel_factory_;
  std::unique_ptr<MessageChannelFactory> message_channel_factory_;

  std::unique_ptr<ChannelMultiplexer> channel_multiplexer_;
  std::unique_ptr<MessageChannelFactory> mux_channel_factory_;

  // Pending remote transport info received before the local channels were
  // created.
  std::list<IceTransportInfo::IceCredentials> pending_remote_ice_credentials_;
  std::list<IceTransportInfo::NamedCandidate> pending_remote_candidates_;

  std::unique_ptr<IceTransportInfo> pending_transport_info_message_;
  base::OneShotTimer transport_info_timer_;

  base::WeakPtrFactory<IceTransport> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IceTransport);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_ICE_TRANSPORT_H_

