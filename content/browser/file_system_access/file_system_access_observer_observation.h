// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_OBSERVATION_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_OBSERVATION_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_system_access_watcher_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_observer.mojom.h"

namespace content {
class FileSystemAccessDirectoryHandleImpl;
class FileSystemAccessFileHandleImpl;
class FileSystemAccessObserverHost;

// Browser-side representation of a successful `FileSystemObserver.observe()`
// call from JavaScript. Forwards changes to the observed file or directory
// to a mojo pipe whose receiver is owned by the renderer.
//
// TODO(https://crbug.com/1019297): Consider removing this class in favor of
// giving the ObserverHost a FileSystemAccessObserver mojo::RemoteSet. See
// https://chromium-review.googlesource.com/c/chromium/src/+/4809069/comment/8d90508d_74ae7891/.
class FileSystemAccessObserverObservation {
 public:
  FileSystemAccessObserverObservation(
      FileSystemAccessObserverHost* host,
      std::unique_ptr<FileSystemAccessWatcherManager::Observation> observation,
      mojo::PendingRemote<blink::mojom::FileSystemAccessObserver> remote,
      absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                    std::unique_ptr<FileSystemAccessFileHandleImpl>> handle);
  ~FileSystemAccessObserverObservation();

  FileSystemAccessObserverObservation(
      FileSystemAccessObserverObservation const&) = delete;
  FileSystemAccessObserverObservation& operator=(
      FileSystemAccessObserverObservation const&) = delete;

  const storage::FileSystemURL& handle_url() const;

 private:
  void OnReceiverDisconnect();

  // Called repeatedly by `observation_`.
  void OnChanges(
      const std::list<FileSystemAccessWatcherManager::Observation::Change>&
          changes);

  SEQUENCE_CHECKER(sequence_checker_);

  // The host which owns this instance.
  const raw_ptr<FileSystemAccessObserverHost> host_ = nullptr;

  // The `FileSystemHandle` being observed.
  const absl::variant<std::unique_ptr<FileSystemAccessDirectoryHandleImpl>,
                      std::unique_ptr<FileSystemAccessFileHandleImpl>>
      handle_;

  std::unique_ptr<FileSystemAccessWatcherManager::Observation> observation_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Mojo pipes that send file change notifications back to the renderer.
  // Each connection corresponds to a file system watch set up with
  // `Observe()`.
  mojo::Remote<blink::mojom::FileSystemAccessObserver> remote_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessObserverObservation> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_OBSERVER_OBSERVATION_H_
