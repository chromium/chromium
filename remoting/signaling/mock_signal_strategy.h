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

  MOCK_METHOD(void, Connect, (), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(State, GetState, (), (const, override));
  MOCK_METHOD(Error, GetError, (), (const, override));
  MOCK_METHOD(void, AddListener, (Listener * listener), (override));
  MOCK_METHOD(void, RemoveListener, (Listener * listener), (override));
  MOCK_METHOD(std::string, GetNextId, (), (override));
  MOCK_METHOD(bool, SendMessage, (JingleMessage && message), (override));
  MOCK_METHOD(bool, SendReply, (JingleMessageReply && message), (override));

  const SignalingAddress& GetLocalAddress() const override;

 private:
  SignalingAddress local_address_;
};

class MockFtlSignalStrategy : public FtlSignalStrategy {
 public:
  explicit MockFtlSignalStrategy(const SignalingAddress& address);
  ~MockFtlSignalStrategy() override;

  MOCK_METHOD(void, Connect, (), (override));
  MOCK_METHOD(void, Disconnect, (), (override));
  MOCK_METHOD(State, GetState, (), (const, override));
  MOCK_METHOD(Error, GetError, (), (const, override));
  MOCK_METHOD(void, AddListener, (Listener * listener), (override));
  MOCK_METHOD(void, RemoveListener, (Listener * listener), (override));
  MOCK_METHOD(std::string, GetNextId, (), (override));
  MOCK_METHOD(bool, SendMessage, (JingleMessage && message), (override));
  MOCK_METHOD(bool, SendReply, (JingleMessageReply && message), (override));
  MOCK_METHOD(bool,
              SendFtlMessage,
              (const SignalingAddress& destination_address,
               ftl::ChromotingMessage&& message),
              (override));
  MOCK_METHOD(void, AddFtlListener, (FtlListener * listener), (override));
  MOCK_METHOD(void, RemoveFtlListener, (FtlListener * listener), (override));

  const SignalingAddress& GetLocalAddress() const override;

 private:
  SignalingAddress local_address_;
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_MOCK_SIGNAL_STRATEGY_H_
