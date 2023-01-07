// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_
#define NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_

#include "net/base/net_export.h"

namespace net {

// Observes changes in the count of peer to peer connections.
class NET_EXPORT_PRIVATE PeerToPeerConnectionsCountObserver {
 public:
  PeerToPeerConnectionsCountObserver(
      const PeerToPeerConnectionsCountObserver&) = delete;
  PeerToPeerConnectionsCountObserver& operator=(
      const PeerToPeerConnectionsCountObserver&) = delete;

  // Called when there is a change in the count of peer to peer connections.
  virtual void OnPeerToPeerConnectionsCountChange(uint32_t count) = 0;

 protected:
  PeerToPeerConnectionsCountObserver() = default;
  virtual ~PeerToPeerConnectionsCountObserver() = default;
};

}  // namespace net

#endif  // NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_
