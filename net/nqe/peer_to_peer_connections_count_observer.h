// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_
#define NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_

#include "base/macros.h"
#include "net/base/net_export.h"

namespace net {

// Observes changes in the count of peer to peer connections.
class NET_EXPORT_PRIVATE PeerToPeerConnectionsCountObserver {
 public:
  // Called when there is a change in the count of peer to peer connections.
  virtual void OnPeerToPeerConnectionsCountChange(uint32_t count) = 0;

 protected:
  PeerToPeerConnectionsCountObserver() {}
  virtual ~PeerToPeerConnectionsCountObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PeerToPeerConnectionsCountObserver);
};

}  // namespace net

#endif  // NET_NQE_PEER_TO_PEER_CONNECTIONS_COUNT_OBSERVER_H_
