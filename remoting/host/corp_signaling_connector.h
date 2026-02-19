// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CORP_SIGNALING_CONNECTOR_H_
#define REMOTING_HOST_CORP_SIGNALING_CONNECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/base/network_change_notifier.h"
#include "remoting/signaling/signal_strategy.h"

namespace remoting {

// CorpSignalingConnector listens for SignalStrategy status notifications and
// attempts to keep it connected when possible. When signaling is not connected
// it keeps trying to reconnect it until it is connected. It also monitors
// network state and reconnects signaling whenever connection type changes or IP
// address changes.
class CorpSignalingConnector
    : public SignalStrategy::Listener,
      public net::NetworkChangeNotifier::NetworkChangeObserver {
 public:
  // |signal_strategy| must outlive |this|.
  explicit CorpSignalingConnector(SignalStrategy* signal_strategy);

  CorpSignalingConnector(const CorpSignalingConnector&) = delete;
  CorpSignalingConnector& operator=(const CorpSignalingConnector&) = delete;

  ~CorpSignalingConnector() override;

  void Start();

  // SignalStrategy::Listener interface.
  void OnSignalStrategyStateChange(SignalStrategy::State state) override;

  // NetworkChangeNotifier::NetworkChangeObserver interface.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

 private:
  friend class CorpSignalingConnectorTest;

  void TryReconnect(base::TimeDelta delay);
  void DoReconnect();

  raw_ptr<SignalStrategy> signal_strategy_;

  net::BackoffEntry backoff_;
  base::OneShotTimer timer_;

  // Timer to reset |backoff_|. We delay resetting the backoff so that we can
  // treat an immediate CONNECTED->DISCONNECTED transition as failure.
  base::OneShotTimer backoff_reset_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CORP_SIGNALING_CONNECTOR_H_
