// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/sql/mock_shared_cache_client_remote.h"

#include <utility>

namespace disk_cache {

MockSharedCacheClientRemote::MockSharedCacheClientRemote() = default;

MockSharedCacheClientRemote::~MockSharedCacheClientRemote() {
  if (on_destroy_handler_) {
    std::move(on_destroy_handler_).Run();
  }
}

void MockSharedCacheClientRemote::Initialize(
    sqlite_vfs::PendingFileSet pending_file_set) {
  pending_file_set_ = std::move(pending_file_set);
  ++initialize_call_count_;
  if (initialize_quit_closure_) {
    std::move(initialize_quit_closure_).Run();
  }
}

void MockSharedCacheClientRemote::SetDisconnectHandler(
    base::OnceClosure disconnect_handler) {
  disconnect_handler_ = std::move(disconnect_handler);
  if (disconnect_handler_set_quit_closure_) {
    std::move(disconnect_handler_set_quit_closure_).Run();
  }
}

void MockSharedCacheClientRemote::WaitUntilInitialized() {
  if (initialize_called()) {
    return;
  }
  base::RunLoop run_loop;
  initialize_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockSharedCacheClientRemote::WaitUntilDisconnectHandlerSet() {
  if (disconnect_handler_) {
    return;
  }
  base::RunLoop run_loop;
  disconnect_handler_set_quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MockSharedCacheClientRemote::SetOnDestroyHandler(
    base::OnceClosure on_destroy_handler) {
  on_destroy_handler_ = std::move(on_destroy_handler);
}

}  // namespace disk_cache
