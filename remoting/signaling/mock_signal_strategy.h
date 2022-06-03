// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_

#include <memory>

#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  MockSignalStrategy(const SignalingAddress& address);
  ~MockSignalStrategy() override;

  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(GetState, State());
  MOCK_CONST_METHOD0(GetError, Error());
  MOCK_METHOD1(AddListener, void(Listener* listener));
  MOCK_METHOD1(RemoveListener, void(Listener* listener));
  MOCK_METHOD0(GetNextId, std::string());
  MOCK_METHOD2(SendMessage,
               bool(const SignalingAddress& destination_address,
                    const ftl::ChromotingMessage& message));

  // GMock currently doesn't support move-only arguments, so we have
  // to use this hack here.
  MOCK_METHOD1(SendStanzaPtr, bool(jingle_xmpp::XmlElement* stanza));
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override {
    return SendStanzaPtr(stanza.release());
  }

  const SignalingAddress& GetLocalAddress() const override;

 private:
  SignalingAddress local_address_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_
