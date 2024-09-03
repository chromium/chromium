// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/sequence_checker.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer_host.mojom.h"

namespace content {
class FileSystemAccessObserverObservation;
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

  void RemoveObservation(FileSystemAccessObserverObservation* observation);

  const BindingContext& binding_context() const { return binding_context_; }
  FileSystemAccessManagerImpl* manager() const { return manager_; }
  FileSystemAccessWatcherManager* watcher_manager() const {
    return watcher_manager_;
  }

 private:
  void DidResolveTransferTokenToObserve(
      bool is_recursive,
      ObserveCallback callback,
      FileSystemAccessTransferTokenImpl* resolved_token);

  void DidCheckIfSymlinkOrJunction(
      absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                    std::unique_ptr<FileSystemAccessFileHandleImpl>> handle,
      ObserveCallback callback,
      storage::FileSystemURL url,
      bool is_recursive,
      FileSystemAccessPermissionContext::HandleType handle_type,
      bool is_symlink_or_junction);

  void DidCheckItemExists(
      absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                    std::unique_ptr<FileSystemAccessFileHandleImpl>> handle,
      ObserveCallback callback,
      storage::FileSystemURL url,
      bool is_recursive,
      base::File::Error result);

  void DidResolveTransferTokenToUnobserve(
      FileSystemAccessTransferTokenImpl* resolved_token);

  void GotObservation(
      absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                    std::unique_ptr<FileSystemAccessFileHandleImpl>> handle,
      ObserveCallback callback,
      base::expected<
          std::unique_ptr<FileSystemAccessWatcherManager::Observation>,
          blink::mojom::FileSystemAccessErrorPtr> observation_or_error);

  void OnHostReceiverDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // The manager which owns `watcher_manager_`.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;
  // The watcher manager which owns this instance.
  const raw_ptr<FileSystemAccessWatcherManager> watcher_manager_ = nullptr;
  const BindingContext binding_context_;

  // Observations which maintain mojo pipes that send file change notifications
  // back to the renderer. Each connection corresponds to a file system watch
  // set up with `Observe()`.
  base::flat_set<std::unique_ptr<FileSystemAccessObserverObservation>,
                 base::UniquePtrComparator>
      observations_;

  // Connection owned by a FileSystemObserver object. When the
  // FileSystemObserver is destroyed, this instance will remove itself from the
  // manager.
  // TODO(crbug.com/40283779): Make the lifetime not depend on GC.
  mojo::Receiver<blink::mojom::FileSystemAccessObserverHost> host_receiver_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessObserverHost> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_HOST_H_
