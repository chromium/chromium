// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_BROKER_H_
#define MOJO_CORE_BROKER_H_

#include <stddef.h>

#include "base/files/scoped_file.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "mojo/core/system_impl_export.h"

namespace mojo::core {

// Synchronously requests shared memory regions from a BrokerHost in the broker
// process, using the protocol in broker_messages.h.
//
// Sandboxed processes on Linux and ChromeOS need it because they can't create
// writable or read-only regions themselves: base creates the read-only
// descriptor that these need by reopening the region's file (through
// /proc/self/fd for a memfd), which the sandbox doesn't allow.
class MOJO_SYSTEM_IMPL_EXPORT Broker {
 public:
  explicit Broker(base::ScopedFD socket);

  Broker(const Broker&) = delete;
  Broker& operator=(const Broker&) = delete;

  ~Broker();

  // Request a shared buffer from the broker process. Blocks the current thread.
  base::WritableSharedMemoryRegion GetWritableSharedMemoryRegion(
      size_t num_bytes);

 private:
  // Lock to only allow one sync message at a time. This avoids having to deal
  // with message ordering since we can only have one request at a time
  // in-flight.
  base::Lock lock_;

  // Socket connected to the BrokerHost in the broker process. Reset when a
  // request fails in a way that leaves the connection unusable.
  base::ScopedFD socket_ GUARDED_BY(lock_);
};

}  // namespace mojo::core

#endif  // MOJO_CORE_BROKER_H_
