// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_SHARED_CACHE_CLIENT_REMOTE_H_
#define NET_DISK_CACHE_SQL_SHARED_CACHE_CLIENT_REMOTE_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "net/base/net_export.h"

namespace disk_cache {

// Interface for communicating with a remote client (e.g., in a renderer
// process) that accesses the shared SQLite cache database for a specific
// NetworkIsolationKey.
//
// An implementation is typically a wrapper around a Mojo remote interface
// (such as `network::mojom::SharedHttpCacheClient`). It provides file handles
// to the remote client to allow sandboxed read-only access to the database.
class NET_EXPORT SharedCacheClientRemote {
 public:
  virtual ~SharedCacheClientRemote() = default;

  // Sends the read-only file set for the isolated cache database to the remote
  // client, allowing it to initialize its local database connection.
  virtual void Initialize(sqlite_vfs::PendingFileSet pending_file_set) = 0;

  // Notifies the remote client that new resources matching the given URL
  // hashes have been added to the shared cache.
  virtual void OnResourcesAdded(const std::vector<uint32_t>& new_hashes) = 0;

  // Sets the disconnect handler callback that is invoked when the remote
  // client connection is severed.
  virtual void SetDisconnectHandler(base::OnceClosure disconnect_handler) = 0;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_SHARED_CACHE_CLIENT_REMOTE_H_
