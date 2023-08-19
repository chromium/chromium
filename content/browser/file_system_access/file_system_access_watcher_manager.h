// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_entry_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"

namespace content {
class FileSystemAccessManagerImpl;
class FileSystemAccessObserverHost;

// Manages all watches to file system changes for a StoragePartition.
// Instances of this class must be accessed exclusively on the UI thread.
// Owned by the FileSystemAccessManagerImpl.
class CONTENT_EXPORT FileSystemAccessWatcherManager {
 public:
  using BindingContext = FileSystemAccessEntryFactory::BindingContext;

  FileSystemAccessWatcherManager(
      FileSystemAccessManagerImpl* manager,
      base::PassKey<FileSystemAccessManagerImpl> pass_key);
  ~FileSystemAccessWatcherManager();

  FileSystemAccessWatcherManager(FileSystemAccessWatcherManager const&) =
      delete;
  FileSystemAccessWatcherManager& operator=(
      FileSystemAccessWatcherManager const&) = delete;

  void BindObserverHost(
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
          host_receiver);
  void RemoveObserverHost(FileSystemAccessObserverHost* host);

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The manager which owns this instance.
  const raw_ptr<FileSystemAccessManagerImpl> manager_;

  base::flat_set<std::unique_ptr<FileSystemAccessObserverHost>,
                 base::UniquePtrComparator>
      observer_hosts_;

  base::WeakPtrFactory<FileSystemAccessWatcherManager> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_WATCHER_MANAGER_H_
