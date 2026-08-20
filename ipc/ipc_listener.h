// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_LISTENER_H_
#define IPC_IPC_LISTENER_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace IPC {

// Implemented by consumers of a Channel to receive messages.
class COMPONENT_EXPORT(IPC) Listener {
 public:
  // Called when the channel is connected and we have received the internal
  // Hello message from the peer.
  virtual void OnChannelConnected(int32_t peer_pid) {}

  // Called when an error is detected that causes the channel to close.
  // This method is not called when a channel is closed normally.
  virtual void OnChannelError() {}


  // Called when an associated interface request is received on a Channel and
  // the Channel has no registered handler for it.
  virtual void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) {}

 protected:
  virtual ~Listener();
};

}  // namespace IPC

#endif  // IPC_IPC_LISTENER_H_
