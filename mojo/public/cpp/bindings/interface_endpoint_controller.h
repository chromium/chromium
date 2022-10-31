// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_

#include <stdint.h>

namespace mojo {

class Message;

// A control interface exposed by AssociatedGroupController for interface
// endpoints.
class InterfaceEndpointController {
 public:
  virtual ~InterfaceEndpointController() {}

  virtual bool SendMessage(Message* message) = 0;

  // Allows the interface endpoint to watch for incoming sync messages while
  // others perform sync handle watching on the same sequence. Please see
  // comments of SyncHandleWatcher::AllowWokenUpBySyncWatchOnSameThread().
  virtual void AllowWokenUpBySyncWatchOnSameThread() = 0;

  // Watches the interface endpoint for incoming sync messages. (It also watches
  // other other handles registered to be watched together.)
  // This method:
  //   - returns true when |should_stop| is set to true;
  //   - return false otherwise, including
  //     MultiplexRouter::DetachEndpointClient() being called for the same
  //     interface endpoint.
  virtual bool SyncWatch(const bool& should_stop) = 0;

  // Watches the endpoint for a specific incoming sync reply. This method only
  // returns true once the reply is received, or false if the endpoint is
  // detached or destroyed beforehand.
  //
  // Unlike with SyncWatch(), no other IPCs (not even other sync IPCs) can be
  // dispatched to the calling thread while SyncWatchExclusive() is waiting on
  // the reply for `request_id`.
  virtual bool SyncWatchExclusive(uint64_t request_id) = 0;

  // Notifies the controller that a specific in-flight sync message identified
  // by `request_id` has an off-thread sync waiter, so its reply must be
  // processed immediately once received.
  virtual void RegisterExternalSyncWaiter(uint64_t request_id) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
