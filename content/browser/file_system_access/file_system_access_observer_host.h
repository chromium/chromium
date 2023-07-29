// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_

#include "base/sequence_checker.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"

namespace content {
class FileSystemAccessWatcherManager;

// Stores the state associated with each FileSystemAccessObserverHost mojo
// connection.
//
// The bulk of the FileSystemObserver implementation is in the
// FileSystemAccessWatcherManager class. Each StoragePartition has a single
// associated FileSystemAccessWatcherManager instance. By contrast, each
// FileSystemAccessObserverHost mojo connection has an associated
// FileSystemAccessObserverHost instance, which stores the per-connection state.
//
// Instances of this class must be accessed exclusively on the UI thread,
// because they call into FileSystemAccessWatcherManager directly.
class FileSystemAccessObserverHost
    : public blink::mojom::FileSystemAccessObserverHost {
 public:
  using BindingContext = FileSystemAccessManagerImpl::BindingContext;

  FileSystemAccessObserverHost(
      FileSystemAccessManagerImpl* manager,
      FileSystemAccessWatcherManager* watcher_manager,
      const BindingContext& binding_context,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessObserverHost>
          host_receiver);
  ~FileSystemAccessObserverHost() override;

  // blink::mojom::FileSystemObserverHost:
  void Observe(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      bool is_recursive,
      ObserveCallback callback) override;
  void Unobserve(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token)
      override;

  const BindingContext& binding_context() const { return binding_context_; }

 private:
  void OnHostReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // The manager which owns `watcher_manager_`.
  const raw_ptr<FileSystemAccessManagerImpl> manager_;
  // The watcher manager which owns this instance.
  const raw_ptr<FileSystemAccessWatcherManager> watcher_manager_;
  const BindingContext binding_context_;

  // Mojo pipes that send file change notifications back to the renderer.
  // Each connection corresponds to a file system watch set up with `Observe()`.
  mojo::RemoteSet<blink::mojom::FileSystemAccessObserver> observer_remotes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Connection owned by a FileSystemObserver object. When the
  // FileSystemObserver is destroyed, this instance will remove itself from the
  // manager.
  // TODO(https://crbug.com/1019297): Make the lifetime not depend on GC.
  mojo::Receiver<blink::mojom::FileSystemAccessObserverHost> host_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessObserverHost> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_
