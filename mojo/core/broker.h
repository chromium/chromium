// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_H_
#define MOJO_CORE_BROKER_H_

#include "base/memory/writable_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/platform/platform_handle.h"

namespace mojo {
namespace core {

// The Broker is a channel to the broker process, which allows synchronous IPCs
// to fulfill shared memory allocation requests on some platforms.
class Broker {
 public:
  // Note: If |wait_for_channel_handle| is |true|, this constructor blocks the
  // calling thread until it reads first message from |handle|, which must
  // contain another PlatformHandle for a NodeChannel.
  //
  // Otherwise, no initialization message is expected and this will not wait for
  // one.
  Broker(PlatformHandle handle, bool wait_for_channel_handle);

  Broker(const Broker&) = delete;
  Broker& operator=(const Broker&) = delete;

  ~Broker();

  // Returns the platform handle that should be used to establish a NodeChannel
  // to the process which is inviting us to join its network. This is the first
  // handle read off the Broker channel upon construction.
  PlatformChannelEndpoint GetInviterEndpoint();

  // Request a shared buffer from the broker process. Blocks the current thread.
  base::WritableSharedMemoryRegion GetWritableSharedMemoryRegion(
      size_t num_bytes);

 private:
  // Handle to the broker process, used for synchronous IPCs.
  PlatformHandle sync_channel_;

  // Channel endpoint connected to the inviter process. Received in the
  // first message over |sync_channel_|.
  PlatformChannelEndpoint inviter_endpoint_;

  // Lock to only allow one sync message at a time. This avoids having to deal
  // with message ordering since we can only have one request at a time
  // in-flight.
  base::Lock lock_;
};

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_BROKER_H_
