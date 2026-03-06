// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FAKE_FTL_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_FAKE_FTL_SIGNAL_STRATEGY_H_

#include <string>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "remoting/signaling/ftl_signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

class FakeFtlSignalStrategy : public FtlSignalStrategy {
 public:
  explicit FakeFtlSignalStrategy(const SignalingAddress& address);

  FakeFtlSignalStrategy(const FakeFtlSignalStrategy&) = delete;
  FakeFtlSignalStrategy& operator=(const FakeFtlSignalStrategy&) = delete;

  ~FakeFtlSignalStrategy() override;

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  const SignalingAddress& GetLocalAddress() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendMessage(JingleMessage&& message) override;
  bool SendReply(JingleMessageReply&& message) override;
  std::string GetNextId() override;
  bool IsSignInError() const override;

  // FtlSignalStrategy interface.
  bool SendFtlMessage(const SignalingAddress& destination_address,
                      ftl::ChromotingMessage&& message) override;
  void AddFtlListener(FtlListener* listener) override;
  void RemoveFtlListener(FtlListener* listener) override;

 private:
  template <typename T>
  bool Send(T&& message);

  void SetState(State state);

  Error error_ = OK;
  State state_ = CONNECTED;

  SignalingAddress address_;
  base::ObserverList<Listener, true> listeners_;
  base::ObserverList<FtlListener, true> ftl_listeners_;

  int last_id_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FAKE_FTL_SIGNAL_STRATEGY_H_
