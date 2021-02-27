// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_
#define REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_

#include <string>

#include "base/macros.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

// FtlEchoMessageListener listens for, and responds to, echo messages which have
// been sent to this endpoint via the signaling channel.  The most common usage
// is determine whether this endpoint is reachable without requiring the
// construction of a well-formed XMPP stanza and won't interfere with the
// standard signaling process if sent mid-connection negotiation.
class FtlEchoMessageListener : public SignalStrategy::Listener {
 public:
  // |signal_strategy| is expected to outlive this object.
  FtlEchoMessageListener(std::string host_owner,
                         SignalStrategy* signal_strategy);
  ~FtlEchoMessageListener() override;

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;
  bool OnSignalStrategyIncomingMessage(
      const ftl::Id& sender_id,
      const std::string& sender_registration_id,
      const ftl::ChromotingMessage& message) override;

 private:
  std::string host_owner_;
  SignalStrategy* signal_strategy_;
  DISALLOW_COPY_AND_ASSIGN(FtlEchoMessageListener);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FTL_ECHO_MESSAGE_LISTENER_H_
