// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_BASE_SHARED_MEMORY_SERVICE_H_
#define MOJO_CORE_IPCZ_DRIVER_BASE_SHARED_MEMORY_SERVICE_H_

#include "base/memory/writable_shared_memory_region.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/core/system_impl_export.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// BaseSharedMemoryService exposes service and client implementations for a
// lightweight shared memory allocation service. Every Mojo-invited process gets
// a client connection to a service instance in the process which sent the
// invitation.
//
// In sandboxed child processes on most platforms, base shared memory hooks are
// installed to use this client interface.
class MOJO_SYSTEM_IMPL_EXPORT BaseSharedMemoryService {
 public:
  // Creates a new instance of shared memory service to handle a client
  // connecting via `portal`. Ownership of `portal` is transferred to the
  // service instance, which manages its own lifetime.
  static void CreateService(ScopedIpczHandle portal);

  // Creates the singleton client object. This passes ownership of `portal` to
  // the client object, which will use the portal to connect to the service in
  // a remote process. In processes which cannot allocate their own shared
  // memory, both this and InstallHooks() must be called before any shared
  // memory allocation is attempted. Note that the relative ordering of these
  // calls is not important.
  static void CreateClient(ScopedIpczHandle portal);

  // Installs base shared memory hooks which direct all shared memory allocation
  // requests through the singleton client instance. In processes which cannot
  // allocate their own shared memory, both this and CreateClient() must be
  // called before any shared memory allocation is attempted. Note that the
  // relative ordering of these calls is not important.
  static void InstallHooks();

  // Indirectly allocates a new shared memory region of `size` bytes using the
  // global client object to issue a request and block on its response. If
  // allocation fails for any reason, this returns an invalid region.
  static base::WritableSharedMemoryRegion CreateWritableRegion(size_t size);
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_BASE_SHARED_MEMORY_SERVICE_H_
