// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "remoting/signaling/iq_sender.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {

class FakeSignalStrategy : public SignalStrategy {
 public:
  using PeerCallback =
      base::RepeatingCallback<void(SignalStrategy::Message message)>;

  // Calls ConnectTo() to connect |peer1| and |peer2|. Both |peer1| and |peer2|
  // must belong to the current thread.
  static void Connect(FakeSignalStrategy* peer1, FakeSignalStrategy* peer2);

  FakeSignalStrategy(const SignalingAddress& address);

  FakeSignalStrategy(const FakeSignalStrategy&) = delete;
  FakeSignalStrategy& operator=(const FakeSignalStrategy&) = delete;

  ~FakeSignalStrategy() override;

  const std::vector<SignalStrategy::Message>& received_messages() {
    return received_messages_;
  }

  void set_send_delay(base::TimeDelta delay) { send_delay_ = delay; }

  void SetError(Error error);
  void SetIsSignInError(bool is_sign_in_error);
  void SetState(State state);
  void SetPeerCallback(const PeerCallback& peer_callback);

  // Connects current FakeSignalStrategy to receive messages from |peer|.
  void ConnectTo(FakeSignalStrategy* peer);

  void SetLocalAddress(const SignalingAddress& address);

  // Simulate IQ messages re-ordering by swapping the delivery order of
  // next pair of messages.
  void SimulateMessageReordering();

  // If this is enabled, calling Connect() will transition the signal strategy
  // state to CONNECTING instead of CONNECTED, and caller needs to call
  // ProceedConnect() to transition to CONNECTED, or Disconnect() to transition
  // to DISCONNECTED.
  void SimulateTwoStageConnect();

  // Called by the |peer_|.
  void OnIncomingMessage(SignalStrategy::Message message);

  void ProceedConnect();

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

 private:
  template <typename T>
  bool Send(T&& message);
  static void DeliverMessageOnThread(
      scoped_refptr<base::SingleThreadTaskRunner> thread,
      base::WeakPtr<FakeSignalStrategy> target,
      SignalStrategy::Message message);

  void NotifyListeners(SignalStrategy::Message message);

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_{
      base::SingleThreadTaskRunner::GetCurrentDefault()};

  Error error_ = OK;
  bool is_sign_in_error_ = false;
  State state_ = CONNECTED;

  SignalingAddress address_;
  PeerCallback peer_callback_;
  base::ObserverList<Listener, true> listeners_;

  int last_id_ = 0;

  base::TimeDelta send_delay_;

  bool simulate_reorder_ = false;
  bool simulate_two_stage_connect_ = false;
  std::optional<SignalStrategy::Message> pending_message_;

  // All received messages, includes those still in |pending_messages_|.
  std::vector<SignalStrategy::Message> received_messages_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FakeSignalStrategy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_FAKE_SIGNAL_STRATEGY_H_
