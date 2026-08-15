// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SQL_MOCK_SHARED_CACHE_CLIENT_REMOTE_H_
#define NET_DISK_CACHE_SQL_MOCK_SHARED_CACHE_CLIENT_REMOTE_H_

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "net/disk_cache/sql/shared_cache_client_remote.h"

namespace disk_cache {

// Mock implementation of `SharedCacheClientRemote` used in unit tests to verify
// initialization and disconnect handling.
class MockSharedCacheClientRemote : public SharedCacheClientRemote {
 public:
  MockSharedCacheClientRemote();
  ~MockSharedCacheClientRemote() override;

  MockSharedCacheClientRemote(const MockSharedCacheClientRemote&) = delete;
  MockSharedCacheClientRemote& operator=(const MockSharedCacheClientRemote&) =
      delete;

  // SharedCacheClientRemote implementation:
  void Initialize(sqlite_vfs::PendingFileSet pending_file_set) override;
  void SetDisconnectHandler(base::OnceClosure disconnect_handler) override;

  // Waits until `Initialize` has been called.
  void WaitUntilInitialized();

  // Waits until `SetDisconnectHandler` has been called.
  void WaitUntilDisconnectHandlerSet();

  // Sets a callback to run when this instance is destroyed.
  void SetOnDestroyHandler(base::OnceClosure on_destroy_handler);

  // Accessors:
  size_t initialize_call_count() const { return initialize_call_count_; }
  bool initialize_called() const { return initialize_call_count_ > 0; }
  bool has_disconnect_handler() const { return !disconnect_handler_.is_null(); }

  // Takes ownership of the `PendingFileSet` passed during `Initialize`.
  sqlite_vfs::PendingFileSet TakePendingFileSet() {
    return std::move(pending_file_set_);
  }

  // Simulates a disconnection by invoking the registered disconnect handler.
  void RunDisconnectHandler() {
    if (disconnect_handler_) {
      std::move(disconnect_handler_).Run();
    }
  }

 private:
  sqlite_vfs::PendingFileSet pending_file_set_;
  base::OnceClosure disconnect_handler_;
  base::OnceClosure on_destroy_handler_;
  size_t initialize_call_count_ = 0;
  base::OnceClosure initialize_quit_closure_;
  base::OnceClosure disconnect_handler_set_quit_closure_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SQL_MOCK_SHARED_CACHE_CLIENT_REMOTE_H_
