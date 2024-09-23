// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_LISTENER_H_
#define IPC_IPC_LISTENER_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"

namespace IPC {

class Message;

// Implemented by consumers of a Channel to receive messages.
class COMPONENT_EXPORT(IPC) Listener {
 public:
  // Called when a message is received.  Returns true iff the message was
  // handled.
  virtual bool OnMessageReceived(const Message& message) = 0;

  // Called when the channel is connected and we have received the internal
  // Hello message from the peer.
  virtual void OnChannelConnected(int32_t peer_pid) {}

  // Called when an error is detected that causes the channel to close.
  // This method is not called when a channel is closed normally.
  virtual void OnChannelError() {}

  // Called when a message's deserialization failed.
  virtual void OnBadMessageReceived(const Message& message) {}

  // Called when an associated interface request is received on a Channel and
  // the Channel has no registered handler for it.
  virtual void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) {}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // Called on the server side when a channel that listens for connections
  // denies an attempt to connect.
  virtual void OnChannelDenied() {}

  // Called on the server side when a channel that listens for connections
  // has an error that causes the listening channel to close.
  virtual void OnChannelListenError() {}
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

  // Debugging helper for identifying what kind of a Listener this is.
  // TODO(crbug.com/40143346): Remove this method once the bug is fixed.
  virtual std::string ToDebugString();

 protected:
  virtual ~Listener() {}
};

}  // namespace IPC

#endif  // IPC_IPC_LISTENER_H_
