// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_COMMON_CLIENT_STATUS_OBSERVER_H_
#define REMOTING_CLIENT_COMMON_CLIENT_STATUS_OBSERVER_H_

#include "base/observer_list_types.h"

namespace remoting {

// Interface for client status observer.
class ClientStatusObserver : public base::CheckedObserver {
 public:
  // Called when the connection to the host failed.
  virtual void OnConnectionFailed() {}

  // Called when connected to the host.
  virtual void OnConnected() {}

  // Called when the connection to the host is disconnected.
  virtual void OnDisconnected() {}

  // Called when the client instance is being destroyed.
  virtual void OnClientDestroyed() {}
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_COMMON_CLIENT_STATUS_OBSERVER_H_
