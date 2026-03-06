// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_

#include <memory>

#include "remoting/proto/ftl/v1/chromoting_message.pb.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace remoting {

class MockSignalStrategy : public SignalStrategy {
 public:
  explicit MockSignalStrategy(const SignalingAddress& address);
  ~MockSignalStrategy() override;

  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(GetState, State());
  MOCK_CONST_METHOD0(GetError, Error());
  MOCK_METHOD1(AddListener, void(Listener* listener));
  MOCK_METHOD1(RemoveListener, void(Listener* listener));
  MOCK_METHOD0(GetNextId, std::string());
  MOCK_METHOD1(SendMessage, bool(JingleMessage&& message));
  MOCK_METHOD1(SendReply, bool(JingleMessageReply&& message));

  const SignalingAddress& GetLocalAddress() const override;

 private:
  SignalingAddress local_address_;
};

class MockFtlSignalStrategy : public FtlSignalStrategy {
 public:
  explicit MockFtlSignalStrategy(const SignalingAddress& address);
  ~MockFtlSignalStrategy() override;

  MOCK_METHOD0(Connect, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(GetState, State());
  MOCK_CONST_METHOD0(GetError, Error());
  MOCK_METHOD1(AddListener, void(Listener* listener));
  MOCK_METHOD1(RemoveListener, void(Listener* listener));
  MOCK_METHOD0(GetNextId, std::string());
  MOCK_METHOD1(SendMessage, bool(JingleMessage&& message));
  MOCK_METHOD1(SendReply, bool(JingleMessageReply&& message));
  MOCK_METHOD2(SendFtlMessage,
               bool(const SignalingAddress& destination_address,
                    ftl::ChromotingMessage&& message));
  MOCK_METHOD1(AddFtlListener, void(FtlListener* listener));
  MOCK_METHOD1(RemoveFtlListener, void(FtlListener* listener));

  const SignalingAddress& GetLocalAddress() const override;

 private:
  SignalingAddress local_address_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_
