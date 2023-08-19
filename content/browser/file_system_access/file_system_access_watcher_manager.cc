// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include <algorithm>

#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "content/browser/file_system_access/file_system_access_observer_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

FileSystemAccessWatcherManager::FileSystemAccessWatcherManager(
    FileSystemAccessManagerImpl* manager,
    base::PassKey<FileSystemAccessManagerImpl> /*pass_key*/)
    : manager_(manager) {}

FileSystemAccessWatcherManager::~FileSystemAccessWatcherManager() = default;

void FileSystemAccessWatcherManager::BindObserverHost(
    const BindingContext& binding_context,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
        host_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observer_hosts_.insert(std::make_unique<FileSystemAccessObserverHost>(
      manager_, this, binding_context, std::move(host_receiver)));
}

void FileSystemAccessWatcherManager::RemoveObserverHost(
    FileSystemAccessObserverHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  size_t count_removed = observer_hosts_.erase(host);
  CHECK_EQ(count_removed, 1u);
}

}  // namespace content
