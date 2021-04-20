// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_

#include <stdint.h>

namespace mojo {

class Message;

// Indicates how a SyncWatch call should behave.
enum class SyncWatchMode {
  // Other sync events are allowed to dispatch during this sync wait. For
  // example if an incoming sync IPC targets some other receiver bound on the
  // waiting thread, we'll allow that message to dispatch before we return to
  // waiting. This is the safer and preferred behavior, and the default for all
  // [Sync] messages.
  kAllowInterrupt,

  // The wait will only wake up once its waiting condition is met, and no other
  // messages (sync or async) will be dispatched on the waiting thread until
  // that happens and control is returned to the caller. While this is sometimes
  // desirable, it is naturally more prone to deadlocks than `kAllowInterrupt`.
  kNoInterrupt,
};

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
  virtual bool SyncWatch(SyncWatchMode mode, const bool& should_stop) = 0;

  // Notifies the controller that a specific in-flight sync message identified
  // by `request_id` has an off-thread sync waiter, so its reply must be
  // processed immediately once received.
  virtual void RegisterExternalSyncWaiter(uint64_t request_id) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_INTERFACE_ENDPOINT_CONTROLLER_H_
