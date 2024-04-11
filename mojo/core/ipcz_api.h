// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_API_H_
#define MOJO_CORE_IPCZ_API_H_

#include "mojo/core/system_impl_export.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core {

// Returns a reference to the global ipcz implementation.
MOJO_SYSTEM_IMPL_EXPORT const IpczAPI& GetIpczAPI();

// Returns a handle to the global ipcz node for the calling process, as
// initialized by InitializeIpczForProcess().
MOJO_SYSTEM_IMPL_EXPORT IpczHandle GetIpczNode();

// Initializes a global ipcz node for the calling process with a set of options
// to configure the node.
struct IpczNodeOptions {
  bool is_broker;
  bool use_local_shared_memory_allocation;
  bool enable_memv2;
};
MOJO_SYSTEM_IMPL_EXPORT bool InitializeIpczNodeForProcess(
    const IpczNodeOptions& options = {});

// Used by tests to tear down global state initialized by
// InitializeIpczNodeForProcess().
MOJO_SYSTEM_IMPL_EXPORT void DestroyIpczNodeForProcess();

// Retrieves the global ipcz node options configured by a call to
// InitializeIpczNodeForProcess().
MOJO_SYSTEM_IMPL_EXPORT const IpczNodeOptions& GetIpczNodeOptions();

}  // namespace mojo::core

#endif  // MOJO_CORE_IPCZ_API_H_
