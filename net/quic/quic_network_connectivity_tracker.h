// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_NETWORK_CONNECTIVITY_TRACKER_H_
#define NET_QUIC_QUIC_NETWORK_CONNECTIVITY_TRACKER_H_

#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"

namespace net {

// Tracks whether QUIC has ever been found to work on the current network.
// This is used by QuicSessionPool to decide how long to wait for a QUIC
// connection to succeed, and whether to attempt zero-RTT handshakes. Created
// and owned by QuicSessionPool.
class NET_EXPORT_PRIVATE QuicNetworkConnectivityTracker final
    : public NetworkChangeNotifier::IPAddressObserver {
 public:
  QuicNetworkConnectivityTracker();
  ~QuicNetworkConnectivityTracker() override;

  QuicNetworkConnectivityTracker(const QuicNetworkConnectivityTracker&) =
      delete;
  QuicNetworkConnectivityTracker& operator=(
      const QuicNetworkConnectivityTracker&) = delete;

  // Called when a QUIC connection successfully works on the current network.
  void OnQuicWorkingOnCurrentNetwork();

  // Resets HasQuicEverWorkedOnCurrentNetwork() to false.
  void ResetQuicWorkingOnCurrentNetwork();

  // Returns true if QUIC has been confirmed to work on the current network.
  bool HasQuicEverWorkedOnCurrentNetwork() const;

  // NetworkChangeNotifier::IPAddressObserver implementation:
  void OnIPAddressChanged(
      NetworkChangeNotifier::IPAddressChangeType change_type) override;

 private:
  bool has_quic_ever_worked_on_current_network_ = false;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_NETWORK_CONNECTIVITY_TRACKER_H_
