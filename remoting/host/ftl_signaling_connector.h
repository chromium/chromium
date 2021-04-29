// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_FTL_SIGNALING_CONNECTOR_H_
#define REMOTING_HOST_FTL_SIGNALING_CONNECTOR_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

// FtlSignalingConnector listens for SignalStrategy status notifications
// and attempts to keep it connected when possible. When signaling is
// not connected it keeps trying to reconnect it until it is
// connected. It also monitors network state and reconnects signaling whenever
// connection type changes or IP address changes.
// TODO(yuweih): Revisit to see if DNS blackhole checking is necessary here.
class FtlSignalingConnector
    : public SignalStrategy::Listener,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // |signal_strategy| must outlive |this|.
  // The |auth_failed_callback| is called when authentication fails. The
  // singaling connector will no longer try to reconnect after this callback is
  // called.
  FtlSignalingConnector(SignalStrategy* signal_strategy,
                        base::OnceClosure auth_failed_callback);
  ~FtlSignalingConnector() override;

  void Start();

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;
  bool OnSignalStrategyIncomingStanza(
      const jingle_xmpp::XmlElement* stanza) override;

  // NetworkChangeNotifier::NetworkChangeObserver interface.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  friend class FtlSignalingConnectorTest;

  void TryReconnect(base::TimeDelta delay);
  void DoReconnect();

  SignalStrategy* signal_strategy_;
  base::OnceClosure auth_failed_callback_;

  net::BackoffEntry backoff_;
  base::OneShotTimer timer_;

  // Timer to reset |backoff_|. We delay resetting the backoff so that we can
  // treat an immediate CONNECTED->DISCONNECTED transition as failure.
  base::OneShotTimer backoff_reset_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(FtlSignalingConnector);
};

}  // namespace remoting

#endif  // REMOTING_HOST_FTL_SIGNALING_CONNECTOR_H_
