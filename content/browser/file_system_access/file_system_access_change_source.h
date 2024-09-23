// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_

#include <list>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_path_watcher/file_path_watcher.h"
#include "content/browser/file_system_access/file_system_access_watch_scope.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace content {

// Notifies of changes to the file system within the given `scope`.
// This class must constructed, used, and destroyed on the same sequence.
class CONTENT_EXPORT FileSystemAccessChangeSource {
 public:
  using ChangeInfo = FilePathWatcher::ChangeInfo;
  using ChangeType = FilePathWatcher::ChangeType;
  using FilePathType = FilePathWatcher::FilePathType;

  class RawChangeObserver : public base::CheckedObserver {
   public:
    // Naively notifies of all changes from the corresponding change source.
    // These events are _not_ safe to be consumed directly by components that
    // forward events to JavaScript.
    //
    // `changed_url` must be valid and within the watch scope of the notifying
    // change source.
    virtual void OnRawChange(const storage::FileSystemURL& changed_url,
                             bool error,
                             const ChangeInfo& change_info,
                             const FileSystemAccessWatchScope& scope) = 0;

    virtual void OnSourceBeingDestroyed(
        FileSystemAccessChangeSource* source) = 0;
  };

  // Constructs a change source which notifies of changes within the given
  // `scope`.
  FileSystemAccessChangeSource(
      FileSystemAccessWatchScope scope,
      scoped_refptr<storage::FileSystemContext> file_system_context);
  virtual ~FileSystemAccessChangeSource();

  void AddObserver(RawChangeObserver* observer);
  void RemoveObserver(RawChangeObserver* observer);

  // Ensures that this change source is ready to watch for changes within its
  // `scope_`. This may fail if the scope cannot be watched.
  // `on_source_initialized` is run with a error status indicating whether
  // setting up this source succeeds.
  // TODO(crbug.com/341095544): Assert that this is called before
  // notifying of changes.
  void EnsureInitialized(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_source_initialized);

  base::WeakPtr<FileSystemAccessChangeSource> AsWeakPtr();

  const FileSystemAccessWatchScope& scope() const { return scope_; }
  storage::FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

 protected:
  virtual void Initialize(
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          on_source_initialized) = 0;

  // Called by subclasses to record changes to watched paths. It is illegal for
  // a change source to pass a `changed_url` which is either invalid or not
  // within its watch scope.
  void NotifyOfChange(const storage::FileSystemURL& changed_url,
                      bool error,
                      const ChangeInfo& change_info);
  // Same as above, but more convenient for subclasses that use file paths
  // rather than FileSystemURLs. Requires that the change source's watch scope
  // has a valid root url and `relative_path` is relative.
  void NotifyOfChange(const base::FilePath& relative_path,
                      bool error,
                      const ChangeInfo& change_info);

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void DidInitialize(blink::mojom::FileSystemAccessErrorPtr result);

  const FileSystemAccessWatchScope scope_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  std::optional<blink::mojom::FileSystemAccessErrorPtr> initialization_result_;
  std::list<base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>>
      initialization_callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ObserverList<RawChangeObserver> observers_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<FileSystemAccessChangeSource> weak_factory_
      GUARDED_BY_CONTEXT(sequence_checker_){this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_CHANGE_SOURCE_H_
