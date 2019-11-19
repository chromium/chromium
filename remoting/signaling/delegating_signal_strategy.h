// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_SIGNALING_DELEGATING_SIGNAL_STRATEGY_H_
#define REMOTING_SIGNALING_DELEGATING_SIGNAL_STRATEGY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "remoting/signaling/signal_strategy.h"
#include "remoting/signaling/signaling_address.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// A signaling strategy class that delegates IQ sending and receiving.
//
// Notes on thread safety:
// 1. This object can be created on any thread.
// 2. |send_iq_callback| will always be called on the thread that it is created.
//    Note that |send_iq_callback| may be called after this object is destroyed.
// 3. The caller should invoke all methods on the SignalStrategy interface on
//    the |client_task_runner|.
// 4. All listeners will be called on |client_task_runner| as well.
// 5. The destructor should always be called on the |client_task_runner|.
// 6. As a result of (5), use MakeIncomingMessageCallback() to obtain a callback
//    when passing incoming signaling messages from the delegate.  The callback
//    can then be invoked at any thread.
class DelegatingSignalStrategy : public SignalStrategy {
 public:
  typedef base::RepeatingCallback<void(const std::string&)> IqCallback;

  DelegatingSignalStrategy(
      const SignalingAddress& local_address,
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
      const IqCallback& send_iq_callback);
  ~DelegatingSignalStrategy() override;

  IqCallback GetIncomingMessageCallback();

  // SignalStrategy interface.
  void Connect() override;
  void Disconnect() override;
  State GetState() const override;
  Error GetError() const override;
  const SignalingAddress& GetLocalAddress() const override;
  void AddListener(Listener* listener) override;
  void RemoveListener(Listener* listener) override;
  bool SendStanza(std::unique_ptr<jingle_xmpp::XmlElement> stanza) override;
  std::string GetNextId() override;

 private:
  static void OnIncomingMessageFromDelegate(
      base::WeakPtr<DelegatingSignalStrategy> weak_ptr,
      scoped_refptr<base::SingleThreadTaskRunner> client_task_runner,
      const std::string& message);

  void OnIncomingMessage(const std::string& message);

  SignalingAddress local_address_;
  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> client_task_runner_;

  IqCallback incoming_iq_callback_;
  IqCallback send_iq_callback_;
  base::ObserverList<Listener> listeners_;

  base::WeakPtrFactory<DelegatingSignalStrategy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DelegatingSignalStrategy);
};

}  // namespace remoting

#endif  // REMOTING_SIGNALING_DELEGATING_SIGNAL_STRATEGY_H_
